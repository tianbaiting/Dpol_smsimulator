#include "PDCMomentumReconstructor.hh"
#include "PDCRkAnalysisInternal.hh"

#include <cmath>
#include <string>

namespace analysis::pdc::anaroot_like {

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
    // [EN] kTwoPointBackprop now uses the same RkLeastSquaresAnalyzer as
    // kFixedTargetPdcOnly (fixed target, 3 params: u, v, p).  The legacy 1D
    // momentum scan with fixed PDC-line direction is replaced. / [CN]
    // kTwoPointBackprop 现在使用与 kFixedTargetPdcOnly 相同的分析器（固定靶点，
    // 3 参数 u,v,p），替代 legacy 的 1D 动量扫描。
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
        result.message = "RK two-point backprop evaluation failed";
        return result;
    }

    analyzer.FillRecoResult(solve_result, &result);
    return result;
}

RecoResult PDCMomentumReconstructor::ReconstructRKThreePointFree(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config
) const {
    // [EN] kThreePointFree uses the 5-parameter (dx, dy, u, v, p) layout with
    // a soft target-xy prior.  Replaces legacy Minuit-based solver. / [CN]
    // kThreePointFree 使用 5 参数 (dx,dy,u,v,p) 加软靶点先验，替代 legacy Minuit。
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
        result.message = "RK three-point free fit evaluation failed";
        return result;
    }

    analyzer.FillRecoResult(solve_result, &result);
    return result;
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
