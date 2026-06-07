#ifndef NEUTRON_DETECTOR_SIM_CONFIG_HH
#define NEUTRON_DETECTOR_SIM_CONFIG_HH

class TNEBULASimParameter;
class TNEBULAPlusSimParameter;

namespace sim_deuteron {

bool IsNEBULAEnabled(const TNEBULASimParameter* parameter);
bool IsNEBULAPlusEnabled(const TNEBULAPlusSimParameter* parameter);
const char* SimNeutronDetectorModeName(bool nebula_enabled, bool nebula_plus_enabled);

}  // namespace sim_deuteron

#endif  // NEUTRON_DETECTOR_SIM_CONFIG_HH
