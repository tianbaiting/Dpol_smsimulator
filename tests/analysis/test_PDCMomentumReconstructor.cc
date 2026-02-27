#include <gtest/gtest.h>

#include "PDCMomentumReconstructor.hh"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>

using analysis::pdc::anaroot_like::PDCInputTrack;
using analysis::pdc::anaroot_like::PDCMomentumReconstructor;
using analysis::pdc::anaroot_like::RecoConfig;
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
