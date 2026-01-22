#include "BeamDeflectionCalculator.hh"
#include "MagneticField.hh"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <cstdlib>
#include <sys/stat.h>
#include <cmath>
#include <cstring>

static const double DEFAULT_DEFLECTION_ANGLES[] = {0.0, 2.0, 4.0, 6.0, 8.0, 10.0};
static const int NUM_DEFLECTION_ANGLES = 6;
static const double DEFAULT_PX_VALUES[] = {100.0, 150.0};
static const int NUM_PX_VALUES = 2;

struct FieldConfig { const char* filename; double strength; };
static const FieldConfig FIELD_MAP_CONFIGS[] = {
    {"141114-0,8T-3000.table",  0.8},
    {"180626-1,00T-3000.table", 1.0},
    {"180626-1,20T-3000.table", 1.2},
    {"180626-1,60T-3000.table", 1.6},
    {"180627-2,00T-3000.table", 2.0}
};
static const int NUM_FIELD_CONFIGS = 5;

void CreateDirectory(const char* path) {
    struct stat info;
    if (stat(path, &info) != 0) {
        char cmd[512]; snprintf(cmd, sizeof(cmd), "mkdir -p %s", path);
        int ret = system(cmd); (void)ret;
    }
}

const char* GetFieldMapDir() {
    static char dir[512];
    const char* smsimdir = std::getenv("SMSIMDIR");
    snprintf(dir, sizeof(dir), "%s/configs/simulation/geometry/filed_map/",
             smsimdir ? smsimdir : "/home/tian/workspace/dpol/smsimulator5.5");
    return dir;
}

void GenerateMacroFile(const BeamDeflectionCalculator::TargetPosition& target,
    double fieldStrength, const char* fieldMapFile,
    const double* pxValues, int numPx, const char* outputDir)
{
    char baseName[64];
    snprintf(baseName, sizeof(baseName), "B%dT_deg%.1f", (int)(fieldStrength * 100), target.deflectionAngle);
    char macroPath[512];
    snprintf(macroPath, sizeof(macroPath), "%s/run_%s.mac", outputDir, baseName);
    
    std::ofstream out(macroPath);
    if (!out.is_open()) { std::cerr << "Error: " << macroPath << std::endl; return; }
    
    out << std::fixed;
    out << "# Field: " << std::setprecision(2) << fieldStrength << " T, Deflection: "
        << std::setprecision(1) << target.deflectionAngle << " deg\n";
    out << "# Target: (" << target.position.X() << ", " << target.position.Y() << ", "
        << target.position.Z() << ") mm, Angle: " << target.rotationAngle << " deg\n\n";
    
    out << "/control/getEnv SMSIMDIR\n\n";
    out << "/samurai/geometry/PDC/Angle 65 deg\n";
    out << "/samurai/geometry/PDC/Position1 0 0 400 cm\n";
    out << "/samurai/geometry/PDC/Position2 0 0 500 cm\n\n";
    out << "/samurai/geometry/Dipole/Angle 30 deg\n";
    out << "/samurai/geometry/Dipole/FieldFile {SMSIMDIR}/configs/simulation/geometry/filed_map/" << fieldMapFile << "\n";
    out << "/samurai/geometry/Dipole/FieldFactor 1\n\n";
    out << "/samurai/geometry/Target/Position " << std::setprecision(4)
        << target.position.X() << " " << target.position.Y() << " " << target.position.Z() << " mm\n";
    out << "/samurai/geometry/Target/Angle " << std::setprecision(2) << target.rotationAngle << " deg\n\n";
    out << "/samurai/geometry/FillAir true\n/samurai/geometry/Update\n\n";
    
    out << "/vis/open OGL 800x600-0+0\n/vis/drawVolume\n";
    out << "/vis/viewer/set/viewpointThetaPhi 90 0\n\n";
    out << "/vis/viewer/set/autoRefresh false\n";
    out << "/vis/scene/endOfEventAction accumulate\n";
    out << "/vis/scene/endOfRunAction accumulate\n";
    out << "/vis/scene/add/trajectories smooth\n";
    out << "/vis/modeling/trajectories/create/drawByParticleID\n";
    out << "/vis/modeling/trajectories/drawByParticleID-0/set deuteron green\n";
    out << "/vis/modeling/trajectories/drawByParticleID-0/set proton red\n";
    out << "/vis/modeling/trajectories/drawByParticleID-0/set neutron blue\n";
    out << "/vis/scene/add/hits\n\n";
    
    out << "# 1. Deuteron: NO targetAngle\n";
    out << "/action/gun/Type Pencil\n";
    out << "/action/gun/SetBeamParticleName deuteron\n";
    out << "/action/gun/Position 0 0 -4000 mm\n";
    out << "/action/gun/AngleY 0 deg\n";
    out << "/action/gun/Energy 380 MeV\n";
    out << "/run/beamOn 1\n\n";
    
    const double protonMass = 938.272, pz = 627.0;
    double rotAngleRad = target.rotationAngle * M_PI / 180.0;
    double cosRot = std::cos(rotAngleRad), sinRot = std::sin(rotAngleRad);
    int idx = 2;
    
    auto shootParticle = [&](const char* name, double px, double mass, const char* label) {
        double pTotal = std::sqrt(px*px + pz*pz);
        double energy = std::sqrt(pTotal*pTotal + mass*mass) - mass;
        double labPx = px * cosRot + pz * sinRot;
        double labPz = -px * sinRot + pz * cosRot;
        double angleY = std::atan2(labPx, labPz);
        out << "# " << idx++ << ". " << label << " (WITH targetAngle)\n";
        out << "/action/gun/SetBeamParticleName " << name << "\n";
        out << "/action/gun/Position " << std::setprecision(4)
            << target.position.X() << " " << target.position.Y() << " " << target.position.Z() << " mm\n";
        out << "/action/gun/AngleY " << std::setprecision(6) << angleY << " rad\n";
        out << "/action/gun/Energy " << std::setprecision(2) << energy << " MeV\n";
        out << "/run/beamOn 1\n\n";
    };
    
    shootParticle("proton", 0.0, protonMass, "Center proton");
    for (int i = 0; i < numPx; ++i) {
        char lbl[32]; snprintf(lbl, sizeof(lbl), "Proton Px=+%d", (int)pxValues[i]);
        shootParticle("proton", pxValues[i], protonMass, lbl);
        snprintf(lbl, sizeof(lbl), "Proton Px=-%d", (int)pxValues[i]);
        shootParticle("proton", -pxValues[i], protonMass, lbl);
    }
    
    {
        const double neutronMass = 939.565;
        double energy = std::sqrt(pz*pz + neutronMass*neutronMass) - neutronMass;
        double labPx = pz * sinRot, labPz = pz * cosRot;
        double angleY = std::atan2(labPx, labPz);
        out << "# " << idx++ << ". Neutron (WITH targetAngle)\n";
        out << "/action/gun/SetBeamParticleName neutron\n";
        out << "/action/gun/Position " << std::setprecision(4)
            << target.position.X() << " " << target.position.Y() << " " << target.position.Z() << " mm\n";
        out << "/action/gun/AngleY " << std::setprecision(6) << angleY << " rad\n";
        out << "/action/gun/Energy " << std::setprecision(2) << energy << " MeV\n";
        out << "/run/beamOn 1\n\n";
    }
    
    out << "/vis/viewer/set/autoRefresh true\n/vis/viewer/refresh\n";
    out << "/vis/ogl/export " << baseName << ".png\n";
    out.close();
    std::cout << "Generated: " << macroPath << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) { std::cout << "Usage: " << argv[0] << " <output_dir>\n"; return 1; }
    const char* outputDir = argv[1];
    CreateDirectory(outputDir);
    std::cout << "\n=== GenerateTargetConfigs ===\nOutput: " << outputDir << "\n\n";
    
    BeamDeflectionCalculator beamCalc;
    for (int fc = 0; fc < NUM_FIELD_CONFIGS; ++fc) {
        const FieldConfig& config = FIELD_MAP_CONFIGS[fc];
        std::cout << "Processing: " << config.filename << " (" << config.strength << " T)" << std::endl;
        
        MagneticField* magField = new MagneticField();
        char fullPath[512]; snprintf(fullPath, sizeof(fullPath), "%s%s", GetFieldMapDir(), config.filename);
        
        if (!magField->LoadFieldMap(fullPath)) {
            char binPath[512]; snprintf(binPath, sizeof(binPath), "%s", fullPath);
            char* tablePos = strstr(binPath, ".table");
            if (tablePos) { strcpy(tablePos, ".bin");
                if (!magField->LoadFieldMap(binPath)) { delete magField; continue; }
            } else { delete magField; continue; }
        }
        
        beamCalc.SetMagneticField(magField);
        std::vector<double> angles(DEFAULT_DEFLECTION_ANGLES, DEFAULT_DEFLECTION_ANGLES + NUM_DEFLECTION_ANGLES);
        std::vector<BeamDeflectionCalculator::TargetPosition> targets = beamCalc.CalculateTargetPositions(angles);
        for (size_t i = 0; i < targets.size(); ++i)
            GenerateMacroFile(targets[i], config.strength, config.filename, DEFAULT_PX_VALUES, NUM_PX_VALUES, outputDir);
        delete magField;
    }
    std::cout << "\n=== Complete ===" << std::endl;
    return 0;
}
