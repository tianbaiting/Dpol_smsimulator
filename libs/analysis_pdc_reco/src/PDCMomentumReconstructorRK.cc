#include "PDCMomentumReconstructor.hh"
#include "PDCRkAnalysisInternal.hh"
#include "TargetReconstructor.hh"

#include "TMatrixD.h"
#include "TMatrixDSym.h"
#include "TMatrixDSymEigen.h"
#include "TVectorD.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>

namespace analysis::pdc::anaroot_like {
namespace {

constexpr std::size_t kResidualCount = 8;
constexpr std::size_t kParameterCount = 5;
constexpr std::size_t kMomentumDim = 3;
constexpr double kBrhoGeVOverCPerTm = 0.299792458;
constexpr double kDefaultMassMeV = 938.2720813;

double SafeSigma(double sigma, double fallback) {
    if (std::isfinite(sigma) && sigma > 0.0) {
        return sigma;
    }
    return fallback;
}

double SafeNorm(const TVector3& value) {
    return std::sqrt(std::max(0.0, value.Mag2()));
}

RecoTrack BuildLegacyTrack(const PDCInputTrack& track) {
    RecoTrack legacy_track(track.pdc1, track.pdc2);
    legacy_track.pdgCode = 2212;
    return legacy_track;
}

TVector3 SelectFartherPdcPoint(const PDCInputTrack& track, const TVector3& target_position) {
    const double dist1 = SafeNorm(track.pdc1 - target_position);
    const double dist2 = SafeNorm(track.pdc2 - target_position);
    return (dist1 > dist2) ? track.pdc1 : track.pdc2;
}

double ComputeBrhoTm(const TLorentzVector& p4, double charge_e) {
    if (!std::isfinite(p4.P()) || p4.P() <= 0.0 || std::abs(charge_e) <= 1.0e-12) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return (p4.P() / 1000.0) / (kBrhoGeVOverCPerTm * std::abs(charge_e));
}

void FillCommonRecoKinematics(const TLorentzVector& p4,
                              double charge_e,
                              const TVector3& fit_start_position,
                              double min_distance_mm,
                              int iterations,
                              RecoResult* result) {
    if (!result) {
        return;
    }
    result->p4_at_target = p4;
    result->fit_start_position = fit_start_position;
    result->min_distance_mm = min_distance_mm;
    result->iterations = iterations;
    result->brho_tm = ComputeBrhoTm(p4, charge_e);
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
        layout.include_start_xy_constraint = false;
        layout.fixed_target_position = true;
    }
    return layout;
}

RecoResult PDCMomentumReconstructor::ReconstructRK(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config
) const {
    switch (config.rk_fit_mode) {
        case RkFitMode::kTwoPointBackprop:
            return ReconstructRKTwoPointBackprop(track, target, config);
        case RkFitMode::kFixedTargetPdcOnly:
            return ReconstructRKFixedTargetPdcOnly(track, target, config);
        case RkFitMode::kThreePointFree:
            return ReconstructRKThreePointFree(track, target, config);
    }

    RecoResult result;
    result.method_used = SolveMethod::kRungeKutta;
    result.status = SolverStatus::kInvalidInput;
    result.message = "unknown RK fit mode";
    return result;
}

RecoResult PDCMomentumReconstructor::ReconstructRKTwoPointBackprop(
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

    // [EN] Reuse the legacy two-point back-tracing logic as a bounded compatibility mode while the new framework owns the mode selection and outputs. / [CN] 在新框架负责模式选择和输出契约的前提下，复用legacy两点反推逻辑作为受控兼容模式。
    TargetReconstructor legacy_reconstructor(fMagneticField);
    legacy_reconstructor.SetTrajectoryStepSize(config.rk_step_mm);
    const RecoTrack legacy_track = BuildLegacyTrack(track);
    const int search_rounds = std::max(1, std::min(8, config.max_iterations / 10));

    const TargetReconstructionResult legacy_result = legacy_reconstructor.ReconstructAtTargetWithDetails(
        legacy_track,
        target.target_position,
        false,
        config.p_min_mevc,
        config.p_max_mevc,
        config.tolerance_mm,
        search_rounds
    );

    if (!IsFinite(legacy_result.bestMomentum) || legacy_result.bestMomentum.P() <= 0.0) {
        result.status = SolverStatus::kNotConverged;
        result.message = "RK two-point backprop produced invalid momentum";
        return result;
    }

    FillCommonRecoKinematics(legacy_result.bestMomentum,
                             target.charge_e,
                             SelectFartherPdcPoint(track, target.target_position),
                             legacy_result.finalDistance,
                             search_rounds,
                             &result);
    result.status =
        (legacy_result.success || legacy_result.finalDistance <= config.tolerance_mm)
            ? SolverStatus::kSuccess
            : SolverStatus::kNotConverged;
    result.message = (result.status == SolverStatus::kSuccess)
        ? "RK two-point backprop converged"
        : "RK two-point backprop did not reach tolerance";
    return result;
}

RecoResult PDCMomentumReconstructor::ReconstructRKThreePointFree(
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

    // [EN] Start the free 6D fit from the measured PDC line direction so Minuit does not waste iterations on a flipped branch. / [CN] 用测得的PDC连线方向初始化自由6维拟合，避免Minuit在错误分支上浪费迭代。
    TVector3 line_direction = track.pdc2 - track.pdc1;
    if (line_direction.Mag2() < 1.0e-12) {
        line_direction = TVector3(0.0, 0.0, 1.0);
    }
    line_direction = line_direction.Unit();
    const double p_init = Clamp(config.initial_p_mevc, config.p_min_mevc, config.p_max_mevc);
    const TVector3 momentum_init = line_direction * p_init;

    TargetReconstructor legacy_reconstructor(fMagneticField);
    legacy_reconstructor.SetTrajectoryStepSize(config.rk_step_mm);
    const RecoTrack legacy_track = BuildLegacyTrack(track);

    const TargetReconstructionResult legacy_result = legacy_reconstructor.ReconstructAtTargetThreePointFreeMinuit(
        legacy_track,
        target.target_position,
        false,
        target.target_position,
        momentum_init,
        target.target_sigma_xy_mm,
        target.pdc_sigma_mm,
        config.tolerance_mm,
        config.max_iterations,
        false
    );

    if (!IsFinite(legacy_result.bestMomentum) || legacy_result.bestMomentum.P() <= 0.0) {
        result.status = SolverStatus::kNotConverged;
        result.message = "RK three-point free fit produced invalid momentum";
        return result;
    }

    FillCommonRecoKinematics(legacy_result.bestMomentum,
                             target.charge_e,
                             legacy_result.bestStartPos,
                             legacy_result.finalDistance,
                             legacy_result.totalIterations,
                             &result);
    result.chi2 = legacy_result.finalLoss;
    result.chi2_raw = legacy_result.finalLoss;
    result.chi2_reduced = legacy_result.finalLoss;
    result.ndf = 3;
    result.status =
        (legacy_result.success || legacy_result.finalDistance <= config.tolerance_mm)
            ? SolverStatus::kSuccess
            : SolverStatus::kNotConverged;
    result.message = (result.status == SolverStatus::kSuccess)
        ? "RK three-point free fit converged"
        : "RK three-point free fit did not reach tolerance";
    return result;
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
    if (layout.include_start_xy_constraint) {
        // [EN] Anchor the fitted emission point in x/y so the 5D problem does not drift into an under-constrained branch. / [CN] 对拟合发射点的x/y加入约束，避免5维问题漂移到欠约束分支。
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

RecoResult PDCMomentumReconstructor::ReconstructRKFixedTargetPdcOnly(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config
) const {
    RecoResult result;
    result.method_used = SolveMethod::kRungeKutta;

    detail::RkLeastSquaresAnalyzer analyzer(fMagneticField, track, target, config);
    std::string reason;
    if (!analyzer.Initialize(&reason)) {
        result.status = SolverStatus::kInvalidInput;
        result.message = reason;
        return result;
    }

    detail::RkSolveResult solve_result;
    if (!analyzer.Solve(config.compute_uncertainty, config.compute_posterior_laplace, &solve_result)) {
        result.status = SolverStatus::kNotConverged;
        result.message = "initial RK evaluation failed";
        return result;
    }

    analyzer.FillRecoResult(solve_result, &result);
    return result;
}

}  // namespace analysis::pdc::anaroot_like
