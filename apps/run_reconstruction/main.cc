#include "EventDataReader.hh"
#include "GeometryManager.hh"
#include "MagneticField.hh"
#include "NEBULAReconstructor.hh"
#include "PDCSimAna.hh"
#include "PDCMomentumReconstructor.hh"
#include "PDCRecoRuntime.hh"
#include "RecoEvent.hh"
#include "SMLogger.hh"
#include "TBeamSimData.hh"

#include "TFile.h"
#include "TNamed.h"
#include "TTree.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace reco = analysis::pdc::anaroot_like;

namespace {

#if defined(SMSIM_RECON_FORCE_BACKEND)
constexpr const char* kForcedBackend = SMSIM_RECON_FORCE_BACKEND;
#else
constexpr const char* kForcedBackend = nullptr;
#endif

#if defined(SMSIM_RECON_LOG_TAG)
constexpr const char* kLogTag = SMSIM_RECON_LOG_TAG;
#else
constexpr const char* kLogTag = "reconstruct_target_momentum";
#endif

#if defined(SMSIM_RECON_LEGACY_DEFAULTS)
constexpr bool kUseLegacyDefaults = true;
#else
constexpr bool kUseLegacyDefaults = false;
#endif

struct CliOptions {
    std::string input_file;
    std::string output_file;
    std::string input_dir;
    std::string output_dir;
    std::string geometry_macro;
    std::string nn_model_json;
    std::string magnetic_field_map;
    reco::RuntimeBackend backend = reco::RuntimeBackend::kAuto;
    int max_files = 0;
    int max_iterations = 40;
    double pdc_sigma_mm = 2.0;
    double target_sigma_xy_mm = 5.0;
    double p_min_mevc = 50.0;
    double p_max_mevc = 5000.0;
    double tolerance_mm = 5.0;
    double rk_step_mm = 5.0;
    double center_brho_tm = 7.2751;
    double magnet_rotation_deg = 30.0;
    reco::RkFitMode rk_fit_mode = reco::RkFitMode::kThreePointFree;
    bool rk_write_errors = true;
    bool rk_write_laplace = true;
};

struct FileStats {
    long long total_events = 0;
    long long processed_events = 0;
    long long pdc_hit_events = 0;
    long long nebula_hit_events = 0;
    long long proton_reco_events = 0;
    long long reco_proton_count = 0;
};

struct RunStats {
    long long files_total = 0;
    long long files_succeeded = 0;
    long long total_events = 0;
    long long processed_events = 0;
    long long pdc_hit_events = 0;
    long long nebula_hit_events = 0;
    long long proton_reco_events = 0;
    long long reco_proton_count = 0;
};

std::string GetEnv(const char* key) {
    const char* value = std::getenv(key);
    return value ? std::string(value) : std::string();
}

double ParseDouble(const std::string& text, const char* key) {
    try {
        return std::stod(text);
    } catch (...) {
        throw std::runtime_error(std::string("invalid numeric value for ") + key + ": " + text);
    }
}

int ParseInt(const std::string& text, const char* key) {
    try {
        return std::stoi(text);
    } catch (...) {
        throw std::runtime_error(std::string("invalid integer value for ") + key + ": " + text);
    }
}

bool ParseBoolOption(const std::string& text, const char* key) {
    const std::string lowered = reco::ToLowerCopy(text);
    if (lowered == "1" || lowered == "true" || lowered == "on" || lowered == "yes") {
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "off" || lowered == "no") {
        return false;
    }
    throw std::runtime_error(std::string("invalid boolean value for ") + key + ": " + text);
}

void PrintUsage(const char* argv0) {
    std::cout
        << "Usage(single-file): " << argv0
        << " --input-file FILE --output-file FILE --geometry-macro FILE [--backend auto|nn|rk|multidim]\n"
        << "                   [--nn-model-json FILE] [--magnetic-field-map FILE] [--magnet-rotation-deg DEG]\n"
        << "                   [--pdc-sigma-mm V] [--target-sigma-mm V] [--p-min-mevc V] [--p-max-mevc V]\n"
        << "                   [--rk-step-mm V] [--max-iterations N] [--tolerance-mm V]\n"
        << "                   [--center-brho-tm V] [--rk-fit-mode two-point-backprop|fixed-target-pdc-only|three-point-free]\n"
        << "                   [--rk-write-errors on|off] [--rk-write-laplace on|off]\n"
        << "\n"
        << "Usage(directory): " << argv0
        << " --input-dir DIR --output-dir DIR --geometry-macro FILE [--backend auto|nn|rk|multidim]\n"
        << "                   [--nn-model-json FILE] [--magnetic-field-map FILE] [--magnet-rotation-deg DEG]\n"
        << "                   [--max-files N] [--pdc-sigma-mm V] [--target-sigma-mm V] [--p-min-mevc V] [--p-max-mevc V]\n"
        << "                   [--rk-write-errors on|off] [--rk-write-laplace on|off]\n";
}

CliOptions ParseArgs(int argc, char* argv[]) {
    CliOptions opts;
    if (kForcedBackend) {
        opts.backend = reco::ParseRuntimeBackend(kForcedBackend);
    }

    if (kUseLegacyDefaults) {
        const std::string smsimdir = GetEnv("SMSIMDIR");
        if (!smsimdir.empty()) {
            opts.input_dir = smsimdir + "/data/simulation/g4output/sn124_nn_B115T";
            opts.output_dir = smsimdir + "/data/reconstruction/sn124_nn_B115T";
            opts.geometry_macro =
                smsimdir + "/build/bin/configs/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227.mac";
        }
    }
    opts.nn_model_json = GetEnv("PDC_NN_MODEL_JSON");

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--input-file" && i + 1 < argc) {
            opts.input_file = argv[++i];
        } else if (arg == "--output-file" && i + 1 < argc) {
            opts.output_file = argv[++i];
        } else if (arg == "--input-dir" && i + 1 < argc) {
            opts.input_dir = argv[++i];
        } else if (arg == "--output-dir" && i + 1 < argc) {
            opts.output_dir = argv[++i];
        } else if (arg == "--geometry-macro" && i + 1 < argc) {
            opts.geometry_macro = argv[++i];
        } else if (arg == "--nn-model-json" && i + 1 < argc) {
            opts.nn_model_json = argv[++i];
        } else if (arg == "--magnetic-field-map" && i + 1 < argc) {
            opts.magnetic_field_map = argv[++i];
        } else if (arg == "--backend" && i + 1 < argc) {
            const reco::RuntimeBackend parsed = reco::ParseRuntimeBackend(argv[++i]);
            if (kForcedBackend && parsed != opts.backend) {
                throw std::runtime_error(
                    "this compatibility wrapper only supports --backend " + reco::RuntimeBackendName(opts.backend)
                );
            }
            opts.backend = parsed;
        } else if (arg == "--max-files" && i + 1 < argc) {
            opts.max_files = ParseInt(argv[++i], "--max-files");
        } else if (arg == "--max-iterations" && i + 1 < argc) {
            opts.max_iterations = ParseInt(argv[++i], "--max-iterations");
        } else if (arg == "--pdc-sigma-mm" && i + 1 < argc) {
            opts.pdc_sigma_mm = ParseDouble(argv[++i], "--pdc-sigma-mm");
        } else if (arg == "--target-sigma-mm" && i + 1 < argc) {
            opts.target_sigma_xy_mm = ParseDouble(argv[++i], "--target-sigma-mm");
        } else if (arg == "--p-min-mevc" && i + 1 < argc) {
            opts.p_min_mevc = ParseDouble(argv[++i], "--p-min-mevc");
        } else if (arg == "--p-max-mevc" && i + 1 < argc) {
            opts.p_max_mevc = ParseDouble(argv[++i], "--p-max-mevc");
        } else if (arg == "--tolerance-mm" && i + 1 < argc) {
            opts.tolerance_mm = ParseDouble(argv[++i], "--tolerance-mm");
        } else if (arg == "--rk-step-mm" && i + 1 < argc) {
            opts.rk_step_mm = ParseDouble(argv[++i], "--rk-step-mm");
        } else if (arg == "--center-brho-tm" && i + 1 < argc) {
            opts.center_brho_tm = ParseDouble(argv[++i], "--center-brho-tm");
        } else if (arg == "--magnet-rotation-deg" && i + 1 < argc) {
            opts.magnet_rotation_deg = ParseDouble(argv[++i], "--magnet-rotation-deg");
        } else if (arg == "--rk-fit-mode" && i + 1 < argc) {
            opts.rk_fit_mode = reco::ParseRkFitMode(argv[++i]);
        } else if (arg == "--rk-write-errors" && i + 1 < argc) {
            opts.rk_write_errors = ParseBoolOption(argv[++i], "--rk-write-errors");
        } else if (arg == "--rk-write-laplace" && i + 1 < argc) {
            opts.rk_write_laplace = ParseBoolOption(argv[++i], "--rk-write-laplace");
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            std::exit(0);
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }

    const bool single_file_mode = !opts.input_file.empty() || !opts.output_file.empty();
    if (single_file_mode) {
        if (opts.input_file.empty() || opts.output_file.empty()) {
            throw std::runtime_error("single-file mode requires both --input-file and --output-file");
        }
    } else if (opts.input_dir.empty() || opts.output_dir.empty()) {
        throw std::runtime_error("directory mode requires --input-dir and --output-dir");
    }

    if (opts.geometry_macro.empty()) {
        throw std::runtime_error("missing required argument --geometry-macro");
    }
    if (reco::RuntimeBackendNeedsNnModel(opts.backend) && opts.nn_model_json.empty()) {
        throw std::runtime_error("backend nn requires --nn-model-json or PDC_NN_MODEL_JSON");
    }
    if (reco::RuntimeBackendNeedsFieldMap(opts.backend) && opts.magnetic_field_map.empty()) {
        throw std::runtime_error("selected backend requires --magnetic-field-map");
    }
    if (opts.max_files < 0) {
        throw std::runtime_error("--max-files must be >= 0");
    }
    if (opts.max_iterations <= 0) {
        throw std::runtime_error("--max-iterations must be > 0");
    }
    if (opts.pdc_sigma_mm <= 0.0 || opts.target_sigma_xy_mm <= 0.0) {
        throw std::runtime_error("sigma parameters must be positive");
    }
    if (opts.p_min_mevc <= 0.0 || opts.p_max_mevc <= opts.p_min_mevc) {
        throw std::runtime_error("invalid momentum range");
    }
    if (opts.rk_step_mm <= 0.0) {
        throw std::runtime_error("--rk-step-mm must be > 0");
    }
    if (opts.tolerance_mm <= 0.0) {
        throw std::runtime_error("--tolerance-mm must be > 0");
    }
    return opts;
}

std::vector<fs::path> CollectInputFiles(const fs::path& input_dir, int max_files) {
    std::vector<fs::path> files;
    if (!fs::exists(input_dir)) {
        return files;
    }
    for (const auto& entry : fs::recursive_directory_iterator(input_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const fs::path path = entry.path();
        if (path.extension() != ".root") {
            continue;
        }
        const std::string name = path.filename().string();
        if (name.find("_reco.root") != std::string::npos) {
            continue;
        }
        files.push_back(path);
    }
    std::sort(files.begin(), files.end());
    if (max_files > 0 && static_cast<int>(files.size()) > max_files) {
        files.resize(static_cast<std::size_t>(max_files));
    }
    return files;
}

fs::path BuildOutputPath(const fs::path& input_file, const fs::path& input_dir, const fs::path& output_dir) {
    std::error_code ec;
    const fs::path rel = fs::relative(input_file, input_dir, ec);
    if (ec) {
        return output_dir / (input_file.stem().string() + "_reco.root");
    }
    fs::path out = output_dir / rel;
    out.replace_filename(out.stem().string() + "_reco.root");
    return out;
}

bool ProcessSingleFile(const fs::path& input_file,
                       const fs::path& output_file,
                       const std::string& log_tag,
                       const std::string& backend_name,
                       bool write_rk_errors,
                       bool write_rk_laplace,
                       PDCSimAna& pdc_ana,
                       NEBULAReconstructor& nebula_reco,
                       reco::PDCMomentumReconstructor& proton_reco,
                       const reco::RecoConfig& proton_config,
                       const reco::TargetConstraint& target_constraint,
                       FileStats* stats) {
    if (!stats) {
        return false;
    }

    EventDataReader reader(input_file.c_str());
    if (!reader.IsOpen()) {
        std::cerr << "[" << log_tag << "] failed to open input file: " << input_file << std::endl;
        return false;
    }

    fs::create_directories(output_file.parent_path());
    TFile out(output_file.c_str(), "RECREATE");
    if (out.IsZombie()) {
        std::cerr << "[" << log_tag << "] failed to create output file: " << output_file << std::endl;
        return false;
    }

    TTree reco_tree("recoTree", "Reconstruction tree with proton and NEBULA neutron");

    RecoEvent reco_event;
    RecoEvent* reco_event_ptr = &reco_event;

    bool truth_has_proton = false;
    bool truth_has_neutron = false;
    TLorentzVector truth_proton_p4(0.0, 0.0, 0.0, 0.0);
    TLorentzVector truth_neutron_p4(0.0, 0.0, 0.0, 0.0);
    TVector3 truth_proton_pos(0.0, 0.0, 0.0);
    TVector3 truth_neutron_pos(0.0, 0.0, 0.0);

    std::vector<double> reco_proton_px;
    std::vector<double> reco_proton_py;
    std::vector<double> reco_proton_pz;
    std::vector<double> reco_proton_e;
    std::vector<double> reco_proton_p;
    std::vector<int> reco_proton_status;
    std::vector<int> reco_proton_method;
    std::vector<int> reco_proton_ndf;
    std::vector<int> reco_proton_iterations;
    std::vector<int> reco_proton_uncertainty_valid;
    std::vector<int> reco_proton_posterior_valid;
    std::vector<double> reco_proton_chi2_raw;
    std::vector<double> reco_proton_chi2_reduced;
    std::vector<double> reco_proton_px_sigma;
    std::vector<double> reco_proton_py_sigma;
    std::vector<double> reco_proton_pz_sigma;
    std::vector<double> reco_proton_p_sigma;
    std::vector<double> reco_proton_px_lower68;
    std::vector<double> reco_proton_px_upper68;
    std::vector<double> reco_proton_px_lower95;
    std::vector<double> reco_proton_px_upper95;
    std::vector<double> reco_proton_py_lower68;
    std::vector<double> reco_proton_py_upper68;
    std::vector<double> reco_proton_py_lower95;
    std::vector<double> reco_proton_py_upper95;
    std::vector<double> reco_proton_pz_lower68;
    std::vector<double> reco_proton_pz_upper68;
    std::vector<double> reco_proton_pz_lower95;
    std::vector<double> reco_proton_pz_upper95;
    std::vector<double> reco_proton_p_lower68;
    std::vector<double> reco_proton_p_upper68;
    std::vector<double> reco_proton_p_lower95;
    std::vector<double> reco_proton_p_upper95;

    reco_tree.Branch("recoEvent", &reco_event_ptr);
    reco_tree.Branch("truth_has_proton", &truth_has_proton);
    reco_tree.Branch("truth_has_neutron", &truth_has_neutron);
    reco_tree.Branch("truth_proton_p4", &truth_proton_p4);
    reco_tree.Branch("truth_neutron_p4", &truth_neutron_p4);
    reco_tree.Branch("truth_proton_pos", &truth_proton_pos);
    reco_tree.Branch("truth_neutron_pos", &truth_neutron_pos);
    reco_tree.Branch("reco_proton_px", &reco_proton_px);
    reco_tree.Branch("reco_proton_py", &reco_proton_py);
    reco_tree.Branch("reco_proton_pz", &reco_proton_pz);
    reco_tree.Branch("reco_proton_e", &reco_proton_e);
    if (write_rk_errors) {
        reco_tree.Branch("reco_proton_p", &reco_proton_p);
        reco_tree.Branch("reco_proton_status", &reco_proton_status);
        reco_tree.Branch("reco_proton_method", &reco_proton_method);
        reco_tree.Branch("reco_proton_ndf", &reco_proton_ndf);
        reco_tree.Branch("reco_proton_iterations", &reco_proton_iterations);
        reco_tree.Branch("reco_proton_uncertainty_valid", &reco_proton_uncertainty_valid);
        reco_tree.Branch("reco_proton_posterior_valid", &reco_proton_posterior_valid);
        reco_tree.Branch("reco_proton_chi2_raw", &reco_proton_chi2_raw);
        reco_tree.Branch("reco_proton_chi2_reduced", &reco_proton_chi2_reduced);
        reco_tree.Branch("reco_proton_px_sigma", &reco_proton_px_sigma);
        reco_tree.Branch("reco_proton_py_sigma", &reco_proton_py_sigma);
        reco_tree.Branch("reco_proton_pz_sigma", &reco_proton_pz_sigma);
        reco_tree.Branch("reco_proton_p_sigma", &reco_proton_p_sigma);
        if (write_rk_laplace) {
            reco_tree.Branch("reco_proton_px_lower68", &reco_proton_px_lower68);
            reco_tree.Branch("reco_proton_px_upper68", &reco_proton_px_upper68);
            reco_tree.Branch("reco_proton_px_lower95", &reco_proton_px_lower95);
            reco_tree.Branch("reco_proton_px_upper95", &reco_proton_px_upper95);
            reco_tree.Branch("reco_proton_py_lower68", &reco_proton_py_lower68);
            reco_tree.Branch("reco_proton_py_upper68", &reco_proton_py_upper68);
            reco_tree.Branch("reco_proton_py_lower95", &reco_proton_py_lower95);
            reco_tree.Branch("reco_proton_py_upper95", &reco_proton_py_upper95);
            reco_tree.Branch("reco_proton_pz_lower68", &reco_proton_pz_lower68);
            reco_tree.Branch("reco_proton_pz_upper68", &reco_proton_pz_upper68);
            reco_tree.Branch("reco_proton_pz_lower95", &reco_proton_pz_lower95);
            reco_tree.Branch("reco_proton_pz_upper95", &reco_proton_pz_upper95);
            reco_tree.Branch("reco_proton_p_lower68", &reco_proton_p_lower68);
            reco_tree.Branch("reco_proton_p_upper68", &reco_proton_p_upper68);
            reco_tree.Branch("reco_proton_p_lower95", &reco_proton_p_lower95);
            reco_tree.Branch("reco_proton_p_upper95", &reco_proton_p_upper95);
        }
    }

    stats->total_events = reader.GetTotalEvents();

    for (Long64_t i = 0; i < stats->total_events; ++i) {
        if (!reader.GoToEvent(i)) {
            continue;
        }
        ++stats->processed_events;

        reco_event.Clear();
        reco_proton_px.clear();
        reco_proton_py.clear();
        reco_proton_pz.clear();
        reco_proton_e.clear();
        reco_proton_p.clear();
        reco_proton_status.clear();
        reco_proton_method.clear();
        reco_proton_ndf.clear();
        reco_proton_iterations.clear();
        reco_proton_uncertainty_valid.clear();
        reco_proton_posterior_valid.clear();
        reco_proton_chi2_raw.clear();
        reco_proton_chi2_reduced.clear();
        reco_proton_px_sigma.clear();
        reco_proton_py_sigma.clear();
        reco_proton_pz_sigma.clear();
        reco_proton_p_sigma.clear();
        reco_proton_px_lower68.clear();
        reco_proton_px_upper68.clear();
        reco_proton_px_lower95.clear();
        reco_proton_px_upper95.clear();
        reco_proton_py_lower68.clear();
        reco_proton_py_upper68.clear();
        reco_proton_py_lower95.clear();
        reco_proton_py_upper95.clear();
        reco_proton_pz_lower68.clear();
        reco_proton_pz_upper68.clear();
        reco_proton_pz_lower95.clear();
        reco_proton_pz_upper95.clear();
        reco_proton_p_lower68.clear();
        reco_proton_p_upper68.clear();
        reco_proton_p_lower95.clear();
        reco_proton_p_upper95.clear();
        truth_has_proton = false;
        truth_has_neutron = false;
        truth_proton_p4.SetPxPyPzE(0.0, 0.0, 0.0, 0.0);
        truth_neutron_p4.SetPxPyPzE(0.0, 0.0, 0.0, 0.0);
        truth_proton_pos.SetXYZ(0.0, 0.0, 0.0);
        truth_neutron_pos.SetXYZ(0.0, 0.0, 0.0);

        TClonesArray* hits = reader.GetHits();
        if (hits && hits->GetEntries() > 0) {
            pdc_ana.ProcessEvent(hits, reco_event);
            ++stats->pdc_hit_events;
        }

        TClonesArray* nebula_hits = reader.GetNEBULAHits();
        if (nebula_hits && nebula_hits->GetEntries() > 0) {
            nebula_reco.ProcessEvent(nebula_hits, reco_event);
            ++stats->nebula_hit_events;
        }

        bool event_has_reco_proton = false;
        for (const auto& track : reco_event.tracks) {
            if (track.pdgCode == 2112) {
                // [EN] Keep neutron reconstruction unchanged while restricting momentum inversion to charged proton tracks. / [CN] 保持中子重建链不变，同时只对带电质子轨迹执行动量反演。
                continue;
            }
            reco::PDCInputTrack pdc_track;
            pdc_track.pdc1 = track.start;
            pdc_track.pdc2 = track.end;

            const reco::RecoResult reco_result = proton_reco.Reconstruct(pdc_track, target_constraint, proton_config);
            if ((reco_result.status == reco::SolverStatus::kSuccess ||
                 reco_result.status == reco::SolverStatus::kNotConverged) &&
                reco_result.p4_at_target.P() > 0.0) {
                const double nan = std::numeric_limits<double>::quiet_NaN();
                auto append_interval = [](const reco::IntervalEstimate& interval,
                                          std::vector<double>* lower68,
                                          std::vector<double>* upper68,
                                          std::vector<double>* lower95,
                                          std::vector<double>* upper95) {
                    lower68->push_back(interval.valid ? interval.lower68
                                                      : std::numeric_limits<double>::quiet_NaN());
                    upper68->push_back(interval.valid ? interval.upper68
                                                      : std::numeric_limits<double>::quiet_NaN());
                    lower95->push_back(interval.valid ? interval.lower95
                                                      : std::numeric_limits<double>::quiet_NaN());
                    upper95->push_back(interval.valid ? interval.upper95
                                                      : std::numeric_limits<double>::quiet_NaN());
                };
                reco_proton_px.push_back(reco_result.p4_at_target.Px());
                reco_proton_py.push_back(reco_result.p4_at_target.Py());
                reco_proton_pz.push_back(reco_result.p4_at_target.Pz());
                reco_proton_e.push_back(reco_result.p4_at_target.E());
                if (write_rk_errors) {
                    reco_proton_p.push_back(reco_result.p4_at_target.P());
                    reco_proton_status.push_back(static_cast<int>(reco_result.status));
                    reco_proton_method.push_back(static_cast<int>(reco_result.method_used));
                    reco_proton_ndf.push_back(reco_result.ndf);
                    reco_proton_iterations.push_back(reco_result.iterations);
                    reco_proton_uncertainty_valid.push_back(reco_result.uncertainty_valid ? 1 : 0);
                    reco_proton_posterior_valid.push_back(reco_result.posterior_valid ? 1 : 0);
                    reco_proton_chi2_raw.push_back(reco_result.chi2_raw);
                    reco_proton_chi2_reduced.push_back(reco_result.chi2_reduced);
                    reco_proton_px_sigma.push_back(reco_result.px_interval.valid ? reco_result.px_interval.sigma : nan);
                    reco_proton_py_sigma.push_back(reco_result.py_interval.valid ? reco_result.py_interval.sigma : nan);
                    reco_proton_pz_sigma.push_back(reco_result.pz_interval.valid ? reco_result.pz_interval.sigma : nan);
                    reco_proton_p_sigma.push_back(reco_result.p_interval.valid ? reco_result.p_interval.sigma : nan);
                    if (write_rk_laplace) {
                        append_interval(reco_result.px_credible,
                                        &reco_proton_px_lower68,
                                        &reco_proton_px_upper68,
                                        &reco_proton_px_lower95,
                                        &reco_proton_px_upper95);
                        append_interval(reco_result.py_credible,
                                        &reco_proton_py_lower68,
                                        &reco_proton_py_upper68,
                                        &reco_proton_py_lower95,
                                        &reco_proton_py_upper95);
                        append_interval(reco_result.pz_credible,
                                        &reco_proton_pz_lower68,
                                        &reco_proton_pz_upper68,
                                        &reco_proton_pz_lower95,
                                        &reco_proton_pz_upper95);
                        append_interval(reco_result.p_credible,
                                        &reco_proton_p_lower68,
                                        &reco_proton_p_upper68,
                                        &reco_proton_p_lower95,
                                        &reco_proton_p_upper95);
                    }
                }
                ++stats->reco_proton_count;
                event_has_reco_proton = true;
            }
        }
        if (event_has_reco_proton) {
            ++stats->proton_reco_events;
        }

        const std::vector<TBeamSimData>* beam_data = reader.GetBeamData();
        if (beam_data) {
            for (const auto& particle : *beam_data) {
                if (!truth_has_proton && (particle.fParticleName == "proton" || (particle.fZ == 1 && particle.fA == 1))) {
                    truth_has_proton = true;
                    truth_proton_p4 = particle.fMomentum;
                    truth_proton_pos = particle.fPosition;
                } else if (!truth_has_neutron &&
                           (particle.fParticleName == "neutron" || (particle.fZ == 0 && particle.fA == 1))) {
                    truth_has_neutron = true;
                    truth_neutron_p4 = particle.fMomentum;
                    truth_neutron_pos = particle.fPosition;
                }
            }
        }

        reco_event.eventID = static_cast<int>(i);
        reco_tree.Fill();
    }

    const std::string processed_events_text = std::to_string(stats->processed_events);
    TNamed info_input("InputFile", input_file.c_str());
    TNamed info_backend("ProtonRecoBackend", backend_name.c_str());
    TNamed info_events("ProcessedEvents", processed_events_text.c_str());
    TNamed info_rk_errors("RkErrorBranches", write_rk_errors ? "true" : "false");
    TNamed info_rk_laplace("RkLaplaceBranches",
                           (write_rk_errors && write_rk_laplace) ? "true" : "false");
    info_input.Write();
    info_backend.Write();
    info_events.Write();
    info_rk_errors.Write();
    info_rk_laplace.Write();

    out.cd();
    reco_tree.Write();
    out.Close();
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        SMLogger::LogConfig log_config;
        log_config.async = false;
        log_config.console = true;
        log_config.file = false;
        log_config.level = SMLogger::LogLevel::WARN;
        SMLogger::Logger::Instance().Initialize(log_config);

        const CliOptions opts = ParseArgs(argc, argv);
        const bool single_file_mode = !opts.input_file.empty();
        const fs::path input_dir(opts.input_dir);
        const fs::path output_dir(opts.output_dir);
        const fs::path input_file_single(opts.input_file);
        const fs::path output_file_single(opts.output_file);
        const fs::path geometry_macro(opts.geometry_macro);
        const fs::path nn_model_json(opts.nn_model_json);
        const fs::path magnetic_field_map(opts.magnetic_field_map);
        const std::string backend_name = reco::RuntimeBackendName(opts.backend);

        if (single_file_mode) {
            if (!fs::exists(input_file_single)) {
                throw std::runtime_error("input-file does not exist: " + input_file_single.string());
            }
        } else if (!fs::exists(input_dir)) {
            throw std::runtime_error("input-dir does not exist: " + input_dir.string());
        }

        if (!fs::exists(geometry_macro)) {
            throw std::runtime_error("geometry-macro does not exist: " + geometry_macro.string());
        }
        if (!opts.nn_model_json.empty() && !fs::exists(nn_model_json)) {
            throw std::runtime_error("nn-model-json does not exist: " + nn_model_json.string());
        }
        if (!opts.magnetic_field_map.empty() && !fs::exists(magnetic_field_map)) {
            throw std::runtime_error("magnetic-field-map does not exist: " + magnetic_field_map.string());
        }
        if (single_file_mode) {
            fs::create_directories(output_file_single.parent_path());
        } else {
            fs::create_directories(output_dir);
        }

        GeometryManager geometry;
        if (!reco::LoadGeometryFromMacro(geometry, geometry_macro.string())) {
            throw std::runtime_error("failed to load geometry macro: " + geometry_macro.string());
        }

        std::unique_ptr<MagneticField> magnetic_field;
        if (!opts.magnetic_field_map.empty()) {
            magnetic_field = std::make_unique<MagneticField>();
            if (!reco::LoadMagneticField(*magnetic_field, magnetic_field_map.string(), opts.magnet_rotation_deg)) {
                throw std::runtime_error("failed to load magnetic field map: " + magnetic_field_map.string());
            }
        }

        PDCSimAna pdc_ana(geometry);
        pdc_ana.SetSmearing(0.5, 0.5);

        NEBULAReconstructor nebula_reco(geometry);
        nebula_reco.SetTargetPosition(geometry.GetTargetPosition());
        nebula_reco.SetTimeWindow(10.0);
        nebula_reco.SetEnergyThreshold(1.0);

        reco::PDCMomentumReconstructor proton_reco(magnetic_field.get());
        reco::RuntimeOptions runtime_options;
        runtime_options.backend = opts.backend;
        runtime_options.pdc_sigma_mm = opts.pdc_sigma_mm;
        runtime_options.target_sigma_xy_mm = opts.target_sigma_xy_mm;
        runtime_options.p_min_mevc = opts.p_min_mevc;
        runtime_options.p_max_mevc = opts.p_max_mevc;
        runtime_options.tolerance_mm = opts.tolerance_mm;
        runtime_options.max_iterations = opts.max_iterations;
        runtime_options.rk_step_mm = opts.rk_step_mm;
        runtime_options.center_brho_tm = opts.center_brho_tm;
        runtime_options.nn_model_json_path = opts.nn_model_json;
        runtime_options.rk_fit_mode = opts.rk_fit_mode;
        runtime_options.compute_uncertainty = opts.rk_write_errors;
        runtime_options.compute_posterior_laplace = opts.rk_write_errors && opts.rk_write_laplace;

        const reco::RecoConfig proton_config = reco::BuildRecoConfig(runtime_options, magnetic_field != nullptr);
        const reco::TargetConstraint target_constraint = reco::BuildTargetConstraint(geometry, runtime_options);

        std::vector<fs::path> files;
        if (single_file_mode) {
            files.push_back(input_file_single);
        } else {
            files = CollectInputFiles(input_dir, opts.max_files);
            if (files.empty()) {
                throw std::runtime_error("no input root files found under " + input_dir.string());
            }
        }

        RunStats run_stats;
        run_stats.files_total = static_cast<long long>(files.size());

        for (const auto& input_file : files) {
            const fs::path output_file = single_file_mode
                ? output_file_single
                : BuildOutputPath(input_file, input_dir, output_dir);
            std::cout << "[" << kLogTag << "] input:  " << input_file << std::endl;
            std::cout << "[" << kLogTag << "] output: " << output_file << std::endl;
            std::cout << "[" << kLogTag << "] backend: " << backend_name << std::endl;

            FileStats file_stats;
            const bool ok = ProcessSingleFile(input_file,
                                              output_file,
                                              kLogTag,
                                              backend_name,
                                              opts.rk_write_errors,
                                              opts.rk_write_laplace,
                                              pdc_ana,
                                              nebula_reco,
                                              proton_reco,
                                              proton_config,
                                              target_constraint,
                                              &file_stats);
            if (!ok) {
                continue;
            }
            ++run_stats.files_succeeded;
            run_stats.total_events += file_stats.total_events;
            run_stats.processed_events += file_stats.processed_events;
            run_stats.pdc_hit_events += file_stats.pdc_hit_events;
            run_stats.nebula_hit_events += file_stats.nebula_hit_events;
            run_stats.proton_reco_events += file_stats.proton_reco_events;
            run_stats.reco_proton_count += file_stats.reco_proton_count;
        }

        const double denom = run_stats.processed_events > 0 ? static_cast<double>(run_stats.processed_events) : 1.0;
        std::cout << "[" << kLogTag << "] files: " << run_stats.files_succeeded << "/" << run_stats.files_total << std::endl;
        std::cout << "[" << kLogTag << "] events processed: " << run_stats.processed_events << "/" << run_stats.total_events << std::endl;
        std::cout << "[" << kLogTag << "] pdc-hit ratio: " << (run_stats.pdc_hit_events * 100.0 / denom) << "%" << std::endl;
        std::cout << "[" << kLogTag << "] nebula-hit ratio: " << (run_stats.nebula_hit_events * 100.0 / denom) << "%" << std::endl;
        std::cout << "[" << kLogTag << "] proton-reco ratio: " << (run_stats.proton_reco_events * 100.0 / denom) << "%" << std::endl;
        std::cout << "[" << kLogTag << "] reco proton count: " << run_stats.reco_proton_count << std::endl;
        SMLogger::Logger::Instance().Shutdown();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[" << kLogTag << "] failed: " << ex.what() << std::endl;
        SMLogger::Logger::Instance().Shutdown();
        return 1;
    }
}
