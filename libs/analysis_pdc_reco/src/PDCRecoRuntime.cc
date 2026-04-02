#include "PDCRecoRuntime.hh"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace analysis::pdc::anaroot_like {

namespace {

std::string TrimCopy(const std::string& input) {
    const auto begin = input.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = input.find_last_not_of(" \t\r\n");
    return input.substr(begin, end - begin + 1);
}

bool PathExists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

}  // namespace

std::string ToLowerCopy(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

RkFitMode ParseRkFitMode(const std::string& text) {
    const std::string lowered = ToLowerCopy(text);
    if (lowered == "two-point-backprop" || lowered == "two_point_backprop" ||
        lowered == "twopointbackprop" || lowered == "two-point" ||
        lowered == "backprop" || lowered == "default") {
        return RkFitMode::kTwoPointBackprop;
    }
    if (lowered == "fixed-target-pdc-only" || lowered == "fixed_target_pdc_only" ||
        lowered == "fixedtargetpdconly" || lowered == "pdc_only") {
        return RkFitMode::kFixedTargetPdcOnly;
    }
    if (lowered == "three-point-free" || lowered == "three_point_free" ||
        lowered == "threepointfree" || lowered == "free-three-point" ||
        lowered == "target-xy-prior" || lowered == "target_xy_prior" ||
        lowered == "targetxprior") {
        return RkFitMode::kThreePointFree;
    }
    throw std::runtime_error("unknown rk fit mode: " + text);
}

RkFitMode ParseRkFitModeOrFallback(const std::string& text, RkFitMode fallback) {
    if (text.empty()) {
        return fallback;
    }
    try {
        return ParseRkFitMode(text);
    } catch (...) {
        return fallback;
    }
}

std::string RkFitModeName(RkFitMode mode) {
    switch (mode) {
        case RkFitMode::kTwoPointBackprop: return "two-point-backprop";
        case RkFitMode::kFixedTargetPdcOnly: return "fixed-target-pdc-only";
        case RkFitMode::kThreePointFree: return "three-point-free";
    }
    return "three-point-free";
}

RuntimeBackend ParseRuntimeBackend(const std::string& text) {
    const std::string lowered = ToLowerCopy(text);
    if (lowered == "auto") {
        return RuntimeBackend::kAuto;
    }
    if (lowered == "nn") {
        return RuntimeBackend::kNeuralNetwork;
    }
    if (lowered == "rk") {
        return RuntimeBackend::kRungeKutta;
    }
    if (lowered == "multidim") {
        return RuntimeBackend::kMultiDim;
    }
    if (lowered == "legacy") {
        return RuntimeBackend::kLegacy;
    }
    throw std::runtime_error("unknown backend: " + text);
}

std::string RuntimeBackendName(RuntimeBackend backend) {
    switch (backend) {
        case RuntimeBackend::kAuto: return "auto";
        case RuntimeBackend::kNeuralNetwork: return "nn";
        case RuntimeBackend::kRungeKutta: return "rk";
        case RuntimeBackend::kMultiDim: return "multidim";
        case RuntimeBackend::kLegacy: return "legacy";
    }
    return "auto";
}

bool RuntimeBackendUsesNewFramework(RuntimeBackend backend) {
    return backend != RuntimeBackend::kLegacy;
}

bool RuntimeBackendNeedsNnModel(RuntimeBackend backend) {
    return backend == RuntimeBackend::kNeuralNetwork;
}

bool RuntimeBackendNeedsFieldMap(RuntimeBackend backend) {
    return backend == RuntimeBackend::kRungeKutta || backend == RuntimeBackend::kMultiDim;
}

std::string ExpandSmsimPathToken(std::string token) {
    const char* sms_dir = std::getenv("SMSIMDIR");
    const std::string sms = sms_dir ? std::string(sms_dir) : std::string();

    const std::string key1 = "{SMSIMDIR}";
    const std::string key2 = "$SMSIMDIR";

    std::size_t pos = token.find(key1);
    while (pos != std::string::npos) {
        token.replace(pos, key1.size(), sms);
        pos = token.find(key1, pos + sms.size());
    }

    pos = token.find(key2);
    while (pos != std::string::npos) {
        token.replace(pos, key2.size(), sms);
        pos = token.find(key2, pos + sms.size());
    }
    return token;
}

bool ResolveMacroPath(const std::string& raw_path,
                      const std::filesystem::path& base_dir,
                      std::filesystem::path* out_path) {
    if (!out_path) {
        return false;
    }

    std::filesystem::path path = ExpandSmsimPathToken(raw_path);
    if (!path.is_absolute()) {
        path = base_dir / path;
    }

    std::error_code ec;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        *out_path = canonical;
        return true;
    }

    *out_path = path.lexically_normal();
    return PathExists(*out_path);
}

bool LoadGeometryRecursive(GeometryManager& geometry,
                           const std::filesystem::path& macro_path,
                           std::set<std::string>* visited) {
    std::set<std::string> local_visited;
    std::set<std::string>& visited_ref = visited ? *visited : local_visited;

    const std::string key = macro_path.lexically_normal().string();
    if (visited_ref.count(key) != 0U) {
        return true;
    }

    std::ifstream fin(macro_path);
    if (!fin.is_open()) {
        return false;
    }
    visited_ref.insert(key);

    std::string line;
    while (std::getline(fin, line)) {
        const std::string clean = TrimCopy(line);
        if (clean.empty() || clean[0] == '#') {
            continue;
        }
        if (clean.rfind("/control/execute", 0) != 0) {
            continue;
        }

        std::stringstream ss(clean);
        std::string command;
        std::string include_raw_path;
        ss >> command >> include_raw_path;
        if (include_raw_path.empty()) {
            continue;
        }

        std::filesystem::path include_path;
        if (!ResolveMacroPath(include_raw_path, macro_path.parent_path(), &include_path)) {
            continue;
        }

        // [EN] Resolve nested include macros before the parent so GeometryManager sees one consistent parameter set. / [CN] 先展开嵌套宏再加载父宏，保证GeometryManager只看到一套一致的参数覆盖结果。
        if (!LoadGeometryRecursive(geometry, include_path, &visited_ref)) {
            return false;
        }
    }

    return geometry.LoadGeometry(macro_path.string());
}

bool LoadGeometryFromMacro(GeometryManager& geometry,
                           const std::string& macro_path,
                           const std::filesystem::path& base_dir) {
    std::filesystem::path resolved_path;
    if (!ResolveMacroPath(macro_path, base_dir, &resolved_path)) {
        return false;
    }
    std::set<std::string> visited;
    return LoadGeometryRecursive(geometry, resolved_path, &visited);
}

bool LoadMagneticField(MagneticField& magnetic_field,
                       const std::string& field_path,
                       double rotation_deg) {
    if (field_path.empty()) {
        return false;
    }

    const std::filesystem::path path(field_path);
    bool loaded = false;
    if (ToLowerCopy(path.extension().string()) == ".root") {
        loaded = magnetic_field.LoadFromROOTFile(path.string());
    } else {
        loaded = magnetic_field.LoadFieldMap(path.string());
    }
    if (!loaded) {
        return false;
    }
    magnetic_field.SetRotationAngle(rotation_deg);
    return true;
}

MagneticFieldLoadResult LoadMagneticFieldWithFallback(MagneticField& magnetic_field,
                                                      const std::string& root_path,
                                                      const std::string& table_path,
                                                      double rotation_deg,
                                                      bool cache_root_copy) {
    MagneticFieldLoadResult result;

    const std::filesystem::path root_candidate(root_path);
    if (!root_path.empty() && PathExists(root_candidate) &&
        LoadMagneticField(magnetic_field, root_path, rotation_deg)) {
        result.success = true;
        result.loaded_from_root = true;
        result.loaded_path = root_candidate.string();
        return result;
    }

    const std::filesystem::path table_candidate(table_path);
    if (!table_path.empty() && PathExists(table_candidate) &&
        LoadMagneticField(magnetic_field, table_path, rotation_deg)) {
        result.success = true;
        result.loaded_from_table = true;
        result.loaded_path = table_candidate.string();

        // [EN] Cache the slow table parse as ROOT only when the caller explicitly treats the ROOT file as disposable derived data. / [CN] 仅在调用方明确把ROOT文件视作可再生派生产物时，才把耗时的table解析缓存成ROOT。
        if (cache_root_copy && !root_path.empty()) {
            try {
                magnetic_field.SaveAsROOTFile(root_path);
            } catch (...) {
            }
        }
        return result;
    }

    return result;
}

RecoConfig BuildRecoConfig(const RuntimeOptions& options, bool have_magnetic_field) {
    RecoConfig config;
    config.p_min_mevc = options.p_min_mevc;
    config.p_max_mevc = options.p_max_mevc;
    config.initial_p_mevc = options.initial_p_mevc;
    config.tolerance_mm = options.tolerance_mm;
    config.max_iterations = (options.backend == RuntimeBackend::kNeuralNetwork) ? 1 : options.max_iterations;
    config.rk_step_mm = options.rk_step_mm;
    config.center_brho_tm = options.center_brho_tm;
    config.nn_model_json_path = options.nn_model_json_path;
    config.rk_fit_mode = options.rk_fit_mode;
    config.compute_uncertainty = options.compute_uncertainty;
    config.compute_posterior_laplace = options.compute_posterior_laplace;

    const bool allow_auto = options.backend == RuntimeBackend::kAuto;
    config.enable_nn =
        (allow_auto || options.backend == RuntimeBackend::kNeuralNetwork) &&
        !options.nn_model_json_path.empty();
    config.enable_rk =
        (allow_auto || options.backend == RuntimeBackend::kRungeKutta) &&
        have_magnetic_field;
    config.enable_multi_dim =
        (allow_auto || options.backend == RuntimeBackend::kMultiDim) &&
        have_magnetic_field;

    if (options.backend == RuntimeBackend::kLegacy) {
        config.enable_nn = false;
        config.enable_rk = false;
        config.enable_multi_dim = false;
    }
    return config;
}

TargetConstraint BuildTargetConstraint(const GeometryManager& geometry, const RuntimeOptions& options) {
    TargetConstraint target;
    target.target_position = geometry.GetTargetPosition();
    target.mass_mev = options.mass_mev;
    target.charge_e = options.charge_e;
    target.pdc_sigma_mm = options.pdc_sigma_mm;
    target.target_sigma_xy_mm = options.target_sigma_xy_mm;
    return target;
}

}  // namespace analysis::pdc::anaroot_like
