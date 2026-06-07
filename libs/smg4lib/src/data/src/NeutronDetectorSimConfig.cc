#include "NeutronDetectorSimConfig.hh"

#include "TNEBULAPlusSimParameter.hh"
#include "TNEBULASimParameter.hh"

namespace sim_deuteron {

bool IsNEBULAEnabled(const TNEBULASimParameter* parameter) {
    return parameter && parameter->fIsLoaded && (parameter->fNeutNum + parameter->fVetoNum) > 0;
}

bool IsNEBULAPlusEnabled(const TNEBULAPlusSimParameter* parameter) {
    return parameter && parameter->fIsLoaded && (parameter->fNeutNum + parameter->fVetoNum) > 0;
}

const char* SimNeutronDetectorModeName(bool nebula_enabled, bool nebula_plus_enabled) {
    if (nebula_enabled && nebula_plus_enabled) return "joint";
    if (nebula_enabled) return "nebula";
    if (nebula_plus_enabled) return "nebula-plus";
    return "none";
}

}  // namespace sim_deuteron
