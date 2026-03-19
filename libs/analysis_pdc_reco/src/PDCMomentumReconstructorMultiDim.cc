#include "PDCMomentumReconstructor.hh"

#include <string>

namespace analysis::pdc::anaroot_like {

RecoResult PDCMomentumReconstructor::ReconstructMultiDim(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config
) const {
    RecoResult result;
    result.method_used = SolveMethod::kMultiDimFit;

    std::string reason;
    if (!ValidateInputs(track, target, config, &reason)) {
        result.status = SolverStatus::kInvalidInput;
        result.message = reason;
        return result;
    }

    result.status = SolverStatus::kNotAvailable;
    result.message = "MultiDimFit interface is reserved but not implemented in this revision";
    return result;
}

}  // namespace analysis::pdc::anaroot_like
