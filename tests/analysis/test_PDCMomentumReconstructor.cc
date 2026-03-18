#include <gtest/gtest.h>

#include "ParticleTrajectory.hh"
#include "PDCMomentumReconstructor.hh"

#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

using analysis::pdc::anaroot_like::PDCInputTrack;
using analysis::pdc::anaroot_like::PDCMomentumReconstructor;
using analysis::pdc::anaroot_like::RecoConfig;
using analysis::pdc::anaroot_like::RkFitMode;
using analysis::pdc::anaroot_like::SolveMethod;
using analysis::pdc::anaroot_like::SolverStatus;
using analysis::pdc::anaroot_like::TargetConstraint;

namespace {

class ScopedEnvValue {
public:
    ScopedEnvValue(const char* name, const char* value) : fName(name) {
        const char* old = std::getenv(name);
        if (old) {
            fHadValue = true;
            fOldValue = old;
        }
        if (value) {
            setenv(name, value, 1);
        } else {
            unsetenv(name);
        }
    }

    ~ScopedEnvValue() {
        if (fHadValue) {
            setenv(fName.c_str(), fOldValue.c_str(), 1);
        } else {
            unsetenv(fName.c_str());
        }
    }

private:
    std::string fName;
    std::string fOldValue;
    bool fHadValue = false;
};

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

double TraceFromArray(const std::array<double, 9>& values) {
    return values[0] + values[4] + values[8];
}

}  // namespace

TEST(PDCMomentumReconstructorTest, MatrixReportsNotAvailableWhenEnvMissing) {
    // [EN] Matrix fallback should explicitly report unavailable when ANAROOT matrix env vars are missing. / [CN] 当ANAROOT矩阵环境变量缺失时，矩阵回退应明确返回不可用状态。
    const ScopedEnvValue env0("SAMURAI_MATRIX0TH_FILE", nullptr);
    const ScopedEnvValue env1("SAMURAI_MATRIX1ST_FILE", nullptr);

    auto* fakeField = reinterpret_cast<MagneticField*>(0x1);
    PDCMomentumReconstructor reconstructor(fakeField);

    RecoConfig config;
    config.enable_rk = false;
    config.enable_multi_dim = false;
    config.enable_matrix = true;

    const auto result = reconstructor.ReconstructMatrix(MakeSimpleTrack(), MakeConstraint(), config);
    EXPECT_EQ(result.status, SolverStatus::kNotAvailable);
}

TEST(PDCMomentumReconstructorTest, MatrixParsesSamuraiStyleFiles) {
    // [EN] Use ANAROOT-compatible 0th/1st matrix text format to validate fallback solver plumbing. / [CN] 使用ANAROOT兼容的0阶/1阶矩阵文本格式验证回退求解链路。
    const std::string mat0_path = "/tmp/pdc_matrix0th_test.txt";
    const std::string mat1_path = "/tmp/pdc_matrix1st_test.txt";
    {
        std::ofstream mat0(mat0_path);
        std::ofstream mat1(mat1_path);
        ASSERT_TRUE(mat0.is_open());
        ASSERT_TRUE(mat1.is_open());
        mat0 << "0 0\n";
        mat1 << "0 1 0\n";
        mat1 << "0 0 1\n";
        mat1 << "0 0 0\n";
    }

    const ScopedEnvValue env0("SAMURAI_MATRIX0TH_FILE", mat0_path.c_str());
    const ScopedEnvValue env1("SAMURAI_MATRIX1ST_FILE", mat1_path.c_str());

    auto* fakeField = reinterpret_cast<MagneticField*>(0x1);
    PDCMomentumReconstructor reconstructor(fakeField);

    RecoConfig config;
    config.enable_rk = false;
    config.enable_multi_dim = false;
    config.enable_matrix = true;
    config.center_brho_tm = 7.2751;

    const auto result = reconstructor.ReconstructMatrix(MakeSimpleTrack(), MakeConstraint(), config);
    EXPECT_EQ(result.status, SolverStatus::kSuccess);
    EXPECT_TRUE(std::isfinite(result.p4_at_target.P()));
    EXPECT_GT(result.p4_at_target.P(), 0.0);
    EXPECT_TRUE(std::isfinite(result.brho_tm));
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

    RecoConfig config;
    config.enable_nn = true;
    config.nn_model_json_path = model_path;
    config.enable_rk = false;
    config.enable_multi_dim = false;
    config.enable_matrix = false;
    config.p_min_mevc = 50.0;
    config.p_max_mevc = 5000.0;

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

    RecoConfig config;
    config.enable_nn = true;
    config.nn_model_json_path = model_path;
    config.enable_rk = false;
    config.enable_multi_dim = false;
    config.enable_matrix = false;
    config.p_min_mevc = 50.0;
    config.p_max_mevc = 5000.0;

    const auto result = reconstructor.Reconstruct(MakeSimpleTrack(), MakeConstraint(), config);
    EXPECT_EQ(result.status, SolverStatus::kSuccess);
    EXPECT_EQ(result.method_used, SolveMethod::kNeuralNetwork);
    EXPECT_NEAR(result.p4_at_target.Px(), 12.0, 1.0e-9);
    EXPECT_NEAR(result.p4_at_target.Py(), 26.0, 1.0e-9);
    EXPECT_NEAR(result.p4_at_target.Pz(), 612.0, 1.0e-9);
}

TEST(PDCMomentumReconstructorTest, RKReportsLocalUncertaintyAndIntervals) {
    // [EN] RK solver should expose raw/reduced chi2, local covariance, and Gaussianized intervals after convergence. / [CN] RK求解器在收敛后应输出原始/约化chi2、局部协方差和高斯化区间。
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

    RecoConfig config;
    config.enable_rk = true;
    config.enable_multi_dim = false;
    config.enable_matrix = false;
    config.enable_nn = false;
    config.initial_p_mevc = truth_momentum.Mag() * 0.95;
    config.max_iterations = 80;
    config.rk_step_mm = 5.0;
    config.tolerance_mm = 3.0;
    config.compute_uncertainty = true;
    config.compute_posterior_laplace = true;

    PDCMomentumReconstructor reconstructor(&mag_field);
    const auto result = reconstructor.ReconstructRK(track, target, config);

    EXPECT_EQ(result.status, SolverStatus::kSuccess);
    EXPECT_TRUE(std::isfinite(result.chi2_raw));
    EXPECT_TRUE(std::isfinite(result.chi2_reduced));
    EXPECT_GT(result.ndf, 0);
    EXPECT_TRUE(result.uncertainty_valid);
    EXPECT_TRUE(result.posterior_valid);
    EXPECT_TRUE(std::isfinite(result.normal_condition_number));
    EXPECT_FALSE(result.used_measurement_covariance);
    EXPECT_TRUE(std::isfinite(result.fit_start_position.X()));
    EXPECT_TRUE(std::isfinite(result.fit_start_position.Y()));
    EXPECT_TRUE(std::isfinite(result.fit_start_position.Z()));
    EXPECT_TRUE(result.px_interval.valid);
    EXPECT_TRUE(result.py_interval.valid);
    EXPECT_TRUE(result.pz_interval.valid);
    EXPECT_TRUE(result.p_interval.valid);
    EXPECT_LE(result.px_interval.lower95, truth_momentum.X());
    EXPECT_GE(result.px_interval.upper95, truth_momentum.X());
    EXPECT_LE(result.py_interval.lower95, truth_momentum.Y());
    EXPECT_GE(result.py_interval.upper95, truth_momentum.Y());
    EXPECT_LE(result.pz_interval.lower95, truth_momentum.Z());
    EXPECT_GE(result.pz_interval.upper95, truth_momentum.Z());
    EXPECT_NEAR(result.p4_at_target.Px(), truth_momentum.X(), 40.0);
    EXPECT_NEAR(result.p4_at_target.Py(), truth_momentum.Y(), 25.0);
    EXPECT_NEAR(result.p4_at_target.Pz(), truth_momentum.Z(), 60.0);
    EXPECT_LE(TraceFromArray(result.posterior_momentum_covariance),
              TraceFromArray(result.momentum_covariance) + 1.0e-9);
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

    RecoConfig config;
    config.enable_rk = true;
    config.enable_multi_dim = false;
    config.enable_matrix = false;
    config.enable_nn = false;
    config.rk_fit_mode = RkFitMode::kFixedTargetPdcOnly;
    config.initial_p_mevc = truth_momentum.Mag();
    config.max_iterations = 80;
    config.rk_step_mm = 5.0;
    config.tolerance_mm = 3.0;
    config.compute_uncertainty = true;
    config.compute_posterior_laplace = true;

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

TEST(PDCMomentumReconstructorTest, RKUsesPerPlaneMeasurementCovariance) {
    // [EN] Full 3x3 PDC covariance inputs should activate whitened residuals and still return usable intervals. / [CN] 完整3x3的PDC协方差输入应启用白化残差，并仍能返回可用区间。
    const std::string field_path = WriteConstantFieldMap("pdc_constant_field_rk_covariance", 0.55);

    MagneticField mag_field;
    ASSERT_TRUE(mag_field.LoadFieldMap(field_path));
    mag_field.SetRotationAngle(0.0);

    const TVector3 target_pos(0.0, 0.0, 0.0);
    const TVector3 truth_momentum(90.0, -15.0, 680.0);
    const PDCInputTrack track = MakeSyntheticCurvedTrack(&mag_field, target_pos, truth_momentum);

    TargetConstraint target = MakeConstraint();
    target.use_pdc_covariance = true;
    target.pdc1_cov_mm2 = {4.0, 0.4, 0.2,
                           0.4, 2.25, 0.1,
                           0.2, 0.1, 5.0};
    target.pdc2_cov_mm2 = {3.5, 0.3, 0.15,
                           0.3, 2.0, 0.05,
                           0.15, 0.05, 4.5};

    RecoConfig config;
    config.enable_rk = true;
    config.enable_multi_dim = false;
    config.enable_matrix = false;
    config.enable_nn = false;
    config.initial_p_mevc = truth_momentum.Mag();
    config.max_iterations = 80;
    config.rk_step_mm = 5.0;
    config.tolerance_mm = 3.5;
    config.compute_uncertainty = true;
    config.compute_posterior_laplace = true;

    PDCMomentumReconstructor reconstructor(&mag_field);
    const auto result = reconstructor.ReconstructRK(track, target, config);

    EXPECT_EQ(result.status, SolverStatus::kSuccess);
    EXPECT_TRUE(result.used_measurement_covariance);
    EXPECT_TRUE(std::isfinite(result.chi2_raw));
    EXPECT_TRUE(result.uncertainty_valid);
    EXPECT_TRUE(result.posterior_valid);
    EXPECT_TRUE(result.px_credible.valid);
    EXPECT_TRUE(result.p_interval.valid);
    EXPECT_GT(result.px_interval.sigma, 0.0);
    EXPECT_GT(result.px_credible.sigma, 0.0);
    EXPECT_LE(TraceFromArray(result.posterior_momentum_covariance),
              TraceFromArray(result.momentum_covariance) + 1.0e-9);
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

    RecoConfig config;
    config.enable_rk = true;
    config.enable_multi_dim = false;
    config.enable_matrix = false;
    config.enable_nn = false;
    config.rk_fit_mode = RkFitMode::kFixedTargetPdcOnly;
    config.initial_p_mevc = truth_momentum.Mag();
    config.max_iterations = 80;
    config.rk_step_mm = 5.0;
    config.tolerance_mm = 3.5;
    config.compute_uncertainty = true;
    config.compute_posterior_laplace = true;

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
