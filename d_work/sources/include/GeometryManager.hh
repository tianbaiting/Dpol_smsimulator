#ifndef GEOMETRYMANAGER_HH
#define GEOMETRYMANAGER_HH

#include "TVector3.h"
#include <string>
#include <map>

class GeometryManager {
public:
    GeometryManager();
    virtual ~GeometryManager();

    // Load geometry parameters from a Geant4 macro file
    bool LoadGeometry(const std::string& filename);

    // --- Getters for geometry parameters ---
    double GetAngleRad() const { return fAngleRad; }
    TVector3 GetPDC1Position() const { return fPDC1_Position; }
    TVector3 GetPDC2Position() const { return fPDC2_Position; }
    TVector3 GetTargetPosition() const { return fTargetPosition; }

private:
    // Map to store key-value pairs from the macro file
    std::map<std::string, std::string> fParams;

    // Parsed geometry parameters
    double fAngleRad;
    TVector3 fPDC1_Position;
    TVector3 fPDC2_Position;
    TVector3 fTargetPosition;


    // Helper to parse a line from the macro file
    void ParseLine(const std::string& line);
};

#endif // GEOMETRYMANAGER_HH
