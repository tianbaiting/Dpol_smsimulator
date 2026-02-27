#ifndef ANALYSIS_PDC_RECO_TYPES_HH
#define ANALYSIS_PDC_RECO_TYPES_HH

#include "TLorentzVector.h"
#include "TVector3.h"

#include <limits>
#include <string>

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
    kMultiDimFit,
    kMatrixFallback
};

struct PDCInputTrack {
    TVector3 pdc1{0.0, 0.0, 0.0};
    TVector3 pdc2{0.0, 0.0, 0.0};

    bool IsValid() const { return (pdc2 - pdc1).Mag2() > 1.0e-12; }
};

struct TargetConstraint {
    TVector3 target_position{0.0, 0.0, 0.0};
    double mass_mev = 938.2720813;
    double charge_e = 1.0;
    double pdc_sigma_mm = 2.0;
    double target_sigma_xy_mm = 5.0;
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
    bool enable_matrix = true;

    double center_brho_tm = 7.2751;
    std::string nn_model_json_path;
};

struct RecoResult {
    SolverStatus status = SolverStatus::kInvalidInput;
    SolveMethod method_used = SolveMethod::kAutoChain;
    TLorentzVector p4_at_target{0.0, 0.0, 0.0, 0.0};
    double chi2 = std::numeric_limits<double>::infinity();
    double min_distance_mm = std::numeric_limits<double>::infinity();
    double path_length_mm = 0.0;
    double brho_tm = std::numeric_limits<double>::quiet_NaN();
    int iterations = 0;
    std::string message;
};

}  // namespace analysis::pdc::anaroot_like

#endif  // ANALYSIS_PDC_RECO_TYPES_HH
