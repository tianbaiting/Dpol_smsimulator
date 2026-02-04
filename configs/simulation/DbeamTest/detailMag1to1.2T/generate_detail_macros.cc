#include "BeamDeflectionCalculator.hh"
#include "MagneticField.hh"
#include "TVector3.h"
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct FieldConfig {
    std::string tag;
    std::string filename;
    double strength;
};

static std::string GetSmsimDir() {
    const char* env = std::getenv("SMSIMDIR");
    if (env && *env) {
        return std::string(env);
    }
    return std::string("/home/tian/workspace/dpol/smsimulator5.5");
}

static std::string JoinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (a.back() == '/') return a + b;
    return a + "/" + b;
}

static std::string FormatAngle(double angle) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << angle;
    return oss.str();
}

static void WriteGeometryMacro(const std::string& outDir,
                               const FieldConfig& field) {
    std::string path = JoinPath(outDir, "geometry_B" + field.tag + "T.mac");
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "Error: cannot write " << path << "\n";
        return;
    }

    out << "# Geometry configuration - Magnetic field: " << std::fixed << std::setprecision(2)
        << field.strength << "T\n";
    out << "/control/getEnv SMSIMDIR\n";
    out << "/samurai/geometry/PDC/Angle 65 deg\n";
    out << "/samurai/geometry/PDC/Position1 0 0 400 cm\n";
    out << "/samurai/geometry/PDC/Position2 0 0 500 cm\n";
    out << "/samurai/geometry/Dump/SetDump false\n";
    out << "/samurai/geometry/Dump/Angle 36.67 deg\n";
    out << "/samurai/geometry/Dump/Position -0.93 -25.00 0.00 cm\n";
    out << "/samurai/geometry/NEBULA/ParameterFileName {SMSIMDIR}/configs/simulation/geometry/NEBULA_Dayone.csv\n";
    out << "/samurai/geometry/NEBULA/DetectorParameterFileName {SMSIMDIR}/configs/simulation/geometry/NEBULA_Detectors_Dayone.csv\n";
    out << "/samurai/geometry/Dipole/Angle 30 deg\n";
    out << "/samurai/geometry/Dipole/FieldFile {SMSIMDIR}/configs/simulation/geometry/filed_map/" << field.filename << "\n";
    out << "/samurai/geometry/Dipole/FieldFactor 1\n";
    out << "/samurai/geometry/FillAir true\n";
    out << "/samurai/geometry/Update\n";
}

static void WriteProtonNeutronMacro(const std::string& outDir,
                                    const std::string& rootFile,
                                    const FieldConfig& field,
                                    const BeamDeflectionCalculator::TargetPosition& target) {
    std::string angleStr = FormatAngle(target.deflectionAngle);
    std::string macName = "run_protons_B" + field.tag + "T_" + angleStr + "deg.mac";
    std::string path = JoinPath(outDir, macName);

    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "Error: cannot write " << path << "\n";
        return;
    }

    const double tx_cm = target.position.X() / 10.0;
    const double ty_cm = target.position.Y() / 10.0;
    const double tz_cm = target.position.Z() / 10.0;

    out << "# =========================================================================\n";
    out << "# [EN] Proton+Neutron trajectories using Tree input + Target parameters\n";
    out << "# [CN] 使用Tree输入 + Target参数的质子/中子轨迹可视化\n";
    out << "# =========================================================================\n";
    out << "# Field: B=" << std::fixed << std::setprecision(2) << field.strength
        << "T, Beam angle=" << angleStr << " deg\n";
    out << "# Target position: X=" << std::setprecision(4)
        << target.position.X() << "mm, Y=" << target.position.Y()
        << "mm, Z=" << target.position.Z() << "mm\n";
    out << "# Proton + Neutron momenta: Px = ±100, ±150 MeV/c (8 tracks total: 4p + 4n)\n";
    out << "# =========================================================================\n\n";

    out << "/vis/open OGLIQt 1280x720\n";
    out << "\n";

    out << "/samurai/geometry/Target/Position " << std::fixed << std::setprecision(4)
        << tx_cm << " " << ty_cm << " " << tz_cm << " cm\n";
    out << "/samurai/geometry/Target/Angle " << std::setprecision(1)
        << target.deflectionAngle << " deg\n\n";

    out << "/control/execute " << JoinPath(outDir, "geometry_B" + field.tag + "T.mac") << "\n\n";

    out << "/action/file/OverWrite y\n";
    out << "/action/file/RunName protons_B" << field.tag << "T_" << angleStr << "deg\n";
    out << "/action/file/SaveDirectory " << JoinPath(outDir, "output") << "\n\n";

    out << "/vis/drawVolume\n";
    out << "/vis/viewer/set/upVector 1 0 0\n";
    out << "/vis/viewer/set/viewpointThetaPhi 90. 90.\n";
    out << "/vis/scene/add/axes 0 0 0 6000 mm\n\n";

    out << "/vis/scene/add/trajectories smooth\n";
    out << "/vis/modeling/trajectories/create/drawByCharge\n";
    out << "/vis/modeling/trajectories/drawByCharge-0/default/setDrawStepPts true\n";
    out << "/vis/modeling/trajectories/drawByCharge-0/default/setStepPtsSize 3\n";
    out << "/vis/scene/endOfEventAction accumulate\n\n";

    out << "/vis/viewer/set/autoRefresh false\n";
    out << "/vis/verbose warnings\n\n";

    out << "/action/gun/Type Tree\n";
    out << "/action/gun/tree/InputFileName " << rootFile << "\n";
    out << "/action/gun/tree/TreeName tree\n";
    out << "/action/gun/tree/beamOn 8\n\n";

    out << "/vis/viewer/set/autoRefresh true\n";
    out << "/vis/viewer/refresh\n";
    out << "/vis/viewer/update\n\n";

    out << "/vis/ogl/export " << JoinPath(outDir, "output")
        << "/protons_B" << field.tag << "T_" << angleStr << "deg.png\n";
}

static void WriteDeuteronMacroPerField(const std::string& outDir,
                                       const FieldConfig& field) {
    std::string macName = "run_deuteron_B" + field.tag + "T.mac";
    std::string path = JoinPath(outDir, macName);

    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "Error: cannot write " << path << "\n";
        return;
    }

    out << "# =========================================================================\n";
    out << "# [EN] Deuteron beam track from fixed upstream position\n";
    out << "# [CN] 氘束轨迹从固定上游位置发射\n";
    out << "# =========================================================================\n";
    out << "# Field: B=" << std::fixed << std::setprecision(2) << field.strength << "T\n";
    out << "# Deuteron start: (0, 0, -4000 mm)\n";
    out << "# =========================================================================\n\n";

    out << "/vis/open OGLIQt 1280x720\n\n";

    out << "/samurai/geometry/Target/Position 0 0 -400 cm\n";
    out << "/samurai/geometry/Target/Angle 0 deg\n\n";

    out << "/control/execute " << JoinPath(outDir, "geometry_B" + field.tag + "T.mac") << "\n\n";

    out << "/action/file/OverWrite y\n";
    out << "/action/file/RunName deuteron_B" << field.tag << "T_380MeV\n";
    out << "/action/file/SaveDirectory " << JoinPath(outDir, "deuteron_output") << "\n\n";

    out << "/vis/drawVolume\n";
    out << "/vis/viewer/set/upVector 1 0 0\n";
    out << "/vis/viewer/set/viewpointThetaPhi 90. 90.\n";
    out << "/vis/scene/add/axes 0 0 0 6000 mm\n\n";

    out << "/vis/scene/add/trajectories smooth\n";
    out << "/vis/modeling/trajectories/create/drawByCharge\n";
    out << "/vis/modeling/trajectories/drawByCharge-0/default/setDrawStepPts true\n";
    out << "/vis/modeling/trajectories/drawByCharge-0/default/setStepPtsSize 2\n";
    out << "/vis/scene/endOfEventAction accumulate\n\n";

    out << "/vis/viewer/set/autoRefresh false\n";
    out << "/vis/verbose warnings\n\n";

    out << "/action/gun/Type Pencil\n";
    out << "/action/gun/SetBeamParticleName deuteron\n";
    out << "/action/gun/Position 0 0 -4000 mm\n";
    out << "/action/gun/AngleY 0 deg\n";
    out << "/action/gun/Energy 380 MeV\n";
    out << "/run/beamOn 1\n\n";

    out << "/vis/viewer/set/autoRefresh true\n";
    out << "/vis/viewer/refresh\n";
    out << "/vis/viewer/update\n\n";

    out << "/vis/ogl/export " << JoinPath(outDir, "deuteron_output")
        << "/deuteron_B" << field.tag << "T_380MeV.png\n";
}

int main(int argc, char* argv[]) {
    std::string outDir = (argc > 1) ? argv[1] : std::filesystem::current_path().string();
    std::filesystem::create_directories(outDir);
    std::filesystem::create_directories(JoinPath(outDir, "output"));
    std::filesystem::create_directories(JoinPath(outDir, "deuteron_output"));

    const std::string smsimdir = GetSmsimDir();
    const std::string fieldDir = JoinPath(smsimdir, "configs/simulation/geometry/filed_map");
    const std::string rootFile = JoinPath(smsimdir, "configs/simulation/DbeamTest/track_vis_useTree/rootfiles/pn_8tracks.root");

    const std::vector<FieldConfig> fields = {
        {"100", "180626-1,00T-3000.table", 1.00},
        {"105", "180703-1,05T-3000.table", 1.05},
        {"110", "180703-1,10T-3000.table", 1.10},
        {"115", "180703-1,15T-3000.table", 1.15},
        {"120", "180626-1,20T-3000.table", 1.20}
    };

    std::vector<double> angles;
    for (int a = 0; a <= 10; ++a) angles.push_back(static_cast<double>(a));

    for (const auto& field : fields) {
        WriteGeometryMacro(outDir, field);
        WriteDeuteronMacroPerField(outDir, field);

        MagneticField magField;
        std::string fieldPath = JoinPath(fieldDir, field.filename);
        if (!magField.LoadFieldMap(fieldPath)) {
            std::cerr << "Error: cannot load field map " << fieldPath << "\n";
            continue;
        }

        BeamDeflectionCalculator beamCalc;
        beamCalc.SetMagneticField(&magField);

        std::string summaryPath = JoinPath(outDir, "target_positions_B" + field.tag + "T.txt");
        std::ofstream summary(summaryPath);
        if (!summary.is_open()) {
            std::cerr << "Error: cannot write " << summaryPath << "\n";
            continue;
        }

        summary << "# angle_deg  x_mm  y_mm  z_mm  rotation_deg\n";

        for (double angle : angles) {
            BeamDeflectionCalculator::TargetPosition target;
            if (std::abs(angle) < 1e-9) {
                target.deflectionAngle = 0.0;
                target.position = TVector3(0, 0, -4000);
                target.beamDirection = TVector3(0, 0, 1);
                target.rotationAngle = 0.0;
            } else {
                target = beamCalc.CalculateTargetPosition(angle);
            }

            summary << std::fixed << std::setprecision(2) << angle << "  "
                    << std::setprecision(4) << target.position.X() << "  "
                    << target.position.Y() << "  "
                    << target.position.Z() << "  "
                    << std::setprecision(2) << target.rotationAngle << "\n";

            WriteProtonNeutronMacro(outDir, rootFile, field, target);
        }
    }

    std::cout << "Generated macros in: " << outDir << "\n";
    return 0;
}
