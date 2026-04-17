#include "PDCErrorAnalysis.hh"

#include "PDCRkAnalysisInternal.hh"

#include "TMatrixDSym.h"
#include "TMatrixDSymEigen.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace analysis::pdc::anaroot_like {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;

double Percentile(const std::vector<double>& sorted_values, double p) {
    if (sorted_values.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (sorted_values.size() == 1U) {
        return sorted_values.front();
    }
    const double index = detail::Clamp(p, 0.0, 1.0) *
                         static_cast<double>(sorted_values.size() - 1U);
    const std::size_t lower = static_cast<std::size_t>(std::floor(index));
    const std::size_t upper = static_cast<std::size_t>(std::ceil(index));
    if (lower == upper || upper >= sorted_values.size()) {
        return sorted_values[std::min(lower, sorted_values.size() - 1U)];
    }
    const double fraction = index - static_cast<double>(lower);
    return sorted_values[lower] * (1.0 - fraction) +
           sorted_values[upper] * fraction;
}

double MedianAbsoluteDeviation(const std::vector<double>& values, double median) {
    if (values.empty()) {
        return 0.0;
    }
    std::vector<double> abs_deviations;
    abs_deviations.reserve(values.size());
    for (double value : values) {
        abs_deviations.push_back(std::abs(value - median));
    }
    std::sort(abs_deviations.begin(), abs_deviations.end());
    return Percentile(abs_deviations, 0.5);
}

double Autocorrelation(const std::vector<double>& chain, int lag) {
    if (chain.size() < static_cast<std::size_t>(lag + 2)) {
        return 0.0;
    }
    const double mean =
        std::accumulate(chain.begin(), chain.end(), 0.0) /
        static_cast<double>(chain.size());
    double variance = 0.0;
    for (double value : chain) {
        variance += (value - mean) * (value - mean);
    }
    variance /= static_cast<double>(chain.size());
    if (variance < 1.0e-15) {
        return 0.0;
    }

    double autocov = 0.0;
    const std::size_t n = chain.size() - static_cast<std::size_t>(lag);
    for (std::size_t i = 0; i < n; ++i) {
        autocov += (chain[i] - mean) *
                   (chain[i + static_cast<std::size_t>(lag)] - mean);
    }
    autocov /= static_cast<double>(chain.size());
    return autocov / variance;
}

std::pair<double, double> HighestPosteriorDensityInterval(
    const std::vector<double>& sorted_values,
    double mass
) {
    if (sorted_values.empty()) {
        return {std::numeric_limits<double>::quiet_NaN(),
                std::numeric_limits<double>::quiet_NaN()};
    }
    if (sorted_values.size() == 1U) {
        return {sorted_values.front(), sorted_values.front()};
    }

    const std::size_t window = std::max<std::size_t>(
        1U,
        static_cast<std::size_t>(std::floor(detail::Clamp(mass, 0.0, 1.0) *
                                            static_cast<double>(sorted_values.size() - 1U)))
    );
    double best_width = std::numeric_limits<double>::infinity();
    std::size_t best_begin = 0U;
    for (std::size_t begin = 0U; begin + window < sorted_values.size(); ++begin) {
        const double width = sorted_values[begin + window] - sorted_values[begin];
        if (width < best_width) {
            best_width = width;
            best_begin = begin;
        }
    }
    return {sorted_values[best_begin], sorted_values[best_begin + window]};
}

IntervalEstimate BuildEmpiricalInterval(const std::vector<double>& values) {
    IntervalEstimate estimate;
    if (values.empty()) {
        return estimate;
    }

    std::vector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    const double median = Percentile(sorted, 0.5);
    const auto interval68 = HighestPosteriorDensityInterval(sorted, 0.682689492137086);
    const auto interval95 = HighestPosteriorDensityInterval(sorted, 0.954499736103642);
    estimate.valid = true;
    estimate.center = median;
    estimate.sigma = 0.5 * (interval68.second - interval68.first);
    estimate.lower68 = interval68.first;
    estimate.upper68 = interval68.second;
    estimate.lower95 = interval95.first;
    estimate.upper95 = interval95.second;
    return estimate;
}

detail::RkParameterState BuildStateFromRecoResult(const RecoResult& reco_result,
                                                  const TargetConstraint& target) {
    detail::RkParameterState state;
    state.dx = reco_result.fit_start_position.X() - target.target_position.X();
    state.dy = reco_result.fit_start_position.Y() - target.target_position.Y();
    const double pz = reco_result.p4_at_target.Pz();
    if (std::abs(pz) > 1.0e-12) {
        state.u = reco_result.p4_at_target.Px() / pz;
        state.v = reco_result.p4_at_target.Py() / pz;
    }
    const double p_mag = reco_result.p4_at_target.P();
    if (p_mag > 1.0e-12) {
        state.p = p_mag;
    }
    return state;
}

double ExtractStateSigma(const RecoResult& reco_result, int full_index) {
    const std::size_t offset =
        static_cast<std::size_t>(full_index * static_cast<int>(detail::kParameterCount) +
                                 full_index);
    if (offset >= reco_result.state_covariance.size()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const double variance = reco_result.state_covariance[offset];
    if (!(variance > 0.0) || !std::isfinite(variance)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return std::sqrt(variance);
}

double DefaultProfileScale(const detail::RkLeastSquaresAnalyzer& analyzer,
                           const detail::RkParameterState& state,
                           int full_index) {
    if (full_index == 0 || full_index == 1) {
        return std::max(2.0, analyzer.ParameterStep(state, full_index) * 20.0);
    }
    if (full_index == 2 || full_index == 3) {
        return std::max(0.05, std::abs(analyzer.ParameterStep(state, full_index) * 50.0));
    }
    // [EN] |p| profile half-range: 10% of current value, floor 1 MeV/c.
    return std::max(1.0, std::abs(state.p) * 0.1);
}

double InterpolateCrossing(double x1, double y1,
                           double x2, double y2,
                           double level) {
    const double dy = y2 - y1;
    if (std::abs(dy) < 1.0e-12) {
        return 0.5 * (x1 + x2);
    }
    const double t = (level - y1) / dy;
    return x1 + detail::Clamp(t, 0.0, 1.0) * (x2 - x1);
}

bool FindProfileCrossing(const std::vector<double>& scan_values,
                         const std::vector<double>& delta_chi2,
                         std::size_t min_index,
                         double level,
                         bool search_left,
                         double* crossing,
                         bool* bounded) {
    if (!crossing || !bounded || scan_values.size() != delta_chi2.size() ||
        scan_values.empty()) {
        return false;
    }

    if (search_left) {
        for (std::size_t i = min_index; i > 0; --i) {
            if (delta_chi2[i - 1U] > level && delta_chi2[i] <= level) {
                *crossing = InterpolateCrossing(scan_values[i - 1U], delta_chi2[i - 1U],
                                                scan_values[i], delta_chi2[i],
                                                level);
                *bounded = false;
                return true;
            }
        }
        *crossing = scan_values.front();
        *bounded = true;
        return delta_chi2.front() <= level;
    }

    for (std::size_t i = min_index; i + 1U < scan_values.size(); ++i) {
        if (delta_chi2[i] <= level && delta_chi2[i + 1U] > level) {
            *crossing = InterpolateCrossing(scan_values[i], delta_chi2[i],
                                            scan_values[i + 1U], delta_chi2[i + 1U],
                                            level);
            *bounded = false;
            return true;
        }
    }
    *crossing = scan_values.back();
    *bounded = true;
    return delta_chi2.back() <= level;
}

std::array<double, 9> ComputeCovarianceMatrix(const std::vector<TVector3>& momenta) {
    std::array<double, 9> covariance{0.0, 0.0, 0.0,
                                     0.0, 0.0, 0.0,
                                     0.0, 0.0, 0.0};
    if (momenta.size() < 2U) {
        return covariance;
    }

    const double inv_n = 1.0 / static_cast<double>(momenta.size());
    TVector3 mean(0.0, 0.0, 0.0);
    for (const TVector3& momentum : momenta) {
        mean += momentum;
    }
    mean *= inv_n;

    for (const TVector3& momentum : momenta) {
        const TVector3 diff = momentum - mean;
        covariance[0] += diff.X() * diff.X();
        covariance[1] += diff.X() * diff.Y();
        covariance[2] += diff.X() * diff.Z();
        covariance[3] += diff.Y() * diff.X();
        covariance[4] += diff.Y() * diff.Y();
        covariance[5] += diff.Y() * diff.Z();
        covariance[6] += diff.Z() * diff.X();
        covariance[7] += diff.Z() * diff.Y();
        covariance[8] += diff.Z() * diff.Z();
    }
    for (double& value : covariance) {
        value *= inv_n;
    }
    return covariance;
}

double ProfileObservableAt(const ProfileLikelihoodResult& profile,
                           std::size_t index,
                           int component) {
    if (component == 0) {
        return (index < profile.scan_px.size()) ? profile.scan_px[index]
                                                : std::numeric_limits<double>::quiet_NaN();
    }
    if (component == 1) {
        return (index < profile.scan_py.size()) ? profile.scan_py[index]
                                                : std::numeric_limits<double>::quiet_NaN();
    }
    if (component == 2) {
        return (index < profile.scan_pz.size()) ? profile.scan_pz[index]
                                                : std::numeric_limits<double>::quiet_NaN();
    }
    return (index < profile.scan_p.size()) ? profile.scan_p[index]
                                           : std::numeric_limits<double>::quiet_NaN();
}

std::array<IntervalEstimate, 4> BuildProfileMomentumIntervals(
    const TVector3& central_momentum,
    const std::array<ProfileLikelihoodResult, 5>& profiles,
    double delta_chi2_1sigma,
    double delta_chi2_2sigma
) {
    std::array<IntervalEstimate, 4> intervals;
    const std::array<double, 4> centers{
        central_momentum.X(),
        central_momentum.Y(),
        central_momentum.Z(),
        central_momentum.Mag()
    };

    for (int component = 0; component < 4; ++component) {
        double lower68 = std::numeric_limits<double>::infinity();
        double upper68 = -std::numeric_limits<double>::infinity();
        double lower95 = std::numeric_limits<double>::infinity();
        double upper95 = -std::numeric_limits<double>::infinity();
        bool have68 = false;
        bool have95 = false;

        for (const ProfileLikelihoodResult& profile : profiles) {
            if (!profile.valid || profile.scan_chi2.empty()) {
                continue;
            }
            const std::size_t n_points = profile.scan_chi2.size();
            for (std::size_t i = 0; i < n_points; ++i) {
                if (!std::isfinite(profile.scan_chi2[i])) {
                    continue;
                }
                const double observable = ProfileObservableAt(profile, i, component);
                if (!std::isfinite(observable)) {
                    continue;
                }
                const double delta_chi2 = profile.scan_chi2[i] - profile.chi2_min;
                if (delta_chi2 <= delta_chi2_1sigma + 1.0e-9) {
                    lower68 = std::min(lower68, observable);
                    upper68 = std::max(upper68, observable);
                    have68 = true;
                }
                if (delta_chi2 <= delta_chi2_2sigma + 1.0e-9) {
                    lower95 = std::min(lower95, observable);
                    upper95 = std::max(upper95, observable);
                    have95 = true;
                }
            }
        }

        if (!have68) {
            continue;
        }

        IntervalEstimate& interval = intervals[static_cast<std::size_t>(component)];
        interval.valid = true;
        interval.center = centers[static_cast<std::size_t>(component)];
        interval.sigma = 0.5 * (upper68 - lower68);
        interval.lower68 = lower68;
        interval.upper68 = upper68;
        interval.lower95 = have95 ? lower95 : lower68;
        interval.upper95 = have95 ? upper95 : upper68;
    }
    return intervals;
}

bool BuildProposalFactor(const TMatrixD& covariance, TMatrixD* factor) {
    if (!factor || covariance.GetNrows() != covariance.GetNcols() ||
        covariance.GetNrows() <= 0) {
        return false;
    }
    TMatrixDSym sym(covariance.GetNrows());
    for (int row = 0; row < covariance.GetNrows(); ++row) {
        for (int col = 0; col < covariance.GetNcols(); ++col) {
            sym(row, col) = 0.5 * (covariance(row, col) + covariance(col, row));
        }
    }
    TMatrixDSymEigen eigen(sym);
    const TVectorD eigen_values = eigen.GetEigenValues();
    const TMatrixD eigen_vectors = eigen.GetEigenVectors();
    TMatrixD sqrt_diag(covariance.GetNrows(), covariance.GetNcols());
    sqrt_diag.Zero();
    for (int i = 0; i < eigen_values.GetNrows(); ++i) {
        if (!(eigen_values(i) > 1.0e-12) || !std::isfinite(eigen_values(i))) {
            return false;
        }
        sqrt_diag(i, i) = std::sqrt(eigen_values(i));
    }
    factor->ResizeTo(covariance.GetNrows(), covariance.GetNcols());
    *factor = eigen_vectors * sqrt_diag;
    return true;
}

detail::RkParameterState LocalVectorToState(
    const detail::RkLeastSquaresAnalyzer& analyzer,
    const detail::RkParameterState& reference_state,
    const TVectorD& local_values
) {
    detail::RkParameterState state = reference_state;
    for (int local = 0; local < analyzer.layout().parameter_count; ++local) {
        const int full_index =
            analyzer.layout().active_parameter_indices[static_cast<std::size_t>(local)];
        if (full_index == 0) state.dx = local_values(local);
        if (full_index == 1) state.dy = local_values(local);
        if (full_index == 2) state.u = local_values(local);
        if (full_index == 3) state.v = local_values(local);
        if (full_index == 4) state.p = local_values(local);
    }
    return state;
}

TVectorD StateToLocalVector(const detail::RkLeastSquaresAnalyzer& analyzer,
                            const detail::RkParameterState& state) {
    TVectorD local(analyzer.layout().parameter_count);
    for (int i = 0; i < analyzer.layout().parameter_count; ++i) {
        const int full_index =
            analyzer.layout().active_parameter_indices[static_cast<std::size_t>(i)];
        if (full_index == 0) local(i) = state.dx;
        if (full_index == 1) local(i) = state.dy;
        if (full_index == 2) local(i) = state.u;
        if (full_index == 3) local(i) = state.v;
        if (full_index == 4) local(i) = state.p;
    }
    return local;
}

bool InBounds(const detail::RkLeastSquaresAnalyzer& analyzer,
              const detail::RkParameterState& state) {
    for (int local = 0; local < analyzer.layout().parameter_count; ++local) {
        const int full_index =
            analyzer.layout().active_parameter_indices[static_cast<std::size_t>(local)];
        double value = 0.0;
        if (full_index == 0) value = state.dx;
        if (full_index == 1) value = state.dy;
        if (full_index == 2) value = state.u;
        if (full_index == 3) value = state.v;
        if (full_index == 4) value = state.p;
        double lower = analyzer.ParameterLowerBound(full_index);
        double upper = analyzer.ParameterUpperBound(full_index);
        if (lower > upper) {
            std::swap(lower, upper);
        }
        if (value < lower || value > upper || !std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

TVectorD SampleStandardNormal(int dimension, std::mt19937& rng) {
    std::normal_distribution<double> dist(0.0, 1.0);
    TVectorD values(dimension);
    for (int i = 0; i < dimension; ++i) {
        values(i) = dist(rng);
    }
    return values;
}

}  // namespace

PDCErrorAnalysis::PDCErrorAnalysis(MagneticField* magnetic_field)
    : fMagneticField(magnetic_field) {
    if (fMagneticField) {
        fReconstructor = std::make_unique<PDCMomentumReconstructor>(fMagneticField);
    }
}

PDCErrorAnalysis::~PDCErrorAnalysis() = default;

std::pair<TVector3, TVector3> PDCErrorAnalysis::ComputeUVDirections(double pdc_angle_deg) {
    const double angle_rad = pdc_angle_deg * kDegToRad;
    const double cos_a = std::cos(angle_rad);
    const double sin_a = std::sin(angle_rad);

    const double inv_sqrt2 = 1.0 / std::sqrt(2.0);
    TVector3 u_hat(inv_sqrt2 * cos_a, inv_sqrt2, inv_sqrt2 * sin_a);
    TVector3 v_hat(inv_sqrt2 * cos_a, -inv_sqrt2, inv_sqrt2 * sin_a);
    u_hat = u_hat.Unit();
    v_hat = v_hat.Unit();
    return {u_hat, v_hat};
}

PDCInputTrack PDCErrorAnalysis::PerturbTrack(const PDCInputTrack& track,
                                             const PDCErrorModel& error_model,
                                             std::mt19937& rng) const {
    PDCInputTrack perturbed = track;
    std::normal_distribution<double> dist(0.0, 1.0);
    const auto [u_hat, v_hat] = ComputeUVDirections();

    // [EN] Sample PDC perturbations in detector wire coordinates before rotating back to lab space. / [CN] 先在探测器丝坐标中采样PDC扰动，再旋回实验室坐标。
    double delta_u1 = 0.0;
    double delta_v1 = 0.0;
    double delta_u2 = 0.0;
    double delta_v2 = 0.0;
    if (error_model.use_correlation) {
        const double a11 = error_model.uv_correlation[0];
        const double a12 = error_model.uv_correlation[1];
        const double a22 = error_model.uv_correlation[3];
        const double l11 = std::sqrt(std::max(0.0, a11));
        const double l21 = (std::abs(l11) > 1.0e-12) ? a12 / l11 : 0.0;
        const double l22 = std::sqrt(std::max(0.0, a22 - l21 * l21));
        const double z11 = dist(rng);
        const double z12 = dist(rng);
        const double z21 = dist(rng);
        const double z22 = dist(rng);
        delta_u1 = error_model.sigma_u_mm * (l11 * z11);
        delta_v1 = error_model.sigma_v_mm * (l21 * z11 + l22 * z12);
        delta_u2 = error_model.sigma_u_mm * (l11 * z21);
        delta_v2 = error_model.sigma_v_mm * (l21 * z21 + l22 * z22);
    } else {
        delta_u1 = error_model.sigma_u_mm * dist(rng);
        delta_v1 = error_model.sigma_v_mm * dist(rng);
        delta_u2 = error_model.sigma_u_mm * dist(rng);
        delta_v2 = error_model.sigma_v_mm * dist(rng);
    }

    perturbed.pdc1 = track.pdc1 + delta_u1 * u_hat + delta_v1 * v_hat;
    perturbed.pdc2 = track.pdc2 + delta_u2 * u_hat + delta_v2 * v_hat;
    return perturbed;
}

ComponentStats PDCErrorAnalysis::ComputeComponentStats(const std::vector<double>& values) {
    ComponentStats stats;
    if (values.empty()) {
        return stats;
    }

    const std::size_t n = values.size();
    stats.mean =
        std::accumulate(values.begin(), values.end(), 0.0) /
        static_cast<double>(n);
    double sum_sq = 0.0;
    for (double value : values) {
        sum_sq += (value - stats.mean) * (value - stats.mean);
    }
    stats.std_dev = std::sqrt(sum_sq / static_cast<double>(n));

    std::vector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    stats.median = Percentile(sorted, 0.5);
    stats.mad = MedianAbsoluteDeviation(values, stats.median);
    stats.q16 = Percentile(sorted, 0.16);
    stats.q84 = Percentile(sorted, 0.84);
    stats.q025 = Percentile(sorted, 0.025);
    stats.q975 = Percentile(sorted, 0.975);

    if (stats.std_dev > 1.0e-15) {
        double skew = 0.0;
        double kurt = 0.0;
        for (double value : values) {
            const double z = (value - stats.mean) / stats.std_dev;
            skew += z * z * z;
            kurt += z * z * z * z;
        }
        stats.skewness = skew / static_cast<double>(n);
        stats.kurtosis = kurt / static_cast<double>(n) - 3.0;
    }
    return stats;
}

std::array<double, 9> PDCErrorAnalysis::ComputeCorrelationMatrix(
    const std::vector<TVector3>& momenta
) {
    std::array<double, 9> covariance = ComputeCovarianceMatrix(momenta);
    std::array<double, 9> correlation{1.0, 0.0, 0.0,
                                      0.0, 1.0, 0.0,
                                      0.0, 0.0, 1.0};
    const double std_x = (covariance[0] > 0.0) ? std::sqrt(covariance[0]) : 0.0;
    const double std_y = (covariance[4] > 0.0) ? std::sqrt(covariance[4]) : 0.0;
    const double std_z = (covariance[8] > 0.0) ? std::sqrt(covariance[8]) : 0.0;
    if (std_x > 1.0e-15 && std_y > 1.0e-15 && std_z > 1.0e-15) {
        correlation[1] = covariance[1] / (std_x * std_y);
        correlation[2] = covariance[2] / (std_x * std_z);
        correlation[3] = covariance[3] / (std_x * std_y);
        correlation[5] = covariance[5] / (std_y * std_z);
        correlation[6] = covariance[6] / (std_x * std_z);
        correlation[7] = covariance[7] / (std_y * std_z);
    }
    return correlation;
}

double PDCErrorAnalysis::ComputeGewekeZScore(const std::vector<double>& chain,
                                             double first_fraction,
                                             double last_fraction) {
    if (chain.size() < 10U) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const std::size_t n = chain.size();
    const std::size_t n_first =
        static_cast<std::size_t>(first_fraction * static_cast<double>(n));
    const std::size_t n_last =
        static_cast<std::size_t>(last_fraction * static_cast<double>(n));
    if (n_first < 2U || n_last < 2U || n_first + n_last > n) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double mean_first = 0.0;
    double mean_last = 0.0;
    for (std::size_t i = 0; i < n_first; ++i) {
        mean_first += chain[i];
    }
    mean_first /= static_cast<double>(n_first);
    for (std::size_t i = n - n_last; i < n; ++i) {
        mean_last += chain[i];
    }
    mean_last /= static_cast<double>(n_last);

    double var_first = 0.0;
    double var_last = 0.0;
    for (std::size_t i = 0; i < n_first; ++i) {
        var_first += (chain[i] - mean_first) * (chain[i] - mean_first);
    }
    for (std::size_t i = n - n_last; i < n; ++i) {
        var_last += (chain[i] - mean_last) * (chain[i] - mean_last);
    }
    var_first /= static_cast<double>(n_first - 1U);
    var_last /= static_cast<double>(n_last - 1U);
    const double se = std::sqrt(var_first / static_cast<double>(n_first) +
                                var_last / static_cast<double>(n_last));
    if (se < 1.0e-15) {
        return 0.0;
    }
    return (mean_first - mean_last) / se;
}

double PDCErrorAnalysis::ComputeEffectiveSampleSize(const std::vector<double>& chain) {
    if (chain.size() < 4U) {
        return static_cast<double>(chain.size());
    }
    const std::size_t n = chain.size();
    double sum_rho = 0.0;
    for (int lag = 1; lag < static_cast<int>(n / 2U); ++lag) {
        const double rho = Autocorrelation(chain, lag);
        if (std::abs(rho) < 0.05) {
            break;
        }
        sum_rho += rho;
    }
    const double tau = 1.0 + 2.0 * sum_rho;
    return (tau > 1.0) ? (static_cast<double>(n) / tau)
                       : static_cast<double>(n);
}

double PDCErrorAnalysis::EvaluateChi2(double dx, double dy, double u, double v, double p,
                                      const PDCInputTrack& track,
                                      const TargetConstraint& target,
                                      const RecoConfig& config) const {
    detail::RkLeastSquaresAnalyzer analyzer(fMagneticField, track, target, config);
    std::string reason;
    if (!analyzer.Initialize(&reason)) {
        return std::numeric_limits<double>::infinity();
    }
    detail::RkParameterState state;
    state.dx = dx;
    state.dy = dy;
    state.u = u;
    state.v = v;
    state.p = p;
    return analyzer.EvaluateChi2Raw(state);
}

MonteCarloResult PDCErrorAnalysis::RunMonteCarlo(const PDCInputTrack& track,
                                                 const TargetConstraint& target,
                                                 const RecoConfig& config,
                                                 const PDCErrorModel& error_model,
                                                 const MonteCarloConfig& mc_config,
                                                 const RecoResult& central_fit) const {
    MonteCarloResult result;
    detail::RkLeastSquaresAnalyzer base_analyzer(fMagneticField, track, target, config);
    std::string reason;
    if (!base_analyzer.Initialize(&reason)) {
        return result;
    }
    const detail::RkParameterState central_state =
        BuildStateFromRecoResult(central_fit, target);

    std::vector<TVector3> sample_momenta;
    sample_momenta.reserve(static_cast<std::size_t>(mc_config.n_samples));
    int n_failures = 0;
    const unsigned int actual_seed = (mc_config.seed == 0U)
        ? static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count())
        : mc_config.seed;

#ifdef _OPENMP
    if (mc_config.parallel) {
        const int n_threads = omp_get_max_threads();
        std::vector<std::vector<TVector3>> thread_samples(static_cast<std::size_t>(n_threads));
        std::vector<int> thread_failures(static_cast<std::size_t>(n_threads), 0);

        #pragma omp parallel
        {
            const int tid = omp_get_thread_num();
            std::mt19937 rng(actual_seed + static_cast<unsigned int>(tid));

            #pragma omp for schedule(dynamic)
            for (int i = 0; i < mc_config.n_samples; ++i) {
                if (thread_failures[static_cast<std::size_t>(tid)] >= mc_config.max_failures) {
                    continue;
                }
                const PDCInputTrack perturbed = PerturbTrack(track, error_model, rng);
                detail::RkLeastSquaresAnalyzer analyzer(fMagneticField, perturbed, target, config);
                std::string local_reason;
                if (!analyzer.Initialize(&local_reason)) {
                    ++thread_failures[static_cast<std::size_t>(tid)];
                    continue;
                }
                detail::RkSolveResult solve_result;
                if (!analyzer.Solve(central_state, false, false, &solve_result) ||
                    !solve_result.eval.valid ||
                    !detail::IsFinite(solve_result.eval.p4_at_target) ||
                    solve_result.eval.p4_at_target.P() <= 0.0) {
                    ++thread_failures[static_cast<std::size_t>(tid)];
                    continue;
                }
                thread_samples[static_cast<std::size_t>(tid)].push_back(
                    solve_result.eval.p4_at_target.Vect()
                );
            }
        }

        for (std::size_t tid = 0; tid < static_cast<std::size_t>(n_threads); ++tid) {
            n_failures += thread_failures[tid];
            sample_momenta.insert(sample_momenta.end(),
                                  thread_samples[tid].begin(),
                                  thread_samples[tid].end());
        }
    } else
#endif
    {
        std::mt19937 rng(actual_seed);
        for (int i = 0; i < mc_config.n_samples; ++i) {
            if (n_failures >= mc_config.max_failures) {
                break;
            }
            const PDCInputTrack perturbed = PerturbTrack(track, error_model, rng);
            detail::RkLeastSquaresAnalyzer analyzer(fMagneticField, perturbed, target, config);
            std::string local_reason;
            if (!analyzer.Initialize(&local_reason)) {
                ++n_failures;
                continue;
            }
            detail::RkSolveResult solve_result;
            if (!analyzer.Solve(central_state, false, false, &solve_result) ||
                !solve_result.eval.valid ||
                !detail::IsFinite(solve_result.eval.p4_at_target) ||
                solve_result.eval.p4_at_target.P() <= 0.0) {
                ++n_failures;
                continue;
            }
            sample_momenta.push_back(solve_result.eval.p4_at_target.Vect());
        }
    }

    result.n_samples_completed = static_cast<int>(sample_momenta.size());
    result.n_failures = n_failures;
    if (sample_momenta.size() < 3U) {
        return result;
    }

    if (mc_config.outlier_fraction > 0.0 && mc_config.outlier_fraction < 1.0 &&
        sample_momenta.size() >= 10U) {
        std::vector<double> p_values_all;
        p_values_all.reserve(sample_momenta.size());
        for (const TVector3& momentum : sample_momenta) {
            p_values_all.push_back(momentum.Mag());
        }
        std::vector<double> sorted_p = p_values_all;
        std::sort(sorted_p.begin(), sorted_p.end());
        const double low = Percentile(sorted_p, 0.5 * mc_config.outlier_fraction);
        const double high = Percentile(sorted_p, 1.0 - 0.5 * mc_config.outlier_fraction);
        std::vector<TVector3> filtered;
        filtered.reserve(sample_momenta.size());
        for (const TVector3& momentum : sample_momenta) {
            if (momentum.Mag() >= low && momentum.Mag() <= high) {
                filtered.push_back(momentum);
            }
        }
        if (filtered.size() >= 3U) {
            sample_momenta = std::move(filtered);
        }
    }

    std::vector<double> px_values;
    std::vector<double> py_values;
    std::vector<double> pz_values;
    std::vector<double> p_values;
    px_values.reserve(sample_momenta.size());
    py_values.reserve(sample_momenta.size());
    pz_values.reserve(sample_momenta.size());
    p_values.reserve(sample_momenta.size());
    for (const TVector3& momentum : sample_momenta) {
        px_values.push_back(momentum.X());
        py_values.push_back(momentum.Y());
        pz_values.push_back(momentum.Z());
        p_values.push_back(momentum.Mag());
    }

    result.px = ComputeComponentStats(px_values);
    result.py = ComputeComponentStats(py_values);
    result.pz = ComputeComponentStats(pz_values);
    result.p_mag = ComputeComponentStats(p_values);
    result.momentum_correlation = ComputeCorrelationMatrix(sample_momenta);
    if (mc_config.store_samples) {
        result.sample_momenta = sample_momenta;
    }
    result.valid = true;
    return result;
}

std::array<ProfileLikelihoodResult, 5> PDCErrorAnalysis::ComputeProfileLikelihood(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config,
    const ProfileLikelihoodConfig& profile_config,
    const RecoResult& central_fit
) const {
    std::array<ProfileLikelihoodResult, 5> results;
    const std::array<std::string, 5> param_names{"dx", "dy", "u", "v", "p"};
    for (int i = 0; i < 5; ++i) {
        results[static_cast<std::size_t>(i)].parameter_index = i;
        results[static_cast<std::size_t>(i)].parameter_name = param_names[static_cast<std::size_t>(i)];
    }

    detail::RkLeastSquaresAnalyzer analyzer(fMagneticField, track, target, config);
    std::string reason;
    if (!analyzer.Initialize(&reason)) {
        return results;
    }
    const detail::RkParameterState central_state =
        BuildStateFromRecoResult(central_fit, target);
    const double chi2_min = std::isfinite(central_fit.chi2_raw)
        ? central_fit.chi2_raw
        : analyzer.EvaluateChi2Raw(central_state);

    for (int full_index = 0; full_index < 5; ++full_index) {
        ProfileLikelihoodResult& profile = results[static_cast<std::size_t>(full_index)];
        if (!analyzer.HasActiveParameter(full_index)) {
            continue;
        }

        const double center = (full_index == 0) ? central_state.dx :
                              (full_index == 1) ? central_state.dy :
                              (full_index == 2) ? central_state.u :
                              (full_index == 3) ? central_state.v :
                                                  central_state.p;
        double sigma = ExtractStateSigma(central_fit, full_index);
        if (!std::isfinite(sigma) || sigma <= 0.0) {
            sigma = DefaultProfileScale(analyzer, central_state, full_index);
        }
        double half_range = std::max(
            2.5 * sigma,
            profile_config.step_fraction *
                static_cast<double>(std::max(1, profile_config.n_points_per_parameter - 1)) *
                sigma
        );
        double lower = analyzer.ParameterLowerBound(full_index);
        double upper = analyzer.ParameterUpperBound(full_index);
        if (lower > upper) {
            std::swap(lower, upper);
        }
        const double scan_min = std::max(lower, center - half_range);
        const double scan_max = std::min(upper, center + half_range);
        if (!(scan_max > scan_min)) {
            continue;
        }

        profile.valid = true;
        profile.best_fit_value = center;
        profile.chi2_min = chi2_min;

        detail::RkParameterState warm_state = central_state;
        for (int i = 0; i < profile_config.n_points_per_parameter; ++i) {
            const double fraction =
                (profile_config.n_points_per_parameter > 1)
                    ? static_cast<double>(i) /
                          static_cast<double>(profile_config.n_points_per_parameter - 1)
                    : 0.0;
            const double value = scan_min + fraction * (scan_max - scan_min);
            profile.scan_values.push_back(value);

            detail::RkSolveResult solve_result;
            if (!analyzer.SolveWithFixedParameter(warm_state,
                                                  full_index,
                                                  value,
                                                  false,
                                                  false,
                                                  &solve_result)) {
                profile.scan_chi2.push_back(std::numeric_limits<double>::infinity());
                profile.scan_px.push_back(std::numeric_limits<double>::quiet_NaN());
                profile.scan_py.push_back(std::numeric_limits<double>::quiet_NaN());
                profile.scan_pz.push_back(std::numeric_limits<double>::quiet_NaN());
                profile.scan_p.push_back(std::numeric_limits<double>::quiet_NaN());
                continue;
            }
            warm_state = solve_result.state;
            profile.scan_chi2.push_back(solve_result.eval.chi2_raw);
            profile.scan_px.push_back(solve_result.eval.p4_at_target.Px());
            profile.scan_py.push_back(solve_result.eval.p4_at_target.Py());
            profile.scan_pz.push_back(solve_result.eval.p4_at_target.Pz());
            profile.scan_p.push_back(solve_result.eval.p4_at_target.P());
        }

        if (profile.scan_chi2.empty()) {
            profile.valid = false;
            continue;
        }
        auto min_it = std::min_element(profile.scan_chi2.begin(), profile.scan_chi2.end());
        if (min_it == profile.scan_chi2.end() || !std::isfinite(*min_it)) {
            profile.valid = false;
            continue;
        }
        const std::size_t min_index =
            static_cast<std::size_t>(std::distance(profile.scan_chi2.begin(), min_it));
        profile.best_fit_value = profile.scan_values[min_index];
        profile.chi2_min = *min_it;

        std::vector<double> delta_chi2(profile.scan_chi2.size(), std::numeric_limits<double>::infinity());
        for (std::size_t i = 0; i < profile.scan_chi2.size(); ++i) {
            if (std::isfinite(profile.scan_chi2[i])) {
                delta_chi2[i] = profile.scan_chi2[i] - profile.chi2_min;
            }
        }

        bool lower_bounded_1 = false;
        bool upper_bounded_1 = false;
        bool lower_bounded_2 = false;
        bool upper_bounded_2 = false;
        if (FindProfileCrossing(profile.scan_values, delta_chi2, min_index,
                                profile_config.delta_chi2_1sigma, true,
                                &profile.lower_1sigma, &lower_bounded_1) &&
            FindProfileCrossing(profile.scan_values, delta_chi2, min_index,
                                profile_config.delta_chi2_1sigma, false,
                                &profile.upper_1sigma, &upper_bounded_1)) {
            profile.sigma_minus = profile.best_fit_value - profile.lower_1sigma;
            profile.sigma_plus = profile.upper_1sigma - profile.best_fit_value;
            profile.lower_bounded = lower_bounded_1;
            profile.upper_bounded = upper_bounded_1;
        } else {
            profile.valid = false;
            continue;
        }

        FindProfileCrossing(profile.scan_values, delta_chi2, min_index,
                            profile_config.delta_chi2_2sigma, true,
                            &profile.lower_2sigma, &lower_bounded_2);
        FindProfileCrossing(profile.scan_values, delta_chi2, min_index,
                            profile_config.delta_chi2_2sigma, false,
                            &profile.upper_2sigma, &upper_bounded_2);
        profile.lower_bounded = profile.lower_bounded || lower_bounded_2;
        profile.upper_bounded = profile.upper_bounded || upper_bounded_2;
    }

    return results;
}

BayesianResult PDCErrorAnalysis::ComputeBayesianPosterior(
    const PDCInputTrack& track,
    const TargetConstraint& target,
    const RecoConfig& config,
    const BayesianConfig& bayesian_config,
    const RecoResult& central_fit
) const {
    BayesianResult result;
    if (central_fit.posterior_valid) {
        result.valid = true;
        result.laplace_momentum_covariance = central_fit.posterior_momentum_covariance;
        result.laplace_px = central_fit.px_credible;
        result.laplace_py = central_fit.py_credible;
        result.laplace_pz = central_fit.pz_credible;
        result.laplace_p = central_fit.p_credible;
    }

    if (!bayesian_config.use_mcmc) {
        return result;
    }

    detail::RkLeastSquaresAnalyzer analyzer(fMagneticField, track, target, config);
    std::string reason;
    if (!analyzer.Initialize(&reason)) {
        return result;
    }

    detail::RkSolveResult central_solve;
    if (!analyzer.Solve(true, bayesian_config.use_laplace, &central_solve)) {
        return result;
    }

    const detail::RkParameterState initial_state = central_solve.state;

    const TMatrixD& proposal_covariance =
        (central_solve.posterior_state_covariance.GetNrows() > 0)
            ? central_solve.posterior_state_covariance
            : central_solve.state_covariance;
    if (proposal_covariance.GetNrows() <= 0) {
        return result;
    }

    TMatrixD proposal_factor;
    if (!BuildProposalFactor(proposal_covariance, &proposal_factor)) {
        return result;
    }

    std::mt19937 rng(0x5A17E001U);
    std::uniform_real_distribution<double> uniform01(0.0, 1.0);
    detail::RkParameterState current_state = initial_state;
    detail::RkEvalResult current_eval = analyzer.Evaluate(current_state);
    if (!current_eval.valid) {
        return result;
    }
    double current_log_posterior =
        -0.5 * current_eval.chi2_raw +
        analyzer.EvaluateLogPrior(current_state);

    double proposal_scale = std::max(1.0e-3, bayesian_config.mcmc_proposal_scale);
    int accepted_total = 0;
    int accepted_window = 0;
    int proposed_window = 0;
    const int total_iterations =
        bayesian_config.mcmc_burn_in +
        bayesian_config.mcmc_n_samples * std::max(1, bayesian_config.mcmc_thin);

    std::vector<TVector3> momentum_samples;
    std::vector<std::vector<double>> parameter_chains(
        static_cast<std::size_t>(analyzer.layout().parameter_count)
    );
    momentum_samples.reserve(static_cast<std::size_t>(bayesian_config.mcmc_n_samples));
    for (auto& chain : parameter_chains) {
        chain.reserve(static_cast<std::size_t>(bayesian_config.mcmc_n_samples));
    }

    for (int iter = 0; iter < total_iterations; ++iter) {
        const TVectorD current_local = StateToLocalVector(analyzer, current_state);
        const TVectorD proposal_local =
            current_local + proposal_scale * (proposal_factor * SampleStandardNormal(current_local.GetNrows(), rng));
        detail::RkParameterState proposal_state =
            LocalVectorToState(analyzer, current_state, proposal_local);

        ++proposed_window;
        if (InBounds(analyzer, proposal_state)) {
            const detail::RkEvalResult proposal_eval = analyzer.Evaluate(proposal_state);
            if (proposal_eval.valid && detail::IsFinite(proposal_eval.p4_at_target)) {
                const double proposal_log_posterior =
                    -0.5 * proposal_eval.chi2_raw +
                    analyzer.EvaluateLogPrior(proposal_state);
                const double log_alpha = proposal_log_posterior - current_log_posterior;
                if (log_alpha >= 0.0 || std::log(uniform01(rng)) < log_alpha) {
                    current_state = proposal_state;
                    current_eval = proposal_eval;
                    current_log_posterior = proposal_log_posterior;
                    ++accepted_total;
                    ++accepted_window;
                }
            }
        }

        if (bayesian_config.adaptive_proposal && iter < bayesian_config.mcmc_burn_in &&
            proposed_window >= 50) {
            const double acceptance =
                static_cast<double>(accepted_window) / static_cast<double>(proposed_window);
            if (acceptance > bayesian_config.target_acceptance) {
                proposal_scale *= 1.1;
            } else {
                proposal_scale *= 0.9;
            }
            proposal_scale = detail::Clamp(proposal_scale, 1.0e-4, 100.0);
            accepted_window = 0;
            proposed_window = 0;
        }

        if (iter < bayesian_config.mcmc_burn_in) {
            continue;
        }
        const int sample_index = iter - bayesian_config.mcmc_burn_in;
        if (sample_index % std::max(1, bayesian_config.mcmc_thin) != 0) {
            continue;
        }

        momentum_samples.push_back(current_eval.p4_at_target.Vect());
        const TVectorD local = StateToLocalVector(analyzer, current_state);
        for (int local_i = 0; local_i < local.GetNrows(); ++local_i) {
            parameter_chains[static_cast<std::size_t>(local_i)].push_back(local(local_i));
        }
    }

    if (momentum_samples.size() < 4U) {
        return result;
    }

    std::vector<double> px_values;
    std::vector<double> py_values;
    std::vector<double> pz_values;
    std::vector<double> p_values;
    px_values.reserve(momentum_samples.size());
    py_values.reserve(momentum_samples.size());
    pz_values.reserve(momentum_samples.size());
    p_values.reserve(momentum_samples.size());
    for (const TVector3& momentum : momentum_samples) {
        px_values.push_back(momentum.X());
        py_values.push_back(momentum.Y());
        pz_values.push_back(momentum.Z());
        p_values.push_back(momentum.Mag());
    }

    result.mcmc_valid = true;
    result.valid = true;
    result.mcmc_momentum_covariance = ComputeCovarianceMatrix(momentum_samples);
    result.mcmc_px = BuildEmpiricalInterval(px_values);
    result.mcmc_py = BuildEmpiricalInterval(py_values);
    result.mcmc_pz = BuildEmpiricalInterval(pz_values);
    result.mcmc_p = BuildEmpiricalInterval(p_values);
    result.mcmc_acceptance_rate =
        static_cast<double>(accepted_total) / static_cast<double>(std::max(1, total_iterations));

    double min_ess = std::numeric_limits<double>::infinity();
    double max_abs_geweke = 0.0;
    for (const auto& chain : parameter_chains) {
        if (chain.empty()) {
            continue;
        }
        min_ess = std::min(min_ess, ComputeEffectiveSampleSize(chain));
        max_abs_geweke = std::max(max_abs_geweke, std::abs(ComputeGewekeZScore(chain)));
    }
    result.mcmc_effective_sample_size =
        std::isfinite(min_ess) ? min_ess : 0.0;
    result.geweke_z_score =
        std::isfinite(max_abs_geweke) ? max_abs_geweke : std::numeric_limits<double>::quiet_NaN();
    result.converged =
        result.mcmc_effective_sample_size >= 50.0 &&
        std::isfinite(result.geweke_z_score) &&
        result.geweke_z_score < 2.0;
    return result;
}

ErrorAnalysisResult PDCErrorAnalysis::Analyze(const PDCInputTrack& track,
                                              const TargetConstraint& target,
                                              const RecoConfig& config,
                                              const PDCErrorModel& error_model,
                                              const MonteCarloConfig& mc_config,
                                              const ProfileLikelihoodConfig& profile_config,
                                              const BayesianConfig& bayesian_config) const {
    ErrorAnalysisResult result;
    detail::RkLeastSquaresAnalyzer analyzer(fMagneticField, track, target, config);
    std::string reason;
    if (!analyzer.Initialize(&reason)) {
        return result;
    }

    detail::RkSolveResult central_solve;
    if (!analyzer.Solve(config.compute_uncertainty,
                        config.compute_posterior_laplace,
                        &central_solve)) {
        return result;
    }
    analyzer.FillRecoResult(central_solve, &result.central_fit);
    if (!(result.central_fit.status == SolverStatus::kSuccess ||
          result.central_fit.status == SolverStatus::kNotConverged)) {
        return result;
    }

    const auto mc_start = std::chrono::high_resolution_clock::now();
    result.monte_carlo =
        RunMonteCarlo(track, target, config, error_model, mc_config, result.central_fit);
    const auto mc_end = std::chrono::high_resolution_clock::now();
    result.mc_time_ms =
        std::chrono::duration<double, std::milli>(mc_end - mc_start).count();
    result.mc_valid = result.monte_carlo.valid;

    const auto profile_start = std::chrono::high_resolution_clock::now();
    result.parameter_profiles =
        ComputeProfileLikelihood(track, target, config, profile_config, result.central_fit);
    const auto profile_end = std::chrono::high_resolution_clock::now();
    result.profile_time_ms =
        std::chrono::duration<double, std::milli>(profile_end - profile_start).count();

    result.profile_valid = false;
    for (const ProfileLikelihoodResult& profile : result.parameter_profiles) {
        if (profile.valid) {
            result.profile_valid = true;
            break;
        }
    }
    if (result.profile_valid) {
        result.profile_momentum =
            BuildProfileMomentumIntervals(central_solve.eval.p4_at_target.Vect(),
                                          result.parameter_profiles,
                                          profile_config.delta_chi2_1sigma,
                                          profile_config.delta_chi2_2sigma);
    }

    const auto bayesian_start = std::chrono::high_resolution_clock::now();
    result.bayesian =
        ComputeBayesianPosterior(track, target, config, bayesian_config, result.central_fit);
    const auto bayesian_end = std::chrono::high_resolution_clock::now();
    result.bayesian_time_ms =
        std::chrono::duration<double, std::milli>(bayesian_end - bayesian_start).count();
    result.bayesian_valid = result.bayesian.valid;

    result.error_summary[0].component = "px";
    result.error_summary[1].component = "py";
    result.error_summary[2].component = "pz";
    result.error_summary[3].component = "p";

    if (result.central_fit.uncertainty_valid) {
        result.error_summary[0].fisher_sigma = result.central_fit.px_interval.sigma;
        result.error_summary[1].fisher_sigma = result.central_fit.py_interval.sigma;
        result.error_summary[2].fisher_sigma = result.central_fit.pz_interval.sigma;
        result.error_summary[3].fisher_sigma = result.central_fit.p_interval.sigma;
    }
    if (result.mc_valid) {
        result.error_summary[0].mc_sigma = result.monte_carlo.px.std_dev;
        result.error_summary[1].mc_sigma = result.monte_carlo.py.std_dev;
        result.error_summary[2].mc_sigma = result.monte_carlo.pz.std_dev;
        result.error_summary[3].mc_sigma = result.monte_carlo.p_mag.std_dev;
    }
    if (result.profile_valid) {
        result.error_summary[0].profile_sigma = result.profile_momentum[0].sigma;
        result.error_summary[1].profile_sigma = result.profile_momentum[1].sigma;
        result.error_summary[2].profile_sigma = result.profile_momentum[2].sigma;
        result.error_summary[3].profile_sigma = result.profile_momentum[3].sigma;
    }
    if (result.bayesian_valid) {
        result.error_summary[0].laplace_sigma = result.bayesian.laplace_px.sigma;
        result.error_summary[1].laplace_sigma = result.bayesian.laplace_py.sigma;
        result.error_summary[2].laplace_sigma = result.bayesian.laplace_pz.sigma;
        result.error_summary[3].laplace_sigma = result.bayesian.laplace_p.sigma;
        if (result.bayesian.mcmc_valid) {
            result.error_summary[0].mcmc_sigma = result.bayesian.mcmc_px.sigma;
            result.error_summary[1].mcmc_sigma = result.bayesian.mcmc_py.sigma;
            result.error_summary[2].mcmc_sigma = result.bayesian.mcmc_pz.sigma;
            result.error_summary[3].mcmc_sigma = result.bayesian.mcmc_p.sigma;
        }
    }

    result.valid = true;
    return result;
}

}  // namespace analysis::pdc::anaroot_like
