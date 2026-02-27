#include "PDCRecoFactory.hh"

#include <cstdlib>
#include <string>

namespace analysis::pdc::anaroot_like {

std::unique_ptr<PDCMomentumReconstructor> PDCRecoFactory::CreateDefault(MagneticField* magnetic_field) {
    return std::make_unique<PDCMomentumReconstructor>(magnetic_field);
}

bool PDCRecoFactory::UseAnarootLikeByDefault() {
    const char* backend = std::getenv("PDC_RECO_BACKEND");
    if (!backend) {
        return true;
    }
    std::string mode = backend;
    for (char& c : mode) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return mode != "legacy";
}

}  // namespace analysis::pdc::anaroot_like
