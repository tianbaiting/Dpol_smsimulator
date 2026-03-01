#ifndef TARGET_RECONSTRUCTOR_H
#define TARGET_RECONSTRUCTOR_H

#include "TVector3.h"
#include "TLorentzVector.h"
#include "RecoEvent.hh"
#include "MagneticField.hh"
#include "ParticleTrajectory.hh"
#include "TMinuit.h"
#include <utility>
#include <vector>
#include <limits>

// 重建结果数据结构
struct TrajectoryPoint {
    TVector3 position;  // 轨迹点位置 (mm)
    TVector3 momentum;  // 轨迹点动量 (MeV/c) 
    double time;        // 时间 (ns)
};

struct TargetReconstructionResult {
    TLorentzVector bestMomentum;          // 最佳重建动量
    std::vector<TrajectoryPoint> bestTrajectory; // 最佳轨迹点
    std::vector<std::vector<TrajectoryPoint>> trialTrajectories; // 试探轨迹点（调试用）
    std::vector<double> trialMomenta;     // 试探动量值 (MeV/c)
    std::vector<double> distances;        // 对应的距离 (mm)
    double finalDistance;                 // 最终距离 (mm)
    bool success;                        // 重建是否成功
    
    // 优化步骤记录（用于可视化loss function）
    std::vector<double> optimizationSteps_P;      // 每步的动量值
    std::vector<double> optimizationSteps_Loss;   // 每步的loss（距离）
    int totalIterations;                          // 总迭代次数

    // [EN] Extra diagnostics for free three-point fit. / [CN] 自由三点拟合的附加诊断信息。
    TVector3 bestStartPos = TVector3(0, 0, 0);   // 最优发射位置
    TVector3 bestInitialMomentum = TVector3(0, 0, 0); // 最优初始动量向量
    double finalLoss = std::numeric_limits<double>::infinity();  // 归一化RMS损失
    double minDistTarget = std::numeric_limits<double>::infinity();
    double minDistPDC1 = std::numeric_limits<double>::infinity();
    double minDistPDC2 = std::numeric_limits<double>::infinity();
    int fitStatus = 0;                            // TMinuit icstat
    double edm = std::numeric_limits<double>::infinity(); // 估计到极小值距离
};

// Reconstruct momentum at a target position by back-propagating a reconstructed track.
// Uses PDC reconstruction logic: assumes RecoTrack has two points (start=PDC1, end=PDC2).
// Uses negative charge for backward propagation from the farther PDC point to target.
// Only supports proton assumption (mass = proton mass).
class TargetReconstructor {
public:
    TargetReconstructor(MagneticField* magField);
    ~TargetReconstructor();

    // [EN] Configure RK step size used by internal ParticleTrajectory calls. / [CN] 配置内部ParticleTrajectory调用使用的RK步长。
    void SetTrajectoryStepSize(double stepSize) { if (stepSize > 0.0) fTrajectoryStepSize = stepSize; }
    double GetTrajectoryStepSize() const { return fTrajectoryStepSize; }

    // Reconstruct four-momentum at the target for a given RecoTrack and target position.
    // Uses negative charge (-1) for backward propagation from farther PDC point.
    // pMin/pMax: search range in MeV/c. tol: acceptance distance in mm. maxRounds controls refinement.
    // Returns TLorentzVector(px,py,pz,E) at the point of closest approach to target.
    TLorentzVector ReconstructAtTarget(const RecoTrack& track,
                                       const TVector3& targetPos,
                                       double pMin = 50.0,
                                       double pMax = 5000.0,
                                       double tol = 1.0,
                                       int maxRounds = 4) const;
    
    // 完整重建方法：返回包含所有重建信息的结构体
    // saveTrajectories: 是否保存轨迹点数据（用于可视化调试）
    TargetReconstructionResult ReconstructAtTargetWithDetails(const RecoTrack& track,
                                                              const TVector3& targetPos,
                                                              bool saveTrajectories = false,
                                                              double pMin = 50.0,
                                                              double pMax = 5000.0,
                                                              double tol = 1.0,
                                                              int maxRounds = 4) const;

    // 梯度下降方法：更高效的优化算法
    TargetReconstructionResult ReconstructAtTargetGradientDescent(const RecoTrack& track,
                                                                  const TVector3& targetPos,
                                                                  bool saveTrajectories = false,
                                                                  double pInit = 1000.0,
                                                                  double learningRate = 50.0,
                                                                  double tol = 1.0,
                                                                  int maxIterations = 100) const;

    // [EN] Three-point constrained gradient descent: target + two PDC points constrain the track; optimize start position while fixing momentum. / [CN] 三点约束梯度下降：用靶点+两个PDC点约束轨迹，优化出射点位置并固定动量。
    // [EN] pdcSigma/targetSigma: position uncertainties (mm) for weighted loss = sum(d^2/sigma^2). / [CN] pdcSigma/targetSigma：位置测量误差(mm)，用于加权loss = sum(d^2/sigma^2)。
    TargetReconstructionResult ReconstructAtTargetThreePointGradientDescent(const RecoTrack& track,
                                                                            const TVector3& targetPos,
                                                                            bool saveTrajectories = false,
                                                                            const TVector3& fixedMomentum = TVector3(0, 0, 627.0),
                                                                            double learningRate = 0.05,
                                                                            double tol = 1.0,
                                                                            int maxIterations = 200,
                                                                            double pdcSigma = 0.5,
                                                                            double targetSigma = 5.0) const;

    // [EN] Free three-point Minuit fit: optimize emission position and momentum vector simultaneously. / [CN] 自由三点Minuit拟合：同时优化发射位置和动量向量。
    // [EN] Loss uses normalized RMS distance to target/PDC1/PDC2. / [CN] 损失函数采用到target/PDC1/PDC2的归一化RMS距离。
    TargetReconstructionResult ReconstructAtTargetThreePointFreeMinuit(const RecoTrack& track,
                                                                       const TVector3& targetPos,
                                                                       bool saveTrajectories = false,
                                                                       const TVector3& startPosInit = TVector3(std::numeric_limits<double>::quiet_NaN(),
                                                                                                               std::numeric_limits<double>::quiet_NaN(),
                                                                                                               std::numeric_limits<double>::quiet_NaN()),
                                                                       const TVector3& momentumInit = TVector3(0, 0, 627.0),
                                                                       double targetSigma = 5.0,
                                                                       double pdcSigma = 0.5,
                                                                       double tol = 1.0,
                                                                       int maxIterations = 1000,
                                                                       bool recordSteps = false) const;

    // TMinuit 优化方法：使用 ROOT 内置的 MIGRAD 算法 (拟牛顿法)
    // recordSteps: 是否记录优化步骤（仅用于调试，会影响性能）
    TargetReconstructionResult ReconstructAtTargetMinuit(const RecoTrack& track,
                                                         const TVector3& targetPos,
                                                         bool saveTrajectories = false,
                                                         double pInit = 1000.0,
                                                         double tol = 1.0,
                                                         int maxIterations = 1000,
                                                         bool recordSteps = false) const;

    // 辅助函数：计算给定动量下到目标点的最小距离（公开供测试使用）
    double CalculateMinimumDistance(double momentum, 
                                   const TVector3& startPos, 
                                   const TVector3& direction,
                                   const TVector3& targetPos,
                                   double charge, 
                                   double mass) const;

private:
    MagneticField* fMagField;
    double fProtonMass; // MeV/c^2
    double fTrajectoryStepSize; // [mm]
    
    // Static data for TMinuit callback function
    static TargetReconstructor* fgCurrentInstance;
    static TVector3 fgStartPos;
    static TVector3 fgDirection;  
    static TVector3 fgTargetPos;
    static double fgCharge;
    static double fgMass;
    static TargetReconstructionResult* fgResultPtr; // 用于记录优化步骤
    static bool fgRecordSteps; // 控制是否记录优化步骤（默认false，仅调试时启用）
    static TVector3 fgThreePDC1;
    static TVector3 fgThreePDC2;
    static TVector3 fgThreeTargetPos;
    static double fgThreePdcSigma;
    static double fgThreeTargetSigma;
    static double fgThreePMin;
    static double fgThreePMax;
    // [EN] Priors for free three-point fit regularization. / [CN] 自由三点拟合的先验正则项参数。
    static TVector3 fgThreeVertexPrior;
    static double fgThreeVertexSigmaXY;
    static double fgThreeVertexSigmaZ;
    static double fgThreeMomentumPrior;
    static double fgThreeMomentumSigma;
    static TVector3 fgThreeDirectionPrior;
    static double fgThreeDirectionSigmaRad;
    static double fgThreeVertexWeight;
    static double fgThreeMomentumWeight;
    static double fgThreeDirectionWeight;

    // TMinuit 回调函数 (必须是静态函数)
    static void MinuitFunction(Int_t& npar, Double_t* grad, Double_t& result, 
                              Double_t* parameters, Int_t flag);
    static void MinuitFunctionThreePointFree(Int_t& npar, Double_t* grad, Double_t& result,
                                            Double_t* parameters, Int_t flag);

    double CalculateThreePointLoss(const TVector3& startPos,
                                   const TVector3& initialMomentum,
                                   const TVector3& targetPos,
                                   const TVector3& pdc1,
                                   const TVector3& pdc2,
                                   double pdcSigma,
                                   double targetSigma,
                                   double* outTargetDist = nullptr,
                                   double* outPdc1Dist = nullptr,
                                   double* outPdc2Dist = nullptr,
                                   std::vector<ParticleTrajectory::TrajectoryPoint>* outTrajectory = nullptr) const;
};

#endif // TARGET_RECONSTRUCTOR_H
