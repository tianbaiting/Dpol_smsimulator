#ifndef ANALYSIS_PDC_MOMENTUM_RECONSTRUCTOR_HH
#define ANALYSIS_PDC_MOMENTUM_RECONSTRUCTOR_HH

#include "MagneticField.hh"
#include "PDCNNMomentumReconstructor.hh"
#include "PDCRecoTypes.hh"

#include <memory>
#include <string>

namespace analysis::pdc::anaroot_like {

class PDCMomentumReconstructor {
public:
    explicit PDCMomentumReconstructor(MagneticField* magnetic_field);

    RecoResult Reconstruct(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config
    ) const;

    RecoResult ReconstructRK(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config
    ) const;

    RecoResult ReconstructMultiDim(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config
    ) const;

    RecoResult ReconstructNN(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config
    ) const;

    RecoResult ReconstructRKTwoPointBackprop(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config
    ) const;

    RecoResult ReconstructRKFixedTargetPdcOnly(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config
    ) const;

    RecoResult ReconstructRKThreePointFree(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config
    ) const;

private:
    static bool IsFinite(const TVector3& value);
    static bool IsFinite(const TLorentzVector& value);
    static double Clamp(double value, double lower, double upper);

    bool ValidateInputs(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config,
        std::string* reason
    ) const;

    MagneticField* fMagneticField = nullptr;
    mutable std::unique_ptr<PDCNNMomentumReconstructor> fNNReconstructor;
    mutable std::string fNNModelPath;
};

}  // namespace analysis::pdc::anaroot_like

#endif  // ANALYSIS_PDC_MOMENTUM_RECONSTRUCTOR_HH
