#include "GeometryManager.hh"
#include <iostream>
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
        std::cerr << "Error: Could not open geometry file: " << filename << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        ParseLine(line);
    }

    // Post-parsing processing
    if (fParams.count("/samurai/geometry/PDC/Angle")) {
        fAngleRad = atof(fParams["/samurai/geometry/PDC/Angle"].c_str()) * TMath::DegToRad();
    }

    if (fParams.count("/samurai/geometry/PDC/Position1")) {
        std::stringstream ss(fParams["/samurai/geometry/PDC/Position1"]);
        double x, y, z;
        std::string unit;
        ss >> x >> y >> z >> unit;
        if (unit == "cm") { x*=10; y*=10; z*=10; } // convert to mm
        fPDC1_Position.SetXYZ(x, y, z);
    }

    if (fParams.count("/samurai/geometry/PDC/Position2")) {
        std::stringstream ss(fParams["/samurai/geometry/PDC/Position2"]);
        double x, y, z;
        std::string unit;
        ss >> x >> y >> z >> unit;
        if (unit == "cm") { x*=10; y*=10; z*=10; } // convert to mm
        fPDC2_Position.SetXYZ(x, y, z);
    }

    // Parse Target Position
    if (fParams.count("/samurai/geometry/Target/Position")) {
        std::stringstream ss(fParams["/samurai/geometry/Target/Position"]);
        double x, y, z;
        std::string unit;
        ss >> x >> y >> z >> unit;
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

    std::cout << "Geometry loaded from " << filename << std::endl;
    std::cout << "  PDC Angle: " << fAngleRad * TMath::RadToDeg() << " deg" << std::endl;
    std::cout << "  PDC1 Pos: (" << fPDC1_Position.X() << ", " << fPDC1_Position.Y() << ", " << fPDC1_Position.Z() << ") mm" << std::endl;
    std::cout << "  PDC2 Pos: (" << fPDC2_Position.X() << ", " << fPDC2_Position.Y() << ", " << fPDC2_Position.Z() << ") mm" << std::endl;
    std::cout << "  Target Pos: (" << fTarget_Position.X() << ", " << fTarget_Position.Y() << ", " << fTarget_Position.Z() << ") mm" << std::endl;
    std::cout << "  Target Angle: " << fTargetAngleRad * TMath::RadToDeg() << " deg" << std::endl;

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
        fParams[key] = value;
    }
}
