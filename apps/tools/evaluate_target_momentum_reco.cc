#include "TLorentzVector.h"
#include "TFile.h"
#include "TNamed.h"
#include "TTree.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;

namespace {

#if defined(SMSIM_EVAL_LOG_TAG)
constexpr const char* kLogTag = SMSIM_EVAL_LOG_TAG;
#else
constexpr const char* kLogTag = "evaluate_target_momentum_reco";
#endif

constexpr double kTruthQuantizationScale = 1000.0;

struct CliOptions {
    std::string input_dir;
    std::string input_file;
    std::string summary_csv;
    std::string event_csv;
    double threshold_px = -1.0;
    double threshold_py = -1.0;
    double threshold_pz = -1.0;
};

struct MetricAccumulator {
    std::vector<double> values;

    void Add(double value) {
        if (std::isfinite(value)) {
            values.push_back(value);
        }
    }

    std::size_t Count() const {
        return values.size();
    }

    double MeanAbs() const {
        if (values.empty()) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        double sum = 0.0;
        for (double value : values) {
            sum += std::abs(value);
        }
        return sum / static_cast<double>(values.size());
    }

    double Bias() const {
        if (values.empty()) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        double sum = 0.0;
        for (double value : values) {
            sum += value;
        }
        return sum / static_cast<double>(values.size());
    }

    double RMSE() const {
        if (values.empty()) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        double sum2 = 0.0;
        for (double value : values) {
            sum2 += value * value;
        }
        return std::sqrt(sum2 / static_cast<double>(values.size()));
    }

    double P95Abs() const {
        if (values.empty()) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        std::vector<double> abs_values;
        abs_values.reserve(values.size());
        for (double value : values) {
            abs_values.push_back(std::abs(value));
        }
        std::sort(abs_values.begin(), abs_values.end());
        const std::size_t idx = std::min<std::size_t>(
            abs_values.size() - 1,
            static_cast<std::size_t>(std::floor(static_cast<double>(abs_values.size()) * 0.95))
        );
        return abs_values[idx];
    }
};

struct EventRecord {
    std::string backend;
    std::string source_file;
    long long event_index = -1;
    double truth_px = std::numeric_limits<double>::quiet_NaN();
    double truth_py = std::numeric_limits<double>::quiet_NaN();
    double truth_pz = std::numeric_limits<double>::quiet_NaN();
    double reco_px = std::numeric_limits<double>::quiet_NaN();
    double reco_py = std::numeric_limits<double>::quiet_NaN();
    double reco_pz = std::numeric_limits<double>::quiet_NaN();
    double dpx = std::numeric_limits<double>::quiet_NaN();
    double dpy = std::numeric_limits<double>::quiet_NaN();
    double dpz = std::numeric_limits<double>::quiet_NaN();
    bool matched = false;
};

struct TruthKey {
    std::string backend;
    std::int64_t truth_px_q = 0;
    std::int64_t truth_py_q = 0;
    std::int64_t truth_pz_q = 0;

    bool operator<(const TruthKey& other) const {
        return std::tie(backend, truth_px_q, truth_py_q, truth_pz_q) <
               std::tie(other.backend, other.truth_px_q, other.truth_py_q, other.truth_pz_q);
    }
};

struct PointSummary {
    std::string backend;
    double truth_px = 0.0;
    double truth_py = 0.0;
    double truth_pz = 0.0;
    long long truth_events = 0;
    long long matched_events = 0;
    MetricAccumulator err_px;
    MetricAccumulator err_py;
    MetricAccumulator err_pz;
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
    std::vector<EventRecord> event_records;
    std::map<TruthKey, PointSummary> point_summaries;
};

std::string FormatDouble(double value, int precision = 6) {
    if (!std::isfinite(value)) {
        return "nan";
    }
    std::ostringstream os;
    os << std::fixed << std::setprecision(precision) << value;
    return os.str();
}

double ParseDouble(const std::string& text, const char* name) {
    try {
        return std::stod(text);
    } catch (...) {
        throw std::runtime_error(std::string("invalid numeric value for ") + name + ": " + text);
    }
}

void PrintUsage(const char* argv0) {
    std::cout
        << "Usage: " << argv0 << " --input-dir DIR [--summary-csv FILE] [--event-csv FILE]\n"
        << "       " << argv0 << " --input-file FILE [--summary-csv FILE] [--event-csv FILE]\n"
        << "       [--threshold-px V] [--threshold-py V] [--threshold-pz V]\n";
}

CliOptions ParseArgs(int argc, char* argv[]) {
    CliOptions opts;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--input-dir" && i + 1 < argc) {
            opts.input_dir = argv[++i];
        } else if (arg == "--input-file" && i + 1 < argc) {
            opts.input_file = argv[++i];
        } else if (arg == "--summary-csv" && i + 1 < argc) {
            opts.summary_csv = argv[++i];
        } else if (arg == "--event-csv" && i + 1 < argc) {
            opts.event_csv = argv[++i];
        } else if (arg == "--threshold-px" && i + 1 < argc) {
            opts.threshold_px = ParseDouble(argv[++i], "--threshold-px");
        } else if (arg == "--threshold-py" && i + 1 < argc) {
            opts.threshold_py = ParseDouble(argv[++i], "--threshold-py");
        } else if (arg == "--threshold-pz" && i + 1 < argc) {
            opts.threshold_pz = ParseDouble(argv[++i], "--threshold-pz");
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            std::exit(0);
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }

    const bool have_input_dir = !opts.input_dir.empty();
    const bool have_input_file = !opts.input_file.empty();
    if (have_input_dir == have_input_file) {
        throw std::runtime_error("provide exactly one of --input-dir or --input-file");
    }
    return opts;
}

std::vector<fs::path> CollectRecoFiles(const fs::path& dir) {
    std::vector<fs::path> files;
    if (!fs::exists(dir)) {
        return files;
    }
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const fs::path path = entry.path();
        if (path.extension() != ".root") {
            continue;
        }
        if (path.filename().string().find("_reco.root") == std::string::npos) {
            continue;
        }
        files.push_back(path);
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::string ReadBackendName(TFile& file) {
    if (auto* named = dynamic_cast<TNamed*>(file.Get("ProtonRecoBackend"))) {
        return named->GetTitle();
    }
    return "unknown";
}

std::int64_t QuantizeTruth(double value) {
    return static_cast<std::int64_t>(std::llround(value * kTruthQuantizationScale));
}

TruthKey MakeTruthKey(const EventRecord& record) {
    TruthKey key;
    key.backend = record.backend;
    key.truth_px_q = QuantizeTruth(record.truth_px);
    key.truth_py_q = QuantizeTruth(record.truth_py);
    key.truth_pz_q = QuantizeTruth(record.truth_pz);
    return key;
}

void AddEventRecord(const EventRecord& record, Summary* summary) {
    if (!summary) {
        return;
    }
    summary->event_records.push_back(record);

    PointSummary& point = summary->point_summaries[MakeTruthKey(record)];
    point.backend = record.backend;
    point.truth_px = record.truth_px;
    point.truth_py = record.truth_py;
    point.truth_pz = record.truth_pz;
    ++point.truth_events;

    if (!record.matched) {
        return;
    }

    ++point.matched_events;
    point.err_px.Add(record.dpx);
    point.err_py.Add(record.dpy);
    point.err_pz.Add(record.dpz);
}

EventRecord MakeEventRecord(const std::string& backend, const fs::path& source_file, long long event_index) {
    EventRecord record;
    record.backend = backend;
    record.source_file = source_file.string();
    record.event_index = event_index;
    return record;
}

void ProcessFile(const fs::path& file_path, Summary* summary) {
    if (!summary) {
        return;
    }

    TFile fin(file_path.c_str(), "READ");
    if (fin.IsZombie()) {
        std::cerr << "[" << kLogTag << "] warning: cannot open " << file_path << "\n";
        return;
    }
    TTree* tree = nullptr;
    fin.GetObject("recoTree", tree);
    if (!tree) {
        std::cerr << "[" << kLogTag << "] warning: recoTree not found in " << file_path << "\n";
        return;
    }

    const std::string backend = ReadBackendName(fin);

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

    const Long64_t n_entries = tree->GetEntries();
    summary->events_total += n_entries;

    for (Long64_t i = 0; i < n_entries; ++i) {
        tree->GetEntry(i);
        if (!truth_has_proton || truth_proton_p4 == nullptr) {
            continue;
        }
        ++summary->events_with_truth_proton;

        EventRecord record = MakeEventRecord(backend, file_path, i);
        record.truth_px = truth_proton_p4->Px();
        record.truth_py = truth_proton_p4->Py();
        record.truth_pz = truth_proton_p4->Pz();

        const std::size_t count_px = reco_proton_px ? reco_proton_px->size() : 0U;
        const std::size_t count_py = reco_proton_py ? reco_proton_py->size() : 0U;
        const std::size_t count_pz = reco_proton_pz ? reco_proton_pz->size() : 0U;
        const std::size_t n_reco = std::min({count_px, count_py, count_pz});
        if (n_reco > 0U) {
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

            record.reco_px = reco_proton_px->at(best_idx);
            record.reco_py = reco_proton_py->at(best_idx);
            record.reco_pz = reco_proton_pz->at(best_idx);
            record.dpx = record.reco_px - record.truth_px;
            record.dpy = record.reco_py - record.truth_py;
            record.dpz = record.reco_pz - record.truth_pz;
            record.matched = true;

            summary->err_px.Add(record.dpx);
            summary->err_py.Add(record.dpy);
            summary->err_pz.Add(record.dpz);
            summary->err_pmag.Add(
                std::sqrt(record.reco_px * record.reco_px + record.reco_py * record.reco_py + record.reco_pz * record.reco_pz) -
                truth_p
            );
            ++summary->matched_events;
        }

        AddEventRecord(record, summary);
    }
}

void PrintMetric(const std::string& prefix, const char* name, const MetricAccumulator& acc) {
    std::cout << prefix << " " << name
              << " count=" << acc.Count()
              << " MAE=" << FormatDouble(acc.MeanAbs())
              << " RMSE=" << FormatDouble(acc.RMSE())
              << " Bias=" << FormatDouble(acc.Bias())
              << " P95Abs=" << FormatDouble(acc.P95Abs())
              << "\n";
}

void EnsureParentDirectory(const fs::path& path) {
    if (path.empty() || path.parent_path().empty()) {
        return;
    }
    fs::create_directories(path.parent_path());
}

int PassThreshold(const MetricAccumulator& acc, long long matched_events, double threshold) {
    if (matched_events <= 0) {
        return 0;
    }
    if (!(std::isfinite(threshold) && threshold > 0.0)) {
        return 1;
    }
    const double mae = acc.MeanAbs();
    return std::isfinite(mae) && mae <= threshold ? 1 : 0;
}

void WriteEventCsv(const fs::path& path, const std::vector<EventRecord>& records) {
    EnsureParentDirectory(path);
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("failed to open event csv for writing: " + path.string());
    }

    out << "backend,source_file,event_index,truth_px,truth_py,truth_pz,reco_px,reco_py,reco_pz,dpx,dpy,dpz,matched\n";
    for (const auto& record : records) {
        out << record.backend << ','
            << '"' << record.source_file << '"' << ','
            << record.event_index << ','
            << FormatDouble(record.truth_px) << ','
            << FormatDouble(record.truth_py) << ','
            << FormatDouble(record.truth_pz) << ','
            << FormatDouble(record.reco_px) << ','
            << FormatDouble(record.reco_py) << ','
            << FormatDouble(record.reco_pz) << ','
            << FormatDouble(record.dpx) << ','
            << FormatDouble(record.dpy) << ','
            << FormatDouble(record.dpz) << ','
            << (record.matched ? 1 : 0) << '\n';
    }
}

void WriteSummaryCsv(const fs::path& path, const Summary& summary, const CliOptions& opts) {
    EnsureParentDirectory(path);
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("failed to open summary csv for writing: " + path.string());
    }

    out << "backend,truth_px,truth_py,truth_pz,truth_events,matched_events,bias_px,bias_py,bias_pz,mae_px,mae_py,mae_pz,rmse_px,rmse_py,rmse_pz,pass_px,pass_py,pass_pz,gate_pass\n";
    for (const auto& [key, point] : summary.point_summaries) {
        (void)key;
        const int pass_px = PassThreshold(point.err_px, point.matched_events, opts.threshold_px);
        const int pass_py = PassThreshold(point.err_py, point.matched_events, opts.threshold_py);
        const int pass_pz = PassThreshold(point.err_pz, point.matched_events, opts.threshold_pz);
        const int gate_pass = (point.matched_events > 0 && pass_px != 0 && pass_py != 0 && pass_pz != 0) ? 1 : 0;

        out << point.backend << ','
            << FormatDouble(point.truth_px) << ','
            << FormatDouble(point.truth_py) << ','
            << FormatDouble(point.truth_pz) << ','
            << point.truth_events << ','
            << point.matched_events << ','
            << FormatDouble(point.err_px.Bias()) << ','
            << FormatDouble(point.err_py.Bias()) << ','
            << FormatDouble(point.err_pz.Bias()) << ','
            << FormatDouble(point.err_px.MeanAbs()) << ','
            << FormatDouble(point.err_py.MeanAbs()) << ','
            << FormatDouble(point.err_pz.MeanAbs()) << ','
            << FormatDouble(point.err_px.RMSE()) << ','
            << FormatDouble(point.err_py.RMSE()) << ','
            << FormatDouble(point.err_pz.RMSE()) << ','
            << pass_px << ','
            << pass_py << ','
            << pass_pz << ','
            << gate_pass << '\n';
    }
}

void PrintPointSummary(const Summary& summary, const CliOptions& opts) {
    const std::string prefix = std::string("[") + kLogTag + "]";
    for (const auto& [key, point] : summary.point_summaries) {
        (void)key;
        const int pass_px = PassThreshold(point.err_px, point.matched_events, opts.threshold_px);
        const int pass_py = PassThreshold(point.err_py, point.matched_events, opts.threshold_py);
        const int pass_pz = PassThreshold(point.err_pz, point.matched_events, opts.threshold_pz);
        const int gate_pass = (point.matched_events > 0 && pass_px != 0 && pass_py != 0 && pass_pz != 0) ? 1 : 0;

        std::cout << prefix
                  << " point backend=" << point.backend
                  << " truth=(" << FormatDouble(point.truth_px, 3)
                  << "," << FormatDouble(point.truth_py, 3)
                  << "," << FormatDouble(point.truth_pz, 3) << ")"
                  << " matched=" << point.matched_events << "/" << point.truth_events
                  << " dpx=" << FormatDouble(point.err_px.Bias(), 3)
                  << " dpy=" << FormatDouble(point.err_py.Bias(), 3)
                  << " dpz=" << FormatDouble(point.err_pz.Bias(), 3)
                  << " gate=" << gate_pass
                  << "\n";
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const CliOptions opts = ParseArgs(argc, argv);

        std::vector<fs::path> files;
        if (!opts.input_file.empty()) {
            const fs::path input_file(opts.input_file);
            if (!fs::exists(input_file)) {
                throw std::runtime_error("input-file does not exist: " + input_file.string());
            }
            files.push_back(input_file);
        } else {
            const fs::path input_dir(opts.input_dir);
            if (!fs::exists(input_dir)) {
                throw std::runtime_error("input-dir does not exist: " + input_dir.string());
            }
            files = CollectRecoFiles(input_dir);
            if (files.empty()) {
                throw std::runtime_error("no *_reco.root files found under: " + input_dir.string());
            }
        }

        Summary summary;
        summary.files = static_cast<long long>(files.size());
        for (const auto& file : files) {
            ProcessFile(file, &summary);
        }

        if (summary.events_with_truth_proton <= 0) {
            throw std::runtime_error("no truth proton events found in selected reco files");
        }

        const std::string prefix = std::string("[") + kLogTag + "]";
        std::cout << prefix << " files=" << summary.files << "\n";
        std::cout << prefix << " events_total=" << summary.events_total << "\n";
        std::cout << prefix << " events_with_truth_proton=" << summary.events_with_truth_proton << "\n";
        std::cout << prefix << " events_with_reco_proton=" << summary.events_with_reco_proton << "\n";
        std::cout << prefix << " matched_events=" << summary.matched_events << "\n";

        const double denom_truth = summary.events_with_truth_proton > 0
            ? static_cast<double>(summary.events_with_truth_proton)
            : 1.0;
        std::cout << prefix << " reco_efficiency_vs_truth="
                  << FormatDouble(summary.events_with_reco_proton * 100.0 / denom_truth, 3) << "%\n";

        PrintMetric(prefix, "error_px", summary.err_px);
        PrintMetric(prefix, "error_py", summary.err_py);
        PrintMetric(prefix, "error_pz", summary.err_pz);
        PrintMetric(prefix, "error_|p|", summary.err_pmag);
        PrintPointSummary(summary, opts);

        if (!opts.event_csv.empty()) {
            WriteEventCsv(opts.event_csv, summary.event_records);
            std::cout << prefix << " event_csv=" << opts.event_csv << "\n";
        }
        if (!opts.summary_csv.empty()) {
            WriteSummaryCsv(opts.summary_csv, summary, opts);
            std::cout << prefix << " summary_csv=" << opts.summary_csv << "\n";
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[" << kLogTag << "] failed: " << ex.what() << "\n";
        return 1;
    }
}
