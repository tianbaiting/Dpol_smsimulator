#include "EventDataReader.hh"
#include "GeometryManager.hh"
#include "MagneticField.hh"
#include "NEBULAReconstructor.hh"
#include "PDCSimAna.hh"
#include "PDCMomentumReconstructor.hh"
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
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct CliOptions {
    std::string input_file;
    std::string output_file;
    std::string input_dir;
    std::string output_dir;
    std::string geometry_macro;
    std::string nn_model_json;
    int max_files = 0;
    double pdc_sigma_mm = 2.0;
    double target_sigma_xy_mm = 5.0;
    double p_min_mevc = 50.0;
    double p_max_mevc = 5000.0;
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

void PrintUsage(const char* argv0) {
    std::cout
        << "Usage(single-file): " << argv0
        << " --input-file FILE --output-file FILE --geometry-macro FILE --nn-model-json FILE\n"
        << "                   [--pdc-sigma-mm V] [--target-sigma-mm V] [--p-min-mevc V] [--p-max-mevc V]\n"
        << "\n"
        << "Usage: " << argv0 << " --input-dir DIR --output-dir DIR --geometry-macro FILE --nn-model-json FILE\n"
        << "       [--max-files N] [--pdc-sigma-mm V] [--target-sigma-mm V] [--p-min-mevc V] [--p-max-mevc V]\n";
}

std::string GetEnv(const char* key) {
    const char* value = std::getenv(key);
    if (!value) {
        return std::string();
    }
    return std::string(value);
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

CliOptions ParseArgs(int argc, char* argv[]) {
    CliOptions opts;
    const std::string smsimdir = GetEnv("SMSIMDIR");
    if (!smsimdir.empty()) {
        opts.input_dir = smsimdir + "/data/simulation/g4output/sn124_nn_B115T";
        opts.output_dir = smsimdir + "/data/reconstruction/sn124_nn_B115T";
        opts.geometry_macro =
            smsimdir + "/build/bin/configs/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227.mac";
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
        } else if (arg == "--max-files" && i + 1 < argc) {
            opts.max_files = ParseInt(argv[++i], "--max-files");
        } else if (arg == "--pdc-sigma-mm" && i + 1 < argc) {
            opts.pdc_sigma_mm = ParseDouble(argv[++i], "--pdc-sigma-mm");
        } else if (arg == "--target-sigma-mm" && i + 1 < argc) {
            opts.target_sigma_xy_mm = ParseDouble(argv[++i], "--target-sigma-mm");
        } else if (arg == "--p-min-mevc" && i + 1 < argc) {
            opts.p_min_mevc = ParseDouble(argv[++i], "--p-min-mevc");
        } else if (arg == "--p-max-mevc" && i + 1 < argc) {
            opts.p_max_mevc = ParseDouble(argv[++i], "--p-max-mevc");
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
    } else {
        if (opts.input_dir.empty() || opts.output_dir.empty()) {
            throw std::runtime_error("directory mode requires --input-dir and --output-dir");
        }
    }
    if (opts.geometry_macro.empty() || opts.nn_model_json.empty()) {
        throw std::runtime_error("missing required arguments; use --help");
    }
    if (opts.max_files < 0) {
        throw std::runtime_error("--max-files must be >= 0");
    }
    if (opts.pdc_sigma_mm <= 0.0 || opts.target_sigma_xy_mm <= 0.0) {
        throw std::runtime_error("sigma parameters must be positive");
    }
    if (opts.p_min_mevc <= 0.0 || opts.p_max_mevc <= opts.p_min_mevc) {
        throw std::runtime_error("invalid momentum range");
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
    const std::string stem = out.stem().string();
    out.replace_filename(stem + "_reco.root");
    return out;
}

bool ProcessSingleFile(
    const fs::path& input_file,
    const fs::path& output_file,
    PDCSimAna& pdc_ana,
    NEBULAReconstructor& nebula_reco,
    analysis::pdc::anaroot_like::PDCMomentumReconstructor& proton_nn_reco,
    const analysis::pdc::anaroot_like::RecoConfig& proton_config,
    const analysis::pdc::anaroot_like::TargetConstraint& target_constraint,
    FileStats* stats
) {
    if (!stats) {
        return false;
    }

    EventDataReader reader(input_file.c_str());
    if (!reader.IsOpen()) {
        std::cerr << "[reconstruct_sn_nn] failed to open input file: " << input_file << std::endl;
        return false;
    }

    fs::create_directories(output_file.parent_path());
    TFile out(output_file.c_str(), "RECREATE");
    if (out.IsZombie()) {
        std::cerr << "[reconstruct_sn_nn] failed to create output file: " << output_file << std::endl;
        return false;
    }

    TTree reco_tree("recoTree", "Reconstruction tree with NN proton and NEBULA neutron");

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
                // [EN] Keep neutron reconstruction path unchanged but exclude neutron pseudo-tracks from proton NN solve. / [CN] 保持中子重建链路不变，但在质子NN求解时排除中子伪轨迹。
                continue;
            }
            analysis::pdc::anaroot_like::PDCInputTrack pdc_track;
            pdc_track.pdc1 = track.start;
            pdc_track.pdc2 = track.end;

            const auto reco = proton_nn_reco.Reconstruct(pdc_track, target_constraint, proton_config);
            if ((reco.status == analysis::pdc::anaroot_like::SolverStatus::kSuccess ||
                 reco.status == analysis::pdc::anaroot_like::SolverStatus::kNotConverged) &&
                reco.p4_at_target.P() > 0.0) {
                reco_proton_px.push_back(reco.p4_at_target.Px());
                reco_proton_py.push_back(reco.p4_at_target.Py());
                reco_proton_pz.push_back(reco.p4_at_target.Pz());
                reco_proton_e.push_back(reco.p4_at_target.E());
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

    TNamed info_input("InputFile", input_file.c_str());
    TNamed info_backend("ProtonRecoBackend", "nn");
    TNamed info_events("ProcessedEvents", std::to_string(stats->processed_events).c_str());
    info_input.Write();
    info_backend.Write();
    info_events.Write();

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

        if (single_file_mode) {
            if (!fs::exists(input_file_single)) {
                throw std::runtime_error("input-file does not exist: " + input_file_single.string());
            }
        } else {
            if (!fs::exists(input_dir)) {
                throw std::runtime_error("input-dir does not exist: " + input_dir.string());
            }
        }
        if (!fs::exists(geometry_macro)) {
            throw std::runtime_error("geometry-macro does not exist: " + geometry_macro.string());
        }
        if (!fs::exists(nn_model_json)) {
            throw std::runtime_error("nn-model-json does not exist: " + nn_model_json.string());
        }
        if (single_file_mode) {
            fs::create_directories(output_file_single.parent_path());
        } else {
            fs::create_directories(output_dir);
        }

        GeometryManager geometry;
        if (!geometry.LoadGeometry(geometry_macro.c_str())) {
            throw std::runtime_error("failed to load geometry macro: " + geometry_macro.string());
        }

        PDCSimAna pdc_ana(geometry);
        pdc_ana.SetSmearing(0.5, 0.5);

        NEBULAReconstructor nebula_reco(geometry);
        nebula_reco.SetTargetPosition(geometry.GetTargetPosition());
        nebula_reco.SetTimeWindow(10.0);
        nebula_reco.SetEnergyThreshold(1.0);

        analysis::pdc::anaroot_like::PDCMomentumReconstructor proton_nn_reco(nullptr);

        analysis::pdc::anaroot_like::RecoConfig proton_config;
        proton_config.enable_nn = true;
        proton_config.enable_rk = false;
        proton_config.enable_multi_dim = false;
        proton_config.enable_matrix = false;
        proton_config.nn_model_json_path = nn_model_json.string();
        proton_config.p_min_mevc = opts.p_min_mevc;
        proton_config.p_max_mevc = opts.p_max_mevc;
        proton_config.tolerance_mm = 5.0;
        proton_config.max_iterations = 1;

        analysis::pdc::anaroot_like::TargetConstraint target_constraint;
        target_constraint.target_position = geometry.GetTargetPosition();
        target_constraint.mass_mev = 938.2720813;
        target_constraint.charge_e = 1.0;
        target_constraint.pdc_sigma_mm = opts.pdc_sigma_mm;
        target_constraint.target_sigma_xy_mm = opts.target_sigma_xy_mm;

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
            std::cout << "[reconstruct_sn_nn] input:  " << input_file << std::endl;
            std::cout << "[reconstruct_sn_nn] output: " << output_file << std::endl;

            FileStats file_stats;
            const bool ok = ProcessSingleFile(
                input_file,
                output_file,
                pdc_ana,
                nebula_reco,
                proton_nn_reco,
                proton_config,
                target_constraint,
                &file_stats
            );
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
        std::cout << "[reconstruct_sn_nn] files: " << run_stats.files_succeeded << "/" << run_stats.files_total << std::endl;
        std::cout << "[reconstruct_sn_nn] events processed: " << run_stats.processed_events << "/" << run_stats.total_events << std::endl;
        std::cout << "[reconstruct_sn_nn] pdc-hit ratio: " << (run_stats.pdc_hit_events * 100.0 / denom) << "%" << std::endl;
        std::cout << "[reconstruct_sn_nn] nebula-hit ratio: " << (run_stats.nebula_hit_events * 100.0 / denom) << "%" << std::endl;
        std::cout << "[reconstruct_sn_nn] proton-reco ratio: " << (run_stats.proton_reco_events * 100.0 / denom) << "%" << std::endl;
        std::cout << "[reconstruct_sn_nn] reco proton count: " << run_stats.reco_proton_count << std::endl;
        SMLogger::Logger::Instance().Shutdown();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[reconstruct_sn_nn] failed: " << ex.what() << std::endl;
        SMLogger::Logger::Instance().Shutdown();
        return 1;
    }
}
