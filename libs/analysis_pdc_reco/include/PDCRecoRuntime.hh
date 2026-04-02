#ifndef ANALYSIS_PDC_RECO_RUNTIME_HH
#define ANALYSIS_PDC_RECO_RUNTIME_HH

#include "GeometryManager.hh"
#include "MagneticField.hh"
#include "PDCRecoTypes.hh"

#include <filesystem>
#include <set>
#include <string>

namespace analysis::pdc::anaroot_like {

enum class RuntimeBackend {
    kAuto,
    kNeuralNetwork,
    kRungeKutta,
    kMultiDim,
    kLegacy
};

struct RuntimeOptions {
    RuntimeBackend backend = RuntimeBackend::kAuto;
    double mass_mev = 938.2720813;
    double charge_e = 1.0;
    double pdc_sigma_mm = 2.0;
    double target_sigma_xy_mm = 5.0;
    double p_min_mevc = 50.0;
    double p_max_mevc = 5000.0;
    double initial_p_mevc = 1000.0;
    double tolerance_mm = 5.0;
    int max_iterations = 40;
    double rk_step_mm = 5.0;
    double center_brho_tm = 7.2751;
    double magnetic_field_rotation_deg = 30.0;
    std::string nn_model_json_path;
    RkFitMode rk_fit_mode = RkFitMode::kThreePointFree;
    bool compute_uncertainty = true;
    bool compute_posterior_laplace = true;
};

struct MagneticFieldLoadResult {
    bool success = false;
    bool loaded_from_root = false;
    bool loaded_from_table = false;
    std::string loaded_path;
};

std::string ToLowerCopy(std::string value);
RkFitMode ParseRkFitMode(const std::string& text);
RkFitMode ParseRkFitModeOrFallback(const std::string& text, RkFitMode fallback);
std::string RkFitModeName(RkFitMode mode);
RuntimeBackend ParseRuntimeBackend(const std::string& text);
std::string RuntimeBackendName(RuntimeBackend backend);
bool RuntimeBackendUsesNewFramework(RuntimeBackend backend);
bool RuntimeBackendNeedsNnModel(RuntimeBackend backend);
bool RuntimeBackendNeedsFieldMap(RuntimeBackend backend);

std::string ExpandSmsimPathToken(std::string token);
bool ResolveMacroPath(const std::string& raw_path,
                      const std::filesystem::path& base_dir,
                      std::filesystem::path* out_path);
bool LoadGeometryRecursive(GeometryManager& geometry,
                           const std::filesystem::path& macro_path,
                           std::set<std::string>* visited = nullptr);
bool LoadGeometryFromMacro(GeometryManager& geometry,
                           const std::string& macro_path,
                           const std::filesystem::path& base_dir = std::filesystem::current_path());

bool LoadMagneticField(MagneticField& magnetic_field,
                       const std::string& field_path,
                       double rotation_deg);
MagneticFieldLoadResult LoadMagneticFieldWithFallback(MagneticField& magnetic_field,
                                                      const std::string& root_path,
                                                      const std::string& table_path,
                                                      double rotation_deg,
                                                      bool cache_root_copy = false);

RecoConfig BuildRecoConfig(const RuntimeOptions& options, bool have_magnetic_field);
TargetConstraint BuildTargetConstraint(const GeometryManager& geometry, const RuntimeOptions& options);

}  // namespace analysis::pdc::anaroot_like

#endif  // ANALYSIS_PDC_RECO_RUNTIME_HH
