#ifndef ANALYSIS_PDC_NN_MOMENTUM_RECONSTRUCTOR_HH
#define ANALYSIS_PDC_NN_MOMENTUM_RECONSTRUCTOR_HH

#include "PDCRecoTypes.hh"

#include <array>
#include <string>
#include <vector>

namespace analysis::pdc::anaroot_like {

class PDCNNMomentumReconstructor {
public:
    bool LoadModel(const std::string& json_path, std::string* reason);
    bool IsLoaded() const { return fLoaded; }
    const std::string& LoadedPath() const { return fLoadedPath; }

    RecoResult Reconstruct(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config
    ) const;

private:
    struct DenseLayer {
        int in_dim = 0;
        int out_dim = 0;
        std::vector<double> weights;  // row-major: out_dim x in_dim
        std::vector<double> bias;     // out_dim
    };

    bool Forward(
        const std::array<double, 6>& features,
        std::array<double, 3>* momentum,
        std::string* reason
    ) const;

    bool fLoaded = false;
    std::string fLoadedPath;
    std::array<double, 6> fXMean{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<double, 6> fXStd{1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    std::vector<DenseLayer> fLayers;
};

}  // namespace analysis::pdc::anaroot_like

#endif  // ANALYSIS_PDC_NN_MOMENTUM_RECONSTRUCTOR_HH
