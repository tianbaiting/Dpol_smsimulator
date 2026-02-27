#include "TLorentzVector.h"
#include "TFile.h"
#include "TTree.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct CliOptions {
    std::string input_dir;
};

struct MetricAccumulator {
    std::vector<double> values;

    void Add(double value) {
        if (std::isfinite(value)) {
            values.push_back(value);
        }
    }

    double MeanAbs() const {
        if (values.empty()) return std::numeric_limits<double>::quiet_NaN();
        double sum = 0.0;
        for (double v : values) sum += std::abs(v);
        return sum / static_cast<double>(values.size());
    }

    double Bias() const {
        if (values.empty()) return std::numeric_limits<double>::quiet_NaN();
        double sum = 0.0;
        for (double v : values) sum += v;
        return sum / static_cast<double>(values.size());
    }

    double RMSE() const {
        if (values.empty()) return std::numeric_limits<double>::quiet_NaN();
        double sum2 = 0.0;
        for (double v : values) sum2 += v * v;
        return std::sqrt(sum2 / static_cast<double>(values.size()));
    }

    double P95Abs() const {
        if (values.empty()) return std::numeric_limits<double>::quiet_NaN();
        std::vector<double> abs_values;
        abs_values.reserve(values.size());
        for (double v : values) abs_values.push_back(std::abs(v));
        std::sort(abs_values.begin(), abs_values.end());
        const std::size_t idx =
            std::min<std::size_t>(abs_values.size() - 1, static_cast<std::size_t>(std::floor(abs_values.size() * 0.95)));
        return abs_values[idx];
    }
};

struct Summary {
    long long files = 0;
    long long events_total = 0;
    long long events_with_truth_proton = 0;
    long long events_with_reco_proton = 0;
    long long matched_events = 0;
    MetricAccumulator err_px;
    MetricAccumulator err_py;
    MetricAccumulator err_pz;
    MetricAccumulator err_pmag;
};

void PrintUsage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " --input-dir DIR\n";
}

CliOptions ParseArgs(int argc, char* argv[]) {
    CliOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--input-dir" && i + 1 < argc) {
            opts.input_dir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            std::exit(0);
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }
    if (opts.input_dir.empty()) {
        throw std::runtime_error("missing --input-dir");
    }
    return opts;
}

std::vector<fs::path> CollectRecoFiles(const fs::path& dir) {
    std::vector<fs::path> files;
    if (!fs::exists(dir)) {
        return files;
    }
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        const fs::path path = entry.path();
        if (path.extension() != ".root") continue;
        if (path.filename().string().find("_reco.root") == std::string::npos) continue;
        files.push_back(path);
    }
    std::sort(files.begin(), files.end());
    return files;
}

void ProcessFile(const fs::path& file_path, Summary* summary) {
    if (!summary) return;

    TFile fin(file_path.c_str(), "READ");
    if (fin.IsZombie()) {
        std::cerr << "[evaluate_reconstruct_sn_nn] warning: cannot open " << file_path << std::endl;
        return;
    }
    TTree* tree = nullptr;
    fin.GetObject("recoTree", tree);
    if (!tree) {
        std::cerr << "[evaluate_reconstruct_sn_nn] warning: recoTree not found in " << file_path << std::endl;
        return;
    }

    bool truth_has_proton = false;
    TLorentzVector* truth_proton_p4 = nullptr;
    std::vector<double>* reco_proton_px = nullptr;
    std::vector<double>* reco_proton_py = nullptr;
    std::vector<double>* reco_proton_pz = nullptr;

    tree->SetBranchAddress("truth_has_proton", &truth_has_proton);
    tree->SetBranchAddress("truth_proton_p4", &truth_proton_p4);
    tree->SetBranchAddress("reco_proton_px", &reco_proton_px);
    tree->SetBranchAddress("reco_proton_py", &reco_proton_py);
    tree->SetBranchAddress("reco_proton_pz", &reco_proton_pz);

    const Long64_t n = tree->GetEntries();
    summary->events_total += n;

    for (Long64_t i = 0; i < n; ++i) {
        tree->GetEntry(i);
        if (!truth_has_proton || truth_proton_p4 == nullptr) {
            continue;
        }
        ++summary->events_with_truth_proton;

        const std::size_t count_px = reco_proton_px ? reco_proton_px->size() : 0U;
        const std::size_t count_py = reco_proton_py ? reco_proton_py->size() : 0U;
        const std::size_t count_pz = reco_proton_pz ? reco_proton_pz->size() : 0U;
        const std::size_t n_reco = std::min({count_px, count_py, count_pz});
        if (n_reco == 0) {
            continue;
        }
        ++summary->events_with_reco_proton;

        const double truth_p = truth_proton_p4->P();
        std::size_t best_idx = 0;
        double best_delta_p = std::numeric_limits<double>::infinity();
        for (std::size_t j = 0; j < n_reco; ++j) {
            const double px = reco_proton_px->at(j);
            const double py = reco_proton_py->at(j);
            const double pz = reco_proton_pz->at(j);
            const double reco_p = std::sqrt(px * px + py * py + pz * pz);
            const double delta = std::abs(reco_p - truth_p);
            if (delta < best_delta_p) {
                best_delta_p = delta;
                best_idx = j;
            }
        }

        const double reco_px = reco_proton_px->at(best_idx);
        const double reco_py = reco_proton_py->at(best_idx);
        const double reco_pz = reco_proton_pz->at(best_idx);
        const double reco_p = std::sqrt(reco_px * reco_px + reco_py * reco_py + reco_pz * reco_pz);

        summary->err_px.Add(reco_px - truth_proton_p4->Px());
        summary->err_py.Add(reco_py - truth_proton_p4->Py());
        summary->err_pz.Add(reco_pz - truth_proton_p4->Pz());
        summary->err_pmag.Add(reco_p - truth_p);
        ++summary->matched_events;
    }
}

void PrintMetric(const char* name, const MetricAccumulator& acc) {
    std::cout << name
              << "  MAE=" << acc.MeanAbs()
              << "  RMSE=" << acc.RMSE()
              << "  Bias=" << acc.Bias()
              << "  P95Abs=" << acc.P95Abs()
              << "\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const CliOptions opts = ParseArgs(argc, argv);
        const fs::path input_dir(opts.input_dir);
        if (!fs::exists(input_dir)) {
            throw std::runtime_error("input-dir does not exist: " + input_dir.string());
        }

        const std::vector<fs::path> files = CollectRecoFiles(input_dir);
        if (files.empty()) {
            throw std::runtime_error("no *_reco.root files found under: " + input_dir.string());
        }

        Summary summary;
        summary.files = static_cast<long long>(files.size());
        for (const auto& file : files) {
            ProcessFile(file, &summary);
        }

        std::cout << "[evaluate_reconstruct_sn_nn] files=" << summary.files << "\n";
        std::cout << "[evaluate_reconstruct_sn_nn] events_total=" << summary.events_total << "\n";
        std::cout << "[evaluate_reconstruct_sn_nn] events_with_truth_proton=" << summary.events_with_truth_proton << "\n";
        std::cout << "[evaluate_reconstruct_sn_nn] events_with_reco_proton=" << summary.events_with_reco_proton << "\n";
        std::cout << "[evaluate_reconstruct_sn_nn] matched_events=" << summary.matched_events << "\n";

        const double denom_truth = summary.events_with_truth_proton > 0
            ? static_cast<double>(summary.events_with_truth_proton)
            : 1.0;
        std::cout << "[evaluate_reconstruct_sn_nn] reco_efficiency_vs_truth="
                  << (summary.events_with_reco_proton * 100.0 / denom_truth) << "%\n";

        PrintMetric("[evaluate_reconstruct_sn_nn] error_px", summary.err_px);
        PrintMetric("[evaluate_reconstruct_sn_nn] error_py", summary.err_py);
        PrintMetric("[evaluate_reconstruct_sn_nn] error_pz", summary.err_pz);
        PrintMetric("[evaluate_reconstruct_sn_nn] error_|p|", summary.err_pmag);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[evaluate_reconstruct_sn_nn] failed: " << ex.what() << "\n";
        return 1;
    }
}
