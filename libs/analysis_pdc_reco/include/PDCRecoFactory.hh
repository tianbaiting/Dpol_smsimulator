#ifndef ANALYSIS_PDC_RECO_FACTORY_HH
#define ANALYSIS_PDC_RECO_FACTORY_HH

#include "PDCMomentumReconstructor.hh"

#include <memory>

namespace analysis::pdc::anaroot_like {

class PDCRecoFactory {
public:
    static std::unique_ptr<PDCMomentumReconstructor> CreateDefault(MagneticField* magnetic_field);
    static bool UseAnarootLikeByDefault();
};

}  // namespace analysis::pdc::anaroot_like

#endif  // ANALYSIS_PDC_RECO_FACTORY_HH
