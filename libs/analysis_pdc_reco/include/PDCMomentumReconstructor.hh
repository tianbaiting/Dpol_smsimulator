#ifndef ANALYSIS_PDC_MOMENTUM_RECONSTRUCTOR_HH
#define ANALYSIS_PDC_MOMENTUM_RECONSTRUCTOR_HH

#include "MagneticField.hh"
#include "ParticleTrajectory.hh"
#include "PDCNNMomentumReconstructor.hh"
#include "PDCRecoTypes.hh"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace analysis::pdc::anaroot_like {

class PDCMomentumReconstructor {
public:
    explicit PDCMomentumReconstructor(MagneticField* magnetic_field);

    RecoResult Reconstruct(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config
    ) const;

    RecoResult ReconstructRK(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config
    ) const;

    RecoResult ReconstructMultiDim(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config
    ) const;

    RecoResult ReconstructNN(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config
    ) const;

    RecoResult ReconstructRKTwoPointBackprop(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config
    ) const;

    RecoResult ReconstructRKFixedTargetPdcOnly(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config
    ) const;

    RecoResult ReconstructRKThreePointFree(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config
    ) const;

private:
    struct ParameterState {
        double dx = 0.0;
        double dy = 0.0;
        double u = 0.0;
        double v = 0.0;
        double q = 0.0;  // q/p, where p in MeV/c
    };

    struct EvalResult {
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

    struct MeasurementModel {
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

    static bool IsFinite(const TVector3& value);
    static bool IsFinite(const TLorentzVector& value);
    static double Clamp(double value, double lower, double upper);

    bool ValidateInputs(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config,
        std::string* reason
    ) const;

    ParameterState BuildInitialState(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config
    ) const;

    ParameterState ClampState(const ParameterState& state, const TargetConstraint& target, const RecoConfig& config) const;

    TVector3 BuildMomentumVector(const ParameterState& state, double charge_e) const;

    EvalResult EvaluateState(
        const ParameterState& state,
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config,
        const MeasurementModel& measurement,
        const RkFitLayout& layout
    ) const;

    static RkFitLayout BuildRkFitLayout(const RecoConfig& config);
    static double ResidualChi2Raw(const std::array<double, 8>& residuals, int residual_count);
    static double ResidualChi2Reduced(const std::array<double, 8>& residuals, int residual_count, int ndf);
    bool BuildMeasurementModel(const TargetConstraint& target, MeasurementModel* measurement, std::string* reason) const;

    MagneticField* fMagneticField = nullptr;
    mutable std::unique_ptr<PDCNNMomentumReconstructor> fNNReconstructor;
    mutable std::string fNNModelPath;
};

}  // namespace analysis::pdc::anaroot_like

#endif  // ANALYSIS_PDC_MOMENTUM_RECONSTRUCTOR_HH
