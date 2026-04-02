#include <gtest/gtest.h>

#include "ParticleTrajectory.hh"
#include "PDCMomentumReconstructor.hh"
#include "PDCRecoRuntime.hh"
#include "TargetReconstructor.hh"

#include <array>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

using analysis::pdc::anaroot_like::PDCInputTrack;
using analysis::pdc::anaroot_like::PDCMomentumReconstructor;
using analysis::pdc::anaroot_like::RecoConfig;
using analysis::pdc::anaroot_like::RecoResult;
using analysis::pdc::anaroot_like::RkFitMode;
using analysis::pdc::anaroot_like::SolveMethod;
using analysis::pdc::anaroot_like::SolverStatus;
using analysis::pdc::anaroot_like::TargetConstraint;

namespace {

PDCInputTrack MakeSimpleTrack() {
    PDCInputTrack track;
    track.pdc1.SetXYZ(10.0, 0.0, 1000.0);
    track.pdc2.SetXYZ(20.0, 0.0, 2000.0);
    return track;
}

TargetConstraint MakeConstraint() {
    TargetConstraint target;
    target.target_position.SetXYZ(0.0, 0.0, 0.0);
    target.mass_mev = 938.2720813;
    target.charge_e = 1.0;
    return target;
}

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

PDCInputTrack MakeSyntheticCurvedTrack(MagneticField* mag_field,
                                       const TVector3& target_pos,
                                       const TVector3& truth_momentum) {
    const double proton_mass = 938.2720813;
    const double energy = std::sqrt(truth_momentum.Mag2() + proton_mass * proton_mass);
    TLorentzVector truth_p4(truth_momentum.X(), truth_momentum.Y(), truth_momentum.Z(), energy);

    ParticleTrajectory tracer(mag_field);
    tracer.SetStepSize(5.0);
    tracer.SetMaxDistance(1400.0);
    tracer.SetMaxTime(120.0);
    tracer.SetMinMomentum(5.0);
    const std::vector<ParticleTrajectory::TrajectoryPoint> trajectory =
        tracer.CalculateTrajectory(target_pos, truth_p4, 1.0, proton_mass);

    EXPECT_GT(trajectory.size(), 40U);
    PDCInputTrack track;
    track.pdc1 = trajectory[trajectory.size() / 3].position;
    track.pdc2 = trajectory[(trajectory.size() * 2) / 3].position;
    return track;
}

RecoConfig MakeNnOnlyConfig(const std::string& model_path) {
    RecoConfig config;
    config.enable_nn = true;
    config.enable_rk = false;
    config.enable_multi_dim = false;
    config.nn_model_json_path = model_path;
    config.p_min_mevc = 50.0;
    config.p_max_mevc = 5000.0;
    return config;
}

RecoConfig MakeRkOnlyConfig(double initial_p_mevc, RkFitMode mode = RkFitMode::kThreePointFree) {
    RecoConfig config;
    config.enable_rk = true;
    config.enable_multi_dim = false;
    config.enable_nn = false;
    config.rk_fit_mode = mode;
    config.initial_p_mevc = initial_p_mevc;
    config.max_iterations = 80;
    config.rk_step_mm = 5.0;
    config.tolerance_mm = 3.0;
    config.compute_uncertainty = true;
    config.compute_posterior_laplace = true;
    return config;
}

}  // namespace

TEST(PDCMomentumReconstructorTest, RuntimeParsesRkModesAndRejectsRemovedMatrixBackend) {
    // [EN] Keep RK mode spellings centralized and fail fast when an operator still requests the removed matrix backend. / [CN] 将RK模式字符串统一到共享解析器，并在操作员仍请求已删除的matrix后端时快速报错。
    EXPECT_EQ(analysis::pdc::anaroot_like::ParseRkFitMode("two-point-backprop"), RkFitMode::kTwoPointBackprop);
    EXPECT_EQ(analysis::pdc::anaroot_like::ParseRkFitMode("target-xy-prior"), RkFitMode::kThreePointFree);
    EXPECT_EQ(analysis::pdc::anaroot_like::ParseRkFitMode("three-point-free"), RkFitMode::kThreePointFree);
    EXPECT_EQ(analysis::pdc::anaroot_like::ParseRkFitMode("fixed-target-pdc-only"), RkFitMode::kFixedTargetPdcOnly);
    EXPECT_EQ(analysis::pdc::anaroot_like::ParseRkFitMode("pdc_only"), RkFitMode::kFixedTargetPdcOnly);
    EXPECT_THROW(analysis::pdc::anaroot_like::ParseRuntimeBackend("matrix"), std::runtime_error);
}

TEST(PDCMomentumReconstructorTest, NeuralNetworkInferenceWorksWithoutMagField) {
    // [EN] NN backend should reconstruct directly from two PDC points without field-map dependency. / [CN] NN后端应可直接由两个PDC点重建，不依赖磁场图。
    const std::string model_path = "/tmp/pdc_nn_model_test.json";
    {
        std::ofstream fout(model_path);
        ASSERT_TRUE(fout.is_open());
        fout << "{\n";
        fout << "  \"format\": \"smsimulator_pdc_mlp_v1\",\n";
        fout << "  \"x_mean\": [0,0,0,0,0,0],\n";
        fout << "  \"x_std\": [1,1,1,1,1,1],\n";
        fout << "  \"layers\": [\n";
        fout << "    {\n";
        fout << "      \"in_dim\": 6,\n";
        fout << "      \"out_dim\": 3,\n";
        fout << "      \"weights\": [\n";
        fout << "        1,0,0,0,0,0,\n";
        fout << "        0,1,0,0,0,0,\n";
        fout << "        0,0,0,0,0,0\n";
        fout << "      ],\n";
        fout << "      \"bias\": [0,0,600]\n";
        fout << "    }\n";
        fout << "  ]\n";
        fout << "}\n";
    }

    PDCMomentumReconstructor reconstructor(nullptr);

    const RecoConfig config = MakeNnOnlyConfig(model_path);

    const auto result = reconstructor.Reconstruct(MakeSimpleTrack(), MakeConstraint(), config);
    EXPECT_EQ(result.status, SolverStatus::kSuccess);
    EXPECT_EQ(result.method_used, SolveMethod::kNeuralNetwork);
    EXPECT_TRUE(std::isfinite(result.p4_at_target.P()));
    EXPECT_GT(result.p4_at_target.P(), 0.0);
    EXPECT_GT(result.p4_at_target.Pz(), 500.0);
}

TEST(PDCMomentumReconstructorTest, NeuralNetworkInferenceAppliesTargetDenormalization) {
    // [EN] Exported NN models may store normalized targets; C++ inference must denormalize them before building the 4-vector. / [CN] 导出的NN模型可能保存归一化目标量；C++推理在构建四动量前必须先做反归一化。
    const std::string model_path = "/tmp/pdc_nn_model_test_denorm.json";
    {
        std::ofstream fout(model_path);
        ASSERT_TRUE(fout.is_open());
        fout << "{\n";
        fout << "  \"format\": \"smsimulator_pdc_mlp_v1\",\n";
        fout << "  \"x_mean\": [0,0,0,0,0,0],\n";
        fout << "  \"x_std\": [1,1,1,1,1,1],\n";
        fout << "  \"target_normalization\": \"zscore\",\n";
        fout << "  \"y_mean\": [10,20,600],\n";
        fout << "  \"y_std\": [2,3,4],\n";
        fout << "  \"layers\": [\n";
        fout << "    {\n";
        fout << "      \"in_dim\": 6,\n";
        fout << "      \"out_dim\": 3,\n";
        fout << "      \"weights\": [\n";
        fout << "        0,0,0,0,0,0,\n";
        fout << "        0,0,0,0,0,0,\n";
        fout << "        0,0,0,0,0,0\n";
        fout << "      ],\n";
        fout << "      \"bias\": [1,2,3]\n";
        fout << "    }\n";
        fout << "  ]\n";
        fout << "}\n";
    }

    PDCMomentumReconstructor reconstructor(nullptr);

    const RecoConfig config = MakeNnOnlyConfig(model_path);

    const auto result = reconstructor.Reconstruct(MakeSimpleTrack(), MakeConstraint(), config);
    EXPECT_EQ(result.status, SolverStatus::kSuccess);
    EXPECT_EQ(result.method_used, SolveMethod::kNeuralNetwork);
    EXPECT_NEAR(result.p4_at_target.Px(), 12.0, 1.0e-9);
    EXPECT_NEAR(result.p4_at_target.Py(), 26.0, 1.0e-9);
    EXPECT_NEAR(result.p4_at_target.Pz(), 612.0, 1.0e-9);
}

TEST(PDCMomentumReconstructorTest, RKTwoPointBackpropMatchesLegacyCompatibilitySolver) {
    // [EN] Two-point backprop is a compatibility mode, so the new runtime should reproduce the legacy solver result instead of inventing a second behavior. / [CN] 两点反推属于兼容模式，因此新运行时应复现legacy求解结果，而不是再发明第二套行为。
    const std::string field_path = WriteConstantFieldMap("pdc_constant_field_rk_uncertainty", 0.6);

    MagneticField mag_field;
    ASSERT_TRUE(mag_field.LoadFieldMap(field_path));
    mag_field.SetRotationAngle(0.0);

    const TVector3 target_pos(0.0, 0.0, 0.0);
    const TVector3 truth_momentum(120.0, 25.0, 700.0);
    const PDCInputTrack track = MakeSyntheticCurvedTrack(&mag_field, target_pos, truth_momentum);

    TargetConstraint target = MakeConstraint();
    target.pdc_sigma_mm = 2.0;
    target.target_sigma_xy_mm = 5.0;

    RecoConfig config = MakeRkOnlyConfig(truth_momentum.Mag() * 0.95, RkFitMode::kTwoPointBackprop);
    config.tolerance_mm = 8.0;

    PDCMomentumReconstructor reconstructor(&mag_field);
    const RecoResult result = reconstructor.ReconstructRK(track, target, config);

    TargetReconstructor legacy_reconstructor(&mag_field);
    legacy_reconstructor.SetTrajectoryStepSize(config.rk_step_mm);
    RecoTrack legacy_track(track.pdc1, track.pdc2);
    legacy_track.pdgCode = 2212;
    const int search_rounds = std::max(1, std::min(8, config.max_iterations / 10));
    const TargetReconstructionResult legacy_result = legacy_reconstructor.ReconstructAtTargetWithDetails(
        legacy_track,
        target.target_position,
        false,
        config.p_min_mevc,
        config.p_max_mevc,
        config.tolerance_mm,
        search_rounds
    );

    EXPECT_TRUE(result.status == SolverStatus::kSuccess || result.status == SolverStatus::kNotConverged);
    EXPECT_EQ(result.method_used, SolveMethod::kRungeKutta);
    EXPECT_TRUE(std::isfinite(result.p4_at_target.P()));
    EXPECT_GT(result.p4_at_target.P(), 0.0);
    EXPECT_TRUE(std::isfinite(result.min_distance_mm));
    EXPECT_TRUE(std::isfinite(result.fit_start_position.X()));
    EXPECT_TRUE(std::isfinite(result.fit_start_position.Y()));
    EXPECT_TRUE(std::isfinite(result.fit_start_position.Z()));
    EXPECT_TRUE(std::isfinite(legacy_result.bestMomentum.P()));
    EXPECT_GT(legacy_result.bestMomentum.P(), 0.0);
    EXPECT_NEAR(result.p4_at_target.Px(), legacy_result.bestMomentum.Px(), 1.0e-9);
    EXPECT_NEAR(result.p4_at_target.Py(), legacy_result.bestMomentum.Py(), 1.0e-9);
    EXPECT_NEAR(result.p4_at_target.Pz(), legacy_result.bestMomentum.Pz(), 1.0e-9);
    EXPECT_NEAR(result.min_distance_mm, legacy_result.finalDistance, 1.0e-9);
    EXPECT_EQ(result.iterations, search_rounds);
    const SolverStatus expected_status =
        (legacy_result.success || legacy_result.finalDistance <= config.tolerance_mm)
            ? SolverStatus::kSuccess
            : SolverStatus::kNotConverged;
    EXPECT_EQ(result.status, expected_status);
    EXPECT_TRUE((result.fit_start_position - track.pdc1).Mag() < 1.0e-9 ||
                (result.fit_start_position - track.pdc2).Mag() < 1.0e-9);
}

TEST(PDCMomentumReconstructorTest, RKFixedTargetPdcOnlyUsesTwoPointLossWithFixedVertex) {
    // [EN] Fixed-target two-point RK mode should keep the start vertex pinned at target and fit momentum only from PDC residuals. / [CN] 固定靶点两点RK模式应将起点钉在靶点，只通过PDC残差拟合动量。
    const std::string field_path = WriteConstantFieldMap("pdc_constant_field_rk_fixed_target", 0.58);

    MagneticField mag_field;
    ASSERT_TRUE(mag_field.LoadFieldMap(field_path));
    mag_field.SetRotationAngle(0.0);

    const TVector3 target_pos(0.0, 0.0, 0.0);
    const TVector3 truth_momentum(110.0, 20.0, 690.0);
    const PDCInputTrack track = MakeSyntheticCurvedTrack(&mag_field, target_pos, truth_momentum);

    TargetConstraint target = MakeConstraint();
    target.pdc_sigma_mm = 2.0;
    target.target_sigma_xy_mm = 5.0;

    RecoConfig config = MakeRkOnlyConfig(truth_momentum.Mag(), RkFitMode::kFixedTargetPdcOnly);

    PDCMomentumReconstructor reconstructor(&mag_field);
    const auto result = reconstructor.ReconstructRK(track, target, config);

    EXPECT_EQ(result.status, SolverStatus::kSuccess);
    EXPECT_TRUE(std::isfinite(result.chi2_raw));
    EXPECT_TRUE(std::isfinite(result.chi2_reduced));
    EXPECT_EQ(result.ndf, 3);
    EXPECT_TRUE(result.uncertainty_valid);
    EXPECT_TRUE(result.posterior_valid);
    EXPECT_NEAR(result.fit_start_position.X(), target_pos.X(), 1.0e-9);
    EXPECT_NEAR(result.fit_start_position.Y(), target_pos.Y(), 1.0e-9);
    EXPECT_NEAR(result.fit_start_position.Z(), target_pos.Z(), 1.0e-9);
    EXPECT_NEAR(result.p4_at_target.Px(), truth_momentum.X(), 40.0);
    EXPECT_NEAR(result.p4_at_target.Py(), truth_momentum.Y(), 25.0);
    EXPECT_NEAR(result.p4_at_target.Pz(), truth_momentum.Z(), 60.0);
    EXPECT_GT(result.px_interval.sigma, 0.0);
    EXPECT_GT(result.px_credible.sigma, 0.0);
}

TEST(PDCMomentumReconstructorTest, RKFixedTargetPdcOnlySupportsPerPlaneCovariance) {
    // [EN] Fixed-target two-point mode should keep covariance whitening and posterior intervals available. / [CN] 固定靶点两点模式应继续支持协方差白化与后验区间输出。
    const std::string field_path = WriteConstantFieldMap("pdc_constant_field_rk_fixed_target_covariance", 0.54);

    MagneticField mag_field;
    ASSERT_TRUE(mag_field.LoadFieldMap(field_path));
    mag_field.SetRotationAngle(0.0);

    const TVector3 target_pos(0.0, 0.0, 0.0);
    const TVector3 truth_momentum(85.0, -12.0, 675.0);
    const PDCInputTrack track = MakeSyntheticCurvedTrack(&mag_field, target_pos, truth_momentum);

    TargetConstraint target = MakeConstraint();
    target.use_pdc_covariance = true;
    target.pdc1_cov_mm2 = {4.0, 0.4, 0.2,
                           0.4, 2.25, 0.1,
                           0.2, 0.1, 5.0};
    target.pdc2_cov_mm2 = {3.5, 0.3, 0.15,
                           0.3, 2.0, 0.05,
                           0.15, 0.05, 4.5};

    RecoConfig config = MakeRkOnlyConfig(truth_momentum.Mag(), RkFitMode::kFixedTargetPdcOnly);
    config.tolerance_mm = 3.5;

    PDCMomentumReconstructor reconstructor(&mag_field);
    const auto result = reconstructor.ReconstructRK(track, target, config);

    EXPECT_EQ(result.status, SolverStatus::kSuccess);
    EXPECT_EQ(result.ndf, 3);
    EXPECT_TRUE(result.used_measurement_covariance);
    EXPECT_TRUE(result.uncertainty_valid);
    EXPECT_TRUE(result.posterior_valid);
    EXPECT_TRUE(result.px_interval.valid);
    EXPECT_TRUE(result.px_credible.valid);
    EXPECT_GT(result.px_interval.sigma, 0.0);
    EXPECT_GT(result.px_credible.sigma, 0.0);
    EXPECT_NEAR(result.fit_start_position.X(), target_pos.X(), 1.0e-9);
    EXPECT_NEAR(result.fit_start_position.Y(), target_pos.Y(), 1.0e-9);
    EXPECT_NEAR(result.fit_start_position.Z(), target_pos.Z(), 1.0e-9);
}
