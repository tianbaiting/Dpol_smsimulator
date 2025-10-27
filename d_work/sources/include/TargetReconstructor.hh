#ifndef TARGET_RECONSTRUCTOR_H
#define TARGET_RECONSTRUCTOR_H

#include "TVector3.h"
#include "TLorentzVector.h"
#include "RecoEvent.hh"
#include "MagneticField.hh"
#include "ParticleTrajectory.hh"
#include <utility>
#include <vector>

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
};

// Reconstruct momentum at a target position by back-propagating a reconstructed track.
// Uses PDC reconstruction logic: assumes RecoTrack has two points (start=PDC1, end=PDC2).
// Uses negative charge for backward propagation from the farther PDC point to target.
// Only supports proton assumption (mass = proton mass).
class TargetReconstructor {
public:
    TargetReconstructor(MagneticField* magField);
    ~TargetReconstructor();

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

private:
    MagneticField* fMagField;
    double fProtonMass; // MeV/c^2
    
    // 辅助函数：计算给定动量下到目标点的最小距离
    double CalculateMinimumDistance(double momentum, 
                                   const TVector3& startPos, 
                                   const TVector3& direction,
                                   const TVector3& targetPos,
                                   double charge, 
                                   double mass) const;
};

#endif // TARGET_RECONSTRUCTOR_H
