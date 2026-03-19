#include "PDCMomentumReconstructor.hh"

#include "TDecompSVD.h"
#include "TMatrixD.h"
#include "TMatrixDSym.h"
#include "TMatrixDSymEigen.h"
#include "TVectorD.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>

namespace analysis::pdc::anaroot_like {
namespace {

constexpr std::size_t kResidualCount = 8;
constexpr std::size_t kParameterCount = 5;
constexpr std::size_t kMomentumDim = 3;
constexpr double kBrhoGeVOverCPerTm = 0.299792458;
constexpr double kDefaultMassMeV = 938.2720813;
constexpr double kSigma95 = 1.959963984540054;

double SafeSigma(double sigma, double fallback) {
    if (std::isfinite(sigma) && sigma > 0.0) {
        return sigma;
    }
    return fallback;
}

double SafeNorm(const TVector3& value) {
    return std::sqrt(std::max(0.0, value.Mag2()));
}

double SegmentLengthToIndex(const std::vector<ParticleTrajectory::TrajectoryPoint>& traj, int index) {
    if (traj.size() < 2 || index <= 0) {
        return 0.0;
    }
    const int capped = std::min<int>(index, static_cast<int>(traj.size()) - 1);
    double length = 0.0;
    for (int i = 1; i <= capped; ++i) {
        length += SafeNorm(traj[static_cast<std::size_t>(i)].position - traj[static_cast<std::size_t>(i - 1)].position);
    }
    return length;
}

void MatrixToArray(const TMatrixD& matrix, std::array<double, 9>* out) {
    if (!out || matrix.GetNrows() != 3 || matrix.GetNcols() != 3) {
        return;
    }
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            (*out)[static_cast<std::size_t>(row * 3 + col)] = matrix(row, col);
        }
    }
}

bool IsFiniteSymmetricCovariance(const std::array<double, 9>& values) {
    for (double value : values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

bool BuildWhiteningMatrix(const std::array<double, 9>& covariance_values,
                          std::array<double, 9>* whitening_values,
                          std::string* reason) {
    if (!whitening_values) {
        if (reason) {
            *reason = "whitening output pointer is null";
        }
        return false;
    }
    if (!IsFiniteSymmetricCovariance(covariance_values)) {
        if (reason) {
            *reason = "PDC covariance has non-finite entries";
        }
        return false;
    }

    TMatrixDSym covariance(3);
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            covariance(row, col) = covariance_values[static_cast<std::size_t>(row * 3 + col)];
        }
    }

    TMatrixDSymEigen eigen(covariance);
    const TVectorD eigen_values = eigen.GetEigenValues();
    const TMatrixD eigen_vectors = eigen.GetEigenVectors();
    TMatrixD inv_sqrt_diag(3, 3);
    inv_sqrt_diag.Zero();
    for (int i = 0; i < 3; ++i) {
        const double lambda = eigen_values(i);
        if (!std::isfinite(lambda) || lambda <= 1.0e-12) {
            if (reason) {
                *reason = "PDC covariance is not positive definite";
            }
            return false;
        }
        inv_sqrt_diag(i, i) = 1.0 / std::sqrt(lambda);
    }

    TMatrixD eigen_vectors_t(TMatrixD::kTransposed, eigen_vectors);
    const TMatrixD whitening = inv_sqrt_diag * eigen_vectors_t;
    MatrixToArray(whitening, whitening_values);
    return true;
}

std::array<double, 3> ApplyWhitening(const std::array<double, 9>& whitening, const TVector3& diff) {
    const std::array<double, 3> input{diff.X(), diff.Y(), diff.Z()};
    std::array<double, 3> output{0.0, 0.0, 0.0};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            output[static_cast<std::size_t>(row)] +=
                whitening[static_cast<std::size_t>(row * 3 + col)] * input[static_cast<std::size_t>(col)];
        }
    }
    return output;
}

double ReducedChi2(double chi2_raw, int ndf) {
    if (!std::isfinite(chi2_raw)) {
        return std::numeric_limits<double>::infinity();
    }
    return chi2_raw / std::max(1, ndf);
}

IntervalEstimate BuildGaussianInterval(double center, double sigma) {
    IntervalEstimate estimate;
    if (!std::isfinite(center) || !std::isfinite(sigma) || sigma < 0.0) {
        return estimate;
    }
    estimate.valid = true;
    estimate.center = center;
    estimate.sigma = sigma;
    estimate.lower68 = center - sigma;
    estimate.upper68 = center + sigma;
    estimate.lower95 = center - kSigma95 * sigma;
    estimate.upper95 = center + kSigma95 * sigma;
    return estimate;
}

double ConditionNumberFromSVD(TDecompSVD& svd) {
    const TVectorD sigmas = svd.GetSig();
    double sigma_max = 0.0;
    double sigma_min = std::numeric_limits<double>::infinity();
    for (int i = 0; i < sigmas.GetNrows(); ++i) {
        const double sigma = sigmas(i);
        if (!std::isfinite(sigma) || sigma <= 0.0) {
            continue;
        }
        sigma_max = std::max(sigma_max, sigma);
        sigma_min = std::min(sigma_min, sigma);
    }
    if (!(sigma_max > 0.0) || !std::isfinite(sigma_min) || sigma_min <= 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    return sigma_max / sigma_min;
}

bool InvertWithSVD(const TMatrixD& matrix, TMatrixD* inverse, double* condition_number) {
    if (!inverse || matrix.GetNrows() != matrix.GetNcols()) {
        return false;
    }
    TDecompSVD svd(matrix);
    if (condition_number) {
        *condition_number = ConditionNumberFromSVD(svd);
    }
    const int dimension = matrix.GetNrows();
    TMatrixD local_inverse(dimension, dimension);
    for (int col = 0; col < dimension; ++col) {
        TVectorD basis(dimension);
        basis.Zero();
        basis(col) = 1.0;
        Bool_t ok = kFALSE;
        const TVectorD solution = svd.Solve(basis, ok);
        if (!ok) {
            return false;
        }
        for (int row = 0; row < dimension; ++row) {
            local_inverse(row, col) = solution(row);
        }
    }
    *inverse = local_inverse;
    return true;
}

}  // namespace

bool PDCMomentumReconstructor::BuildMeasurementModel(const TargetConstraint& target,
                                                     MeasurementModel* measurement,
                                                     std::string* reason) const {
    if (!measurement) {
        if (reason) {
            *reason = "measurement model pointer is null";
        }
        return false;
    }
    *measurement = MeasurementModel{};
    if (!target.use_pdc_covariance) {
        return true;
    }

    std::string local_reason;
    if (!BuildWhiteningMatrix(target.pdc1_cov_mm2, &measurement->pdc1_whitening, &local_reason)) {
        if (reason) {
            *reason = "invalid PDC1 covariance: " + local_reason;
        }
        return false;
    }
    if (!BuildWhiteningMatrix(target.pdc2_cov_mm2, &measurement->pdc2_whitening, &local_reason)) {
        if (reason) {
            *reason = "invalid PDC2 covariance: " + local_reason;
        }
        return false;
    }
    measurement->use_covariance = true;
    return true;
}

bool PDCMomentumReconstructor::ValidateInputs(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config,
    std::string* reason
) const {
    if (!fMagneticField) {
        if (reason) *reason = "magnetic field pointer is null";
        return false;
    }
    if (!track.IsValid() || !IsFinite(track.pdc1) || !IsFinite(track.pdc2)) {
        if (reason) *reason = "track points are invalid";
        return false;
    }
    if (!IsFinite(target.target_position)) {
        if (reason) *reason = "target position is invalid";
        return false;
    }
    if (!std::isfinite(target.mass_mev) || target.mass_mev <= 0.0) {
        if (reason) *reason = "target mass is invalid";
        return false;
    }
    if (!std::isfinite(target.charge_e) || std::abs(target.charge_e) < 1.0e-9) {
        if (reason) *reason = "target charge is invalid";
        return false;
    }
    if (!std::isfinite(config.p_min_mevc) || !std::isfinite(config.p_max_mevc) || config.p_min_mevc <= 0.0 ||
        config.p_max_mevc <= config.p_min_mevc) {
        if (reason) *reason = "momentum range is invalid";
        return false;
    }
    if (config.max_iterations <= 0) {
        if (reason) *reason = "max_iterations must be positive";
        return false;
    }
    if (!std::isfinite(config.rk_step_mm) || config.rk_step_mm <= 0.0) {
        if (reason) *reason = "rk_step_mm must be positive";
        return false;
    }
    return true;
}

PDCMomentumReconstructor::ParameterState PDCMomentumReconstructor::BuildInitialState(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config
) const {
    const double dist1 = SafeNorm(track.pdc1 - target.target_position);
    const double dist2 = SafeNorm(track.pdc2 - target.target_position);
    const TVector3 farther = (dist1 > dist2) ? track.pdc1 : track.pdc2;
    TVector3 init_dir = farther - target.target_position;
    if (init_dir.Mag2() < 1.0e-12) {
        init_dir = track.pdc2 - track.pdc1;
    }
    if (init_dir.Mag2() < 1.0e-12) {
        init_dir.SetXYZ(0.0, 0.0, 1.0);
    }
    init_dir = init_dir.Unit();
    if (std::abs(init_dir.Z()) < 1.0e-6) {
        init_dir.SetZ((init_dir.Z() >= 0.0) ? 1.0e-6 : -1.0e-6);
        init_dir = init_dir.Unit();
    }

    const double p_init = Clamp(config.initial_p_mevc, config.p_min_mevc, config.p_max_mevc);
    ParameterState state;
    state.dx = 0.0;
    state.dy = 0.0;
    state.u = init_dir.X() / init_dir.Z();
    state.v = init_dir.Y() / init_dir.Z();
    state.q = target.charge_e / p_init;
    return ClampState(state, target, config);
}

PDCMomentumReconstructor::ParameterState PDCMomentumReconstructor::ClampState(
    const ParameterState& state,
    const TargetConstraint& target,
    const RecoConfig& config
) const {
    ParameterState clipped = state;
    const double invp_min = std::abs(target.charge_e) / config.p_max_mevc;
    const double invp_max = std::abs(target.charge_e) / config.p_min_mevc;
    const double q_sign = (target.charge_e >= 0.0) ? 1.0 : -1.0;

    clipped.dx = Clamp(clipped.dx, -200.0, 200.0);
    clipped.dy = Clamp(clipped.dy, -200.0, 200.0);
    clipped.u = Clamp(clipped.u, -2.5, 2.5);
    clipped.v = Clamp(clipped.v, -2.5, 2.5);

    double q_abs = std::abs(clipped.q);
    if (!std::isfinite(q_abs) || q_abs < invp_min) q_abs = invp_min;
    if (q_abs > invp_max) q_abs = invp_max;
    clipped.q = q_sign * q_abs;
    return clipped;
}

TVector3 PDCMomentumReconstructor::BuildMomentumVector(const ParameterState& state, double charge_e) const {
    const double q_abs = std::abs(state.q);
    if (!std::isfinite(q_abs) || q_abs < 1.0e-12) {
        return TVector3(0.0, 0.0, 0.0);
    }
    const double p_mag = std::abs(charge_e) / q_abs;
    if (!std::isfinite(p_mag) || p_mag <= 0.0) {
        return TVector3(0.0, 0.0, 0.0);
    }

    const double denom = std::sqrt(1.0 + state.u * state.u + state.v * state.v);
    if (!std::isfinite(denom) || denom <= 1.0e-12) {
        return TVector3(0.0, 0.0, 0.0);
    }

    const double pz = p_mag / denom;
    return TVector3(state.u * pz, state.v * pz, pz);
}

PDCMomentumReconstructor::RkFitLayout PDCMomentumReconstructor::BuildRkFitLayout(const RecoConfig& config) {
    RkFitLayout layout;
    if (config.rk_fit_mode == RkFitMode::kFixedTargetPdcOnly) {
        layout.active_parameter_indices = {2, 3, 4, 0, 0};
        layout.parameter_count = 3;
        layout.residual_count = 6;
        layout.include_target_xy_prior = false;
        layout.fixed_target_position = true;
    }
    return layout;
}

double PDCMomentumReconstructor::ResidualChi2Raw(const std::array<double, 8>& residuals, int residual_count) {
    double sum = 0.0;
    const int count = std::max(0, std::min<int>(residual_count, static_cast<int>(residuals.size())));
    for (int i = 0; i < count; ++i) {
        sum += residuals[static_cast<std::size_t>(i)] * residuals[static_cast<std::size_t>(i)];
    }
    return sum;
}

double PDCMomentumReconstructor::ResidualChi2Reduced(const std::array<double, 8>& residuals,
                                                     int residual_count,
                                                     int ndf) {
    const double raw = ResidualChi2Raw(residuals, residual_count);
    return ReducedChi2(raw, ndf);
}

PDCMomentumReconstructor::EvalResult PDCMomentumReconstructor::EvaluateState(
    const ParameterState& state,
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config,
    const MeasurementModel& measurement,
    const RkFitLayout& layout
) const {
    EvalResult eval;
    const TVector3 start_pos = layout.fixed_target_position
        ? target.target_position
        : TVector3(target.target_position.X() + state.dx, target.target_position.Y() + state.dy, target.target_position.Z());
    const TVector3 momentum = BuildMomentumVector(state, target.charge_e);
    if (!IsFinite(start_pos) || momentum.Mag2() < 1.0e-12 || !IsFinite(momentum)) {
        return eval;
    }

    const double mass = std::isfinite(target.mass_mev) ? target.mass_mev : kDefaultMassMeV;
    const double energy = std::sqrt(momentum.Mag2() + mass * mass);
    TLorentzVector p4(momentum.X(), momentum.Y(), momentum.Z(), energy);
    if (!IsFinite(p4)) {
        return eval;
    }

    ParticleTrajectory tracer(fMagneticField);
    tracer.SetStepSize(config.rk_step_mm);
    // [EN] Extend trajectory length enough to intersect both PDC planes even under strong bending. / [CN] 延长轨迹长度以保证在强弯转情况下仍能穿过两个PDC平面。
    const double far_dist = std::max(SafeNorm(track.pdc1 - start_pos), SafeNorm(track.pdc2 - start_pos));
    tracer.SetMaxDistance(std::max(1500.0, far_dist + 1500.0));
    tracer.SetMaxTime(400.0);
    tracer.SetMinMomentum(5.0);

    eval.trajectory = tracer.CalculateTrajectory(start_pos, p4, target.charge_e, mass);
    if (eval.trajectory.size() < 2) {
        return eval;
    }

    double best1 = std::numeric_limits<double>::infinity();
    double best2 = std::numeric_limits<double>::infinity();
    int idx1 = -1;
    int idx2 = -1;

    for (int i = 0; i < static_cast<int>(eval.trajectory.size()); ++i) {
        const auto& point = eval.trajectory[static_cast<std::size_t>(i)];
        const double d1 = (point.position - track.pdc1).Mag2();
        const double d2 = (point.position - track.pdc2).Mag2();
        if (d1 < best1) {
            best1 = d1;
            idx1 = i;
        }
        if (d2 < best2) {
            best2 = d2;
            idx2 = i;
        }
    }

    if (idx1 < 0 || idx2 < 0) {
        return eval;
    }

    eval.idx_pdc1 = idx1;
    eval.idx_pdc2 = idx2;

    const TVector3 diff1 = eval.trajectory[static_cast<std::size_t>(idx1)].position - track.pdc1;
    const TVector3 diff2 = eval.trajectory[static_cast<std::size_t>(idx2)].position - track.pdc2;

    const double pdc_sigma = SafeSigma(target.pdc_sigma_mm, 2.0);
    const double target_sigma = SafeSigma(target.target_sigma_xy_mm, 5.0);
    if (measurement.use_covariance) {
        const std::array<double, 3> residual1 = ApplyWhitening(measurement.pdc1_whitening, diff1);
        const std::array<double, 3> residual2 = ApplyWhitening(measurement.pdc2_whitening, diff2);
        eval.residuals[0] = residual1[0];
        eval.residuals[1] = residual1[1];
        eval.residuals[2] = residual1[2];
        eval.residuals[3] = residual2[0];
        eval.residuals[4] = residual2[1];
        eval.residuals[5] = residual2[2];
        eval.used_measurement_covariance = true;
    } else {
        eval.residuals[0] = diff1.X() / pdc_sigma;
        eval.residuals[1] = diff1.Y() / pdc_sigma;
        eval.residuals[2] = diff1.Z() / pdc_sigma;
        eval.residuals[3] = diff2.X() / pdc_sigma;
        eval.residuals[4] = diff2.Y() / pdc_sigma;
        eval.residuals[5] = diff2.Z() / pdc_sigma;
        eval.used_measurement_covariance = false;
    }
    if (layout.include_target_xy_prior) {
        // [EN] Anchor target offsets to avoid drifting into unconstrained solutions in 5D fit. / [CN] 给靶点偏移加约束，避免5维拟合漂移到欠约束解。
        eval.residuals[6] = state.dx / target_sigma;
        eval.residuals[7] = state.dy / target_sigma;
    }

    eval.residual_count = layout.residual_count;
    eval.ndf = layout.residual_count - layout.parameter_count;
    eval.chi2_raw = ResidualChi2Raw(eval.residuals, eval.residual_count);
    eval.chi2_reduced = ResidualChi2Reduced(eval.residuals, eval.residual_count, eval.ndf);
    eval.min_distance_mm = std::max(std::sqrt(best1), std::sqrt(best2));

    const double d_target_1 = SafeNorm(track.pdc1 - target.target_position);
    const double d_target_2 = SafeNorm(track.pdc2 - target.target_position);
    const int idx_far = (d_target_1 > d_target_2) ? idx1 : idx2;
    eval.path_length_mm = SegmentLengthToIndex(eval.trajectory, idx_far);

    eval.p4_at_target = p4;
    if (std::abs(target.charge_e) > 1.0e-12) {
        eval.brho_tm = (p4.P() / 1000.0) / (kBrhoGeVOverCPerTm * std::abs(target.charge_e));
    } else {
        eval.brho_tm = std::numeric_limits<double>::quiet_NaN();
    }
    eval.valid = true;
    return eval;
}

RecoResult PDCMomentumReconstructor::ReconstructRK(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config
) const {
    RecoResult result;
    result.method_used = SolveMethod::kRungeKutta;

    std::string reason;
    if (!ValidateInputs(track, target, config, &reason)) {
        result.status = SolverStatus::kInvalidInput;
        result.message = reason;
        return result;
    }

    MeasurementModel measurement;
    if (!BuildMeasurementModel(target, &measurement, &reason)) {
        result.status = SolverStatus::kInvalidInput;
        result.message = reason;
        return result;
    }

    const RkFitLayout layout = BuildRkFitLayout(config);
    if (layout.parameter_count <= 0 || layout.parameter_count > static_cast<int>(kParameterCount) ||
        layout.residual_count <= 0 || layout.residual_count > static_cast<int>(kResidualCount)) {
        result.status = SolverStatus::kInvalidInput;
        result.message = "invalid RK fit layout";
        return result;
    }

    const ParameterState initial_state = BuildInitialState(track, target, config);
    ParameterState state = initial_state;

    auto get_parameter = [](const ParameterState& parameters, int full_index) {
        if (full_index == 0) return parameters.dx;
        if (full_index == 1) return parameters.dy;
        if (full_index == 2) return parameters.u;
        if (full_index == 3) return parameters.v;
        return parameters.q;
    };

    auto add_delta = [](const ParameterState& parameters, int full_index, double delta) {
        ParameterState shifted = parameters;
        if (full_index == 0) shifted.dx += delta;
        if (full_index == 1) shifted.dy += delta;
        if (full_index == 2) shifted.u += delta;
        if (full_index == 3) shifted.v += delta;
        if (full_index == 4) shifted.q += delta;
        return shifted;
    };

    auto parameter_step = [](const ParameterState& parameters, int full_index) {
        if (full_index == 0 || full_index == 1) return 0.2;
        if (full_index == 2 || full_index == 3) return 1.0e-3;
        return std::max(1.0e-6, std::abs(parameters.q) * 2.0e-3);
    };

    auto find_local_parameter = [&](int full_index) {
        for (int local = 0; local < layout.parameter_count; ++local) {
            if (layout.active_parameter_indices[static_cast<std::size_t>(local)] == full_index) {
                return local;
            }
        }
        return -1;
    };

    auto build_residual_jacobian =
        [&](const ParameterState& reference_state, const EvalResult& reference_eval, TMatrixD* jacobian) {
            if (!jacobian) {
                return false;
            }
            jacobian->ResizeTo(layout.residual_count, layout.parameter_count);
            jacobian->Zero();
            for (int local_j = 0; local_j < layout.parameter_count; ++local_j) {
                const int full_j = layout.active_parameter_indices[static_cast<std::size_t>(local_j)];
                const double step = parameter_step(reference_state, full_j);
                ParameterState plus = add_delta(reference_state, full_j, step);
                ParameterState minus = add_delta(reference_state, full_j, -step);
                plus = ClampState(plus, target, config);
                minus = ClampState(minus, target, config);

                const double plus_value = get_parameter(plus, full_j);
                const double minus_value = get_parameter(minus, full_j);
                const double denominator = plus_value - minus_value;
                if (!std::isfinite(denominator) || std::abs(denominator) < 1.0e-12) {
                    continue;
                }

                const EvalResult eval_plus = EvaluateState(plus, track, target, config, measurement, layout);
                const EvalResult eval_minus = EvaluateState(minus, track, target, config, measurement, layout);
                if (eval_plus.valid && eval_minus.valid) {
                    for (int i = 0; i < reference_eval.residual_count; ++i) {
                        (*jacobian)(i, local_j) =
                            (eval_plus.residuals[static_cast<std::size_t>(i)] -
                             eval_minus.residuals[static_cast<std::size_t>(i)]) / denominator;
                    }
                    continue;
                }

                if (!eval_plus.valid) {
                    continue;
                }

                const double forward_denominator = plus_value - get_parameter(reference_state, full_j);
                if (!std::isfinite(forward_denominator) || std::abs(forward_denominator) < 1.0e-12) {
                    continue;
                }
                for (int i = 0; i < reference_eval.residual_count; ++i) {
                    (*jacobian)(i, local_j) =
                        (eval_plus.residuals[static_cast<std::size_t>(i)] -
                         reference_eval.residuals[static_cast<std::size_t>(i)]) / forward_denominator;
                }
            }
            return true;
        };

    auto build_normal_matrix = [&](const TMatrixD& jacobian) {
        TMatrixD normal(layout.parameter_count, layout.parameter_count);
        normal.Zero();
        for (int i = 0; i < jacobian.GetNrows(); ++i) {
            for (int j = 0; j < jacobian.GetNcols(); ++j) {
                for (int k = 0; k < jacobian.GetNcols(); ++k) {
                    normal(j, k) += jacobian(i, j) * jacobian(i, k);
                }
            }
        }
        return normal;
    };

    auto build_gradient = [&](const TMatrixD& jacobian, const EvalResult& eval) {
        TVectorD gradient(layout.parameter_count);
        gradient.Zero();
        for (int i = 0; i < eval.residual_count; ++i) {
            for (int j = 0; j < layout.parameter_count; ++j) {
                gradient(j) += jacobian(i, j) * eval.residuals[static_cast<std::size_t>(i)];
            }
        }
        return gradient;
    };

    auto build_momentum_jacobian = [&](const ParameterState& reference_state) {
        TMatrixD jacobian(static_cast<int>(kMomentumDim), layout.parameter_count);
        jacobian.Zero();
        const TVector3 base_momentum = BuildMomentumVector(reference_state, target.charge_e);
        for (int local_j = 0; local_j < layout.parameter_count; ++local_j) {
            const int full_j = layout.active_parameter_indices[static_cast<std::size_t>(local_j)];
            const double step = parameter_step(reference_state, full_j);
            ParameterState plus = add_delta(reference_state, full_j, step);
            ParameterState minus = add_delta(reference_state, full_j, -step);
            plus = ClampState(plus, target, config);
            minus = ClampState(minus, target, config);

            const double plus_value = get_parameter(plus, full_j);
            const double minus_value = get_parameter(minus, full_j);
            double denominator = plus_value - minus_value;
            TVector3 delta_momentum(0.0, 0.0, 0.0);

            if (std::isfinite(denominator) && std::abs(denominator) >= 1.0e-12) {
                const TVector3 plus_momentum = BuildMomentumVector(plus, target.charge_e);
                const TVector3 minus_momentum = BuildMomentumVector(minus, target.charge_e);
                delta_momentum = plus_momentum - minus_momentum;
            } else {
                const TVector3 plus_momentum = BuildMomentumVector(plus, target.charge_e);
                denominator = plus_value - get_parameter(reference_state, full_j);
                if (!std::isfinite(denominator) || std::abs(denominator) < 1.0e-12) {
                    continue;
                }
                delta_momentum = plus_momentum - base_momentum;
            }

            jacobian(0, local_j) = delta_momentum.X() / denominator;
            jacobian(1, local_j) = delta_momentum.Y() / denominator;
            jacobian(2, local_j) = delta_momentum.Z() / denominator;
        }
        return jacobian;
    };

    auto build_prior_precision = [&]() {
        TMatrixD precision(layout.parameter_count, layout.parameter_count);
        precision.Zero();
        const double sigma_u = SafeSigma(config.posterior_u_sigma, 0.25);
        const double sigma_v = SafeSigma(config.posterior_v_sigma, 0.25);
        const double q_sigma =
            std::max(std::abs(initial_state.q) * std::max(config.posterior_q_rel_sigma, 1.0e-3),
                     std::abs(target.charge_e) / std::max(config.p_max_mevc, config.p_min_mevc));
        const int u_local = find_local_parameter(2);
        const int v_local = find_local_parameter(3);
        const int q_local = find_local_parameter(4);
        if (u_local >= 0 && sigma_u > 0.0) {
            precision(u_local, u_local) = 1.0 / (sigma_u * sigma_u);
        }
        if (v_local >= 0 && sigma_v > 0.0) {
            precision(v_local, v_local) = 1.0 / (sigma_v * sigma_v);
        }
        if (q_local >= 0 && q_sigma > 0.0) {
            precision(q_local, q_local) = 1.0 / (q_sigma * q_sigma);
        }
        return precision;
    };

    auto store_state_covariance = [&](const TMatrixD& state_covariance, std::array<double, 25>* out) {
        if (!out) {
            return;
        }
        out->fill(0.0);
        for (int local_row = 0; local_row < layout.parameter_count; ++local_row) {
            const int full_row = layout.active_parameter_indices[static_cast<std::size_t>(local_row)];
            for (int local_col = 0; local_col < layout.parameter_count; ++local_col) {
                const int full_col = layout.active_parameter_indices[static_cast<std::size_t>(local_col)];
                (*out)[static_cast<std::size_t>(full_row * static_cast<int>(kParameterCount) + full_col)] =
                    state_covariance(local_row, local_col);
            }
        }
    };

    auto fill_momentum_uncertainty =
        [&](const ParameterState& parameter_state,
            const TMatrixD& state_covariance,
            std::array<double, 9>* covariance_out,
            IntervalEstimate* px_interval,
            IntervalEstimate* py_interval,
            IntervalEstimate* pz_interval,
            IntervalEstimate* p_interval) {
            const TMatrixD momentum_jacobian = build_momentum_jacobian(parameter_state);
            TMatrixD momentum_covariance =
                momentum_jacobian * state_covariance * TMatrixD(TMatrixD::kTransposed, momentum_jacobian);
            if (covariance_out) {
                MatrixToArray(momentum_covariance, covariance_out);
            }

            const TVector3 momentum = BuildMomentumVector(parameter_state, target.charge_e);
            const double sigma_px = (momentum_covariance(0, 0) > 0.0) ? std::sqrt(momentum_covariance(0, 0)) : 0.0;
            const double sigma_py = (momentum_covariance(1, 1) > 0.0) ? std::sqrt(momentum_covariance(1, 1)) : 0.0;
            const double sigma_pz = (momentum_covariance(2, 2) > 0.0) ? std::sqrt(momentum_covariance(2, 2)) : 0.0;
            if (px_interval) *px_interval = BuildGaussianInterval(momentum.X(), sigma_px);
            if (py_interval) *py_interval = BuildGaussianInterval(momentum.Y(), sigma_py);
            if (pz_interval) *pz_interval = BuildGaussianInterval(momentum.Z(), sigma_pz);

            if (p_interval) {
                const double p_mag = momentum.Mag();
                if (p_mag > 1.0e-12) {
                    TVectorD grad(static_cast<int>(kMomentumDim));
                    grad(0) = momentum.X() / p_mag;
                    grad(1) = momentum.Y() / p_mag;
                    grad(2) = momentum.Z() / p_mag;
                    const double variance_p =
                        grad(0) * (momentum_covariance(0, 0) * grad(0) + momentum_covariance(0, 1) * grad(1) + momentum_covariance(0, 2) * grad(2)) +
                        grad(1) * (momentum_covariance(1, 0) * grad(0) + momentum_covariance(1, 1) * grad(1) + momentum_covariance(1, 2) * grad(2)) +
                        grad(2) * (momentum_covariance(2, 0) * grad(0) + momentum_covariance(2, 1) * grad(1) + momentum_covariance(2, 2) * grad(2));
                    *p_interval = BuildGaussianInterval(p_mag, (variance_p > 0.0) ? std::sqrt(variance_p) : 0.0);
                }
            }
        };

    EvalResult current = EvaluateState(state, track, target, config, measurement, layout);
    if (!current.valid) {
        result.status = SolverStatus::kNotConverged;
        result.message = "initial RK evaluation failed";
        return result;
    }

    double lambda = Clamp(config.lm_lambda_init, config.lm_lambda_min, config.lm_lambda_max);
    int accepted_iterations = 0;

    for (int iter = 0; iter < config.max_iterations; ++iter) {
        TMatrixD jacobian(layout.residual_count, layout.parameter_count);
        build_residual_jacobian(state, current, &jacobian);
        TMatrixD normal = build_normal_matrix(jacobian);
        TVectorD gradient = build_gradient(jacobian, current);
        for (int j = 0; j < layout.parameter_count; ++j) {
            normal(j, j) += lambda * std::max(1.0, normal(j, j));
        }

        TVectorD rhs(layout.parameter_count);
        for (int j = 0; j < layout.parameter_count; ++j) {
            rhs(j) = -gradient(j);
        }

        TDecompSVD solver(normal);
        Bool_t ok = kFALSE;
        const TVectorD delta = solver.Solve(rhs, ok);
        if (!ok) {
            lambda = std::min(config.lm_lambda_max, lambda * 10.0);
            continue;
        }

        ParameterState candidate = state;
        for (int local_j = 0; local_j < layout.parameter_count; ++local_j) {
            const int full_j = layout.active_parameter_indices[static_cast<std::size_t>(local_j)];
            if (full_j == 0) candidate.dx += delta(local_j);
            if (full_j == 1) candidate.dy += delta(local_j);
            if (full_j == 2) candidate.u += delta(local_j);
            if (full_j == 3) candidate.v += delta(local_j);
            if (full_j == 4) candidate.q += delta(local_j);
        }
        candidate = ClampState(candidate, target, config);

        const EvalResult candidate_eval = EvaluateState(candidate, track, target, config, measurement, layout);
        if (candidate_eval.valid && candidate_eval.chi2_raw < current.chi2_raw) {
            const double improvement = current.chi2_raw - candidate_eval.chi2_raw;
            state = candidate;
            current = candidate_eval;
            lambda = std::max(config.lm_lambda_min, lambda / 3.0);
            ++accepted_iterations;
            double delta_norm_sq = 0.0;
            for (int local_j = 0; local_j < layout.parameter_count; ++local_j) {
                delta_norm_sq += delta(local_j) * delta(local_j);
            }
            const double delta_norm = std::sqrt(delta_norm_sq);
            if (improvement < 1.0e-8 || delta_norm < 1.0e-5) {
                break;
            }
        } else {
            lambda = std::min(config.lm_lambda_max, lambda * 4.0);
            if (lambda >= config.lm_lambda_max) {
                break;
            }
        }
    }

    result.iterations = accepted_iterations;
    result.chi2 = current.chi2_reduced;
    result.chi2_raw = current.chi2_raw;
    result.chi2_reduced = current.chi2_reduced;
    result.ndf = current.ndf;
    result.used_measurement_covariance = current.used_measurement_covariance;
    result.min_distance_mm = current.min_distance_mm;
    result.path_length_mm = current.path_length_mm;
    result.brho_tm = current.brho_tm;
    result.p4_at_target = current.p4_at_target;
    result.fit_start_position = layout.fixed_target_position
        ? target.target_position
        : TVector3(target.target_position.X() + state.dx,
                   target.target_position.Y() + state.dy,
                   target.target_position.Z());

    if (!current.valid || !IsFinite(result.p4_at_target)) {
        result.status = SolverStatus::kNotConverged;
        result.message = "RK evaluation became invalid";
        return result;
    }

    if (config.compute_uncertainty) {
        TMatrixD jacobian(layout.residual_count, layout.parameter_count);
        if (build_residual_jacobian(state, current, &jacobian)) {
            const TMatrixD normal = build_normal_matrix(jacobian);
            TMatrixD state_covariance(layout.parameter_count, layout.parameter_count);
            double condition_number = std::numeric_limits<double>::quiet_NaN();
            if (InvertWithSVD(normal, &state_covariance, &condition_number)) {
                result.normal_condition_number = condition_number;
                store_state_covariance(state_covariance, &result.state_covariance);
                fill_momentum_uncertainty(state,
                                          state_covariance,
                                          &result.momentum_covariance,
                                          &result.px_interval,
                                          &result.py_interval,
                                          &result.pz_interval,
                                          &result.p_interval);
                result.uncertainty_valid =
                    result.px_interval.valid && result.py_interval.valid && result.pz_interval.valid && result.p_interval.valid;

                if (config.compute_posterior_laplace) {
                    TMatrixD posterior_normal = normal;
                    posterior_normal += build_prior_precision();
                    TMatrixD posterior_state_covariance(layout.parameter_count, layout.parameter_count);
                    double posterior_condition_number = std::numeric_limits<double>::quiet_NaN();
                    if (InvertWithSVD(posterior_normal, &posterior_state_covariance, &posterior_condition_number)) {
                        fill_momentum_uncertainty(state,
                                                  posterior_state_covariance,
                                                  &result.posterior_momentum_covariance,
                                                  &result.px_credible,
                                                  &result.py_credible,
                                                  &result.pz_credible,
                                                  &result.p_credible);
                        result.posterior_valid =
                            result.px_credible.valid && result.py_credible.valid &&
                            result.pz_credible.valid && result.p_credible.valid;
                        if (!std::isfinite(result.normal_condition_number) && std::isfinite(posterior_condition_number)) {
                            result.normal_condition_number = posterior_condition_number;
                        }
                    }
                }
            }
        }
    }

    if (current.min_distance_mm <= config.tolerance_mm) {
        result.status = SolverStatus::kSuccess;
        result.message = "RK fit converged";
    } else {
        result.status = SolverStatus::kNotConverged;
        result.message = "RK fit reached iteration limit";
    }
    return result;
}

}  // namespace analysis::pdc::anaroot_like
