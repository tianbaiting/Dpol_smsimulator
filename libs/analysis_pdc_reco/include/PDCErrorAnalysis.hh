#ifndef ANALYSIS_PDC_ERROR_ANALYSIS_HH
#define ANALYSIS_PDC_ERROR_ANALYSIS_HH

#include "MagneticField.hh"
#include "PDCMomentumReconstructor.hh"
#include "PDCRecoTypes.hh"

#include "TVector3.h"

#include <array>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace analysis::pdc::anaroot_like {

// [EN] Error analysis class for PDC target momentum reconstruction
// [CN] PDC靶点动量重建误差分析类
class PDCErrorAnalysis {
public:
    explicit PDCErrorAnalysis(MagneticField* magnetic_field);
    ~PDCErrorAnalysis();

    // [EN] Full error analysis pipeline combining all methods
    // [CN] 完整误差分析流程，组合所有方法
    ErrorAnalysisResult Analyze(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config,
        const PDCErrorModel& error_model,
        const MonteCarloConfig& mc_config,
        const ProfileLikelihoodConfig& profile_config,
        const BayesianConfig& bayesian_config
    ) const;

    // [EN] Monte Carlo error propagation
    // [CN] Monte Carlo误差传播
    MonteCarloResult RunMonteCarlo(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config,
        const PDCErrorModel& error_model,
        const MonteCarloConfig& mc_config,
        const RecoResult& central_fit
    ) const;

    // [EN] Profile likelihood scanning for frequentist confidence intervals
    // [CN] Profile likelihood扫描获取频率派置信区间
    std::array<ProfileLikelihoodResult, 5> ComputeProfileLikelihood(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config,
        const ProfileLikelihoodConfig& profile_config,
        const RecoResult& central_fit
    ) const;

    // [EN] Bayesian posterior estimation (Laplace + optional MCMC)
    // [CN] 贝叶斯后验估计（Laplace + 可选MCMC）
    BayesianResult ComputeBayesianPosterior(
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config,
        const BayesianConfig& bayesian_config,
        const RecoResult& central_fit
    ) const;

    // [EN] Expose chi2 evaluation for external profiling
    // [CN] 暴露chi2评估用于外部profiling
    double EvaluateChi2(
        double dx, double dy, double u, double v, double q,
        const PDCInputTrack& track,
        const TargetConstraint& target,
        const RecoConfig& config
    ) const;

private:
    // [EN] Perturb PDC track points with Gaussian noise according to error model
    // [CN] 根据误差模型对PDC轨迹点添加高斯噪声
    PDCInputTrack PerturbTrack(
        const PDCInputTrack& track,
        const PDCErrorModel& error_model,
        std::mt19937& rng
    ) const;

    // [EN] Compute U/V direction vectors for PDC coordinate system
    // [CN] 计算PDC坐标系的U/V方向向量
    static std::pair<TVector3, TVector3> ComputeUVDirections(double pdc_angle_deg = 57.0);

    // [EN] Compute component statistics from momentum samples
    // [CN] 从动量样本计算分量统计
    static ComponentStats ComputeComponentStats(const std::vector<double>& values);

    // [EN] Compute correlation matrix from momentum samples
    // [CN] 从动量样本计算相关矩阵
    static std::array<double, 9> ComputeCorrelationMatrix(
        const std::vector<TVector3>& momenta
    );

    // [EN] MCMC Geweke convergence diagnostic
    // [CN] MCMC Geweke收敛诊断
    static double ComputeGewekeZScore(
        const std::vector<double>& chain,
        double first_fraction = 0.1,
        double last_fraction = 0.5
    );

    // [EN] Compute effective sample size for MCMC chain
    // [CN] 计算MCMC链的有效样本大小
    static double ComputeEffectiveSampleSize(const std::vector<double>& chain);

    MagneticField* fMagneticField = nullptr;
    mutable std::unique_ptr<PDCMomentumReconstructor> fReconstructor;
};

}  // namespace analysis::pdc::anaroot_like

#endif  // ANALYSIS_PDC_ERROR_ANALYSIS_HH
