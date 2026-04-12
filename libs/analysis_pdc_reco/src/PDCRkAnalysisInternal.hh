#ifndef ANALYSIS_PDC_RK_ANALYSIS_INTERNAL_HH
#define ANALYSIS_PDC_RK_ANALYSIS_INTERNAL_HH

#include "MagneticField.hh"
#include "ParticleTrajectory.hh"
#include "PDCRecoTypes.hh"

#include "TMatrixD.h"
#include "TLorentzVector.h"
#include "TVectorD.h"
#include "TVector3.h"

#include <array>
#include <limits>
#include <string>
#include <vector>

namespace analysis::pdc::anaroot_like::detail {

inline constexpr std::size_t kResidualCount = 8;
inline constexpr std::size_t kParameterCount = 5;
inline constexpr std::size_t kMomentumDim = 3;
inline constexpr double kBrhoGeVOverCPerTm = 0.299792458;
inline constexpr double kDefaultMassMeV = 938.2720813;
inline constexpr double kSigma95 = 1.959963984540054;

struct RkParameterState {
    double dx = 0.0;
    double dy = 0.0;
    double u = 0.0;
    double v = 0.0;
    double q = 0.0;
};

struct RkEvalResult {
    bool valid = false;
    std::array<double, 8> residuals{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    int residual_count = 0;
    double chi2_raw = 0.0;
    double chi2_reduced = 0.0;
    int ndf = 0;
    double min_distance_mm = 0.0;
    double path_length_mm = 0.0;
    double brho_tm = 0.0;
    bool used_measurement_covariance = false;
    TLorentzVector p4_at_target{0.0, 0.0, 0.0, 0.0};
    int idx_pdc1 = -1;
    int idx_pdc2 = -1;
    std::vector<ParticleTrajectory::TrajectoryPoint> trajectory;
};

struct RkMeasurementModel {
    bool use_covariance = false;
    std::array<double, 9> pdc1_whitening{0.0, 0.0, 0.0,
                                         0.0, 0.0, 0.0,
                                         0.0, 0.0, 0.0};
    std::array<double, 9> pdc2_whitening{0.0, 0.0, 0.0,
                                         0.0, 0.0, 0.0,
                                         0.0, 0.0, 0.0};
};

struct RkFitLayout {
    std::array<int, 5> active_parameter_indices{0, 1, 2, 3, 4};
    int parameter_count = 5;
    int residual_count = 8;
    bool include_start_xy_constraint = true;
    bool fixed_target_position = false;
};

struct RkSolveResult {
    bool valid = false;
    RkParameterState state;
    RkEvalResult eval;
    int accepted_iterations = 0;
    double condition_number = std::numeric_limits<double>::quiet_NaN();
    double posterior_condition_number = std::numeric_limits<double>::quiet_NaN();
    TMatrixD normal_matrix;
    TMatrixD state_covariance;
    TMatrixD posterior_state_covariance;
    std::array<double, 25> full_state_covariance{0.0, 0.0, 0.0, 0.0, 0.0,
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
};

class RkLeastSquaresAnalyzer {
public:
    RkLeastSquaresAnalyzer(MagneticField* magnetic_field,
                           const PDCInputTrack& track,
                           const TargetConstraint& target,
                           const RecoConfig& config);

    bool Initialize(std::string* reason);
    bool SupportsCurrentMode() const;

    const RkFitLayout& layout() const { return fLayout; }
    const RkMeasurementModel& measurement() const { return fMeasurement; }
    const RkParameterState& initial_state() const { return fInitialState; }

    RkParameterState ClampState(const RkParameterState& state) const;
    TVector3 BuildMomentumVector(const RkParameterState& state) const;
    RkEvalResult Evaluate(const RkParameterState& state) const;
    bool BuildResidualJacobian(const RkParameterState& state,
                               const RkEvalResult& eval,
                               TMatrixD* jacobian) const;
    TMatrixD BuildNormalMatrix(const TMatrixD& jacobian) const;
    TVectorD BuildGradient(const TMatrixD& jacobian,
                           const RkEvalResult& eval) const;
    TMatrixD BuildMomentumJacobian(const RkParameterState& state) const;
    TMatrixD BuildPriorPrecision() const;
    int FindLocalParameter(int full_index) const;
    bool HasActiveParameter(int full_index) const;
    double ParameterStep(const RkParameterState& state, int full_index) const;
    double ParameterLowerBound(int full_index) const;
    double ParameterUpperBound(int full_index) const;
    bool CovarianceIsPositiveDefinite(const TMatrixD& covariance) const;
    bool InvertWithSVD(const TMatrixD& matrix,
                       TMatrixD* inverse,
                       double* condition_number) const;
    void StoreStateCovariance(const TMatrixD& state_covariance,
                              std::array<double, 25>* out) const;
    void FillMomentumUncertainty(const RkParameterState& state,
                                 const TMatrixD& state_covariance,
                                 std::array<double, 9>* covariance_out,
                                 IntervalEstimate* px_interval,
                                 IntervalEstimate* py_interval,
                                 IntervalEstimate* pz_interval,
                                 IntervalEstimate* p_interval) const;
    double EvaluateChi2Raw(const RkParameterState& state) const;
    double EvaluateLogPrior(const RkParameterState& state,
                            const BayesianConfig& bayesian_config,
                            const RkParameterState& prior_center) const;

    bool Solve(const RkParameterState& initial_state,
               bool compute_uncertainty,
               bool compute_posterior_laplace,
               RkSolveResult* result) const;
    bool Solve(bool compute_uncertainty,
               bool compute_posterior_laplace,
               RkSolveResult* result) const;
    bool SolveWithFixedParameter(const RkParameterState& initial_state,
                                 int fixed_full_index,
                                 double fixed_value,
                                 bool compute_uncertainty,
                                 bool compute_posterior_laplace,
                                 RkSolveResult* result) const;
    void FillRecoResult(const RkSolveResult& solve_result,
                        RecoResult* result) const;

private:
    bool ValidateInputs(std::string* reason) const;
    bool BuildMeasurementModel(std::string* reason);
    RkFitLayout BuildRkFitLayout() const;
    RkParameterState BuildInitialState() const;
    RkParameterState AddDelta(const RkParameterState& state,
                              int full_index,
                              double delta) const;
    double GetParameter(const RkParameterState& state, int full_index) const;
    bool SolveImpl(const RkParameterState& initial_state,
                   int fixed_full_index,
                   double fixed_value,
                   bool compute_uncertainty,
                   bool compute_posterior_laplace,
                   RkSolveResult* result) const;

    MagneticField* fMagneticField = nullptr;
    PDCInputTrack fTrack;
    TargetConstraint fTarget;
    RecoConfig fConfig;
    RkMeasurementModel fMeasurement;
    RkFitLayout fLayout;
    RkParameterState fInitialState;
    bool fInitialized = false;
};

bool IsFinite(const TVector3& value);
bool IsFinite(const TLorentzVector& value);
double SafeSigma(double sigma, double fallback);
double SafeNorm(const TVector3& value);
double Clamp(double value, double lower, double upper);
void MatrixToArray(const TMatrixD& matrix, std::array<double, 9>* out);
double ReducedChi2(double chi2_raw, int ndf);
IntervalEstimate BuildGaussianInterval(double center, double sigma);

}  // namespace analysis::pdc::anaroot_like::detail

#endif  // ANALYSIS_PDC_RK_ANALYSIS_INTERNAL_HH
