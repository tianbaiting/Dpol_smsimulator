#include "GeometryManager.hh"
#include "SMLogger.hh"
#include <fstream>
#include <sstream>
#include <cstdlib>

GeometryManager::GeometryManager()
    : fAngleRad(0.0), fPDC1_Position(0,0,0), fPDC2_Position(0,0,0),
      fTarget_Position(0,0,0), fTargetAngleRad(0.0) {}

GeometryManager::~GeometryManager() {}

bool GeometryManager::LoadGeometry(const std::string& filename) {
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        SM_ERROR("Could not open geometry file: {}", filename);
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        ParseLine(line);
    }

    // Post-parsing processing
    if (fParams.count("/samurai/geometry/PDC/Angle")) {
        // [EN] Store angles in radians to keep trigonometry consistent in reconstruction. / [CN] 角度统一存为弧度以保持重建中的三角计算一致。
        fAngleRad = atof(fParams["/samurai/geometry/PDC/Angle"].c_str()) * TMath::DegToRad();
    }

    if (fParams.count("/samurai/geometry/PDC/Position1")) {
        std::stringstream ss(fParams["/samurai/geometry/PDC/Position1"]);
        double x, y, z;
        std::string unit;
        ss >> x >> y >> z >> unit;
        // [EN] Geometry is normalized to mm for downstream tracking and display. / [CN] 几何参数统一为mm以匹配后续追踪与显示。
        if (unit == "cm") { x*=10; y*=10; z*=10; } // convert to mm
        fPDC1_Position.SetXYZ(x, y, z);
    }

    if (fParams.count("/samurai/geometry/PDC/Position2")) {
        std::stringstream ss(fParams["/samurai/geometry/PDC/Position2"]);
        double x, y, z;
        std::string unit;
        ss >> x >> y >> z >> unit;
        // [EN] Geometry is normalized to mm for downstream tracking and display. / [CN] 几何参数统一为mm以匹配后续追踪与显示。
        if (unit == "cm") { x*=10; y*=10; z*=10; } // convert to mm
        fPDC2_Position.SetXYZ(x, y, z);
    }

    // Parse Target Position
    if (fParams.count("/samurai/geometry/Target/Position")) {
        std::stringstream ss(fParams["/samurai/geometry/Target/Position"]);
        double x, y, z;
        std::string unit;
        ss >> x >> y >> z >> unit;
        // [EN] Geometry is normalized to mm for downstream tracking and display. / [CN] 几何参数统一为mm以匹配后续追踪与显示。
        if (unit == "cm") { x*=10; y*=10; z*=10; } // convert to mm
        fTarget_Position.SetXYZ(x, y, z);
    }

    // Parse Target Angle
    if (fParams.count("/samurai/geometry/Target/Angle")) {
        std::stringstream ss(fParams["/samurai/geometry/Target/Angle"]);
        double angle;
        std::string unit;
        ss >> angle >> unit;
        if (unit == "deg") {
            fTargetAngleRad = angle * TMath::DegToRad();
        } else {
            fTargetAngleRad = angle; // assume radians
        }
    }

    SM_INFO("Geometry loaded from {}", filename);
    SM_INFO("  PDC Angle: {:.2f} deg", fAngleRad * TMath::RadToDeg());
    SM_INFO("  PDC1 Pos: ({:.2f}, {:.2f}, {:.2f}) mm", fPDC1_Position.X(), fPDC1_Position.Y(), fPDC1_Position.Z());
    SM_INFO("  PDC2 Pos: ({:.2f}, {:.2f}, {:.2f}) mm", fPDC2_Position.X(), fPDC2_Position.Y(), fPDC2_Position.Z());
    SM_INFO("  Target Pos: ({:.2f}, {:.2f}, {:.2f}) mm", fTarget_Position.X(), fTarget_Position.Y(), fTarget_Position.Z());
    SM_INFO("  Target Angle: {:.2f} deg", fTargetAngleRad * TMath::RadToDeg());

    return true;
}

void GeometryManager::ParseLine(const std::string& line) {
    std::stringstream ss(line);
    std::string key;
    ss >> key;

    if (key.empty() || key[0] == '#') return; // Skip comments and empty lines

    std::string value;
    std::getline(ss, value);

    // Trim leading whitespace from value
    size_t first = value.find_first_not_of(" ");
    if (std::string::npos != first) {
        value = value.substr(first);
    }

    if (key == "/samurai/geometry/PDC/Angle" || 
        key == "/samurai/geometry/PDC/Position1" || 
        key == "/samurai/geometry/PDC/Position2" ||
        key == "/samurai/geometry/Target/Position" ||
        key == "/samurai/geometry/Target/Angle") {
        // [EN] Only whitelist parameters used in reconstruction to keep parsing deterministic. / [CN] 仅保留重建所需参数以确保解析确定性。
        fParams[key] = value;
    }
}
