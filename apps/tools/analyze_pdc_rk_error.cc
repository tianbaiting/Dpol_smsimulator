#include "GeometryManager.hh"
#include "MagneticField.hh"
#include "PDCErrorAnalysis.hh"
#include "PDCRecoRuntime.hh"
#include "RecoEvent.hh"
#include "SMLogger.hh"

#include "TCanvas.h"
#include "TFile.h"
#include "TGraph.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TLatex.h"
#include "TLegend.h"
#include "TLorentzVector.h"
#include "TROOT.h"
#include "TStyle.h"
#include "TTree.h"
#include "TVector3.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace reco = analysis::pdc::anaroot_like;

namespace {

constexpr const char* kLogTag = "analyze_pdc_rk_error";

struct CliOptions {
    std::string input_dir;
    std::string input_file;
    std::string output_dir;
    std::string geometry_macro;
    std::string magnetic_field_map;
    double magnet_rotation_deg = 30.0;
    double pdc_sigma_u_mm = 2.0;
    double pdc_sigma_v_mm = 2.0;
    double pdc_uv_correlation = 0.0;
    double pdc_angle_deg = 57.0;
    double target_sigma_xy_mm = 5.0;
    bool momentum_prior_enabled = false;
    double momentum_prior_center_mev_c = 0.0;
    double momentum_prior_sigma_mev_c = 0.0;
    double p_min_mevc = 50.0;
    double p_max_mevc = 5000.0;
    double tolerance_mm = 5.0;
    double rk_step_mm = 5.0;
    double center_brho_tm = 7.2751;
    int max_iterations = 40;
    int max_events_per_file = 0;
    int profile_points = 17;
    int profile_per_quartile = 24;
    int mcmc_per_quartile = 8;
    int mcmc_n_samples = 160;
    int mcmc_burn_in = 80;
    int mcmc_thin = 2;
    unsigned int sample_seed = 20260410U;
    reco::RkFitMode rk_fit_mode = reco::RkFitMode::kThreePointFree;
};

struct TrackTruthInfo {
    bool available = false;
    TLorentzVector p4{0.0, 0.0, 0.0, 0.0};
};

struct TrackCandidate {
    int file_index = -1;
    long long event_index = -1;
    int track_index = -1;
    reco::PDCInputTrack track;
    TLorentzVector central_p4{0.0, 0.0, 0.0, 0.0};
    double chi2_raw = std::numeric_limits<double>::quiet_NaN();
    double chi2_reduced = std::numeric_limits<double>::quiet_NaN();
    int ndf = 0;
    int iterations = 0;
    reco::IntervalEstimate fisher_px;
    reco::IntervalEstimate fisher_py;
    reco::IntervalEstimate fisher_pz;
    reco::IntervalEstimate fisher_p;
    reco::IntervalEstimate laplace_px;
    reco::IntervalEstimate laplace_py;
    reco::IntervalEstimate laplace_pz;
    reco::IntervalEstimate laplace_p;
    TrackTruthInfo truth;
};

struct ProfileSampleResult {
    int candidate_index = -1;
    int quartile = -1;
    reco::IntervalEstimate px;
    reco::IntervalEstimate py;
    reco::IntervalEstimate pz;
    reco::IntervalEstimate p;
    std::array<reco::ProfileLikelihoodResult, 5> parameter_profiles;
};

struct BayesianSampleResult {
    int candidate_index = -1;
    int quartile = -1;
    reco::IntervalEstimate laplace_px;
    reco::IntervalEstimate laplace_py;
    reco::IntervalEstimate laplace_pz;
    reco::IntervalEstimate laplace_p;
    reco::IntervalEstimate mcmc_px;
    reco::IntervalEstimate mcmc_py;
    reco::IntervalEstimate mcmc_pz;
    reco::IntervalEstimate mcmc_p;
    double acceptance_rate = std::numeric_limits<double>::quiet_NaN();
    double effective_sample_size = std::numeric_limits<double>::quiet_NaN();
    double geweke_z_score = std::numeric_limits<double>::quiet_NaN();
    bool converged = false;
};

struct EventKey {
    int file_index = -1;
    long long event_index = -1;

    bool operator<(const EventKey& other) const {
        return std::tie(file_index, event_index) < std::tie(other.file_index, other.event_index);
    }
};

struct CoverageSummary {
    int count = 0;
    int cover68 = 0;
    int cover95 = 0;
};

std::string FormatDouble(double value, int precision = 6) {
    if (!std::isfinite(value)) {
        return "nan";
    }
    std::ostringstream os;
    os << std::fixed << std::setprecision(precision) << value;
    return os.str();
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

unsigned int ParseUnsigned(const std::string& text, const char* key) {
    try {
        return static_cast<unsigned int>(std::stoul(text));
    } catch (...) {
        throw std::runtime_error(std::string("invalid unsigned value for ") + key + ": " + text);
    }
}

void PrintUsage(const char* argv0) {
    std::cout
        << "Usage: " << argv0
        << " --input-file FILE|--input-dir DIR --output-dir DIR"
        << " --geometry-macro FILE --magnetic-field-map FILE"
        << " [--magnet-rotation-deg DEG] [--rk-fit-mode NAME]"
        << " [--max-events-per-file N] [--profile-points N]"
        << " [--profile-per-quartile N] [--mcmc-per-quartile N]"
        << " [--mcmc-n-samples N] [--mcmc-burn-in N] [--mcmc-thin N]"
        << " [--sample-seed N]\n";
}

CliOptions ParseArgs(int argc, char* argv[]) {
    CliOptions opts;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--input-dir" && i + 1 < argc) {
            opts.input_dir = argv[++i];
        } else if (arg == "--input-file" && i + 1 < argc) {
            opts.input_file = argv[++i];
        } else if (arg == "--output-dir" && i + 1 < argc) {
            opts.output_dir = argv[++i];
        } else if (arg == "--geometry-macro" && i + 1 < argc) {
            opts.geometry_macro = argv[++i];
        } else if (arg == "--magnetic-field-map" && i + 1 < argc) {
            opts.magnetic_field_map = argv[++i];
        } else if (arg == "--magnet-rotation-deg" && i + 1 < argc) {
            opts.magnet_rotation_deg = ParseDouble(argv[++i], "--magnet-rotation-deg");
        } else if (arg == "--pdc-sigma-u-mm" && i + 1 < argc) {
            opts.pdc_sigma_u_mm = ParseDouble(argv[++i], "--pdc-sigma-u-mm");
        } else if (arg == "--pdc-sigma-v-mm" && i + 1 < argc) {
            opts.pdc_sigma_v_mm = ParseDouble(argv[++i], "--pdc-sigma-v-mm");
        } else if (arg == "--pdc-uv-correlation" && i + 1 < argc) {
            opts.pdc_uv_correlation = ParseDouble(argv[++i], "--pdc-uv-correlation");
        } else if (arg == "--pdc-angle-deg" && i + 1 < argc) {
            opts.pdc_angle_deg = ParseDouble(argv[++i], "--pdc-angle-deg");
        } else if (arg == "--momentum-prior-mev-c" && i + 1 < argc) {
            opts.momentum_prior_center_mev_c = ParseDouble(argv[++i], "--momentum-prior-mev-c");
            opts.momentum_prior_enabled = true;
        } else if (arg == "--momentum-prior-sigma-mev-c" && i + 1 < argc) {
            opts.momentum_prior_sigma_mev_c = ParseDouble(argv[++i], "--momentum-prior-sigma-mev-c");
            opts.momentum_prior_enabled = true;
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
        } else if (arg == "--max-iterations" && i + 1 < argc) {
            opts.max_iterations = ParseInt(argv[++i], "--max-iterations");
        } else if (arg == "--max-events-per-file" && i + 1 < argc) {
            opts.max_events_per_file = ParseInt(argv[++i], "--max-events-per-file");
        } else if (arg == "--profile-points" && i + 1 < argc) {
            opts.profile_points = ParseInt(argv[++i], "--profile-points");
        } else if (arg == "--profile-per-quartile" && i + 1 < argc) {
            opts.profile_per_quartile = ParseInt(argv[++i], "--profile-per-quartile");
        } else if (arg == "--mcmc-per-quartile" && i + 1 < argc) {
            opts.mcmc_per_quartile = ParseInt(argv[++i], "--mcmc-per-quartile");
        } else if (arg == "--mcmc-n-samples" && i + 1 < argc) {
            opts.mcmc_n_samples = ParseInt(argv[++i], "--mcmc-n-samples");
        } else if (arg == "--mcmc-burn-in" && i + 1 < argc) {
            opts.mcmc_burn_in = ParseInt(argv[++i], "--mcmc-burn-in");
        } else if (arg == "--mcmc-thin" && i + 1 < argc) {
            opts.mcmc_thin = ParseInt(argv[++i], "--mcmc-thin");
        } else if (arg == "--sample-seed" && i + 1 < argc) {
            opts.sample_seed = ParseUnsigned(argv[++i], "--sample-seed");
        } else if (arg == "--rk-fit-mode" && i + 1 < argc) {
            opts.rk_fit_mode = reco::ParseRkFitMode(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            std::exit(0);
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }

    const bool have_file = !opts.input_file.empty();
    const bool have_dir = !opts.input_dir.empty();
    if (have_file == have_dir) {
        throw std::runtime_error("provide exactly one of --input-file or --input-dir");
    }
    if (opts.output_dir.empty() || opts.geometry_macro.empty() || opts.magnetic_field_map.empty()) {
        throw std::runtime_error("missing required arguments");
    }
    if (opts.max_iterations <= 0 || opts.profile_points <= 0 || opts.profile_per_quartile < 0 ||
        opts.mcmc_per_quartile < 0 || opts.mcmc_n_samples <= 0 || opts.mcmc_burn_in < 0 ||
        opts.mcmc_thin <= 0) {
        throw std::runtime_error("invalid numeric option");
    }
    return opts;
}

std::vector<fs::path> CollectInputFiles(const CliOptions& opts) {
    std::vector<fs::path> files;
    if (!opts.input_file.empty()) {
        files.push_back(fs::path(opts.input_file));
        return files;
    }

    for (const auto& entry : fs::recursive_directory_iterator(opts.input_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".root") {
            continue;
        }
        files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    return files;
}

double Quantile(std::vector<double> values, double q) {
    if (values.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    std::sort(values.begin(), values.end());
    if (values.size() == 1U) {
        return values.front();
    }
    const double index = std::clamp(q, 0.0, 1.0) * static_cast<double>(values.size() - 1U);
    const std::size_t lower = static_cast<std::size_t>(std::floor(index));
    const std::size_t upper = static_cast<std::size_t>(std::ceil(index));
    const double fraction = index - static_cast<double>(lower);
    return values[lower] * (1.0 - fraction) + values[upper] * fraction;
}

double IntervalWidth68(const reco::IntervalEstimate& interval) {
    return interval.valid ? (interval.upper68 - interval.lower68)
                          : std::numeric_limits<double>::quiet_NaN();
}

bool ContainsValue68(const reco::IntervalEstimate& interval, double value) {
    return interval.valid && std::isfinite(value) &&
           value >= interval.lower68 && value <= interval.upper68;
}

bool ContainsValue95(const reco::IntervalEstimate& interval, double value) {
    return interval.valid && std::isfinite(value) &&
           value >= interval.lower95 && value <= interval.upper95;
}

std::string CsvEscape(const std::string& text) {
    std::string escaped = "\"";
    for (char ch : text) {
        if (ch == '"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(ch);
        }
    }
    escaped.push_back('"');
    return escaped;
}

reco::RuntimeOptions BuildRuntimeOptions(const CliOptions& opts) {
    reco::RuntimeOptions runtime_options;
    runtime_options.backend = reco::RuntimeBackend::kRungeKutta;
    runtime_options.pdc_sigma_u_mm = opts.pdc_sigma_u_mm;
    runtime_options.pdc_sigma_v_mm = opts.pdc_sigma_v_mm;
    runtime_options.pdc_uv_correlation = opts.pdc_uv_correlation;
    runtime_options.pdc_angle_deg = opts.pdc_angle_deg;
    runtime_options.target_sigma_xy_mm = opts.target_sigma_xy_mm;
    runtime_options.momentum_prior_enabled = opts.momentum_prior_enabled;
    runtime_options.momentum_prior_center_mev_c = opts.momentum_prior_center_mev_c;
    runtime_options.momentum_prior_sigma_mev_c = opts.momentum_prior_sigma_mev_c;
    runtime_options.p_min_mevc = opts.p_min_mevc;
    runtime_options.p_max_mevc = opts.p_max_mevc;
    runtime_options.initial_p_mevc = 1000.0;
    runtime_options.tolerance_mm = opts.tolerance_mm;
    runtime_options.max_iterations = opts.max_iterations;
    runtime_options.rk_step_mm = opts.rk_step_mm;
    runtime_options.center_brho_tm = opts.center_brho_tm;
    runtime_options.magnetic_field_rotation_deg = opts.magnet_rotation_deg;
    runtime_options.rk_fit_mode = opts.rk_fit_mode;
    runtime_options.compute_uncertainty = true;
    runtime_options.compute_posterior_laplace = true;
    return runtime_options;
}

TrackTruthInfo ExtractTruth(bool have_runtime_truth,
                            bool runtime_truth_has_proton,
                            TLorentzVector* runtime_truth_p4) {
    TrackTruthInfo truth;
    if (have_runtime_truth && runtime_truth_has_proton && runtime_truth_p4 != nullptr) {
        truth.available = true;
        truth.p4 = *runtime_truth_p4;
    }
    return truth;
}

void WriteTrackSummaryCsv(const fs::path& path,
                          const std::vector<fs::path>& files,
                          const std::vector<TrackCandidate>& candidates) {
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("failed to open track summary csv: " + path.string());
    }

    out << "source_file,event_index,track_index,truth_available,truth_px,truth_py,truth_pz,"
           "reco_px,reco_py,reco_pz,reco_p,chi2_raw,chi2_reduced,ndf,iterations,"
           "fisher_px_sigma,fisher_py_sigma,fisher_pz_sigma,fisher_p_sigma,"
           "laplace_px_lower68,laplace_px_upper68,laplace_py_lower68,laplace_py_upper68,"
           "laplace_pz_lower68,laplace_pz_upper68,laplace_p_lower68,laplace_p_upper68\n";
    for (const TrackCandidate& candidate : candidates) {
        out << CsvEscape(files[static_cast<std::size_t>(candidate.file_index)].string()) << ','
            << candidate.event_index << ','
            << candidate.track_index << ','
            << (candidate.truth.available ? 1 : 0) << ','
            << FormatDouble(candidate.truth.p4.Px()) << ','
            << FormatDouble(candidate.truth.p4.Py()) << ','
            << FormatDouble(candidate.truth.p4.Pz()) << ','
            << FormatDouble(candidate.central_p4.Px()) << ','
            << FormatDouble(candidate.central_p4.Py()) << ','
            << FormatDouble(candidate.central_p4.Pz()) << ','
            << FormatDouble(candidate.central_p4.P()) << ','
            << FormatDouble(candidate.chi2_raw) << ','
            << FormatDouble(candidate.chi2_reduced) << ','
            << candidate.ndf << ','
            << candidate.iterations << ','
            << FormatDouble(candidate.fisher_px.sigma) << ','
            << FormatDouble(candidate.fisher_py.sigma) << ','
            << FormatDouble(candidate.fisher_pz.sigma) << ','
            << FormatDouble(candidate.fisher_p.sigma) << ','
            << FormatDouble(candidate.laplace_px.lower68) << ','
            << FormatDouble(candidate.laplace_px.upper68) << ','
            << FormatDouble(candidate.laplace_py.lower68) << ','
            << FormatDouble(candidate.laplace_py.upper68) << ','
            << FormatDouble(candidate.laplace_pz.lower68) << ','
            << FormatDouble(candidate.laplace_pz.upper68) << ','
            << FormatDouble(candidate.laplace_p.lower68) << ','
            << FormatDouble(candidate.laplace_p.upper68) << '\n';
    }
}

void WriteProfileSampleCsv(const fs::path& path,
                           const std::vector<fs::path>& files,
                           const std::vector<TrackCandidate>& candidates,
                           const std::vector<ProfileSampleResult>& results) {
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("failed to open profile sample csv: " + path.string());
    }

    out << "source_file,event_index,track_index,quartile,reco_p,"
           "profile_px_lower68,profile_px_upper68,profile_py_lower68,profile_py_upper68,"
           "profile_pz_lower68,profile_pz_upper68,profile_p_lower68,profile_p_upper68,"
           "profile_px_lower95,profile_px_upper95,profile_py_lower95,profile_py_upper95,"
           "profile_pz_lower95,profile_pz_upper95,profile_p_lower95,profile_p_upper95\n";
    for (const ProfileSampleResult& result : results) {
        const TrackCandidate& candidate = candidates[static_cast<std::size_t>(result.candidate_index)];
        out << CsvEscape(files[static_cast<std::size_t>(candidate.file_index)].string()) << ','
            << candidate.event_index << ','
            << candidate.track_index << ','
            << result.quartile << ','
            << FormatDouble(candidate.central_p4.P()) << ','
            << FormatDouble(result.px.lower68) << ','
            << FormatDouble(result.px.upper68) << ','
            << FormatDouble(result.py.lower68) << ','
            << FormatDouble(result.py.upper68) << ','
            << FormatDouble(result.pz.lower68) << ','
            << FormatDouble(result.pz.upper68) << ','
            << FormatDouble(result.p.lower68) << ','
            << FormatDouble(result.p.upper68) << ','
            << FormatDouble(result.px.lower95) << ','
            << FormatDouble(result.px.upper95) << ','
            << FormatDouble(result.py.lower95) << ','
            << FormatDouble(result.py.upper95) << ','
            << FormatDouble(result.pz.lower95) << ','
            << FormatDouble(result.pz.upper95) << ','
            << FormatDouble(result.p.lower95) << ','
            << FormatDouble(result.p.upper95) << '\n';
    }
}

void WriteBayesianSampleCsv(const fs::path& path,
                            const std::vector<fs::path>& files,
                            const std::vector<TrackCandidate>& candidates,
                            const std::vector<BayesianSampleResult>& results) {
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("failed to open bayesian sample csv: " + path.string());
    }

    out << "source_file,event_index,track_index,quartile,reco_p,"
           "laplace_p_lower68,laplace_p_upper68,mcmc_p_lower68,mcmc_p_upper68,"
           "laplace_p_lower95,laplace_p_upper95,mcmc_p_lower95,mcmc_p_upper95,"
           "acceptance_rate,effective_sample_size,geweke_z_score,converged\n";
    for (const BayesianSampleResult& result : results) {
        const TrackCandidate& candidate = candidates[static_cast<std::size_t>(result.candidate_index)];
        out << CsvEscape(files[static_cast<std::size_t>(candidate.file_index)].string()) << ','
            << candidate.event_index << ','
            << candidate.track_index << ','
            << result.quartile << ','
            << FormatDouble(candidate.central_p4.P()) << ','
            << FormatDouble(result.laplace_p.lower68) << ','
            << FormatDouble(result.laplace_p.upper68) << ','
            << FormatDouble(result.mcmc_p.lower68) << ','
            << FormatDouble(result.mcmc_p.upper68) << ','
            << FormatDouble(result.laplace_p.lower95) << ','
            << FormatDouble(result.laplace_p.upper95) << ','
            << FormatDouble(result.mcmc_p.lower95) << ','
            << FormatDouble(result.mcmc_p.upper95) << ','
            << FormatDouble(result.acceptance_rate) << ','
            << FormatDouble(result.effective_sample_size) << ','
            << FormatDouble(result.geweke_z_score) << ','
            << (result.converged ? 1 : 0) << '\n';
    }
}

void WriteValidationCsv(const fs::path& path,
                        const CoverageSummary& fisher_p,
                        const CoverageSummary& laplace_p,
                        const CoverageSummary& profile_p,
                        const CoverageSummary& mcmc_p) {
    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("failed to open validation csv: " + path.string());
    }

    out << "method,count,cover68,cover95\n";
    out << "fisher_p," << fisher_p.count << ',' << fisher_p.cover68 << ',' << fisher_p.cover95 << '\n';
    out << "laplace_p," << laplace_p.count << ',' << laplace_p.cover68 << ',' << laplace_p.cover95 << '\n';
    out << "profile_p," << profile_p.count << ',' << profile_p.cover68 << ',' << profile_p.cover95 << '\n';
    out << "mcmc_p," << mcmc_p.count << ',' << mcmc_p.cover68 << ',' << mcmc_p.cover95 << '\n';
}

void WriteSummaryCsv(const fs::path& path,
                     const std::vector<TrackCandidate>& candidates,
                     const std::vector<ProfileSampleResult>& profile_results,
                     const std::vector<BayesianSampleResult>& bayesian_results) {
    std::vector<double> p_values;
    std::vector<double> fisher_widths;
    std::vector<double> laplace_widths;
    std::vector<double> profile_widths;
    std::vector<double> mcmc_widths;
    std::vector<double> acceptance_rates;
    std::vector<double> ess_values;
    p_values.reserve(candidates.size());
    fisher_widths.reserve(candidates.size());
    laplace_widths.reserve(candidates.size());
    for (const TrackCandidate& candidate : candidates) {
        p_values.push_back(candidate.central_p4.P());
        fisher_widths.push_back(IntervalWidth68(candidate.fisher_p));
        laplace_widths.push_back(IntervalWidth68(candidate.laplace_p));
    }
    for (const ProfileSampleResult& result : profile_results) {
        profile_widths.push_back(IntervalWidth68(result.p));
    }
    for (const BayesianSampleResult& result : bayesian_results) {
        mcmc_widths.push_back(IntervalWidth68(result.mcmc_p));
        acceptance_rates.push_back(result.acceptance_rate);
        ess_values.push_back(result.effective_sample_size);
    }

    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("failed to open summary csv: " + path.string());
    }
    out << "metric,value\n";
    out << "successful_tracks," << candidates.size() << '\n';
    out << "median_reco_p," << FormatDouble(Quantile(p_values, 0.5)) << '\n';
    out << "p_q25," << FormatDouble(Quantile(p_values, 0.25)) << '\n';
    out << "p_q75," << FormatDouble(Quantile(p_values, 0.75)) << '\n';
    out << "median_fisher_p_width68," << FormatDouble(Quantile(fisher_widths, 0.5)) << '\n';
    out << "median_laplace_p_width68," << FormatDouble(Quantile(laplace_widths, 0.5)) << '\n';
    out << "profile_sample_count," << profile_results.size() << '\n';
    out << "median_profile_p_width68," << FormatDouble(Quantile(profile_widths, 0.5)) << '\n';
    out << "mcmc_sample_count," << bayesian_results.size() << '\n';
    out << "median_mcmc_p_width68," << FormatDouble(Quantile(mcmc_widths, 0.5)) << '\n';
    out << "median_mcmc_acceptance_rate," << FormatDouble(Quantile(acceptance_rates, 0.5)) << '\n';
    out << "median_mcmc_effective_sample_size," << FormatDouble(Quantile(ess_values, 0.5)) << '\n';
}

void SaveHistogramPng(TH1* histogram,
                      const fs::path& path,
                      const char* draw_option = "") {
    TCanvas canvas("canvas", "canvas", 1100, 800);
    histogram->SetLineWidth(2);
    histogram->Draw(draw_option);
    canvas.SaveAs(path.c_str());
}

void SaveRepresentativeProfilePlot(const fs::path& path,
                                   const TrackCandidate& candidate,
                                   const std::array<reco::ProfileLikelihoodResult, 5>& profiles) {
    TCanvas canvas("profile_canvas", "profile_canvas", 1400, 900);
    canvas.Divide(3, 2);

    int pad_index = 1;
    for (const reco::ProfileLikelihoodResult& profile : profiles) {
        if (!profile.valid || profile.scan_values.empty() || profile.scan_chi2.empty()) {
            continue;
        }

        std::vector<double> delta_chi2(profile.scan_chi2.size(), std::numeric_limits<double>::quiet_NaN());
        for (std::size_t i = 0; i < profile.scan_chi2.size(); ++i) {
            if (std::isfinite(profile.scan_chi2[i])) {
                delta_chi2[i] = profile.scan_chi2[i] - profile.chi2_min;
            }
        }

        canvas.cd(pad_index++);
        TGraph graph(static_cast<int>(profile.scan_values.size()));
        for (std::size_t i = 0; i < profile.scan_values.size(); ++i) {
            graph.SetPoint(static_cast<int>(i), profile.scan_values[i], delta_chi2[i]);
        }
        graph.SetLineWidth(2);
        graph.SetMarkerStyle(20);
        graph.SetTitle((profile.parameter_name + ";scan value;#Delta#chi^{2}").c_str());
        graph.Draw("ALP");

        TLatex label;
        label.SetNDC(true);
        label.SetTextSize(0.04);
        label.DrawLatex(0.15, 0.85, Form("event=%lld track=%d |p|=%.1f MeV/c",
                                         candidate.event_index,
                                         candidate.track_index,
                                         candidate.central_p4.P()));
    }

    canvas.SaveAs(path.c_str());
}

std::array<double, 3> QuartileEdges(const std::vector<double>& values) {
    return {
        Quantile(values, 0.25),
        Quantile(values, 0.50),
        Quantile(values, 0.75)
    };
}

int QuartileIndex(double value, const std::array<double, 3>& edges) {
    if (value <= edges[0]) {
        return 0;
    }
    if (value <= edges[1]) {
        return 1;
    }
    if (value <= edges[2]) {
        return 2;
    }
    return 3;
}

void UpdateCoverage(const reco::IntervalEstimate& interval,
                    double truth_value,
                    CoverageSummary* summary) {
    if (!summary || !std::isfinite(truth_value) || !interval.valid) {
        return;
    }
    ++summary->count;
    if (ContainsValue68(interval, truth_value)) {
        ++summary->cover68;
    }
    if (ContainsValue95(interval, truth_value)) {
        ++summary->cover95;
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        gROOT->SetBatch(kTRUE);
        gStyle->SetOptStat(0);
        SMLogger::LogConfig log_config;
        log_config.async = false;
        log_config.console = true;
        log_config.file = false;
        log_config.level = SMLogger::LogLevel::WARN;
        SMLogger::Logger::Instance().Initialize(log_config);

        const CliOptions opts = ParseArgs(argc, argv);
        const std::vector<fs::path> files = CollectInputFiles(opts);
        if (files.empty()) {
            throw std::runtime_error("no input ROOT files found");
        }

        fs::create_directories(opts.output_dir);
        fs::create_directories(fs::path(opts.output_dir) / "plots");

        GeometryManager geometry;
        if (!reco::LoadGeometryFromMacro(geometry, opts.geometry_macro)) {
            throw std::runtime_error("failed to load geometry macro: " + opts.geometry_macro);
        }

        MagneticField magnetic_field;
        if (!reco::LoadMagneticField(magnetic_field, opts.magnetic_field_map, opts.magnet_rotation_deg)) {
            throw std::runtime_error("failed to load magnetic field map: " + opts.magnetic_field_map);
        }

        const reco::RuntimeOptions runtime_options = BuildRuntimeOptions(opts);
        const reco::RecoConfig config = reco::BuildRecoConfig(runtime_options, true);
        const reco::TargetConstraint target = reco::BuildTargetConstraint(geometry, runtime_options);

        reco::PDCMomentumReconstructor reconstructor(&magnetic_field);
        reco::PDCErrorAnalysis error_analysis(&magnetic_field);

        std::vector<TrackCandidate> candidates;
        std::vector<std::vector<int>> candidate_indices_per_file(files.size());
        candidates.reserve(4096);

        for (std::size_t file_index = 0; file_index < files.size(); ++file_index) {
            TFile input(files[file_index].c_str(), "READ");
            if (input.IsZombie()) {
                std::cerr << "[" << kLogTag << "] warning: cannot open " << files[file_index] << "\n";
                continue;
            }

            TTree* tree = nullptr;
            input.GetObject("recoTree", tree);
            if (!tree) {
                std::cerr << "[" << kLogTag << "] warning: recoTree not found in " << files[file_index] << "\n";
                continue;
            }

            RecoEvent* reco_event = nullptr;
            tree->SetBranchAddress("recoEvent", &reco_event);

            bool runtime_truth_has_proton = false;
            TLorentzVector* runtime_truth_p4 = nullptr;
            bool have_runtime_truth =
                tree->GetBranch("truth_has_proton") != nullptr &&
                tree->GetBranch("truth_proton_p4") != nullptr;
            if (have_runtime_truth) {
                tree->SetBranchAddress("truth_has_proton", &runtime_truth_has_proton);
                tree->SetBranchAddress("truth_proton_p4", &runtime_truth_p4);
            }

            const Long64_t total_entries = tree->GetEntries();
            const Long64_t entry_limit =
                (opts.max_events_per_file > 0)
                    ? std::min<Long64_t>(total_entries, opts.max_events_per_file)
                    : total_entries;

            for (Long64_t entry = 0; entry < entry_limit; ++entry) {
                tree->GetEntry(entry);
                if (reco_event == nullptr) {
                    continue;
                }

                const TrackTruthInfo truth = ExtractTruth(have_runtime_truth,
                                                          runtime_truth_has_proton,
                                                          runtime_truth_p4);

                for (std::size_t track_index = 0; track_index < reco_event->tracks.size(); ++track_index) {
                    const RecoTrack& track = reco_event->tracks[track_index];
                    if (track.pdgCode == 2112) {
                        continue;
                    }

                    reco::PDCInputTrack pdc_track;
                    pdc_track.pdc1 = track.start;
                    pdc_track.pdc2 = track.end;

                    const reco::RecoResult reco_result =
                        reconstructor.ReconstructRK(pdc_track, target, config);
                    if (!((reco_result.status == reco::SolverStatus::kSuccess ||
                           reco_result.status == reco::SolverStatus::kNotConverged) &&
                          reco_result.p4_at_target.P() > 0.0)) {
                        continue;
                    }

                    TrackCandidate candidate;
                    candidate.file_index = static_cast<int>(file_index);
                    candidate.event_index = entry;
                    candidate.track_index = static_cast<int>(track_index);
                    candidate.track = pdc_track;
                    candidate.central_p4 = reco_result.p4_at_target;
                    candidate.chi2_raw = reco_result.chi2_raw;
                    candidate.chi2_reduced = reco_result.chi2_reduced;
                    candidate.ndf = reco_result.ndf;
                    candidate.iterations = reco_result.iterations;
                    candidate.fisher_px = reco_result.px_interval;
                    candidate.fisher_py = reco_result.py_interval;
                    candidate.fisher_pz = reco_result.pz_interval;
                    candidate.fisher_p = reco_result.p_interval;
                    candidate.laplace_px = reco_result.px_credible;
                    candidate.laplace_py = reco_result.py_credible;
                    candidate.laplace_pz = reco_result.pz_credible;
                    candidate.laplace_p = reco_result.p_credible;
                    candidate.truth = truth;

                    candidate_indices_per_file[file_index].push_back(static_cast<int>(candidates.size()));
                    candidates.push_back(candidate);
                }
            }
        }

        if (candidates.empty()) {
            throw std::runtime_error("no successful RK candidates found");
        }

        WriteTrackSummaryCsv(fs::path(opts.output_dir) / "track_summary.csv", files, candidates);

        std::vector<double> all_p_values;
        all_p_values.reserve(candidates.size());
        for (const TrackCandidate& candidate : candidates) {
            all_p_values.push_back(candidate.central_p4.P());
        }
        const double global_median_p = Quantile(all_p_values, 0.5);

        std::vector<ProfileSampleResult> profile_results;
        std::vector<BayesianSampleResult> bayesian_results;
        std::map<EventKey, int> best_candidate_for_truth;
        for (int index = 0; index < static_cast<int>(candidates.size()); ++index) {
            const TrackCandidate& candidate = candidates[static_cast<std::size_t>(index)];
            if (!candidate.truth.available) {
                continue;
            }
            const EventKey key{candidate.file_index, candidate.event_index};
            const double delta_p = std::abs(candidate.central_p4.P() - candidate.truth.p4.P());
            auto it = best_candidate_for_truth.find(key);
            if (it == best_candidate_for_truth.end()) {
                best_candidate_for_truth.emplace(key, index);
                continue;
            }
            const TrackCandidate& current_best = candidates[static_cast<std::size_t>(it->second)];
            const double current_delta =
                std::abs(current_best.central_p4.P() - current_best.truth.p4.P());
            if (delta_p < current_delta) {
                it->second = index;
            }
        }

        std::vector<int> profile_indices;
        std::vector<int> mcmc_indices;
        profile_indices.reserve(files.size() * 4U * static_cast<std::size_t>(opts.profile_per_quartile));
        mcmc_indices.reserve(files.size() * 4U * static_cast<std::size_t>(opts.mcmc_per_quartile));

        std::mt19937 sampler_rng(opts.sample_seed);
        for (std::size_t file_index = 0; file_index < files.size(); ++file_index) {
            std::vector<double> p_values;
            p_values.reserve(candidate_indices_per_file[file_index].size());
            for (int candidate_index : candidate_indices_per_file[file_index]) {
                p_values.push_back(candidates[static_cast<std::size_t>(candidate_index)].central_p4.P());
            }
            if (p_values.empty()) {
                continue;
            }

            const std::array<double, 3> edges = QuartileEdges(p_values);
            std::array<std::vector<int>, 4> quartile_bins;
            for (int candidate_index : candidate_indices_per_file[file_index]) {
                const double momentum = candidates[static_cast<std::size_t>(candidate_index)].central_p4.P();
                quartile_bins[static_cast<std::size_t>(QuartileIndex(momentum, edges))].push_back(candidate_index);
            }

            for (std::vector<int>& bin : quartile_bins) {
                std::shuffle(bin.begin(), bin.end(), sampler_rng);
                const int profile_take = std::min<int>(static_cast<int>(bin.size()), opts.profile_per_quartile);
                const int mcmc_take = std::min<int>(static_cast<int>(bin.size()), opts.mcmc_per_quartile);
                profile_indices.insert(profile_indices.end(), bin.begin(), bin.begin() + profile_take);
                mcmc_indices.insert(mcmc_indices.end(), bin.begin(), bin.begin() + mcmc_take);
            }
        }

        reco::MonteCarloConfig mc_config;
        mc_config.n_samples = 0;

        reco::ProfileLikelihoodConfig profile_config;
        profile_config.n_points_per_parameter = opts.profile_points;
        profile_config.step_fraction = 0.25;

        reco::BayesianConfig bayes_profile_only;
        bayes_profile_only.use_laplace = true;
        bayes_profile_only.use_mcmc = false;

        reco::BayesianConfig bayes_with_mcmc;
        bayes_with_mcmc.use_laplace = true;
        bayes_with_mcmc.use_mcmc = true;
        bayes_with_mcmc.mcmc_n_samples = opts.mcmc_n_samples;
        bayes_with_mcmc.mcmc_burn_in = opts.mcmc_burn_in;
        bayes_with_mcmc.mcmc_thin = opts.mcmc_thin;
        bayes_with_mcmc.mcmc_proposal_scale = 0.8;

        std::optional<TrackCandidate> representative_candidate;
        std::optional<std::array<reco::ProfileLikelihoodResult, 5>> representative_profiles;
        double representative_distance = std::numeric_limits<double>::infinity();

        for (int candidate_index : profile_indices) {
            const TrackCandidate& candidate = candidates[static_cast<std::size_t>(candidate_index)];
            const reco::ErrorAnalysisResult result =
                error_analysis.Analyze(candidate.track,
                                       target,
                                       config,
                                       reco::PDCErrorModel{},
                                       mc_config,
                                       profile_config,
                                       bayes_profile_only);
            if (!result.valid || !result.profile_valid) {
                continue;
            }

            ProfileSampleResult sample;
            sample.candidate_index = candidate_index;
            sample.px = result.profile_momentum[0];
            sample.py = result.profile_momentum[1];
            sample.pz = result.profile_momentum[2];
            sample.p = result.profile_momentum[3];
            sample.parameter_profiles = result.parameter_profiles;
            sample.quartile = -1;
            profile_results.push_back(sample);

            const double distance = std::abs(candidate.central_p4.P() - global_median_p);
            if (distance < representative_distance) {
                representative_distance = distance;
                representative_candidate = candidate;
                representative_profiles = result.parameter_profiles;
            }
        }

        for (int candidate_index : mcmc_indices) {
            const TrackCandidate& candidate = candidates[static_cast<std::size_t>(candidate_index)];
            const reco::RecoResult central_fit =
                reconstructor.ReconstructRK(candidate.track, target, config);
            if (!((central_fit.status == reco::SolverStatus::kSuccess ||
                   central_fit.status == reco::SolverStatus::kNotConverged) &&
                  central_fit.p4_at_target.P() > 0.0)) {
                continue;
            }

            const reco::BayesianResult bayesian =
                error_analysis.ComputeBayesianPosterior(candidate.track,
                                                       target,
                                                       config,
                                                       bayes_with_mcmc,
                                                       central_fit);
            if (!bayesian.valid || !bayesian.mcmc_valid) {
                continue;
            }

            BayesianSampleResult sample;
            sample.candidate_index = candidate_index;
            sample.laplace_px = bayesian.laplace_px;
            sample.laplace_py = bayesian.laplace_py;
            sample.laplace_pz = bayesian.laplace_pz;
            sample.laplace_p = bayesian.laplace_p;
            sample.mcmc_px = bayesian.mcmc_px;
            sample.mcmc_py = bayesian.mcmc_py;
            sample.mcmc_pz = bayesian.mcmc_pz;
            sample.mcmc_p = bayesian.mcmc_p;
            sample.acceptance_rate = bayesian.mcmc_acceptance_rate;
            sample.effective_sample_size = bayesian.mcmc_effective_sample_size;
            sample.geweke_z_score = bayesian.geweke_z_score;
            sample.converged = bayesian.converged;
            sample.quartile = -1;
            bayesian_results.push_back(sample);
        }

        std::map<int, int> profile_result_by_candidate;
        for (int i = 0; i < static_cast<int>(profile_results.size()); ++i) {
            profile_result_by_candidate[profile_results[static_cast<std::size_t>(i)].candidate_index] = i;
        }
        std::map<int, int> bayes_result_by_candidate;
        for (int i = 0; i < static_cast<int>(bayesian_results.size()); ++i) {
            bayes_result_by_candidate[bayesian_results[static_cast<std::size_t>(i)].candidate_index] = i;
        }

        for (std::size_t file_index = 0; file_index < files.size(); ++file_index) {
            std::vector<double> p_values;
            p_values.reserve(candidate_indices_per_file[file_index].size());
            for (int candidate_index : candidate_indices_per_file[file_index]) {
                p_values.push_back(candidates[static_cast<std::size_t>(candidate_index)].central_p4.P());
            }
            if (p_values.empty()) {
                continue;
            }
            const std::array<double, 3> edges = QuartileEdges(p_values);
            for (ProfileSampleResult& result : profile_results) {
                const TrackCandidate& candidate = candidates[static_cast<std::size_t>(result.candidate_index)];
                if (candidate.file_index == static_cast<int>(file_index) && result.quartile < 0) {
                    result.quartile = QuartileIndex(candidate.central_p4.P(), edges);
                }
            }
            for (BayesianSampleResult& result : bayesian_results) {
                const TrackCandidate& candidate = candidates[static_cast<std::size_t>(result.candidate_index)];
                if (candidate.file_index == static_cast<int>(file_index) && result.quartile < 0) {
                    result.quartile = QuartileIndex(candidate.central_p4.P(), edges);
                }
            }
        }

        CoverageSummary fisher_p_coverage;
        CoverageSummary laplace_p_coverage;
        CoverageSummary profile_p_coverage;
        CoverageSummary mcmc_p_coverage;
        for (const auto& [key, candidate_index] : best_candidate_for_truth) {
            (void)key;
            const TrackCandidate& candidate = candidates[static_cast<std::size_t>(candidate_index)];
            UpdateCoverage(candidate.fisher_p, candidate.truth.p4.P(), &fisher_p_coverage);
            UpdateCoverage(candidate.laplace_p, candidate.truth.p4.P(), &laplace_p_coverage);

            auto profile_it = profile_result_by_candidate.find(candidate_index);
            if (profile_it != profile_result_by_candidate.end()) {
                UpdateCoverage(profile_results[static_cast<std::size_t>(profile_it->second)].p,
                               candidate.truth.p4.P(),
                               &profile_p_coverage);
            }
            auto bayes_it = bayes_result_by_candidate.find(candidate_index);
            if (bayes_it != bayes_result_by_candidate.end()) {
                UpdateCoverage(bayesian_results[static_cast<std::size_t>(bayes_it->second)].mcmc_p,
                               candidate.truth.p4.P(),
                               &mcmc_p_coverage);
            }
        }

        WriteProfileSampleCsv(fs::path(opts.output_dir) / "profile_samples.csv",
                              files,
                              candidates,
                              profile_results);
        WriteBayesianSampleCsv(fs::path(opts.output_dir) / "bayesian_samples.csv",
                               files,
                               candidates,
                               bayesian_results);
        WriteValidationCsv(fs::path(opts.output_dir) / "validation_summary.csv",
                           fisher_p_coverage,
                           laplace_p_coverage,
                           profile_p_coverage,
                           mcmc_p_coverage);
        WriteSummaryCsv(fs::path(opts.output_dir) / "summary.csv",
                        candidates,
                        profile_results,
                        bayesian_results);

        TH1D h_reco_p("h_reco_p", "RK reconstructed |p|;|p| (MeV/c);Tracks", 120, 0.0, 2400.0);
        TH1D h_fisher_p_width68("h_fisher_p_width68",
                                "Fisher 68% width in |p|;68% width (MeV/c);Tracks",
                                120,
                                0.0,
                                120.0);
        TH1D h_profile_p_width68("h_profile_p_width68",
                                 "Profile-likelihood 68% width in |p|;68% width (MeV/c);Sampled tracks",
                                 120,
                                 0.0,
                                 160.0);
        TH2D h_laplace_vs_mcmc("h_laplace_vs_mcmc",
                               "Laplace vs MCMC 68% width in |p|;Laplace 68% width (MeV/c);MCMC 68% width (MeV/c)",
                               80,
                               0.0,
                               160.0,
                               80,
                               0.0,
                               160.0);

        for (const TrackCandidate& candidate : candidates) {
            h_reco_p.Fill(candidate.central_p4.P());
            const double fisher_width = IntervalWidth68(candidate.fisher_p);
            if (std::isfinite(fisher_width)) {
                h_fisher_p_width68.Fill(fisher_width);
            }
        }
        for (const ProfileSampleResult& result : profile_results) {
            const double width = IntervalWidth68(result.p);
            if (std::isfinite(width)) {
                h_profile_p_width68.Fill(width);
            }
        }
        for (const BayesianSampleResult& result : bayesian_results) {
            const double laplace_width = IntervalWidth68(result.laplace_p);
            const double mcmc_width = IntervalWidth68(result.mcmc_p);
            if (std::isfinite(laplace_width) && std::isfinite(mcmc_width)) {
                h_laplace_vs_mcmc.Fill(laplace_width, mcmc_width);
            }
        }

        SaveHistogramPng(&h_reco_p, fs::path(opts.output_dir) / "plots" / "reco_p_distribution.png");
        SaveHistogramPng(&h_fisher_p_width68,
                         fs::path(opts.output_dir) / "plots" / "fisher_p_width68.png");
        SaveHistogramPng(&h_profile_p_width68,
                         fs::path(opts.output_dir) / "plots" / "profile_p_width68.png");
        SaveHistogramPng(&h_laplace_vs_mcmc,
                         fs::path(opts.output_dir) / "plots" / "laplace_vs_mcmc_p_width68.png",
                         "COLZ");

        if (representative_candidate.has_value() && representative_profiles.has_value()) {
            SaveRepresentativeProfilePlot(fs::path(opts.output_dir) / "plots" / "representative_profile_curves.png",
                                          representative_candidate.value(),
                                          representative_profiles.value());
        }

        std::cout << "[" << kLogTag << "] candidates=" << candidates.size()
                  << " profile_samples=" << profile_results.size()
                  << " bayesian_samples=" << bayesian_results.size()
                  << " output_dir=" << opts.output_dir << "\n";
        SMLogger::Logger::Instance().Shutdown();
        return 0;
    } catch (const std::exception& ex) {
        SMLogger::Logger::Instance().Shutdown();
        std::cerr << "[" << kLogTag << "] error: " << ex.what() << "\n";
        return 1;
    }
}
