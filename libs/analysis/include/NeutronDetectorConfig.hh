#ifndef NEUTRON_DETECTOR_CONFIG_HH
#define NEUTRON_DETECTOR_CONFIG_HH

#include <stdexcept>
#include <string>

namespace analysis::neutron {

enum class NeutronDetectorMode {
    kAuto,
    kNone,
    kNebula,
    kNebulaPlus,
    kJoint
};

struct DetectorAvailability {
    bool metadata_available = true;
    bool has_nebula_branch = false;
    bool has_nebula_plus_branch = false;
    bool nebula_parameter_loaded = false;
    bool nebula_plus_parameter_loaded = false;
    int nebula_detector_count = 0;
    int nebula_plus_detector_count = 0;
};

struct ResolvedNeutronDetectorMode {
    NeutronDetectorMode requested_mode = NeutronDetectorMode::kAuto;
    NeutronDetectorMode effective_mode = NeutronDetectorMode::kNone;
    bool used_metadata = false;
    bool ignored_nebula_branch = false;
    bool ignored_nebula_plus_branch = false;
};

NeutronDetectorMode ParseNeutronDetectorMode(const std::string& text);
std::string NeutronDetectorModeName(NeutronDetectorMode mode);
ResolvedNeutronDetectorMode ResolveNeutronDetectorMode(NeutronDetectorMode requested,
                                                       const DetectorAvailability& availability);

}  // namespace analysis::neutron

#endif  // NEUTRON_DETECTOR_CONFIG_HH
