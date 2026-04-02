#include "QMDInputMetadata.hh"
#include "TBeamSimData.hh"
#include "TSimData.hh"

#include <TClonesArray.h>
#include <TFile.h>
#include <TString.h>
#include <TTree.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr int kBMax = 10;
constexpr double kDefaultConstraint = 0.005;
constexpr double kUnknownBimp = -1.0;

enum class InputCategory {
    kElastic,
    kAllevent
};

struct BinnedCount {
    int selected_total = 0;
    int selected_ips = 0;
};

struct Metrics {
    double offset_mm = 0.0;
    int elastic_selected_total = 0;
    int elastic_selected_ips = 0;
    int smallb_selected_total = 0;
    int smallb_selected_ips = 0;
    int smallb_raw_total = 0;
    int smallb_raw_ips = 0;
    std::array<BinnedCount, kBMax> binned{};
};

struct Candidate {
    Metrics metrics;
    bool feasible = false;
};

struct Options {
    fs::path sim_bin;
    fs::path geometry_macro;
    fs::path output_dir;
    std::vector<fs::path> scan_elastic_inputs;
    std::vector<fs::path> scan_allevent_inputs;
    std::vector<fs::path> validation_elastic_inputs;
    std::vector<fs::path> validation_allevent_inputs;
    double coarse_min_mm = -200.0;
    double coarse_max_mm = 200.0;
    double coarse_step_mm = 20.0;
    double refine_half_window_mm = 20.0;
    double refine_step_mm = 5.0;
    int topk = 3;
    int beam_on = 0;
    double small_b_max = 7.0;
    double elastic_leakage_limit = kDefaultConstraint;
    bool require_forward = true;
};

struct DatasetSet {
    std::vector<fs::path> elastic_inputs;
    std::vector<fs::path> allevent_inputs;
    std::string phase;
};

std::string EnsureTrailingSlash(const fs::path& path) {
    std::string s = path.string();
    if (!s.empty() && s.back() != '/') {
        s.push_back('/');
    }
    return s;
}

std::string SanitizeName(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (unsigned char c : input) {
        if (std::isalnum(c)) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('_');
        }
    }
    return out;
}

std::string FormatOffsetTag(double offset_mm) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << offset_mm;
    return SanitizeName(oss.str());
}

void PrintUsage(const char* prog) {
    std::cerr
        << "Usage: " << prog << " --sim-bin <bin/sim_deuteron> --geometry-macro <macro> --output-dir <dir>\n"
        << "  --scan-elastic <root-or-dir>     repeatable\n"
        << "  --scan-allevent <root-or-dir>    repeatable\n"
        << "  [--validation-elastic <root-or-dir>] [--validation-allevent <root-or-dir>]\n"
        << "  [--coarse-min-mm -200 --coarse-max-mm 200 --coarse-step-mm 20]\n"
        << "  [--refine-half-window-mm 20 --refine-step-mm 5 --topk 3]\n"
        << "  [--small-b-max 7 --elastic-leakage-limit 0.005 --beam-on 0]\n"
        << "  [--require-forward true|false]\n";
}

std::vector<fs::path> CollectRootFiles(const std::vector<fs::path>& inputs) {
    std::set<fs::path> ordered;
    for (const auto& input : inputs) {
        if (!fs::exists(input)) {
            throw std::runtime_error("Input path does not exist: " + input.string());
        }
        if (fs::is_regular_file(input)) {
            if (input.extension() == ".root") {
                ordered.insert(fs::absolute(input));
            }
            continue;
        }
        for (const auto& entry : fs::recursive_directory_iterator(input)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            if (entry.path().extension() == ".root") {
                ordered.insert(fs::absolute(entry.path()));
            }
        }
    }
    return std::vector<fs::path>(ordered.begin(), ordered.end());
}

Options ParseArgs(int argc, char** argv) {
    Options opts;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--sim-bin" && i + 1 < argc) {
            opts.sim_bin = fs::path(argv[++i]);
        } else if (arg == "--geometry-macro" && i + 1 < argc) {
            opts.geometry_macro = fs::path(argv[++i]);
        } else if (arg == "--output-dir" && i + 1 < argc) {
            opts.output_dir = fs::path(argv[++i]);
        } else if (arg == "--scan-elastic" && i + 1 < argc) {
            opts.scan_elastic_inputs.push_back(fs::path(argv[++i]));
        } else if (arg == "--scan-allevent" && i + 1 < argc) {
            opts.scan_allevent_inputs.push_back(fs::path(argv[++i]));
        } else if (arg == "--validation-elastic" && i + 1 < argc) {
            opts.validation_elastic_inputs.push_back(fs::path(argv[++i]));
        } else if (arg == "--validation-allevent" && i + 1 < argc) {
            opts.validation_allevent_inputs.push_back(fs::path(argv[++i]));
        } else if (arg == "--coarse-min-mm" && i + 1 < argc) {
            opts.coarse_min_mm = std::atof(argv[++i]);
        } else if (arg == "--coarse-max-mm" && i + 1 < argc) {
            opts.coarse_max_mm = std::atof(argv[++i]);
        } else if (arg == "--coarse-step-mm" && i + 1 < argc) {
            opts.coarse_step_mm = std::atof(argv[++i]);
        } else if (arg == "--refine-half-window-mm" && i + 1 < argc) {
            opts.refine_half_window_mm = std::atof(argv[++i]);
        } else if (arg == "--refine-step-mm" && i + 1 < argc) {
            opts.refine_step_mm = std::atof(argv[++i]);
        } else if (arg == "--topk" && i + 1 < argc) {
            opts.topk = std::atoi(argv[++i]);
        } else if (arg == "--beam-on" && i + 1 < argc) {
            opts.beam_on = std::atoi(argv[++i]);
        } else if (arg == "--small-b-max" && i + 1 < argc) {
            opts.small_b_max = std::atof(argv[++i]);
        } else if (arg == "--elastic-leakage-limit" && i + 1 < argc) {
            opts.elastic_leakage_limit = std::atof(argv[++i]);
        } else if (arg == "--require-forward" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "true" || value == "1" || value == "yes") {
                opts.require_forward = true;
            } else if (value == "false" || value == "0" || value == "no") {
                opts.require_forward = false;
            } else {
                throw std::runtime_error("Invalid value for --require-forward: " + value);
            }
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }

    if (opts.sim_bin.empty() || opts.geometry_macro.empty() || opts.output_dir.empty()) {
        throw std::runtime_error("sim-bin, geometry-macro, and output-dir are required");
    }
    if (opts.scan_elastic_inputs.empty() && opts.scan_allevent_inputs.empty()) {
        throw std::runtime_error("At least one scan input set is required");
    }
    if (opts.coarse_step_mm <= 0.0 || opts.refine_step_mm <= 0.0 || opts.topk <= 0) {
        throw std::runtime_error("Scan steps and topk must be positive");
    }

    auto normalize = [](fs::path& path) {
        if (!path.empty()) {
            path = fs::absolute(path);
        }
    };
    normalize(opts.sim_bin);
    normalize(opts.geometry_macro);
    normalize(opts.output_dir);
    for (auto& path : opts.scan_elastic_inputs) {
        normalize(path);
    }
    for (auto& path : opts.scan_allevent_inputs) {
        normalize(path);
    }
    for (auto& path : opts.validation_elastic_inputs) {
        normalize(path);
    }
    for (auto& path : opts.validation_allevent_inputs) {
        normalize(path);
    }
    return opts;
}

std::vector<double> BuildGrid(double min_mm, double max_mm, double step_mm) {
    std::vector<double> values;
    const double eps = std::abs(step_mm) * 1.0e-9;
    for (double value = min_mm; value <= max_mm + eps; value += step_mm) {
        values.push_back(value);
    }
    return values;
}

double ExtractBimp(const std::vector<TBeamSimData>* beam) {
    if (!beam || beam->empty()) {
        return kUnknownBimp;
    }
    const auto& primary = beam->front();
    if (primary.fUserDouble.size() <= static_cast<std::size_t>(qmd_input_metadata::kBimpIndex)) {
        return kUnknownBimp;
    }
    return primary.fUserDouble[qmd_input_metadata::kBimpIndex];
}

bool HasIPSHit(TClonesArray* frag_hits) {
    if (!frag_hits) {
        return false;
    }
    const int entries = frag_hits->GetEntriesFast();
    for (int i = 0; i < entries; ++i) {
        auto* hit = static_cast<TSimData*>(frag_hits->At(i));
        if (!hit) {
            continue;
        }
        if (hit->fDetectorName == "IPS" && hit->fEnergyDeposit > 0.0) {
            return true;
        }
    }
    return false;
}

int BIndex(double bimp) {
    if (!std::isfinite(bimp)) {
        return -1;
    }
    const int rounded = static_cast<int>(std::lround(bimp));
    if (rounded < 1 || rounded > kBMax) {
        return -1;
    }
    return rounded - 1;
}

void MergeMetrics(Metrics& dst, const Metrics& src) {
    dst.elastic_selected_total += src.elastic_selected_total;
    dst.elastic_selected_ips += src.elastic_selected_ips;
    dst.smallb_selected_total += src.smallb_selected_total;
    dst.smallb_selected_ips += src.smallb_selected_ips;
    dst.smallb_raw_total += src.smallb_raw_total;
    dst.smallb_raw_ips += src.smallb_raw_ips;
    for (int i = 0; i < kBMax; ++i) {
        dst.binned[i].selected_total += src.binned[i].selected_total;
        dst.binned[i].selected_ips += src.binned[i].selected_ips;
    }
}

Metrics ReadOutputMetrics(const fs::path& root_path, InputCategory category, double small_b_max, bool require_forward) {
    TFile file(root_path.c_str(), "READ");
    if (file.IsZombie()) {
        throw std::runtime_error("Cannot open simulation output: " + root_path.string());
    }

    auto* tree = dynamic_cast<TTree*>(file.Get("tree"));
    if (!tree) {
        throw std::runtime_error("Missing tree in simulation output: " + root_path.string());
    }

    TClonesArray* frag_hits = nullptr;
    TClonesArray* nebula_hits = nullptr;
    std::vector<TBeamSimData>* beam = nullptr;
    Bool_t ok_pdc1 = kFALSE;
    Bool_t ok_pdc2 = kFALSE;

    tree->SetBranchAddress("FragSimData", &frag_hits);
    if (tree->GetBranch("NEBULAPla")) {
        tree->SetBranchAddress("NEBULAPla", &nebula_hits);
    }
    if (tree->GetBranch("beam")) {
        tree->SetBranchAddress("beam", &beam);
    }
    if (tree->GetBranch("OK_PDC1")) {
        tree->SetBranchAddress("OK_PDC1", &ok_pdc1);
    }
    if (tree->GetBranch("OK_PDC2")) {
        tree->SetBranchAddress("OK_PDC2", &ok_pdc2);
    }

    Metrics metrics;
    const Long64_t entries = tree->GetEntries();
    for (Long64_t i = 0; i < entries; ++i) {
        tree->GetEntry(i);
        const bool forward = (ok_pdc1 != kFALSE) && (ok_pdc2 != kFALSE) && (nebula_hits != nullptr) && (nebula_hits->GetEntriesFast() > 0);
        const bool selected = require_forward ? forward : true;
        const bool ips_hit = HasIPSHit(frag_hits);
        const double bimp = ExtractBimp(beam);
        const bool is_small_b = std::isfinite(bimp) && bimp <= small_b_max;

        if (category == InputCategory::kElastic) {
            if (selected) {
                ++metrics.elastic_selected_total;
                if (ips_hit) {
                    ++metrics.elastic_selected_ips;
                }
            }
        } else {
            if (is_small_b) {
                ++metrics.smallb_raw_total;
                if (ips_hit) {
                    ++metrics.smallb_raw_ips;
                }
            }
            const int b_index = BIndex(bimp);
            if (selected && b_index >= 0) {
                ++metrics.binned[b_index].selected_total;
                if (ips_hit) {
                    ++metrics.binned[b_index].selected_ips;
                }
            }
            if (selected && is_small_b) {
                ++metrics.smallb_selected_total;
                if (ips_hit) {
                    ++metrics.smallb_selected_ips;
                }
            }
        }
    }

    return metrics;
}

std::string ShellQuote(const std::string& text) {
    std::ostringstream oss;
    oss << std::quoted(text);
    return oss.str();
}

fs::path WriteMacro(
    const fs::path& macro_dir,
    const fs::path& geometry_macro,
    const fs::path& input_root,
    const fs::path& save_dir,
    const std::string& run_name,
    double offset_mm,
    int beam_on
) {
    fs::create_directories(macro_dir);
    const fs::path macro_path = macro_dir / (run_name + ".mac");
    std::ofstream out(macro_path);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot create macro: " + macro_path.string());
    }
    out << "/action/file/OverWrite y\n";
    out << "/action/file/SaveDirectory " << EnsureTrailingSlash(save_dir) << "\n";
    out << "/action/file/RunName " << run_name << "\n";
    out << "/tracking/storeTrajectory 0\n";
    out << "/control/execute " << geometry_macro.string() << "\n";
    out << "/samurai/geometry/Target/SetTarget false\n";
    out << "/samurai/geometry/IPS/SetIPS true\n";
    out << "/samurai/geometry/IPS/AxisOffset " << std::fixed << std::setprecision(3) << offset_mm << " mm\n";
    out << "/samurai/geometry/Update\n";
    out << "/action/gun/Type Tree\n";
    out << "/action/gun/tree/InputFileName " << input_root.string() << "\n";
    out << "/action/gun/tree/TreeName tree\n";
    out << "/action/gun/tree/beamOn " << beam_on << "\n";
    return macro_path;
}

fs::path DetermineSimulationWorkingDirectory(const fs::path& geometry_macro) {
    const fs::path geometry_dir = geometry_macro.parent_path();
    const fs::path simulation_dir = geometry_dir.parent_path();
    if (!simulation_dir.empty() && fs::exists(simulation_dir / "geometry")) {
        return simulation_dir;
    }
    return fs::current_path();
}

int RunSimulation(
    const fs::path& sim_bin,
    const fs::path& macro_path,
    const fs::path& log_path,
    const fs::path& working_dir
) {
    const fs::path gdml_path = working_dir / "detector_geometry.gdml";
    std::ostringstream cmd;
    cmd << "cd " << ShellQuote(working_dir.string())
        << " && rm -f " << ShellQuote(gdml_path.string())
        << " && "
        << ShellQuote(sim_bin.string())
        << " " << ShellQuote(macro_path.string())
        << " > " << ShellQuote(log_path.string())
        << " 2>&1";
    return std::system(cmd.str().c_str());
}

long long OffsetKey(double offset_mm) {
    return static_cast<long long>(std::llround(offset_mm * 1000.0));
}

double ElasticLeakage(const Metrics& metrics) {
    if (metrics.elastic_selected_total <= 0) {
        return 0.0;
    }
    return static_cast<double>(metrics.elastic_selected_ips) / static_cast<double>(metrics.elastic_selected_total);
}

double SmallBSelectedRate(const Metrics& metrics) {
    if (metrics.smallb_selected_total <= 0) {
        return 0.0;
    }
    return static_cast<double>(metrics.smallb_selected_ips) / static_cast<double>(metrics.smallb_selected_total);
}

double SmallBRawRate(const Metrics& metrics) {
    if (metrics.smallb_raw_total <= 0) {
        return 0.0;
    }
    return static_cast<double>(metrics.smallb_raw_ips) / static_cast<double>(metrics.smallb_raw_total);
}

Candidate EvaluateOffset(
    double offset_mm,
    const DatasetSet& datasets,
    const Options& opts,
    const fs::path& run_root_dir,
    bool keep_outputs
) {
    Metrics total;
    total.offset_mm = offset_mm;

    const auto elastic_roots = CollectRootFiles(datasets.elastic_inputs);
    const auto allevent_roots = CollectRootFiles(datasets.allevent_inputs);

    auto evaluate_inputs = [&](const std::vector<fs::path>& roots, InputCategory category, const std::string& tag) {
        const fs::path simulation_work_dir = DetermineSimulationWorkingDirectory(opts.geometry_macro);
        for (const auto& input_root : roots) {
            const std::string run_name = tag + "_" + datasets.phase + "_" + SanitizeName(input_root.stem().string())
                + "_off_" + FormatOffsetTag(offset_mm);
            const fs::path macro_dir = run_root_dir / "macros";
            const fs::path log_dir = run_root_dir / "logs";
            const fs::path out_dir = run_root_dir / "g4output" / datasets.phase / tag;
            fs::create_directories(log_dir);
            fs::create_directories(out_dir);
            const fs::path macro_path = WriteMacro(macro_dir, opts.geometry_macro, input_root, out_dir, run_name, offset_mm, opts.beam_on);
            const fs::path log_path = log_dir / (run_name + ".log");
            const int rc = RunSimulation(opts.sim_bin, macro_path, log_path, simulation_work_dir);
            if (rc != 0) {
                throw std::runtime_error("Simulation failed for offset " + std::to_string(offset_mm) + " with log " + log_path.string());
            }
            const fs::path output_root = out_dir / (run_name + "0000.root");
            if (!fs::exists(output_root)) {
                throw std::runtime_error("Expected simulation output missing: " + output_root.string());
            }
            const Metrics file_metrics = ReadOutputMetrics(output_root, category, opts.small_b_max, opts.require_forward);
            MergeMetrics(total, file_metrics);
            if (!keep_outputs) {
                fs::remove(output_root);
                fs::remove(macro_path);
                fs::remove(log_path);
            }
        }
    };

    evaluate_inputs(elastic_roots, InputCategory::kElastic, "elastic");
    evaluate_inputs(allevent_roots, InputCategory::kAllevent, "allevent");

    Candidate candidate;
    candidate.metrics = total;
    candidate.feasible = ElasticLeakage(total) < opts.elastic_leakage_limit;
    return candidate;
}

std::vector<Candidate> RankCandidates(std::vector<Candidate> candidates, const Options&) {
    const bool any_feasible = std::any_of(candidates.begin(), candidates.end(), [](const Candidate& c) {
        return c.feasible;
    });
    std::sort(candidates.begin(), candidates.end(), [&](const Candidate& lhs, const Candidate& rhs) {
        if (any_feasible && lhs.feasible != rhs.feasible) {
            return lhs.feasible > rhs.feasible;
        }
        const double lhs_leak = ElasticLeakage(lhs.metrics);
        const double rhs_leak = ElasticLeakage(rhs.metrics);
        const double lhs_score = SmallBSelectedRate(lhs.metrics);
        const double rhs_score = SmallBSelectedRate(rhs.metrics);
        if (!any_feasible) {
            if (std::abs(lhs_leak - rhs_leak) > 1.0e-12) {
                return lhs_leak < rhs_leak;
            }
            if (std::abs(lhs_score - rhs_score) > 1.0e-12) {
                return lhs_score > rhs_score;
            }
        } else {
            if (std::abs(lhs_score - rhs_score) > 1.0e-12) {
                return lhs_score > rhs_score;
            }
            if (std::abs(lhs_leak - rhs_leak) > 1.0e-12) {
                return lhs_leak < rhs_leak;
            }
        }
        return std::abs(lhs.metrics.offset_mm) < std::abs(rhs.metrics.offset_mm);
    });
    return candidates;
}

std::vector<double> BuildRefineGrid(const std::vector<Candidate>& coarse_ranked, const Options& opts) {
    std::set<long long> unique_keys;
    std::vector<double> refine_offsets;
    const int coarse_take = std::min<int>(opts.topk, static_cast<int>(coarse_ranked.size()));
    for (int i = 0; i < coarse_take; ++i) {
        const double center = coarse_ranked[i].metrics.offset_mm;
        for (double value = center - opts.refine_half_window_mm;
             value <= center + opts.refine_half_window_mm + 1.0e-9;
             value += opts.refine_step_mm) {
            const long long key = OffsetKey(value);
            if (unique_keys.insert(key).second) {
                refine_offsets.push_back(value);
            }
        }
    }
    std::sort(refine_offsets.begin(), refine_offsets.end());
    return refine_offsets;
}

Candidate EvaluateWithCache(
    double offset_mm,
    const DatasetSet& datasets,
    const Options& opts,
    const fs::path& run_root_dir,
    std::map<long long, Candidate>& cache,
    bool keep_outputs = false
) {
    const long long key = OffsetKey(offset_mm);
    const auto it = cache.find(key);
    if (it != cache.end() && !keep_outputs) {
        return it->second;
    }
    const Candidate candidate = EvaluateOffset(offset_mm, datasets, opts, run_root_dir, keep_outputs);
    if (!keep_outputs) {
        cache[key] = candidate;
    }
    return candidate;
}

void WriteSummaryCsv(
    const fs::path& csv_path,
    const std::vector<Candidate>& scan_ranked,
    const std::vector<Candidate>& validation_ranked
) {
    std::ofstream out(csv_path);
    out << "phase,rank,offset_mm,feasible,elastic_selected_total,elastic_selected_ips,elastic_leakage,"
        << "smallb_selected_total,smallb_selected_ips,smallb_selected_rate,"
        << "smallb_raw_total,smallb_raw_ips,smallb_raw_rate";
    for (int i = 1; i <= kBMax; ++i) {
        out << ",b" << i << "_selected_total,b" << i << "_selected_ips";
    }
    out << "\n";

    auto write_rows = [&](const std::vector<Candidate>& rows, const std::string& phase) {
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto& metrics = rows[i].metrics;
            out << phase << "," << (i + 1) << ","
                << std::fixed << std::setprecision(3) << metrics.offset_mm << ","
                << (rows[i].feasible ? 1 : 0) << ","
                << metrics.elastic_selected_total << ","
                << metrics.elastic_selected_ips << ","
                << ElasticLeakage(metrics) << ","
                << metrics.smallb_selected_total << ","
                << metrics.smallb_selected_ips << ","
                << SmallBSelectedRate(metrics) << ","
                << metrics.smallb_raw_total << ","
                << metrics.smallb_raw_ips << ","
                << SmallBRawRate(metrics);
            for (int b = 0; b < kBMax; ++b) {
                out << "," << metrics.binned[b].selected_total
                    << "," << metrics.binned[b].selected_ips;
            }
            out << "\n";
        }
    };

    write_rows(scan_ranked, "scan");
    write_rows(validation_ranked, "validation");
}

void WriteSummaryRoot(
    const fs::path& root_path,
    const std::vector<Candidate>& scan_ranked,
    const std::vector<Candidate>& validation_ranked
) {
    TFile out(root_path.c_str(), "RECREATE");
    TTree tree("ips_scan", "IPS position scan summary");

    TString phase;
    int rank = 0;
    double offset_mm = 0.0;
    int feasible = 0;
    int elastic_selected_total = 0;
    int elastic_selected_ips = 0;
    double elastic_leakage = 0.0;
    int smallb_selected_total = 0;
    int smallb_selected_ips = 0;
    double smallb_selected_rate = 0.0;
    int smallb_raw_total = 0;
    int smallb_raw_ips = 0;
    double smallb_raw_rate = 0.0;
    int b_selected_total[kBMax] = {0};
    int b_selected_ips[kBMax] = {0};

    tree.Branch("phase", &phase);
    tree.Branch("rank", &rank, "rank/I");
    tree.Branch("offset_mm", &offset_mm, "offset_mm/D");
    tree.Branch("feasible", &feasible, "feasible/I");
    tree.Branch("elastic_selected_total", &elastic_selected_total, "elastic_selected_total/I");
    tree.Branch("elastic_selected_ips", &elastic_selected_ips, "elastic_selected_ips/I");
    tree.Branch("elastic_leakage", &elastic_leakage, "elastic_leakage/D");
    tree.Branch("smallb_selected_total", &smallb_selected_total, "smallb_selected_total/I");
    tree.Branch("smallb_selected_ips", &smallb_selected_ips, "smallb_selected_ips/I");
    tree.Branch("smallb_selected_rate", &smallb_selected_rate, "smallb_selected_rate/D");
    tree.Branch("smallb_raw_total", &smallb_raw_total, "smallb_raw_total/I");
    tree.Branch("smallb_raw_ips", &smallb_raw_ips, "smallb_raw_ips/I");
    tree.Branch("smallb_raw_rate", &smallb_raw_rate, "smallb_raw_rate/D");
    tree.Branch("b_selected_total", b_selected_total, "b_selected_total[10]/I");
    tree.Branch("b_selected_ips", b_selected_ips, "b_selected_ips[10]/I");

    auto fill_rows = [&](const std::vector<Candidate>& rows, const char* row_phase) {
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto& metrics = rows[i].metrics;
            phase = row_phase;
            rank = static_cast<int>(i + 1);
            offset_mm = metrics.offset_mm;
            feasible = rows[i].feasible ? 1 : 0;
            elastic_selected_total = metrics.elastic_selected_total;
            elastic_selected_ips = metrics.elastic_selected_ips;
            elastic_leakage = ElasticLeakage(metrics);
            smallb_selected_total = metrics.smallb_selected_total;
            smallb_selected_ips = metrics.smallb_selected_ips;
            smallb_selected_rate = SmallBSelectedRate(metrics);
            smallb_raw_total = metrics.smallb_raw_total;
            smallb_raw_ips = metrics.smallb_raw_ips;
            smallb_raw_rate = SmallBRawRate(metrics);
            for (int b = 0; b < kBMax; ++b) {
                b_selected_total[b] = metrics.binned[b].selected_total;
                b_selected_ips[b] = metrics.binned[b].selected_ips;
            }
            tree.Fill();
        }
    };

    fill_rows(scan_ranked, "scan");
    fill_rows(validation_ranked, "validation");
    tree.Write();
}

void WriteBestPositions(const fs::path& txt_path, const std::vector<Candidate>& ranked, const Options& opts) {
    std::ofstream out(txt_path);
    if (ranked.empty()) {
        out << "No scan results available.\n";
        return;
    }
    out << "Selection mode: " << (opts.require_forward ? "forward_selected" : "all_events") << "\n";
    out << "Small-b definition: bimp <= " << std::fixed << std::setprecision(3) << opts.small_b_max << "\n";
    out << "Recommended IPS offset (mm): " << std::fixed << std::setprecision(3) << ranked.front().metrics.offset_mm << "\n";
    out << "Elastic leakage: " << ElasticLeakage(ranked.front().metrics) << "\n";
    out << "Small-b selected IPS rate: " << SmallBSelectedRate(ranked.front().metrics) << "\n";
    out << "Small-b raw IPS rate: " << SmallBRawRate(ranked.front().metrics) << "\n\n";
    out << "Top candidates:\n";
    for (std::size_t i = 0; i < ranked.size(); ++i) {
        out << "  rank=" << (i + 1)
            << " offset_mm=" << ranked[i].metrics.offset_mm
            << " feasible=" << (ranked[i].feasible ? "yes" : "no")
            << " elastic_leakage=" << ElasticLeakage(ranked[i].metrics)
            << " smallb_selected_rate=" << SmallBSelectedRate(ranked[i].metrics)
            << "\n";
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options opts = ParseArgs(argc, argv);
        fs::create_directories(opts.output_dir);

        DatasetSet scan_dataset{
            opts.scan_elastic_inputs,
            opts.scan_allevent_inputs,
            "scan"
        };
        DatasetSet validation_dataset{
            opts.validation_elastic_inputs,
            opts.validation_allevent_inputs,
            "validation"
        };

        const std::vector<double> coarse_offsets = BuildGrid(opts.coarse_min_mm, opts.coarse_max_mm, opts.coarse_step_mm);
        std::map<long long, Candidate> scan_cache;
        std::vector<Candidate> coarse_candidates;
        coarse_candidates.reserve(coarse_offsets.size());
        for (double offset : coarse_offsets) {
            std::cout << "[scan_ips_position] coarse offset_mm=" << offset << "\n";
            coarse_candidates.push_back(EvaluateWithCache(offset, scan_dataset, opts, opts.output_dir / "scan_runs", scan_cache));
        }

        const std::vector<Candidate> coarse_ranked = RankCandidates(coarse_candidates, opts);
        const std::vector<double> refine_offsets = BuildRefineGrid(coarse_ranked, opts);
        std::vector<Candidate> refine_candidates;
        refine_candidates.reserve(refine_offsets.size());
        for (double offset : refine_offsets) {
            std::cout << "[scan_ips_position] refine offset_mm=" << offset << "\n";
            refine_candidates.push_back(EvaluateWithCache(offset, scan_dataset, opts, opts.output_dir / "scan_runs", scan_cache));
        }
        std::vector<Candidate> final_ranked = RankCandidates(refine_candidates, opts);
        if (final_ranked.empty()) {
            throw std::runtime_error("No scan candidates were evaluated");
        }

        const int keep_count = std::min<int>(opts.topk, static_cast<int>(final_ranked.size()));
        std::vector<Candidate> top_scan(final_ranked.begin(), final_ranked.begin() + keep_count);

        // [EN] Re-run the recommended scan point and validation points so the retained Geant4 outputs correspond only to the final shortlist. / [CN] 重新运行推荐扫描点和验证点，使最终保留的Geant4输出只对应最后 shortlist。
        EvaluateOffset(top_scan.front().metrics.offset_mm, scan_dataset, opts, opts.output_dir / "kept_runs" / "recommended", true);

        std::vector<Candidate> validation_ranked;
        if (!validation_dataset.elastic_inputs.empty() || !validation_dataset.allevent_inputs.empty()) {
            std::vector<Candidate> validation_results;
            for (int i = 0; i < keep_count; ++i) {
                Candidate validation = EvaluateOffset(
                    top_scan[i].metrics.offset_mm,
                    validation_dataset,
                    opts,
                    opts.output_dir / "kept_runs" / ("validation_rank_" + std::to_string(i + 1)),
                    true
                );
                validation.metrics.offset_mm = top_scan[i].metrics.offset_mm;
                validation_results.push_back(validation);
            }
            validation_ranked = RankCandidates(validation_results, opts);
        }

        WriteSummaryCsv(opts.output_dir / "ips_scan_summary.csv", top_scan, validation_ranked);
        WriteSummaryRoot(opts.output_dir / "ips_scan_summary.root", top_scan, validation_ranked);
        WriteBestPositions(opts.output_dir / "best_ips_positions.txt", top_scan, opts);

        std::cout << "[scan_ips_position] recommended_offset_mm=" << top_scan.front().metrics.offset_mm
                  << " elastic_leakage=" << ElasticLeakage(top_scan.front().metrics)
                  << " smallb_selected_rate=" << SmallBSelectedRate(top_scan.front().metrics)
                  << " selection_mode=" << (opts.require_forward ? "forward_selected" : "all_events")
                  << "\n";
        std::cout << "[scan_ips_position] summary_csv=" << (opts.output_dir / "ips_scan_summary.csv") << "\n";
        std::cout << "[scan_ips_position] summary_root=" << (opts.output_dir / "ips_scan_summary.root") << "\n";
        std::cout << "[scan_ips_position] best_txt=" << (opts.output_dir / "best_ips_positions.txt") << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "[scan_ips_position] failed: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
