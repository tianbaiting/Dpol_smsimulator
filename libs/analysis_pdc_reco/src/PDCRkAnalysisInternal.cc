#include "PDCRkAnalysisInternal.hh"

#include "ParticleTrajectory.hh"

#include "TDecompSVD.h"
#include "TMatrixDSym.h"
#include "TMatrixDSymEigen.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace analysis::pdc::anaroot_like::detail {
namespace {

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
            *reason = "covariance has non-finite entries";
        }
        return false;
    }

    TMatrixDSym covariance(3);
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            covariance(row, col) =
                covariance_values[static_cast<std::size_t>(row * 3 + col)];
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
                *reason = "covariance is not positive definite";
            }
            return false;
        }
        inv_sqrt_diag(i, i) = 1.0 / std::sqrt(lambda);
    }

    const TMatrixD whitening = inv_sqrt_diag * TMatrixD(TMatrixD::kTransposed, eigen_vectors);
    MatrixToArray(whitening, whitening_values);
    return true;
}

std::array<double, 3> ApplyWhitening(const std::array<double, 9>& whitening,
                                     const TVector3& diff) {
    const std::array<double, 3> input{diff.X(), diff.Y(), diff.Z()};
    std::array<double, 3> output{0.0, 0.0, 0.0};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            output[static_cast<std::size_t>(row)] +=
                whitening[static_cast<std::size_t>(row * 3 + col)] *
                input[static_cast<std::size_t>(col)];
        }
    }
    return output;
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

double SegmentLengthToIndex(const std::vector<ParticleTrajectory::TrajectoryPoint>& traj,
                            int index) {
    if (traj.size() < 2 || index <= 0) {
        return 0.0;
    }
    const int capped = std::min<int>(index, static_cast<int>(traj.size()) - 1);
    double length = 0.0;
    for (int i = 1; i <= capped; ++i) {
        length += SafeNorm(
            traj[static_cast<std::size_t>(i)].position -
            traj[static_cast<std::size_t>(i - 1)].position
        );
    }
    return length;
}

double ComputeBrhoTm(const TLorentzVector& p4, double charge_e) {
    if (!std::isfinite(p4.P()) || p4.P() <= 0.0 ||
        std::abs(charge_e) <= 1.0e-12) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return (p4.P() / 1000.0) /
           (kBrhoGeVOverCPerTm * std::abs(charge_e));
}

}  // namespace

bool IsFinite(const TVector3& value) {
    return std::isfinite(value.X()) && std::isfinite(value.Y()) &&
           std::isfinite(value.Z());
}

bool IsFinite(const TLorentzVector& value) {
    return std::isfinite(value.Px()) && std::isfinite(value.Py()) &&
           std::isfinite(value.Pz()) && std::isfinite(value.E());
}

double SafeSigma(double sigma, double fallback) {
    if (std::isfinite(sigma) && sigma > 0.0) {
        return sigma;
    }
    return fallback;
}

double SafeNorm(const TVector3& value) {
    return std::sqrt(std::max(0.0, value.Mag2()));
}

double Clamp(double value, double lower, double upper) {
    if (!std::isfinite(value)) {
        return lower;
    }
    return std::max(lower, std::min(value, upper));
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

RkLeastSquaresAnalyzer::RkLeastSquaresAnalyzer(MagneticField* magnetic_field,
                                               const PDCInputTrack& track,
                                               const TargetConstraint& target,
                                               const RecoConfig& config)
    : fMagneticField(magnetic_field),
      fTrack(track),
      fTarget(target),
      fConfig(config),
      fLayout(BuildRkFitLayout()) {}

bool RkLeastSquaresAnalyzer::ValidateInputs(std::string* reason) const {
    if (!fMagneticField) {
        if (reason) *reason = "magnetic field pointer is null";
        return false;
    }
    if (!fTrack.IsValid() || !IsFinite(fTrack.pdc1) || !IsFinite(fTrack.pdc2)) {
        if (reason) *reason = "track points are invalid";
        return false;
    }
    if (!IsFinite(fTarget.target_position)) {
        if (reason) *reason = "target position is invalid";
        return false;
    }
    if (!std::isfinite(fTarget.mass_mev) || fTarget.mass_mev <= 0.0) {
        if (reason) *reason = "target mass is invalid";
        return false;
    }
    if (!std::isfinite(fTarget.charge_e) || std::abs(fTarget.charge_e) < 1.0e-9) {
        if (reason) *reason = "target charge is invalid";
        return false;
    }
    if (!std::isfinite(fConfig.p_min_mevc) || !std::isfinite(fConfig.p_max_mevc) ||
        fConfig.p_min_mevc <= 0.0 || fConfig.p_max_mevc <= fConfig.p_min_mevc) {
        if (reason) *reason = "momentum range is invalid";
        return false;
    }
    if (fConfig.max_iterations <= 0) {
        if (reason) *reason = "max_iterations must be positive";
        return false;
    }
    if (!std::isfinite(fConfig.rk_step_mm) || fConfig.rk_step_mm <= 0.0) {
        if (reason) *reason = "rk_step_mm must be positive";
        return false;
    }
    if (!SupportsCurrentMode()) {
        if (reason) *reason = "RK least-squares analysis does not support two-point-backprop mode";
        return false;
    }
    return true;
}

bool RkLeastSquaresAnalyzer::BuildMeasurementModel(std::string* reason) {
    fMeasurement = RkMeasurementModel{};
    if (!fTarget.use_pdc_covariance) {
        return true;
    }

    std::string local_reason;
    if (!BuildWhiteningMatrix(fTarget.pdc1_cov_mm2, &fMeasurement.pdc1_whitening, &local_reason)) {
        if (reason) {
            *reason = "invalid PDC1 covariance: " + local_reason;
        }
        return false;
    }
    if (!BuildWhiteningMatrix(fTarget.pdc2_cov_mm2, &fMeasurement.pdc2_whitening, &local_reason)) {
        if (reason) {
            *reason = "invalid PDC2 covariance: " + local_reason;
        }
        return false;
    }
    fMeasurement.use_covariance = true;
    return true;
}

RkFitLayout RkLeastSquaresAnalyzer::BuildRkFitLayout() const {
    RkFitLayout layout;
    if (fConfig.rk_fit_mode == RkFitMode::kFixedTargetPdcOnly) {
        layout.active_parameter_indices = {2, 3, 4, 0, 0};
        layout.parameter_count = 3;
        layout.residual_count = 6;
        layout.include_start_xy_constraint = false;
        layout.fixed_target_position = true;
    }
    return layout;
}

bool RkLeastSquaresAnalyzer::Initialize(std::string* reason) {
    if (!ValidateInputs(reason)) {
        return false;
    }
    if (!BuildMeasurementModel(reason)) {
        return false;
    }
    fInitialState = BuildInitialState();
    fInitialized = true;
    return true;
}

bool RkLeastSquaresAnalyzer::SupportsCurrentMode() const {
    return fConfig.rk_fit_mode != RkFitMode::kTwoPointBackprop;
}

RkParameterState RkLeastSquaresAnalyzer::BuildInitialState() const {
    const double dist1 = SafeNorm(fTrack.pdc1 - fTarget.target_position);
    const double dist2 = SafeNorm(fTrack.pdc2 - fTarget.target_position);
    TVector3 init_dir = ((dist1 > dist2) ? fTrack.pdc1 : fTrack.pdc2) -
                        fTarget.target_position;
    if (init_dir.Mag2() < 1.0e-12) {
        init_dir = fTrack.pdc2 - fTrack.pdc1;
    }
    if (init_dir.Mag2() < 1.0e-12) {
        init_dir.SetXYZ(0.0, 0.0, 1.0);
    }
    init_dir = init_dir.Unit();
    if (std::abs(init_dir.Z()) < 1.0e-6) {
        init_dir.SetZ((init_dir.Z() >= 0.0) ? 1.0e-6 : -1.0e-6);
        init_dir = init_dir.Unit();
    }

    const double p_init = Clamp(fConfig.initial_p_mevc, fConfig.p_min_mevc, fConfig.p_max_mevc);
    RkParameterState state;
    state.u = init_dir.X() / init_dir.Z();
    state.v = init_dir.Y() / init_dir.Z();
    state.q = fTarget.charge_e / p_init;
    return ClampState(state);
}

RkParameterState RkLeastSquaresAnalyzer::ClampState(const RkParameterState& state) const {
    RkParameterState clipped = state;
    const double invp_min = std::abs(fTarget.charge_e) / fConfig.p_max_mevc;
    const double invp_max = std::abs(fTarget.charge_e) / fConfig.p_min_mevc;
    const double q_sign = (fTarget.charge_e >= 0.0) ? 1.0 : -1.0;

    clipped.dx = Clamp(clipped.dx, -200.0, 200.0);
    clipped.dy = Clamp(clipped.dy, -200.0, 200.0);
    clipped.u = Clamp(clipped.u, -2.5, 2.5);
    clipped.v = Clamp(clipped.v, -2.5, 2.5);

    double q_abs = std::abs(clipped.q);
    if (!std::isfinite(q_abs) || q_abs < invp_min) {
        q_abs = invp_min;
    }
    if (q_abs > invp_max) {
        q_abs = invp_max;
    }
    clipped.q = q_sign * q_abs;
    return clipped;
}

double RkLeastSquaresAnalyzer::GetParameter(const RkParameterState& state,
                                            int full_index) const {
    if (full_index == 0) return state.dx;
    if (full_index == 1) return state.dy;
    if (full_index == 2) return state.u;
    if (full_index == 3) return state.v;
    return state.q;
}

RkParameterState RkLeastSquaresAnalyzer::AddDelta(const RkParameterState& state,
                                                  int full_index,
                                                  double delta) const {
    RkParameterState shifted = state;
    if (full_index == 0) shifted.dx += delta;
    if (full_index == 1) shifted.dy += delta;
    if (full_index == 2) shifted.u += delta;
    if (full_index == 3) shifted.v += delta;
    if (full_index == 4) shifted.q += delta;
    return shifted;
}

TVector3 RkLeastSquaresAnalyzer::BuildMomentumVector(const RkParameterState& state) const {
    const double q_abs = std::abs(state.q);
    if (!std::isfinite(q_abs) || q_abs < 1.0e-12) {
        return TVector3(0.0, 0.0, 0.0);
    }
    const double p_mag = std::abs(fTarget.charge_e) / q_abs;
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

RkEvalResult RkLeastSquaresAnalyzer::Evaluate(const RkParameterState& raw_state) const {
    RkEvalResult eval;
    const RkParameterState state = ClampState(raw_state);
    const TVector3 start_pos = fLayout.fixed_target_position
        ? fTarget.target_position
        : TVector3(fTarget.target_position.X() + state.dx,
                   fTarget.target_position.Y() + state.dy,
                   fTarget.target_position.Z());
    const TVector3 momentum = BuildMomentumVector(state);
    if (!IsFinite(start_pos) || momentum.Mag2() < 1.0e-12 || !IsFinite(momentum)) {
        return eval;
    }

    const double mass = std::isfinite(fTarget.mass_mev) ? fTarget.mass_mev : kDefaultMassMeV;
    const double energy = std::sqrt(momentum.Mag2() + mass * mass);
    TLorentzVector p4(momentum.X(), momentum.Y(), momentum.Z(), energy);
    if (!IsFinite(p4)) {
        return eval;
    }

    ParticleTrajectory tracer(fMagneticField);
    tracer.SetStepSize(fConfig.rk_step_mm);
    // [EN] Keep the transport window long enough to intersect both PDC planes under strong curvature. / [CN] 在强弯转情况下仍保持足够长的输运窗口，使轨迹能与两个PDC平面相交。
    const double far_dist = std::max(SafeNorm(fTrack.pdc1 - start_pos),
                                     SafeNorm(fTrack.pdc2 - start_pos));
    tracer.SetMaxDistance(std::max(1500.0, far_dist + 1500.0));
    tracer.SetMaxTime(400.0);
    tracer.SetMinMomentum(5.0);

    eval.trajectory = tracer.CalculateTrajectory(start_pos, p4, fTarget.charge_e, mass);
    if (eval.trajectory.size() < 2) {
        return eval;
    }

    double best1 = std::numeric_limits<double>::infinity();
    double best2 = std::numeric_limits<double>::infinity();
    int idx1 = -1;
    int idx2 = -1;
    for (int i = 0; i < static_cast<int>(eval.trajectory.size()); ++i) {
        const auto& point = eval.trajectory[static_cast<std::size_t>(i)];
        const double d1 = (point.position - fTrack.pdc1).Mag2();
        const double d2 = (point.position - fTrack.pdc2).Mag2();
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

    const TVector3 diff1 = eval.trajectory[static_cast<std::size_t>(idx1)].position - fTrack.pdc1;
    const TVector3 diff2 = eval.trajectory[static_cast<std::size_t>(idx2)].position - fTrack.pdc2;
    const double pdc_sigma = SafeSigma(fTarget.pdc_sigma_mm, 2.0);
    const double target_sigma = SafeSigma(fTarget.target_sigma_xy_mm, 5.0);
    if (fMeasurement.use_covariance) {
        const std::array<double, 3> residual1 = ApplyWhitening(fMeasurement.pdc1_whitening, diff1);
        const std::array<double, 3> residual2 = ApplyWhitening(fMeasurement.pdc2_whitening, diff2);
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
    }
    if (fLayout.include_start_xy_constraint) {
        eval.residuals[6] = state.dx / target_sigma;
        eval.residuals[7] = state.dy / target_sigma;
    }

    eval.residual_count = fLayout.residual_count;
    eval.ndf = fLayout.residual_count - fLayout.parameter_count;
    eval.chi2_raw = 0.0;
    for (int i = 0; i < eval.residual_count; ++i) {
        eval.chi2_raw += eval.residuals[static_cast<std::size_t>(i)] *
                         eval.residuals[static_cast<std::size_t>(i)];
    }
    eval.chi2_reduced = ReducedChi2(eval.chi2_raw, eval.ndf);
    eval.min_distance_mm = std::max(std::sqrt(best1), std::sqrt(best2));

    const double d_target_1 = SafeNorm(fTrack.pdc1 - fTarget.target_position);
    const double d_target_2 = SafeNorm(fTrack.pdc2 - fTarget.target_position);
    const int idx_far = (d_target_1 > d_target_2) ? idx1 : idx2;
    eval.path_length_mm = SegmentLengthToIndex(eval.trajectory, idx_far);
    eval.p4_at_target = p4;
    eval.brho_tm = ComputeBrhoTm(p4, fTarget.charge_e);
    eval.valid = true;
    return eval;
}

double RkLeastSquaresAnalyzer::ParameterStep(const RkParameterState& state,
                                             int full_index) const {
    if (full_index == 0 || full_index == 1) {
        return std::max(0.1, 0.05 * SafeSigma(fTarget.target_sigma_xy_mm, 5.0));
    }
    if (full_index == 2 || full_index == 3) {
        const double local = std::abs(GetParameter(state, full_index));
        return std::max(1.0e-4, 0.01 * std::max(local, 0.05));
    }
    return std::max(1.0e-7, std::abs(state.q) * 1.0e-3);
}

double RkLeastSquaresAnalyzer::ParameterLowerBound(int full_index) const {
    if (full_index == 0 || full_index == 1) return -200.0;
    if (full_index == 2 || full_index == 3) return -2.5;
    const double invp_min = std::abs(fTarget.charge_e) / fConfig.p_max_mevc;
    const double invp_max = std::abs(fTarget.charge_e) / fConfig.p_min_mevc;
    return (fTarget.charge_e >= 0.0) ? invp_min : -invp_max;
}

double RkLeastSquaresAnalyzer::ParameterUpperBound(int full_index) const {
    if (full_index == 0 || full_index == 1) return 200.0;
    if (full_index == 2 || full_index == 3) return 2.5;
    const double invp_min = std::abs(fTarget.charge_e) / fConfig.p_max_mevc;
    const double invp_max = std::abs(fTarget.charge_e) / fConfig.p_min_mevc;
    return (fTarget.charge_e >= 0.0) ? invp_max : -invp_min;
}

bool RkLeastSquaresAnalyzer::BuildResidualJacobian(const RkParameterState& state,
                                                   const RkEvalResult& eval,
                                                   TMatrixD* jacobian) const {
    if (!jacobian) {
        return false;
    }
    jacobian->ResizeTo(fLayout.residual_count, fLayout.parameter_count);
    jacobian->Zero();
    for (int local_j = 0; local_j < fLayout.parameter_count; ++local_j) {
        const int full_j = fLayout.active_parameter_indices[static_cast<std::size_t>(local_j)];
        const double step = ParameterStep(state, full_j);
        RkParameterState plus = ClampState(AddDelta(state, full_j, step));
        RkParameterState minus = ClampState(AddDelta(state, full_j, -step));
        const double plus_value = GetParameter(plus, full_j);
        const double minus_value = GetParameter(minus, full_j);
        const double denominator = plus_value - minus_value;
        if (!std::isfinite(denominator) || std::abs(denominator) < 1.0e-12) {
            continue;
        }

        const RkEvalResult eval_plus = Evaluate(plus);
        const RkEvalResult eval_minus = Evaluate(minus);
        if (eval_plus.valid && eval_minus.valid) {
            for (int i = 0; i < eval.residual_count; ++i) {
                (*jacobian)(i, local_j) =
                    (eval_plus.residuals[static_cast<std::size_t>(i)] -
                     eval_minus.residuals[static_cast<std::size_t>(i)]) /
                    denominator;
            }
            continue;
        }
        if (!eval_plus.valid) {
            continue;
        }
        const double forward_denom = plus_value - GetParameter(state, full_j);
        if (!std::isfinite(forward_denom) || std::abs(forward_denom) < 1.0e-12) {
            continue;
        }
        for (int i = 0; i < eval.residual_count; ++i) {
            (*jacobian)(i, local_j) =
                (eval_plus.residuals[static_cast<std::size_t>(i)] -
                 eval.residuals[static_cast<std::size_t>(i)]) /
                forward_denom;
        }
    }
    return true;
}

TMatrixD RkLeastSquaresAnalyzer::BuildNormalMatrix(const TMatrixD& jacobian) const {
    TMatrixD normal(fLayout.parameter_count, fLayout.parameter_count);
    normal.Zero();
    for (int i = 0; i < jacobian.GetNrows(); ++i) {
        for (int j = 0; j < jacobian.GetNcols(); ++j) {
            for (int k = 0; k < jacobian.GetNcols(); ++k) {
                normal(j, k) += jacobian(i, j) * jacobian(i, k);
            }
        }
    }
    return normal;
}

TVectorD RkLeastSquaresAnalyzer::BuildGradient(const TMatrixD& jacobian,
                                               const RkEvalResult& eval) const {
    TVectorD gradient(fLayout.parameter_count);
    gradient.Zero();
    for (int i = 0; i < eval.residual_count; ++i) {
        for (int j = 0; j < fLayout.parameter_count; ++j) {
            gradient(j) += jacobian(i, j) * eval.residuals[static_cast<std::size_t>(i)];
        }
    }
    return gradient;
}

TMatrixD RkLeastSquaresAnalyzer::BuildMomentumJacobian(const RkParameterState& state) const {
    TMatrixD jacobian(static_cast<int>(kMomentumDim), fLayout.parameter_count);
    jacobian.Zero();
    const TVector3 base = BuildMomentumVector(state);
    for (int local_j = 0; local_j < fLayout.parameter_count; ++local_j) {
        const int full_j = fLayout.active_parameter_indices[static_cast<std::size_t>(local_j)];
        const double step = ParameterStep(state, full_j);
        RkParameterState plus = ClampState(AddDelta(state, full_j, step));
        RkParameterState minus = ClampState(AddDelta(state, full_j, -step));
        const double plus_value = GetParameter(plus, full_j);
        const double minus_value = GetParameter(minus, full_j);
        double denominator = plus_value - minus_value;
        TVector3 delta(0.0, 0.0, 0.0);
        if (std::isfinite(denominator) && std::abs(denominator) >= 1.0e-12) {
            delta = BuildMomentumVector(plus) - BuildMomentumVector(minus);
        } else {
            denominator = plus_value - GetParameter(state, full_j);
            if (!std::isfinite(denominator) || std::abs(denominator) < 1.0e-12) {
                continue;
            }
            delta = BuildMomentumVector(plus) - base;
        }
        jacobian(0, local_j) = delta.X() / denominator;
        jacobian(1, local_j) = delta.Y() / denominator;
        jacobian(2, local_j) = delta.Z() / denominator;
    }
    return jacobian;
}

TMatrixD RkLeastSquaresAnalyzer::BuildPriorPrecision() const {
    TMatrixD precision(fLayout.parameter_count, fLayout.parameter_count);
    precision.Zero();
    const double sigma_u = SafeSigma(fConfig.posterior_u_sigma, 0.25);
    const double sigma_v = SafeSigma(fConfig.posterior_v_sigma, 0.25);
    const double q_sigma =
        std::max(std::abs(fInitialState.q) * std::max(fConfig.posterior_q_rel_sigma, 1.0e-3),
                 std::abs(fTarget.charge_e) /
                     std::max(fConfig.p_max_mevc, fConfig.p_min_mevc));
    const int u_local = FindLocalParameter(2);
    const int v_local = FindLocalParameter(3);
    const int q_local = FindLocalParameter(4);
    if (u_local >= 0) {
        precision(u_local, u_local) = 1.0 / (sigma_u * sigma_u);
    }
    if (v_local >= 0) {
        precision(v_local, v_local) = 1.0 / (sigma_v * sigma_v);
    }
    if (q_local >= 0) {
        precision(q_local, q_local) = 1.0 / (q_sigma * q_sigma);
    }
    return precision;
}

int RkLeastSquaresAnalyzer::FindLocalParameter(int full_index) const {
    for (int local = 0; local < fLayout.parameter_count; ++local) {
        if (fLayout.active_parameter_indices[static_cast<std::size_t>(local)] == full_index) {
            return local;
        }
    }
    return -1;
}

bool RkLeastSquaresAnalyzer::HasActiveParameter(int full_index) const {
    return FindLocalParameter(full_index) >= 0;
}

bool RkLeastSquaresAnalyzer::CovarianceIsPositiveDefinite(const TMatrixD& covariance) const {
    if (covariance.GetNrows() != covariance.GetNcols() || covariance.GetNrows() <= 0) {
        return false;
    }
    TMatrixDSym sym(covariance.GetNrows());
    for (int row = 0; row < covariance.GetNrows(); ++row) {
        for (int col = 0; col < covariance.GetNcols(); ++col) {
            const double value = 0.5 * (covariance(row, col) + covariance(col, row));
            if (!std::isfinite(value)) {
                return false;
            }
            sym(row, col) = value;
        }
    }
    TMatrixDSymEigen eigen(sym);
    const TVectorD eigen_values = eigen.GetEigenValues();
    for (int i = 0; i < eigen_values.GetNrows(); ++i) {
        if (!(eigen_values(i) > 1.0e-12) || !std::isfinite(eigen_values(i))) {
            return false;
        }
    }
    return true;
}

bool RkLeastSquaresAnalyzer::InvertWithSVD(const TMatrixD& matrix,
                                           TMatrixD* inverse,
                                           double* condition_number) const {
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
    inverse->ResizeTo(dimension, dimension);
    *inverse = local_inverse;
    return true;
}

void RkLeastSquaresAnalyzer::StoreStateCovariance(const TMatrixD& state_covariance,
                                                  std::array<double, 25>* out) const {
    if (!out) {
        return;
    }
    out->fill(0.0);
    for (int local_row = 0; local_row < fLayout.parameter_count; ++local_row) {
        const int full_row = fLayout.active_parameter_indices[static_cast<std::size_t>(local_row)];
        for (int local_col = 0; local_col < fLayout.parameter_count; ++local_col) {
            const int full_col = fLayout.active_parameter_indices[static_cast<std::size_t>(local_col)];
            (*out)[static_cast<std::size_t>(full_row * static_cast<int>(kParameterCount) + full_col)] =
                state_covariance(local_row, local_col);
        }
    }
}

void RkLeastSquaresAnalyzer::FillMomentumUncertainty(const RkParameterState& state,
                                                     const TMatrixD& state_covariance,
                                                     std::array<double, 9>* covariance_out,
                                                     IntervalEstimate* px_interval,
                                                     IntervalEstimate* py_interval,
                                                     IntervalEstimate* pz_interval,
                                                     IntervalEstimate* p_interval) const {
    const TMatrixD momentum_jacobian = BuildMomentumJacobian(state);
    const TMatrixD momentum_covariance =
        momentum_jacobian * state_covariance *
        TMatrixD(TMatrixD::kTransposed, momentum_jacobian);
    if (covariance_out) {
        MatrixToArray(momentum_covariance, covariance_out);
    }

    const TVector3 momentum = BuildMomentumVector(state);
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
                grad(0) * (momentum_covariance(0, 0) * grad(0) +
                           momentum_covariance(0, 1) * grad(1) +
                           momentum_covariance(0, 2) * grad(2)) +
                grad(1) * (momentum_covariance(1, 0) * grad(0) +
                           momentum_covariance(1, 1) * grad(1) +
                           momentum_covariance(1, 2) * grad(2)) +
                grad(2) * (momentum_covariance(2, 0) * grad(0) +
                           momentum_covariance(2, 1) * grad(1) +
                           momentum_covariance(2, 2) * grad(2));
            *p_interval = BuildGaussianInterval(p_mag, (variance_p > 0.0) ? std::sqrt(variance_p) : 0.0);
        }
    }
}

double RkLeastSquaresAnalyzer::EvaluateChi2Raw(const RkParameterState& state) const {
    const RkEvalResult eval = Evaluate(state);
    return eval.valid ? eval.chi2_raw : std::numeric_limits<double>::infinity();
}

double RkLeastSquaresAnalyzer::EvaluateLogPrior(const RkParameterState& raw_state,
                                                const BayesianConfig& bayesian_config,
                                                const RkParameterState& prior_center) const {
    if (bayesian_config.prior_type == BayesianConfig::PriorType::kFlat) {
        return 0.0;
    }
    const RkParameterState state = ClampState(raw_state);
    const double sigma_u = SafeSigma(bayesian_config.prior_u_sigma, 0.25);
    const double sigma_v = SafeSigma(bayesian_config.prior_v_sigma, 0.25);
    const double q_sigma =
        std::max(std::abs(fInitialState.q) * std::max(bayesian_config.prior_q_rel_sigma, 1.0e-3),
                 std::abs(fTarget.charge_e) /
                     std::max(fConfig.p_max_mevc, fConfig.p_min_mevc));
    const RkParameterState center = bayesian_config.prior_centered_at_mle
        ? prior_center
        : RkParameterState{};
    const double du = state.u - center.u;
    const double dv = state.v - center.v;
    const double dq = state.q - center.q;
    return -0.5 * ((du * du) / (sigma_u * sigma_u) +
                   (dv * dv) / (sigma_v * sigma_v) +
                   (dq * dq) / (q_sigma * q_sigma));
}

bool RkLeastSquaresAnalyzer::Solve(bool compute_uncertainty,
                                   bool compute_posterior_laplace,
                                   RkSolveResult* result) const {
    return Solve(fInitialState, compute_uncertainty, compute_posterior_laplace, result);
}

bool RkLeastSquaresAnalyzer::Solve(const RkParameterState& initial_state,
                                   bool compute_uncertainty,
                                   bool compute_posterior_laplace,
                                   RkSolveResult* result) const {
    return SolveImpl(initial_state, -1, 0.0, compute_uncertainty, compute_posterior_laplace, result);
}

bool RkLeastSquaresAnalyzer::SolveWithFixedParameter(const RkParameterState& initial_state,
                                                     int fixed_full_index,
                                                     double fixed_value,
                                                     bool compute_uncertainty,
                                                     bool compute_posterior_laplace,
                                                     RkSolveResult* result) const {
    return SolveImpl(initial_state, fixed_full_index, fixed_value, compute_uncertainty, compute_posterior_laplace, result);
}

bool RkLeastSquaresAnalyzer::SolveImpl(const RkParameterState& raw_initial_state,
                                       int fixed_full_index,
                                       double fixed_value,
                                       bool compute_uncertainty,
                                       bool compute_posterior_laplace,
                                       RkSolveResult* result) const {
    if (!fInitialized || !result) {
        return false;
    }

    RkParameterState state = ClampState(raw_initial_state);
    if (fixed_full_index >= 0) {
        state = AddDelta(state, fixed_full_index, fixed_value - GetParameter(state, fixed_full_index));
        state = ClampState(state);
        state = AddDelta(state, fixed_full_index, fixed_value - GetParameter(state, fixed_full_index));
    }

    RkEvalResult current = Evaluate(state);
    if (!current.valid) {
        return false;
    }

    double lambda = Clamp(fConfig.lm_lambda_init, fConfig.lm_lambda_min, fConfig.lm_lambda_max);
    int accepted_iterations = 0;
    for (int iter = 0; iter < fConfig.max_iterations; ++iter) {
        TMatrixD jacobian(fLayout.residual_count, fLayout.parameter_count);
        BuildResidualJacobian(state, current, &jacobian);
        TMatrixD normal = BuildNormalMatrix(jacobian);
        TVectorD gradient = BuildGradient(jacobian, current);
        for (int j = 0; j < fLayout.parameter_count; ++j) {
            normal(j, j) += lambda * std::max(1.0, normal(j, j));
        }

        if (fixed_full_index >= 0) {
            const int fixed_local = FindLocalParameter(fixed_full_index);
            if (fixed_local >= 0) {
                for (int j = 0; j < fLayout.parameter_count; ++j) {
                    normal(fixed_local, j) = 0.0;
                    normal(j, fixed_local) = 0.0;
                }
                normal(fixed_local, fixed_local) = 1.0;
                gradient(fixed_local) = 0.0;
            }
        }

        TVectorD rhs(fLayout.parameter_count);
        for (int j = 0; j < fLayout.parameter_count; ++j) {
            rhs(j) = -gradient(j);
        }

        TDecompSVD solver(normal);
        Bool_t ok = kFALSE;
        const TVectorD delta = solver.Solve(rhs, ok);
        if (!ok) {
            lambda = std::min(fConfig.lm_lambda_max, lambda * 10.0);
            continue;
        }

        RkParameterState candidate = state;
        for (int local_j = 0; local_j < fLayout.parameter_count; ++local_j) {
            const int full_j = fLayout.active_parameter_indices[static_cast<std::size_t>(local_j)];
            if (fixed_full_index >= 0 && full_j == fixed_full_index) {
                continue;
            }
            candidate = AddDelta(candidate, full_j, delta(local_j));
        }
        if (fixed_full_index >= 0) {
            candidate = AddDelta(candidate, fixed_full_index, fixed_value - GetParameter(candidate, fixed_full_index));
        }
        candidate = ClampState(candidate);
        if (fixed_full_index >= 0) {
            candidate = AddDelta(candidate, fixed_full_index, fixed_value - GetParameter(candidate, fixed_full_index));
            candidate = ClampState(candidate);
        }

        const RkEvalResult candidate_eval = Evaluate(candidate);
        if (candidate_eval.valid && candidate_eval.chi2_raw < current.chi2_raw) {
            const double improvement = current.chi2_raw - candidate_eval.chi2_raw;
            state = candidate;
            current = candidate_eval;
            lambda = std::max(fConfig.lm_lambda_min, lambda / 3.0);
            ++accepted_iterations;
            double delta_norm_sq = 0.0;
            for (int local_j = 0; local_j < fLayout.parameter_count; ++local_j) {
                if (fixed_full_index >= 0 &&
                    fLayout.active_parameter_indices[static_cast<std::size_t>(local_j)] == fixed_full_index) {
                    continue;
                }
                delta_norm_sq += delta(local_j) * delta(local_j);
            }
            if (improvement < 1.0e-8 || std::sqrt(delta_norm_sq) < 1.0e-5) {
                break;
            }
        } else {
            lambda = std::min(fConfig.lm_lambda_max, lambda * 4.0);
            if (lambda >= fConfig.lm_lambda_max) {
                break;
            }
        }
    }

    result->valid = true;
    result->state = state;
    result->eval = current;
    result->accepted_iterations = accepted_iterations;

    if (compute_uncertainty) {
        TMatrixD jacobian(fLayout.residual_count, fLayout.parameter_count);
        if (BuildResidualJacobian(state, current, &jacobian)) {
            const TMatrixD normal = BuildNormalMatrix(jacobian);
            result->normal_matrix.ResizeTo(normal.GetNrows(), normal.GetNcols());
            result->normal_matrix = normal;
            TMatrixD state_covariance(fLayout.parameter_count, fLayout.parameter_count);
            double condition_number = std::numeric_limits<double>::quiet_NaN();
            if (InvertWithSVD(normal, &state_covariance, &condition_number)) {
                result->condition_number = condition_number;
                result->state_covariance.ResizeTo(state_covariance.GetNrows(),
                                                 state_covariance.GetNcols());
                result->state_covariance = state_covariance;
                StoreStateCovariance(state_covariance, &result->full_state_covariance);
                FillMomentumUncertainty(state,
                                        state_covariance,
                                        &result->momentum_covariance,
                                        &result->px_interval,
                                        &result->py_interval,
                                        &result->pz_interval,
                                        &result->p_interval);

                if (compute_posterior_laplace) {
                    TMatrixD posterior_normal = normal;
                    posterior_normal += BuildPriorPrecision();
                    TMatrixD posterior_covariance(fLayout.parameter_count, fLayout.parameter_count);
                    double posterior_condition = std::numeric_limits<double>::quiet_NaN();
                    if (InvertWithSVD(posterior_normal, &posterior_covariance, &posterior_condition)) {
                        result->posterior_condition_number = posterior_condition;
                        result->posterior_state_covariance.ResizeTo(
                            posterior_covariance.GetNrows(),
                            posterior_covariance.GetNcols()
                        );
                        result->posterior_state_covariance = posterior_covariance;
                        FillMomentumUncertainty(state,
                                                posterior_covariance,
                                                &result->posterior_momentum_covariance,
                                                &result->px_credible,
                                                &result->py_credible,
                                                &result->pz_credible,
                                                &result->p_credible);
                    }
                }
            }
        }
    }

    return true;
}

void RkLeastSquaresAnalyzer::FillRecoResult(const RkSolveResult& solve_result,
                                            RecoResult* result) const {
    if (!result) {
        return;
    }
    *result = RecoResult{};
    result->method_used = SolveMethod::kRungeKutta;
    result->iterations = solve_result.accepted_iterations;
    result->chi2 = solve_result.eval.chi2_reduced;
    result->chi2_raw = solve_result.eval.chi2_raw;
    result->chi2_reduced = solve_result.eval.chi2_reduced;
    result->ndf = solve_result.eval.ndf;
    result->used_measurement_covariance = solve_result.eval.used_measurement_covariance;
    result->min_distance_mm = solve_result.eval.min_distance_mm;
    result->path_length_mm = solve_result.eval.path_length_mm;
    result->brho_tm = solve_result.eval.brho_tm;
    result->p4_at_target = solve_result.eval.p4_at_target;
    result->fit_start_position = fLayout.fixed_target_position
        ? fTarget.target_position
        : TVector3(fTarget.target_position.X() + solve_result.state.dx,
                   fTarget.target_position.Y() + solve_result.state.dy,
                   fTarget.target_position.Z());
    result->normal_condition_number = solve_result.condition_number;
    result->state_covariance = solve_result.full_state_covariance;
    result->momentum_covariance = solve_result.momentum_covariance;
    result->posterior_momentum_covariance = solve_result.posterior_momentum_covariance;
    result->px_interval = solve_result.px_interval;
    result->py_interval = solve_result.py_interval;
    result->pz_interval = solve_result.pz_interval;
    result->p_interval = solve_result.p_interval;
    result->px_credible = solve_result.px_credible;
    result->py_credible = solve_result.py_credible;
    result->pz_credible = solve_result.pz_credible;
    result->p_credible = solve_result.p_credible;
    result->uncertainty_valid =
        solve_result.px_interval.valid && solve_result.py_interval.valid &&
        solve_result.pz_interval.valid && solve_result.p_interval.valid;
    result->posterior_valid =
        solve_result.px_credible.valid && solve_result.py_credible.valid &&
        solve_result.pz_credible.valid && solve_result.p_credible.valid;
    if (solve_result.eval.min_distance_mm <= fConfig.tolerance_mm) {
        result->status = SolverStatus::kSuccess;
        result->message = "RK fit converged";
    } else {
        result->status = SolverStatus::kNotConverged;
        result->message = "RK fit reached iteration limit";
    }
}

}  // namespace analysis::pdc::anaroot_like::detail
