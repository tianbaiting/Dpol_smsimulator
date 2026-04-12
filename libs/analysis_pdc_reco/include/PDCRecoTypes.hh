#ifndef ANALYSIS_PDC_RECO_TYPES_HH
#define ANALYSIS_PDC_RECO_TYPES_HH

#include "TLorentzVector.h"
#include "TVector3.h"

#include <array>
#include <limits>
#include <string>
#include <vector>

namespace analysis::pdc::anaroot_like {

enum class SolverStatus {
    kSuccess,
    kNotConverged,
    kNotAvailable,
    kInvalidInput
};

enum class SolveMethod {
    kAutoChain,
    kNeuralNetwork,
    kRungeKutta,
    kMultiDimFit
};

enum class RkFitMode {
    kTwoPointBackprop,
    kFixedTargetPdcOnly,
    kThreePointFree
};

struct PDCInputTrack {
    TVector3 pdc1{0.0, 0.0, 0.0};
    TVector3 pdc2{0.0, 0.0, 0.0};

    bool IsValid() const { return (pdc2 - pdc1).Mag2() > 1.0e-12; }
};

struct IntervalEstimate {
    bool valid = false;
    double center = std::numeric_limits<double>::quiet_NaN();
    double sigma = std::numeric_limits<double>::quiet_NaN();
    double lower68 = std::numeric_limits<double>::quiet_NaN();
    double upper68 = std::numeric_limits<double>::quiet_NaN();
    double lower95 = std::numeric_limits<double>::quiet_NaN();
    double upper95 = std::numeric_limits<double>::quiet_NaN();
};

struct TargetConstraint {
    TVector3 target_position{0.0, 0.0, 0.0};
    double mass_mev = 938.2720813;
    double charge_e = 1.0;
    double pdc_sigma_mm = 2.0;
    double target_sigma_xy_mm = 5.0;
    bool use_pdc_covariance = false;
    std::array<double, 9> pdc1_cov_mm2{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<double, 9> pdc2_cov_mm2{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
};

struct RecoConfig {
    double p_min_mevc = 50.0;
    double p_max_mevc = 5000.0;
    double initial_p_mevc = 1000.0;
    double tolerance_mm = 3.0;
    int max_iterations = 40;
    double rk_step_mm = 5.0;

    double lm_lambda_init = 1.0e-2;
    double lm_lambda_min = 1.0e-6;
    double lm_lambda_max = 1.0e8;

    bool enable_rk = true;
    bool enable_nn = false;
    bool enable_multi_dim = true;
    RkFitMode rk_fit_mode = RkFitMode::kThreePointFree;

    double center_brho_tm = 7.2751;
    std::string nn_model_json_path;
    bool compute_uncertainty = true;
    bool compute_posterior_laplace = true;
    double posterior_u_sigma = 0.25;
    double posterior_v_sigma = 0.25;
    double posterior_q_rel_sigma = 0.35;
};

struct RecoResult {
    SolverStatus status = SolverStatus::kInvalidInput;
    SolveMethod method_used = SolveMethod::kAutoChain;
    TLorentzVector p4_at_target{0.0, 0.0, 0.0, 0.0};
    TVector3 fit_start_position{std::numeric_limits<double>::quiet_NaN(),
                                std::numeric_limits<double>::quiet_NaN(),
                                std::numeric_limits<double>::quiet_NaN()};
    double chi2 = std::numeric_limits<double>::infinity();
    double chi2_raw = std::numeric_limits<double>::infinity();
    double chi2_reduced = std::numeric_limits<double>::infinity();
    double min_distance_mm = std::numeric_limits<double>::infinity();
    double path_length_mm = 0.0;
    double brho_tm = std::numeric_limits<double>::quiet_NaN();
    int iterations = 0;
    int ndf = 0;
    double normal_condition_number = std::numeric_limits<double>::quiet_NaN();
    bool used_measurement_covariance = false;
    bool uncertainty_valid = false;
    bool posterior_valid = false;
    std::array<double, 25> state_covariance{0.0, 0.0, 0.0, 0.0, 0.0,
                                            0.0, 0.0, 0.0, 0.0, 0.0,
                                            0.0, 0.0, 0.0, 0.0, 0.0,
                                            0.0, 0.0, 0.0, 0.0, 0.0,
                                            0.0, 0.0, 0.0, 0.0, 0.0};
    std::array<double, 9> momentum_covariance{0.0, 0.0, 0.0,
                                              0.0, 0.0, 0.0,
                                              0.0, 0.0, 0.0};
    std::array<double, 9> posterior_momentum_covariance{0.0, 0.0, 0.0,
                                                        0.0, 0.0, 0.0,
                                                        0.0, 0.0, 0.0};
    IntervalEstimate px_interval;
    IntervalEstimate py_interval;
    IntervalEstimate pz_interval;
    IntervalEstimate p_interval;
    IntervalEstimate px_credible;
    IntervalEstimate py_credible;
    IntervalEstimate pz_credible;
    IntervalEstimate p_credible;
    std::string message;
};

// ============================================================================
// Error Analysis Structures (Phase 1: Data Structure Foundation)
// ============================================================================

// [EN] PDC error model parameterization for Monte Carlo sampling
// [CN] PDC误差模型参数化，用于Monte Carlo采样
struct PDCErrorModel {
    double sigma_u_mm = 2.0;       // U-wire position resolution
    double sigma_v_mm = 2.0;       // V-wire position resolution
    bool anisotropic = false;      // Whether U/V have different resolution
    bool use_correlation = false;  // Whether to use U-V correlation
    std::array<double, 4> uv_correlation{1.0, 0.0, 0.0, 1.0};  // 2x2 correlation matrix
    double target_xy_sigma_mm = 5.0;
    double target_z_sigma_mm = 0.1;
};

// [EN] Monte Carlo sampling configuration
// [CN] Monte Carlo采样配置
struct MonteCarloConfig {
    int n_samples = 100;
    unsigned int seed = 0;         // 0 = random seed
    bool parallel = false;         // Enable OpenMP parallelization
    bool store_samples = false;    // Keep individual sample results
    double outlier_fraction = 0.05;
    int max_failures = 10;         // Maximum allowed solver failures
};

// [EN] Component-wise statistics for momentum distribution
// [CN] 动量分布的分量统计
struct ComponentStats {
    double mean = 0.0;
    double std_dev = 0.0;
    double median = 0.0;
    double mad = 0.0;              // Median absolute deviation
    double q16 = 0.0, q84 = 0.0;   // 16th and 84th percentiles (1-sigma)
    double q025 = 0.0, q975 = 0.0; // 2.5th and 97.5th percentiles (95%)
    double skewness = 0.0;
    double kurtosis = 0.0;
};

// [EN] Monte Carlo sampling result
// [CN] Monte Carlo采样结果
struct MonteCarloResult {
    bool valid = false;
    int n_samples_completed = 0;
    int n_failures = 0;

    ComponentStats px, py, pz, p_mag;
    std::array<double, 9> momentum_correlation{0.0, 0.0, 0.0,
                                                0.0, 0.0, 0.0,
                                                0.0, 0.0, 0.0};

    // Optional: all sample momenta (if store_samples=true)
    std::vector<TVector3> sample_momenta;
};

// [EN] Profile likelihood scan configuration
// [CN] Profile likelihood扫描配置
struct ProfileLikelihoodConfig {
    int n_points_per_parameter = 20;
    double delta_chi2_1sigma = 1.0;
    double delta_chi2_2sigma = 4.0;
    bool scan_all_parameters = true;
    bool compute_2d_contours = false;  // Expensive 2D correlation scans
    double step_fraction = 0.1;        // Scan range as fraction of Fisher sigma
};

// [EN] Profile likelihood result per parameter
// [CN] 每个参数的Profile likelihood结果
struct ProfileLikelihoodResult {
    bool valid = false;
    int parameter_index = -1;      // 0=dx, 1=dy, 2=u, 3=v, 4=q
    std::string parameter_name;
    bool lower_bounded = false;
    bool upper_bounded = false;

    double best_fit_value = 0.0;
    double chi2_min = 0.0;

    double lower_1sigma = 0.0;
    double upper_1sigma = 0.0;
    double sigma_minus = 0.0;      // Asymmetric lower error
    double sigma_plus = 0.0;       // Asymmetric upper error

    double lower_2sigma = 0.0;
    double upper_2sigma = 0.0;

    // Profile scan data for plotting
    std::vector<double> scan_values;
    std::vector<double> scan_chi2;
    std::vector<double> scan_px;
    std::vector<double> scan_py;
    std::vector<double> scan_pz;
    std::vector<double> scan_p;
};

// [EN] Bayesian posterior configuration
// [CN] 贝叶斯后验配置
struct BayesianConfig {
    enum class PriorType { kFlat, kGaussian };
    PriorType prior_type = PriorType::kGaussian;
    bool prior_centered_at_mle = true;  // MLE-centered vs zero-centered
    double prior_u_sigma = 0.25;
    double prior_v_sigma = 0.25;
    double prior_q_rel_sigma = 0.35;
    bool use_laplace = true;
    bool use_mcmc = false;
    int mcmc_n_samples = 5000;
    int mcmc_burn_in = 500;
    int mcmc_thin = 5;
    double mcmc_proposal_scale = 1.0;
    bool adaptive_proposal = true;
    double target_acceptance = 0.234;  // Rosenthal optimal
};

// [EN] Bayesian posterior result
// [CN] 贝叶斯后验结果
struct BayesianResult {
    bool valid = false;
    std::array<double, 9> laplace_momentum_covariance{0.0, 0.0, 0.0,
                                                       0.0, 0.0, 0.0,
                                                       0.0, 0.0, 0.0};
    IntervalEstimate laplace_px, laplace_py, laplace_pz, laplace_p;
    bool mcmc_valid = false;
    std::array<double, 9> mcmc_momentum_covariance{0.0, 0.0, 0.0,
                                                    0.0, 0.0, 0.0,
                                                    0.0, 0.0, 0.0};
    IntervalEstimate mcmc_px, mcmc_py, mcmc_pz, mcmc_p;
    double mcmc_acceptance_rate = 0.0;
    double mcmc_effective_sample_size = 0.0;
    double geweke_z_score = 0.0;       // |Z| < 2 indicates convergence
    bool converged = false;
};

// [EN] Error comparison summary for each momentum component
// [CN] 每个动量分量的误差比较摘要
struct ErrorComparison {
    std::string component;
    double fisher_sigma = 0.0;
    double profile_sigma = 0.0;
    double mc_sigma = 0.0;
    double laplace_sigma = 0.0;
    double mcmc_sigma = 0.0;
};

// [EN] Comprehensive error analysis result
// [CN] 综合误差分析结果
struct ErrorAnalysisResult {
    bool valid = false;
    RecoResult central_fit;

    bool mc_valid = false;
    MonteCarloResult monte_carlo;

    bool profile_valid = false;
    std::array<ProfileLikelihoodResult, 5> parameter_profiles;
    std::array<IntervalEstimate, 4> profile_momentum;  // px, py, pz, p

    bool bayesian_valid = false;
    BayesianResult bayesian;

    std::array<ErrorComparison, 4> error_summary;  // px, py, pz, p

    double mc_time_ms = 0.0;
    double profile_time_ms = 0.0;
    double bayesian_time_ms = 0.0;
};

}  // namespace analysis::pdc::anaroot_like

#endif  // ANALYSIS_PDC_RECO_TYPES_HH
