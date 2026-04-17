#include <gtest/gtest.h>

#include "ParticleTrajectory.hh"
#include "PDCErrorAnalysis.hh"
#include "PDCMomentumReconstructor.hh"

#include <array>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

using analysis::pdc::anaroot_like::BayesianConfig;
using analysis::pdc::anaroot_like::ErrorAnalysisResult;
using analysis::pdc::anaroot_like::MonteCarloConfig;
using analysis::pdc::anaroot_like::PDCErrorAnalysis;
using analysis::pdc::anaroot_like::PDCErrorModel;
using analysis::pdc::anaroot_like::PDCInputTrack;
using analysis::pdc::anaroot_like::PDCMomentumReconstructor;
using analysis::pdc::anaroot_like::ProfileLikelihoodConfig;
using analysis::pdc::anaroot_like::RecoConfig;
using analysis::pdc::anaroot_like::RecoResult;
using analysis::pdc::anaroot_like::RkFitMode;
using analysis::pdc::anaroot_like::SolverStatus;
using analysis::pdc::anaroot_like::TargetConstraint;

namespace {

std::string WriteConstantFieldMap(const std::string& stem, double by_tesla) {
    const std::string path = "/tmp/" + stem + ".fieldmap";
    std::ofstream out(path);
    EXPECT_TRUE(out.is_open());
    out << "3 3 4 2\n";
    out << "# header 1\n";
    out << "# header 2\n";
    out << "# header 3\n";
    out << "# header 4\n";
    out << "# header 5\n";
    out << "# header 6\n";
    out << "0\n";

    const std::array<double, 3> xs{0.0, 1000.0, 2000.0};
    const std::array<double, 3> ys{-500.0, 0.0, 500.0};
    const std::array<double, 4> zs{0.0, 500.0, 1000.0, 1500.0};
    for (double x : xs) {
        for (double y : ys) {
            for (double z : zs) {
                out << x << " " << y << " " << z << " 0 " << by_tesla << " 0\n";
            }
        }
    }
    out.close();
    return path;
}

TargetConstraint MakeConstraint() {
    TargetConstraint target;
    target.target_position.SetXYZ(0.0, 0.0, 0.0);
    target.mass_mev = 938.2720813;
    target.charge_e = 1.0;
    target.pdc_sigma_u_mm = 2.0;
    target.pdc_sigma_v_mm = 2.0;
    target.pdc_uv_correlation = 0.0;
    target.pdc_angle_deg = 57.0;
    target.target_sigma_xy_mm = 5.0;
    return target;
}

RecoConfig MakeConfig(double initial_p_mevc) {
    RecoConfig config;
    config.enable_rk = true;
    config.enable_multi_dim = false;
    config.enable_nn = false;
    config.rk_fit_mode = RkFitMode::kFixedTargetPdcOnly;
    config.initial_p_mevc = initial_p_mevc;
    config.max_iterations = 80;
    config.rk_step_mm = 5.0;
    config.tolerance_mm = 3.5;
    config.compute_uncertainty = true;
    config.compute_posterior_laplace = true;
    return config;
}

PDCInputTrack MakeSyntheticCurvedTrack(MagneticField* mag_field,
                                       const TVector3& target_pos,
                                       const TVector3& truth_momentum) {
    const double proton_mass = 938.2720813;
    const double energy =
        std::sqrt(truth_momentum.Mag2() + proton_mass * proton_mass);
    TLorentzVector truth_p4(truth_momentum.X(), truth_momentum.Y(),
                            truth_momentum.Z(), energy);

    ParticleTrajectory tracer(mag_field);
    tracer.SetStepSize(5.0);
    tracer.SetMaxDistance(1400.0);
    tracer.SetMaxTime(120.0);
    tracer.SetMinMomentum(5.0);
    const std::vector<ParticleTrajectory::TrajectoryPoint> trajectory =
        tracer.CalculateTrajectory(target_pos, truth_p4, 1.0, proton_mass);

    EXPECT_GT(trajectory.size(), 40U);
    PDCInputTrack track;
    track.pdc1 = trajectory[trajectory.size() / 3U].position;
    track.pdc2 = trajectory[(trajectory.size() * 2U) / 3U].position;
    return track;
}

struct Fixture {
    MagneticField mag_field;
    TargetConstraint target;
    RecoConfig config;
    PDCInputTrack track;
};

Fixture MakeFixture(const std::string& stem, const TVector3& truth_momentum) {
    Fixture fixture;
    const std::string field_path = WriteConstantFieldMap(stem, 0.56);
    EXPECT_TRUE(fixture.mag_field.LoadFieldMap(field_path));
    fixture.mag_field.SetRotationAngle(0.0);
    fixture.target = MakeConstraint();
    fixture.config = MakeConfig(truth_momentum.Mag());
    fixture.track = MakeSyntheticCurvedTrack(&fixture.mag_field,
                                             fixture.target.target_position,
                                             truth_momentum);
    return fixture;
}

}  // namespace

TEST(PDCErrorAnalysisTest, AnalyzeMatchesCentralRuntimeAndFillsAllMainOutputs) {
    Fixture fixture =
        MakeFixture("pdc_error_analysis_main", TVector3(95.0, 18.0, 685.0));

    PDCMomentumReconstructor reconstructor(&fixture.mag_field);
    const RecoResult runtime_result =
        reconstructor.ReconstructRK(fixture.track, fixture.target, fixture.config);
    ASSERT_EQ(runtime_result.status, SolverStatus::kSuccess);

    PDCErrorAnalysis analysis(&fixture.mag_field);
    MonteCarloConfig mc_config;
    mc_config.n_samples = 48;
    mc_config.seed = 1337U;

    ProfileLikelihoodConfig profile_config;
    profile_config.n_points_per_parameter = 11;
    profile_config.step_fraction = 0.25;

    BayesianConfig bayesian_config;
    bayesian_config.use_mcmc = false;

    const ErrorAnalysisResult result = analysis.Analyze(
        fixture.track,
        fixture.target,
        fixture.config,
        PDCErrorModel{},
        mc_config,
        profile_config,
        bayesian_config
    );

    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.central_fit.status, SolverStatus::kSuccess);
    EXPECT_NEAR(result.central_fit.p4_at_target.Px(), runtime_result.p4_at_target.Px(), 1.0e-9);
    EXPECT_NEAR(result.central_fit.p4_at_target.Py(), runtime_result.p4_at_target.Py(), 1.0e-9);
    EXPECT_NEAR(result.central_fit.p4_at_target.Pz(), runtime_result.p4_at_target.Pz(), 1.0e-9);
    EXPECT_TRUE(result.central_fit.uncertainty_valid);
    EXPECT_TRUE(result.central_fit.posterior_valid);
    EXPECT_TRUE(result.mc_valid);
    EXPECT_TRUE(result.profile_valid);
    EXPECT_TRUE(result.bayesian_valid);
    EXPECT_GT(result.error_summary[0].fisher_sigma, 0.0);
    EXPECT_GT(result.error_summary[0].mc_sigma, 0.0);
    EXPECT_GT(result.error_summary[0].profile_sigma, 0.0);
    EXPECT_GT(result.error_summary[0].laplace_sigma, 0.0);
}

TEST(PDCErrorAnalysisTest, ProfileLikelihoodOnlyScansActiveParametersInFixedTargetMode) {
    Fixture fixture =
        MakeFixture("pdc_error_analysis_profile", TVector3(102.0, -14.0, 700.0));

    PDCErrorAnalysis analysis(&fixture.mag_field);
    PDCMomentumReconstructor reconstructor(&fixture.mag_field);
    const RecoResult central_fit =
        reconstructor.ReconstructRK(fixture.track, fixture.target, fixture.config);
    ASSERT_EQ(central_fit.status, SolverStatus::kSuccess);

    ProfileLikelihoodConfig profile_config;
    profile_config.n_points_per_parameter = 9;
    profile_config.step_fraction = 0.25;
    const auto profiles = analysis.ComputeProfileLikelihood(
        fixture.track,
        fixture.target,
        fixture.config,
        profile_config,
        central_fit
    );

    EXPECT_FALSE(profiles[0].valid);
    EXPECT_FALSE(profiles[1].valid);
    EXPECT_TRUE(profiles[2].valid);
    EXPECT_TRUE(profiles[3].valid);
    EXPECT_TRUE(profiles[4].valid);
    EXPECT_GT(profiles[2].sigma_minus, 0.0);
    EXPECT_GT(profiles[2].sigma_plus, 0.0);
    EXPECT_EQ(profiles[2].scan_values.size(), 9U);
    EXPECT_EQ(profiles[2].scan_chi2.size(), 9U);
}

TEST(PDCErrorAnalysisTest, EvaluateChi2IsLocallyMinimalNearCentralFit) {
    Fixture fixture =
        MakeFixture("pdc_error_analysis_chi2", TVector3(88.0, 12.0, 672.0));

    PDCMomentumReconstructor reconstructor(&fixture.mag_field);
    const RecoResult central_fit =
        reconstructor.ReconstructRK(fixture.track, fixture.target, fixture.config);
    ASSERT_EQ(central_fit.status, SolverStatus::kSuccess);

    const double pz = central_fit.p4_at_target.Pz();
    ASSERT_GT(std::abs(pz), 1.0e-9);
    const double dx =
        central_fit.fit_start_position.X() - fixture.target.target_position.X();
    const double dy =
        central_fit.fit_start_position.Y() - fixture.target.target_position.Y();
    const double u = central_fit.p4_at_target.Px() / pz;
    const double v = central_fit.p4_at_target.Py() / pz;
    const double p = central_fit.p4_at_target.P();

    PDCErrorAnalysis analysis(&fixture.mag_field);
    const double chi2_center = analysis.EvaluateChi2(
        dx, dy, u, v, p, fixture.track, fixture.target, fixture.config
    );
    const double chi2_shifted = analysis.EvaluateChi2(
        dx, dy, u + 0.03, v - 0.02, p * 0.98,
        fixture.track, fixture.target, fixture.config
    );

    EXPECT_TRUE(std::isfinite(chi2_center));
    EXPECT_TRUE(std::isfinite(chi2_shifted));
    EXPECT_LE(chi2_center, chi2_shifted + 1.0e-6);
}

TEST(PDCErrorAnalysisTest, BayesianPosteriorMcmcProducesCredibleIntervalsAndDiagnostics) {
    Fixture fixture =
        MakeFixture("pdc_error_analysis_mcmc", TVector3(92.0, 10.0, 690.0));

    PDCMomentumReconstructor reconstructor(&fixture.mag_field);
    const RecoResult central_fit =
        reconstructor.ReconstructRK(fixture.track, fixture.target, fixture.config);
    ASSERT_EQ(central_fit.status, SolverStatus::kSuccess);

    BayesianConfig bayesian_config;
    bayesian_config.use_mcmc = true;
    bayesian_config.use_laplace = true;
    bayesian_config.mcmc_n_samples = 160;
    bayesian_config.mcmc_burn_in = 80;
    bayesian_config.mcmc_thin = 2;
    bayesian_config.mcmc_proposal_scale = 0.8;

    PDCErrorAnalysis analysis(&fixture.mag_field);
    const auto bayesian = analysis.ComputeBayesianPosterior(
        fixture.track,
        fixture.target,
        fixture.config,
        bayesian_config,
        central_fit
    );

    ASSERT_TRUE(bayesian.valid);
    ASSERT_TRUE(bayesian.mcmc_valid);
    EXPECT_TRUE(bayesian.mcmc_px.valid);
    EXPECT_TRUE(bayesian.mcmc_py.valid);
    EXPECT_TRUE(bayesian.mcmc_pz.valid);
    EXPECT_GT(bayesian.mcmc_effective_sample_size, 5.0);
    EXPECT_TRUE(std::isfinite(bayesian.geweke_z_score));
    EXPECT_GT(bayesian.mcmc_acceptance_rate, 0.0);
    EXPECT_LT(bayesian.mcmc_acceptance_rate, 1.0);
}
