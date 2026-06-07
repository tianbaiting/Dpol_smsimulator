#include "NeutronDetectorConfig.hh"

#include <algorithm>
#include <cctype>

namespace analysis::neutron {
namespace {

std::string ToLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool HasMetadataNebula(const DetectorAvailability& availability) {
    return availability.nebula_parameter_loaded && availability.nebula_detector_count > 0;
}

bool HasMetadataNebulaPlus(const DetectorAvailability& availability) {
    return availability.nebula_plus_parameter_loaded && availability.nebula_plus_detector_count > 0;
}

bool HasNebula(const DetectorAvailability& availability) {
    return availability.metadata_available ? HasMetadataNebula(availability)
                                           : availability.has_nebula_branch;
}

bool HasNebulaPlus(const DetectorAvailability& availability) {
    return availability.metadata_available ? HasMetadataNebulaPlus(availability)
                                           : availability.has_nebula_plus_branch;
}

}  // namespace

NeutronDetectorMode ParseNeutronDetectorMode(const std::string& text) {
    const std::string lowered = ToLower(text);
    if (lowered == "auto") return NeutronDetectorMode::kAuto;
    if (lowered == "none" || lowered == "off") return NeutronDetectorMode::kNone;
    if (lowered == "nebula") return NeutronDetectorMode::kNebula;
    if (lowered == "nebula-plus" || lowered == "nebulaplus" || lowered == "plus") {
        return NeutronDetectorMode::kNebulaPlus;
    }
    if (lowered == "joint" || lowered == "both") return NeutronDetectorMode::kJoint;
    throw std::runtime_error("invalid neutron detector mode: " + text);
}

std::string NeutronDetectorModeName(NeutronDetectorMode mode) {
    switch (mode) {
        case NeutronDetectorMode::kAuto: return "auto";
        case NeutronDetectorMode::kNone: return "none";
        case NeutronDetectorMode::kNebula: return "nebula";
        case NeutronDetectorMode::kNebulaPlus: return "nebula-plus";
        case NeutronDetectorMode::kJoint: return "joint";
    }
    return "unknown";
}

ResolvedNeutronDetectorMode ResolveNeutronDetectorMode(
        NeutronDetectorMode requested,
        const DetectorAvailability& availability) {
    ResolvedNeutronDetectorMode resolved;
    resolved.requested_mode = requested;
    resolved.used_metadata = availability.metadata_available;

    const bool nebula = HasNebula(availability);
    const bool nebula_plus = HasNebulaPlus(availability);
    resolved.ignored_nebula_branch = availability.has_nebula_branch && !nebula;
    resolved.ignored_nebula_plus_branch = availability.has_nebula_plus_branch && !nebula_plus;

    if (requested == NeutronDetectorMode::kAuto) {
        if (nebula && nebula_plus) {
            resolved.effective_mode = NeutronDetectorMode::kJoint;
        } else if (nebula) {
            resolved.effective_mode = NeutronDetectorMode::kNebula;
        } else if (nebula_plus) {
            resolved.effective_mode = NeutronDetectorMode::kNebulaPlus;
        } else {
            resolved.effective_mode = NeutronDetectorMode::kNone;
        }
        return resolved;
    }

    if (requested == NeutronDetectorMode::kNone) {
        resolved.effective_mode = NeutronDetectorMode::kNone;
        return resolved;
    }

    if (requested == NeutronDetectorMode::kNebula && !nebula) {
        throw std::runtime_error("requested NEBULA reconstruction but NEBULAPla is unavailable or disabled");
    }
    if (requested == NeutronDetectorMode::kNebulaPlus && !nebula_plus) {
        throw std::runtime_error("requested NEBULA-Plus reconstruction but NEBULAPlusPla is unavailable or disabled");
    }
    if (requested == NeutronDetectorMode::kJoint && (!nebula || !nebula_plus)) {
        throw std::runtime_error("requested joint neutron reconstruction but NEBULA and NEBULA-Plus are not both available");
    }

    resolved.effective_mode = requested;
    return resolved;
}

}  // namespace analysis::neutron
