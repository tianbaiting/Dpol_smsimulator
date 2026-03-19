#include "PDCMomentumReconstructor.hh"

#include <cstdlib>
#include <limits>
#include <memory>
#include <string>

namespace analysis::pdc::anaroot_like {

RecoResult PDCMomentumReconstructor::ReconstructNN(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config
) const {
    RecoResult result;
    result.method_used = SolveMethod::kNeuralNetwork;
    result.chi2 = std::numeric_limits<double>::quiet_NaN();
    result.chi2_raw = std::numeric_limits<double>::quiet_NaN();
    result.chi2_reduced = std::numeric_limits<double>::quiet_NaN();
    result.ndf = 0;
    result.min_distance_mm = std::numeric_limits<double>::quiet_NaN();
    result.path_length_mm = std::numeric_limits<double>::quiet_NaN();
    result.iterations = 1;

    std::string model_path = config.nn_model_json_path;
    if (model_path.empty()) {
        const char* env_model = std::getenv("PDC_NN_MODEL_JSON");
        if (env_model) {
            model_path = env_model;
        }
    }

    if (model_path.empty()) {
        result.status = SolverStatus::kNotAvailable;
        result.message = "NN model json path is empty (set RecoConfig::nn_model_json_path or PDC_NN_MODEL_JSON)";
        return result;
    }

    if (!fNNReconstructor || fNNModelPath != model_path || !fNNReconstructor->IsLoaded()) {
        auto nn = std::make_unique<PDCNNMomentumReconstructor>();
        std::string reason;
        if (!nn->LoadModel(model_path, &reason)) {
            result.status = SolverStatus::kNotAvailable;
            result.message = reason;
            return result;
        }
        fNNReconstructor = std::move(nn);
        fNNModelPath = model_path;
    }

    return fNNReconstructor->Reconstruct(track, target, config);
}

}  // namespace analysis::pdc::anaroot_like
