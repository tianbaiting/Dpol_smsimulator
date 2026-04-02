#include "TBeamSimData.hh"

#include "TFile.h"
#include "TLorentzVector.h"
#include "TTree.h"
#include "TVector3.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr double kProtonMassMeV = 938.2720813;
constexpr std::array<double, 5> kTruthPxGridMeV{-100.0, -50.0, 0.0, 50.0, 100.0};
constexpr double kTruthPyMeV = 0.0;
constexpr double kTruthPzMeV = 627.0;

struct CliOptions {
    std::string backend;
    std::string rk_fit_mode = "three-point-free";
    std::string smsimdir;
    std::string output_dir;
    std::string sim_bin;
    std::string reco_bin;
    std::string eval_bin;
    std::string geometry_macro;
    std::string field_map;
    std::string nn_model_json;
    double threshold_px = 20.0;
    double threshold_py = 15.0;
    double threshold_pz = 20.0;
    long long seed_a = 20260326;
    long long seed_b = 20260327;
    bool require_gate_pass = true;
    long long require_min_matched = 0;
};

struct SummaryRow {
    std::string backend;
    double truth_px = std::numeric_limits<double>::quiet_NaN();
    double truth_py = std::numeric_limits<double>::quiet_NaN();
    double truth_pz = std::numeric_limits<double>::quiet_NaN();
    long long truth_events = 0;
    long long matched_events = 0;
    double bias_px = std::numeric_limits<double>::quiet_NaN();
    double bias_py = std::numeric_limits<double>::quiet_NaN();
    double bias_pz = std::numeric_limits<double>::quiet_NaN();
    double mae_px = std::numeric_limits<double>::quiet_NaN();
    double mae_py = std::numeric_limits<double>::quiet_NaN();
    double mae_pz = std::numeric_limits<double>::quiet_NaN();
    int pass_px = 0;
    int pass_py = 0;
    int pass_pz = 0;
    int gate_pass = 0;
};

std::string ShellQuote(const std::string& text) {
    std::string quoted = "'";
    for (char ch : text) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted.push_back('\'');
    return quoted;
}

double ParseDouble(const std::string& text, const char* name) {
    try {
        return std::stod(text);
    } catch (...) {
        throw std::runtime_error(std::string("invalid numeric value for ") + name + ": " + text);
    }
}

long long ParseInteger(const std::string& text, const char* name) {
    try {
        return std::stoll(text);
    } catch (...) {
        throw std::runtime_error(std::string("invalid integer value for ") + name + ": " + text);
    }
}

bool ParseBoolFlag(const std::string& text, const char* name) {
    if (text == "1" || text == "true" || text == "on" || text == "yes") {
        return true;
    }
    if (text == "0" || text == "false" || text == "off" || text == "no") {
        return false;
    }
    throw std::runtime_error(std::string("invalid boolean value for ") + name + ": " + text);
}

void PrintUsage(const char* argv0) {
    std::cout
        << "Usage: " << argv0
        << " --backend NAME --smsimdir DIR --output-dir DIR"
        << " --sim-bin FILE --reco-bin FILE --eval-bin FILE"
        << " --geometry-macro FILE [--field-map FILE] [--nn-model-json FILE]"
        << " [--rk-fit-mode NAME]"
        << " [--threshold-px V] [--threshold-py V] [--threshold-pz V]"
        << " [--seed-a N] [--seed-b N] [--require-gate-pass 0|1] [--require-min-matched N]\n";
}

CliOptions ParseArgs(int argc, char* argv[]) {
    CliOptions opts;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--backend" && i + 1 < argc) {
            opts.backend = argv[++i];
        } else if (arg == "--rk-fit-mode" && i + 1 < argc) {
            opts.rk_fit_mode = argv[++i];
        } else if (arg == "--smsimdir" && i + 1 < argc) {
            opts.smsimdir = argv[++i];
        } else if (arg == "--output-dir" && i + 1 < argc) {
            opts.output_dir = argv[++i];
        } else if (arg == "--sim-bin" && i + 1 < argc) {
            opts.sim_bin = argv[++i];
        } else if (arg == "--reco-bin" && i + 1 < argc) {
            opts.reco_bin = argv[++i];
        } else if (arg == "--eval-bin" && i + 1 < argc) {
            opts.eval_bin = argv[++i];
        } else if (arg == "--geometry-macro" && i + 1 < argc) {
            opts.geometry_macro = argv[++i];
        } else if (arg == "--field-map" && i + 1 < argc) {
            opts.field_map = argv[++i];
        } else if (arg == "--nn-model-json" && i + 1 < argc) {
            opts.nn_model_json = argv[++i];
        } else if (arg == "--threshold-px" && i + 1 < argc) {
            opts.threshold_px = ParseDouble(argv[++i], "--threshold-px");
        } else if (arg == "--threshold-py" && i + 1 < argc) {
            opts.threshold_py = ParseDouble(argv[++i], "--threshold-py");
        } else if (arg == "--threshold-pz" && i + 1 < argc) {
            opts.threshold_pz = ParseDouble(argv[++i], "--threshold-pz");
        } else if (arg == "--seed-a" && i + 1 < argc) {
            opts.seed_a = ParseInteger(argv[++i], "--seed-a");
        } else if (arg == "--seed-b" && i + 1 < argc) {
            opts.seed_b = ParseInteger(argv[++i], "--seed-b");
        } else if (arg == "--require-gate-pass" && i + 1 < argc) {
            opts.require_gate_pass = ParseBoolFlag(argv[++i], "--require-gate-pass");
        } else if (arg == "--require-min-matched" && i + 1 < argc) {
            opts.require_min_matched = ParseInteger(argv[++i], "--require-min-matched");
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            std::exit(0);
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }

    if (opts.backend.empty() || opts.smsimdir.empty() || opts.output_dir.empty() || opts.sim_bin.empty() ||
        opts.reco_bin.empty() || opts.eval_bin.empty() || opts.geometry_macro.empty()) {
        throw std::runtime_error("missing required arguments");
    }
    return opts;
}

std::string SanitizeTag(std::string value) {
    for (char& ch : value) {
        if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')) {
            ch = '_';
        }
    }
    return value;
}

std::string BuildRunTag(const CliOptions& opts) {
    if (opts.backend != "rk") {
        return opts.backend;
    }
    return opts.backend + "_" + SanitizeTag(opts.rk_fit_mode);
}

void EnsureExists(const fs::path& path, const char* label) {
    if (!fs::exists(path)) {
        throw std::runtime_error(std::string("missing ") + label + ": " + path.string());
    }
}

void SetSmsimEnvironment(const std::string& smsimdir) {
    // [EN] Force all child processes to resolve {SMSIMDIR} against the repo under test, not the caller shell. / [CN] 强制所有子进程将{SMSIMDIR}解析到当前被测仓库，而不是调用者外部shell环境。
    if (::setenv("SMSIMDIR", smsimdir.c_str(), 1) != 0) {
        throw std::runtime_error("failed to set SMSIMDIR environment");
    }
}

void WriteFixedTruthInputTree(const fs::path& output_root) {
    fs::create_directories(output_root.parent_path());

    TFile out_file(output_root.c_str(), "RECREATE");
    if (out_file.IsZombie()) {
        throw std::runtime_error("failed to create fixed truth input ROOT: " + output_root.string());
    }

    TTree tree("tree_input", "Deterministic proton truth grid for PDC target-momentum E2E tests");
    auto beam_array = std::make_unique<TBeamSimDataArray>();
    gBeamSimDataArray = beam_array.get();
    tree.Branch("TBeamSimData", &gBeamSimDataArray);

    const TVector3 origin_mm(0.0, 0.0, 0.0);
    for (std::size_t i = 0; i < kTruthPxGridMeV.size(); ++i) {
        beam_array->clear();
        const double px = kTruthPxGridMeV[i];
        const double energy = std::sqrt(px * px + kTruthPyMeV * kTruthPyMeV + kTruthPzMeV * kTruthPzMeV +
                                        kProtonMassMeV * kProtonMassMeV);
        TLorentzVector momentum(px, kTruthPyMeV, kTruthPzMeV, energy);
        TBeamSimData proton(1, 1, momentum, origin_mm);
        proton.fParticleName = "proton";
        proton.fPrimaryParticleID = 0;
        proton.fTime = 0.0;
        proton.fIsAccepted = true;
        beam_array->push_back(proton);
        tree.Fill();
    }

    tree.Write();
    out_file.Close();
    gBeamSimDataArray = nullptr;
}

void WriteGeant4Macro(const CliOptions& opts,
                      const std::string& run_tag,
                      const fs::path& input_root,
                      const fs::path& macro_path,
                      const fs::path& sim_dir) {
    fs::create_directories(macro_path.parent_path());
    fs::create_directories(sim_dir);

    std::ofstream out(macro_path);
    if (!out.is_open()) {
        throw std::runtime_error("failed to create Geant4 macro: " + macro_path.string());
    }

    // [EN] Fix both CLHEP seeds so Geant4 transport fluctuations remain reproducible across regression runs. / [CN] 固定两组CLHEP随机种子，保证Geant4输运涨落在回归运行中可重复。
    out << "/control/getEnv SMSIMDIR\n";
    out << "/control/execute " << opts.geometry_macro << "\n";
    out << "/random/setSeeds " << opts.seed_a << " " << opts.seed_b << "\n";
    out << "/action/file/OverWrite y\n";
    out << "/action/file/RunName pdc_target_momentum_" << run_tag << "\n";
    out << "/action/file/SaveDirectory " << sim_dir.string() << "\n";
    out << "/tracking/storeTrajectory 0\n";
    out << "/action/gun/Type Tree\n";
    out << "/action/gun/tree/InputFileName " << input_root.string() << "\n";
    out << "/action/gun/tree/TreeName tree_input\n";
    out << "/action/gun/tree/beamOn 0\n";
}

void RunCommand(const std::vector<std::string>& args, const fs::path& workdir, const fs::path& log_path) {
    fs::create_directories(log_path.parent_path());

    std::ostringstream cmd;
    cmd << "cd " << ShellQuote(workdir.string()) << " && ";
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i != 0U) {
            cmd << ' ';
        }
        cmd << ShellQuote(args[i]);
    }
    cmd << " > " << ShellQuote(log_path.string()) << " 2>&1";

    const int rc = std::system(cmd.str().c_str());
    if (rc != 0) {
        throw std::runtime_error("command failed with exit code " + std::to_string(rc) + ", see log: " + log_path.string());
    }
}

fs::path FindSingleSimOutput(const fs::path& sim_dir, const std::string& run_tag) {
    const std::string prefix = "pdc_target_momentum_" + run_tag;
    std::vector<fs::path> matches;
    for (const auto& entry : fs::directory_iterator(sim_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const fs::path path = entry.path();
        if (path.extension() != ".root") {
            continue;
        }
        if (path.filename().string().find(prefix) != 0U) {
            continue;
        }
        matches.push_back(path);
    }
    std::sort(matches.begin(), matches.end());
    if (matches.empty()) {
        throw std::runtime_error("no Geant4 ROOT output found in: " + sim_dir.string());
    }
    return matches.front();
}

std::vector<std::string> SplitCsvLine(const std::string& line) {
    std::vector<std::string> cells;
    std::string current;
    for (char ch : line) {
        if (ch == ',') {
            cells.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    cells.push_back(current);
    return cells;
}

std::map<std::string, std::size_t> ReadHeaderIndex(const std::string& header_line) {
    const std::vector<std::string> headers = SplitCsvLine(header_line);
    std::map<std::string, std::size_t> index;
    for (std::size_t i = 0; i < headers.size(); ++i) {
        index[headers[i]] = i;
    }
    return index;
}

std::string GetCell(const std::vector<std::string>& cells,
                    const std::map<std::string, std::size_t>& index,
                    std::string_view name) {
    const auto it = index.find(std::string(name));
    if (it == index.end() || it->second >= cells.size()) {
        throw std::runtime_error("missing csv column: " + std::string(name));
    }
    return cells[it->second];
}

double ParseCsvDouble(const std::vector<std::string>& cells,
                      const std::map<std::string, std::size_t>& index,
                      std::string_view name) {
    return std::stod(GetCell(cells, index, name));
}

long long ParseCsvInteger(const std::vector<std::string>& cells,
                          const std::map<std::string, std::size_t>& index,
                          std::string_view name) {
    return std::stoll(GetCell(cells, index, name));
}

std::vector<SummaryRow> ReadSummaryRows(const fs::path& csv_path) {
    std::ifstream in(csv_path);
    if (!in.is_open()) {
        throw std::runtime_error("failed to open summary csv: " + csv_path.string());
    }

    std::string line;
    if (!std::getline(in, line)) {
        throw std::runtime_error("summary csv is empty: " + csv_path.string());
    }
    const std::map<std::string, std::size_t> index = ReadHeaderIndex(line);

    std::vector<SummaryRow> rows;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        const std::vector<std::string> cells = SplitCsvLine(line);
        SummaryRow row;
        row.backend = GetCell(cells, index, "backend");
        row.truth_px = ParseCsvDouble(cells, index, "truth_px");
        row.truth_py = ParseCsvDouble(cells, index, "truth_py");
        row.truth_pz = ParseCsvDouble(cells, index, "truth_pz");
        row.truth_events = ParseCsvInteger(cells, index, "truth_events");
        row.matched_events = ParseCsvInteger(cells, index, "matched_events");
        row.bias_px = ParseCsvDouble(cells, index, "bias_px");
        row.bias_py = ParseCsvDouble(cells, index, "bias_py");
        row.bias_pz = ParseCsvDouble(cells, index, "bias_pz");
        row.mae_px = ParseCsvDouble(cells, index, "mae_px");
        row.mae_py = ParseCsvDouble(cells, index, "mae_py");
        row.mae_pz = ParseCsvDouble(cells, index, "mae_pz");
        row.pass_px = static_cast<int>(ParseCsvInteger(cells, index, "pass_px"));
        row.pass_py = static_cast<int>(ParseCsvInteger(cells, index, "pass_py"));
        row.pass_pz = static_cast<int>(ParseCsvInteger(cells, index, "pass_pz"));
        row.gate_pass = static_cast<int>(ParseCsvInteger(cells, index, "gate_pass"));
        rows.push_back(row);
    }

    std::sort(rows.begin(), rows.end(), [](const SummaryRow& a, const SummaryRow& b) {
        return a.truth_px < b.truth_px;
    });
    return rows;
}

void ValidateRows(const std::vector<SummaryRow>& rows, const CliOptions& opts) {
    if (rows.size() != kTruthPxGridMeV.size()) {
        throw std::runtime_error(
            "unexpected number of summary rows: expected " + std::to_string(kTruthPxGridMeV.size()) +
            ", got " + std::to_string(rows.size())
        );
    }

    long long matched_total = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const SummaryRow& row = rows[i];
        const double expected_px = kTruthPxGridMeV[i];
        if (row.backend != opts.backend) {
            throw std::runtime_error("summary backend mismatch: expected " + opts.backend + ", got " + row.backend);
        }
        if (std::abs(row.truth_px - expected_px) > 1.0e-6 ||
            std::abs(row.truth_py - kTruthPyMeV) > 1.0e-6 ||
            std::abs(row.truth_pz - kTruthPzMeV) > 1.0e-6) {
            throw std::runtime_error("truth grid mismatch in summary csv");
        }
        if (row.truth_events != 1) {
            throw std::runtime_error("expected exactly one truth event per grid point");
        }
        matched_total += row.matched_events;

        std::cout << "[test_pdc_target_momentum_e2e] backend=" << row.backend;
        if (opts.backend == "rk") {
            std::cout << " rk_mode=" << opts.rk_fit_mode;
        }
        std::cout << " truth_px=" << row.truth_px
                  << " matched=" << row.matched_events
                  << " dpx=" << row.bias_px
                  << " dpy=" << row.bias_py
                  << " dpz=" << row.bias_pz
                  << " gate=" << row.gate_pass
                  << "\n";

        if (!opts.require_gate_pass) {
            continue;
        }

        if (row.matched_events != 1) {
            throw std::runtime_error("gate backend did not reconstruct the truth point");
        }
        if (row.pass_px != 1 || row.pass_py != 1 || row.pass_pz != 1 || row.gate_pass != 1) {
            throw std::runtime_error("gate backend exceeded configured momentum-error thresholds");
        }
        if (!(std::isfinite(row.mae_px) && std::isfinite(row.mae_py) && std::isfinite(row.mae_pz))) {
            throw std::runtime_error("gate backend produced non-finite summary metrics");
        }
    }

    if (opts.require_min_matched > 0 && matched_total < opts.require_min_matched) {
        throw std::runtime_error("reconstructed too few truth points for requested backend/mode");
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const CliOptions opts = ParseArgs(argc, argv);
        const fs::path smsimdir(opts.smsimdir);
        const fs::path output_dir(opts.output_dir);
        const fs::path sim_bin(opts.sim_bin);
        const fs::path reco_bin(opts.reco_bin);
        const fs::path eval_bin(opts.eval_bin);
        const fs::path geometry_macro(opts.geometry_macro);

        EnsureExists(smsimdir, "smsimdir");
        EnsureExists(sim_bin, "sim_deuteron binary");
        EnsureExists(reco_bin, "reconstruct_target_momentum binary");
        EnsureExists(eval_bin, "evaluate_target_momentum_reco binary");
        EnsureExists(geometry_macro, "geometry macro");

        if ((opts.backend == "rk" || opts.backend == "multidim") && opts.field_map.empty()) {
            throw std::runtime_error("selected backend requires --field-map");
        }
        if (opts.backend == "nn" && opts.nn_model_json.empty()) {
            throw std::runtime_error("nn backend requires --nn-model-json");
        }
        if (!opts.field_map.empty()) {
            EnsureExists(fs::path(opts.field_map), "field map");
        }
        if (!opts.nn_model_json.empty()) {
            EnsureExists(fs::path(opts.nn_model_json), "nn model json");
        }

        SetSmsimEnvironment(opts.smsimdir);
        fs::create_directories(output_dir);
        const std::string run_tag = BuildRunTag(opts);

        const fs::path input_root = output_dir / "input" / "pdc_truth_grid.root";
        const fs::path macro_path = output_dir / "macros" / ("run_" + run_tag + ".mac");
        const fs::path sim_dir = output_dir / "sim";
        const fs::path reco_dir = output_dir / "reco";
        const fs::path eval_dir = output_dir / "eval";
        const fs::path log_dir = output_dir / "logs";
        const fs::path reco_root = reco_dir / ("pdc_truth_grid_" + run_tag + "_reco.root");
        const fs::path summary_csv = eval_dir / ("summary_" + run_tag + ".csv");
        const fs::path event_csv = eval_dir / ("events_" + run_tag + ".csv");

        WriteFixedTruthInputTree(input_root);
        WriteGeant4Macro(opts, run_tag, input_root, macro_path, sim_dir);

        RunCommand({sim_bin.string(), macro_path.string()}, output_dir, log_dir / ("sim_" + run_tag + ".log"));
        const fs::path sim_root = FindSingleSimOutput(sim_dir, run_tag);

        fs::create_directories(reco_dir);
        std::vector<std::string> reco_cmd = {
            reco_bin.string(),
            "--backend", opts.backend,
            "--input-file", sim_root.string(),
            "--output-file", reco_root.string(),
            "--geometry-macro", geometry_macro.string()
        };
        if (!opts.field_map.empty()) {
            reco_cmd.push_back("--magnetic-field-map");
            reco_cmd.push_back(opts.field_map);
        }
        if (opts.backend == "rk") {
            reco_cmd.push_back("--rk-fit-mode");
            reco_cmd.push_back(opts.rk_fit_mode);
        }
        if (!opts.nn_model_json.empty()) {
            reco_cmd.push_back("--nn-model-json");
            reco_cmd.push_back(opts.nn_model_json);
        }
        RunCommand(reco_cmd, output_dir, log_dir / ("reco_" + run_tag + ".log"));

        std::vector<std::string> eval_cmd = {
            eval_bin.string(),
            "--input-file", reco_root.string(),
            "--summary-csv", summary_csv.string(),
            "--event-csv", event_csv.string(),
            "--threshold-px", std::to_string(opts.threshold_px),
            "--threshold-py", std::to_string(opts.threshold_py),
            "--threshold-pz", std::to_string(opts.threshold_pz)
        };
        RunCommand(eval_cmd, output_dir, log_dir / ("eval_" + run_tag + ".log"));

        EnsureExists(summary_csv, "summary csv");
        EnsureExists(event_csv, "event csv");
        const std::vector<SummaryRow> rows = ReadSummaryRows(summary_csv);
        ValidateRows(rows, opts);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[test_pdc_target_momentum_e2e] failed: " << ex.what() << "\n";
        return 1;
    }
}
