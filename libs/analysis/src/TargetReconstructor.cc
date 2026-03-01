#include "TargetReconstructor.hh"
#include "ParticleTrajectory.hh"
#include "TMinuit.h"
#include "TMath.h"
#include <algorithm>
#include <limits>
#include <cmath>
#include <iostream>

// 静态成员变量初始化
TargetReconstructor* TargetReconstructor::fgCurrentInstance = nullptr;
TVector3 TargetReconstructor::fgStartPos(0, 0, 0);
TVector3 TargetReconstructor::fgDirection(0, 0, 1);
TVector3 TargetReconstructor::fgTargetPos(0, 0, 0);
double TargetReconstructor::fgCharge = -1.0;
double TargetReconstructor::fgMass = 938.272;
TargetReconstructionResult* TargetReconstructor::fgResultPtr = nullptr;
bool TargetReconstructor::fgRecordSteps = false;
TVector3 TargetReconstructor::fgThreePDC1(0, 0, 0);
TVector3 TargetReconstructor::fgThreePDC2(0, 0, 0);
TVector3 TargetReconstructor::fgThreeTargetPos(0, 0, 0);
double TargetReconstructor::fgThreePdcSigma = 0.5;
double TargetReconstructor::fgThreeTargetSigma = 5.0;
double TargetReconstructor::fgThreePMin = 50.0;
double TargetReconstructor::fgThreePMax = 2000.0;
TVector3 TargetReconstructor::fgThreeVertexPrior(0, 0, 0);
double TargetReconstructor::fgThreeVertexSigmaXY = 2.0;
double TargetReconstructor::fgThreeVertexSigmaZ = 15.0;
double TargetReconstructor::fgThreeMomentumPrior = 627.0;
double TargetReconstructor::fgThreeMomentumSigma = 60.0;
TVector3 TargetReconstructor::fgThreeDirectionPrior(0, 0, 1);
double TargetReconstructor::fgThreeDirectionSigmaRad = 6.0 * TMath::DegToRad();
double TargetReconstructor::fgThreeVertexWeight = 1.0;
double TargetReconstructor::fgThreeMomentumWeight = 1.0;
double TargetReconstructor::fgThreeDirectionWeight = 1.0;

TargetReconstructor::TargetReconstructor(MagneticField* magField)
    // [EN] Default 10 mm is selected from B=1.15T, 3deg scan to keep reconstruction stable while reducing runtime. / [CN] 默认10 mm来自B=1.15T、3deg扫描，在保持重建稳定的同时显著降低计算耗时。
    : fMagField(magField), fProtonMass(938.272), fTrajectoryStepSize(10.0) {
}

TargetReconstructor::~TargetReconstructor() {}

TLorentzVector TargetReconstructor::ReconstructAtTarget(const RecoTrack& track,
                                                        const TVector3& targetPos,
                                                        double pMin,
                                                        double pMax,
                                                        double tol,
                                                        int maxRounds) const {
    // Simple wrapper for the detailed reconstruction
    TargetReconstructionResult result = ReconstructAtTargetWithDetails(track, targetPos, false, pMin, pMax, tol, maxRounds);
    return result.bestMomentum;
}

TargetReconstructionResult TargetReconstructor::ReconstructAtTargetWithDetails(const RecoTrack& track,
                                                                              const TVector3& targetPos,
                                                                              bool saveTrajectories,
                                                                              double pMin,
                                                                              double pMax,
                                                                              double tol,
                                                                              int maxRounds) const {
    TargetReconstructionResult result;
    result.success = false;
    result.finalDistance = std::numeric_limits<double>::max();
    
    // Calculate distances from both PDC points to target
    double distStart = (track.start - targetPos).Mag();  // PDC1 到靶点距离
    double distEnd = (track.end - targetPos).Mag();      // PDC2 到靶点距离
    
    // Choose the PDC point farther from target as starting point for backpropagation
    bool useEndPoint = (distEnd > distStart);
    TVector3 startPos = useEndPoint ? track.end : track.start;
    TVector3 otherPos = useEndPoint ? track.start : track.end;
    
    // Calculate correct backward direction: from farther PDC toward closer PDC
    TVector3 backwardDirection = (otherPos - startPos);
    if (backwardDirection.Mag() < 1e-6) {
        std::cerr << "TargetReconstructor: track has zero length" << std::endl;
        return result;
    }
    
    // For backward propagation: use negative charge for time-reversed propagation
    double charge = -1.0; // negative charge for time-reversed propagation
    double mass = fProtonMass;

    std::cout << "TargetReconstructor: Starting from " 
              << (useEndPoint ? "PDC2" : "PDC1") 
              << " at (" << startPos.X() << ", " << startPos.Y() << ", " << startPos.Z() 
              << ") mm, target at (" << targetPos.X() << ", " << targetPos.Y() 
              << ", " << targetPos.Z() << ") mm" << std::endl;
    std::cout << "  Direction: toward " << (useEndPoint ? "PDC1" : "PDC2") 
              << " at (" << otherPos.X() << ", " << otherPos.Y() << ", " << otherPos.Z() 
              << ") mm" << std::endl;

    double bestP = (pMin + pMax) * 0.5;
    double searchMin = pMin, searchMax = pMax;

    for (int round = 0; round < maxRounds; round++) {
        double localBestP = bestP;
        double localBestDist = std::numeric_limits<double>::max();
        std::vector<ParticleTrajectory::TrajectoryPoint> localBestTraj;

        int nSamples = 25;
        double dp = (searchMax - searchMin) / (nSamples - 1);
        
        for (int i = 0; i < nSamples; i++) {
            double pTrial = searchMin + i * dp;
            TVector3 pDirection = backwardDirection.Unit();
            TVector3 initialP = pTrial * pDirection;
            
            // Create TLorentzVector for ParticleTrajectory interface
            double energy = sqrt(initialP.Mag2() + mass * mass);
            TLorentzVector initialP4(initialP.X(), initialP.Y(), initialP.Z(), energy);

            ParticleTrajectory traj(fMagField);
            traj.SetStepSize(fTrajectoryStepSize);
            std::vector<ParticleTrajectory::TrajectoryPoint> pts = traj.CalculateTrajectory(
                startPos, initialP4, charge, mass
            );

            if (pts.empty()) continue;

            double minDist = std::numeric_limits<double>::max();
            ParticleTrajectory::TrajectoryPoint closestPt;
            for (const auto& pt : pts) {
                double dist = (pt.position - targetPos).Mag();
                if (dist < minDist) {
                    minDist = dist;
                    closestPt = pt;
                }
            }

            // 保存试探轨迹数据（用于调试可视化）
            if (saveTrajectories) {
                std::vector<TrajectoryPoint> trajPoints;
                for (const auto& pt : pts) {
                    TrajectoryPoint tp;
                    tp.position = pt.position;
                    tp.momentum = pt.momentum;
                    tp.time = pt.time;
                    trajPoints.push_back(tp);
                }
                result.trialTrajectories.push_back(trajPoints);
                result.trialMomenta.push_back(pTrial);
                result.distances.push_back(minDist);
            }

            if (minDist < localBestDist) {
                localBestDist = minDist;
                localBestP = pTrial;
                localBestTraj = pts;
            }
        }

        bestP = localBestP;
        
        // 保存最佳轨迹
        if (saveTrajectories && !localBestTraj.empty()) {
            result.bestTrajectory.clear();
            for (const auto& pt : localBestTraj) {
                TrajectoryPoint tp;
                tp.position = pt.position;
                tp.momentum = pt.momentum;
                tp.time = pt.time;
                result.bestTrajectory.push_back(tp);
            }
        }

        // 构建最佳动量四矢量
        if (!localBestTraj.empty()) {
            // Find closest point to target
            ParticleTrajectory::TrajectoryPoint closestPt;
            double minDist = std::numeric_limits<double>::max();
            for (const auto& pt : localBestTraj) {
                double dist = (pt.position - targetPos).Mag();
                if (dist < minDist) {
                    minDist = dist;
                    closestPt = pt;
                }
            }
            
            // Convert back to original particle momentum
            // Since we used negative charge and reversed direction, we need to reverse again
            result.bestMomentum.SetXYZT(-closestPt.momentum.X(), -closestPt.momentum.Y(), 
                                       -closestPt.momentum.Z(), 
                                       sqrt(closestPt.momentum.Mag2() + mass * mass));
            result.finalDistance = minDist;
        }

        std::cout << "Round " << round << ": bestP=" << bestP << " MeV/c, dist=" << localBestDist << " mm" << std::endl;

        if (localBestDist <= tol) {
            result.success = true;
            break;
        }

        // Narrow search range for next round
        double range = (searchMax - searchMin) * 0.3;
        searchMin = std::max(pMin, bestP - range);
        searchMax = std::min(pMax, bestP + range);
    }

    return result;
}

// 辅助函数：计算给定动量下到目标点的最小距离
double TargetReconstructor::CalculateMinimumDistance(double momentum,
                                                     const TVector3& startPos,
                                                     const TVector3& direction,
                                                     const TVector3& targetPos,
                                                     double charge,
                                                     double mass) const {
    TVector3 pDirection = direction.Unit();
    TVector3 initialP = momentum * pDirection;
    
    // Create TLorentzVector for ParticleTrajectory interface
    double energy = sqrt(initialP.Mag2() + mass * mass);
    TLorentzVector initialP4(initialP.X(), initialP.Y(), initialP.Z(), energy);

    ParticleTrajectory traj(fMagField);
    traj.SetStepSize(fTrajectoryStepSize);
    std::vector<ParticleTrajectory::TrajectoryPoint> pts = traj.CalculateTrajectory(
        startPos, initialP4, charge, mass
    );

    if (pts.empty()) {
        return std::numeric_limits<double>::max();
    }

    double minDist = std::numeric_limits<double>::max();
    for (const auto& pt : pts) {
        double dist = (pt.position - targetPos).Mag();
        if (dist < minDist) {
            minDist = dist;
        }
    }
    
    return minDist;
}

// 梯度下降方法：更高效的优化算法
TargetReconstructionResult TargetReconstructor::ReconstructAtTargetGradientDescent(
    const RecoTrack& track,
    const TVector3& targetPos,
    bool saveTrajectories,
    double pInit,
    double learningRate,
    double tol,
    int maxIterations) const {
    
    TargetReconstructionResult result;
    result.success = false;
    result.finalDistance = std::numeric_limits<double>::max();
    
    // Calculate distances from both PDC points to target
    double distStart = (track.start - targetPos).Mag();
    double distEnd = (track.end - targetPos).Mag();
    
    // Choose the PDC point farther from target as starting point
    bool useEndPoint = (distEnd > distStart);
    TVector3 startPos = useEndPoint ? track.end : track.start;
    TVector3 otherPos = useEndPoint ? track.start : track.end;
    
    // Calculate backward direction
    TVector3 backwardDirection = (otherPos - startPos);
    if (backwardDirection.Mag() < 1e-6) {
        std::cerr << "TargetReconstructor: track has zero length" << std::endl;
        return result;
    }
    
    std::cout << "TargetReconstructor (Gradient Descent): Starting from " 
              << (useEndPoint ? "PDC2" : "PDC1") 
              << " at (" << startPos.X() << ", " << startPos.Y() << ", " << startPos.Z() 
              << ") mm, target at (" << targetPos.X() << ", " << targetPos.Y() 
              << ", " << targetPos.Z() << ") mm" << std::endl;
    
    double mass = fProtonMass;
    double charge = -1.0; // Use negative charge for backward propagation
    
    double currentP = pInit;
    double bestP = currentP;
    double bestDistance = CalculateMinimumDistance(currentP, startPos, backwardDirection, 
                                                   targetPos, charge, mass);
    
    std::cout << "Initial: P=" << currentP << " MeV/c, distance=" << bestDistance << " mm" << std::endl;
    
    // 梯度下降主循环
    for (int iter = 0; iter < maxIterations; iter++) {
        // 数值梯度计算 (有限差分法)
        double dp = std::max(10.0, currentP * 0.01); // 增大步长：动量的1%
        
        double distPlus = CalculateMinimumDistance(currentP + dp, startPos, backwardDirection,
                                                  targetPos, charge, mass);
        double distMinus = CalculateMinimumDistance(currentP - dp, startPos, backwardDirection,
                                                   targetPos, charge, mass);
        double currentDist = CalculateMinimumDistance(currentP, startPos, backwardDirection,
                                                     targetPos, charge, mass);
        
        // 数值梯度：dDistance/dP
        double gradient = (distPlus - distMinus) / (2.0 * dp);
        
        std::cout << "  Iter " << iter << ": P=" << currentP << ", dist=" << currentDist 
                  << ", dp=" << dp << ", distPlus=" << distPlus << ", distMinus=" << distMinus 
                  << ", gradient=" << gradient << std::endl;
        
        // 自适应学习率
        double currentLearningRate = learningRate;
        if (iter > 10) {
            currentLearningRate *= 0.95; // 逐渐减小学习率
        }
        
        // 梯度下降更新
        double newP = currentP - currentLearningRate * gradient;
        
        // 边界限制
        newP = std::max(50.0, std::min(5000.0, newP));
        
        // 计算新动量下的距离
        double newDistance = CalculateMinimumDistance(newP, startPos, backwardDirection,
                                                     targetPos, charge, mass);
        
        // 如果改进了，接受新值
        if (newDistance < bestDistance) {
            bestDistance = newDistance;
            bestP = newP;
            currentP = newP;
        } else {
            // 如果没有改进，减小学习率继续尝试
            currentLearningRate *= 0.5;
            if (currentLearningRate < 0.1) {
                std::cout << "Convergence: Learning rate too small" << std::endl;
                break;
            }
            newP = currentP - currentLearningRate * gradient;
            newP = std::max(50.0, std::min(5000.0, newP));
            newDistance = CalculateMinimumDistance(newP, startPos, backwardDirection,
                                                  targetPos, charge, mass);
            if (newDistance < bestDistance) {
                bestDistance = newDistance;
                bestP = newP;
                currentP = newP;
            }
        }
        
        // 每10次迭代输出一次进度
        if (iter % 10 == 0 || iter < 5) {
            std::cout << "Iter " << iter << ": P=" << currentP << " MeV/c, distance=" 
                      << newDistance << " mm, gradient=" << gradient << std::endl;
        }
        
        // 收敛检查
        if (bestDistance <= tol) {
            std::cout << "Converged at iteration " << iter << std::endl;
            result.success = true;
            break;
        }
        
        // 梯度过小，认为收敛
        if (std::abs(gradient) < 1e-6) {
            std::cout << "Converged: gradient too small at iteration " << iter << std::endl;
            result.success = true;
            break;
        }
    }
    
    // 计算最终轨迹和动量
    TVector3 pDirection = backwardDirection.Unit();
    TVector3 initialP = bestP * pDirection;
    double energy = sqrt(initialP.Mag2() + mass * mass);
    TLorentzVector initialP4(initialP.X(), initialP.Y(), initialP.Z(), energy);

    ParticleTrajectory traj(fMagField);
    traj.SetStepSize(fTrajectoryStepSize);
    std::vector<ParticleTrajectory::TrajectoryPoint> finalTraj = traj.CalculateTrajectory(
        startPos, initialP4, charge, mass
    );
    
    if (!finalTraj.empty()) {
        // 找到最接近目标的点
        ParticleTrajectory::TrajectoryPoint closestPt;
        double minDist = std::numeric_limits<double>::max();
        for (const auto& pt : finalTraj) {
            double dist = (pt.position - targetPos).Mag();
            if (dist < minDist) {
                minDist = dist;
                closestPt = pt;
            }
        }
        
        // Convert back to original particle momentum (reverse the charge flip)
        result.bestMomentum.SetXYZT(-closestPt.momentum.X(), -closestPt.momentum.Y(), 
                                   -closestPt.momentum.Z(), 
                                   sqrt(closestPt.momentum.Mag2() + mass * mass));
        result.finalDistance = minDist;
        
        // 保存最佳轨迹
        if (saveTrajectories) {
            result.bestTrajectory.clear();
            for (const auto& pt : finalTraj) {
                TrajectoryPoint tp;
                tp.position = pt.position;
                tp.momentum = pt.momentum;
                tp.time = pt.time;
                result.bestTrajectory.push_back(tp);
            }
        }
    }
    
    std::cout << "Final result: P=" << bestP << " MeV/c, distance=" << bestDistance << " mm" << std::endl;
    
    return result;
}

// [EN] Three-point constrained gradient descent: minimize weighted distance^2 to target + PDC1/PDC2. / [CN] 三点约束梯度下降：最小化到靶点+PDC1/PDC2的加权距离平方。
TargetReconstructionResult TargetReconstructor::ReconstructAtTargetThreePointGradientDescent(
    const RecoTrack& track,
    const TVector3& targetPos,
    bool saveTrajectories,
    const TVector3& fixedMomentum,
    double learningRate,
    double tol,
    int maxIterations,
    double pdcSigma,
    double targetSigma) const {

    TargetReconstructionResult result;
    result.success = false;
    result.finalDistance = std::numeric_limits<double>::max();
    result.totalIterations = 0;

    TVector3 pdc1 = track.start;
    TVector3 pdc2 = track.end;
    if (fixedMomentum.Mag() < 1e-6) {
        std::cerr << "TargetReconstructor (3-point GD): fixed momentum is zero" << std::endl;
        return result;
    }
    if (pdcSigma <= 0.0 || targetSigma <= 0.0) {
        std::cerr << "TargetReconstructor (3-point GD): sigma must be positive" << std::endl;
        return result;
    }

    // [EN] Use target position as initial guess and optimize start position. / [CN] 以靶点为初值，优化出射点位置。
    TVector3 currentPos = targetPos;
    TVector3 bestPos = currentPos;

    double mass = fProtonMass;
    double charge = +1.0; // [EN] Forward propagation from start position to PDCs. / [CN] 从出射点向PDC正向传播。

    auto computeLoss = [&](const TVector3& startPos, double& outMinDist) -> double {
        double energy = std::sqrt(fixedMomentum.Mag2() + mass * mass);
        TLorentzVector p4(fixedMomentum.X(), fixedMomentum.Y(), fixedMomentum.Z(), energy);

        ParticleTrajectory traj(fMagField);
        traj.SetStepSize(fTrajectoryStepSize);
        std::vector<ParticleTrajectory::TrajectoryPoint> pts =
            traj.CalculateTrajectory(startPos, p4, charge, mass);

        if (pts.empty()) {
            outMinDist = std::numeric_limits<double>::max();
            return std::numeric_limits<double>::max();
        }

        auto minDistToPoint = [&](const TVector3& point) -> double {
            double minDist = std::numeric_limits<double>::max();
            for (const auto& pt : pts) {
                double dist = (pt.position - point).Mag();
                if (dist < minDist) minDist = dist;
            }
            return minDist;
        };

        double dTarget = minDistToPoint(targetPos);
        double dPDC1 = minDistToPoint(pdc1);
        double dPDC2 = minDistToPoint(pdc2);

        // [EN] Weighted sum of squared distances (chi2-like). / [CN] 加权距离平方和（类似卡方）。
        double loss = (dTarget * dTarget) / (targetSigma * targetSigma)
                    + (dPDC1 * dPDC1) / (pdcSigma * pdcSigma)
                    + (dPDC2 * dPDC2) / (pdcSigma * pdcSigma);

        outMinDist = std::max({dTarget, dPDC1, dPDC2});

        if (saveTrajectories) {
            result.trialTrajectories.emplace_back();
            auto& trajPoints = result.trialTrajectories.back();
            trajPoints.reserve(pts.size());
            for (const auto& pt : pts) {
                TrajectoryPoint tp;
                tp.position = pt.position;
                tp.momentum = pt.momentum;
                tp.time = pt.time;
                trajPoints.push_back(tp);
            }
            result.trialMomenta.push_back(fixedMomentum.Mag());
            result.distances.push_back(outMinDist);
        }

        return loss;
    };

    double bestMinDist = std::numeric_limits<double>::max();
    double bestLoss = computeLoss(bestPos, bestMinDist);

    // [EN] Gradient descent on start position with finite differences. / [CN] 对出射点位置做有限差分梯度下降。
    for (int iter = 0; iter < maxIterations; ++iter) {
        double currentMinDist = std::numeric_limits<double>::max();
        double currentLoss = computeLoss(currentPos, currentMinDist);

        result.optimizationSteps_P.push_back(fixedMomentum.Mag());
        result.optimizationSteps_Loss.push_back(currentLoss);
        result.totalIterations++;

        if (currentLoss < bestLoss) {
            bestLoss = currentLoss;
            bestPos = currentPos;
            bestMinDist = currentMinDist;
        }

        // [EN] Convergence check using RMS distance threshold. / [CN] 用RMS距离阈值判断收敛。
        double rmsDist = std::sqrt(currentLoss / 3.0);
        if (rmsDist <= tol) {
            result.success = true;
            break;
        }

        // Finite-difference gradient
        TVector3 grad(0, 0, 0);
        for (int k = 0; k < 3; ++k) {
            double comp = (k == 0) ? currentPos.X() : (k == 1 ? currentPos.Y() : currentPos.Z());
            double dp = std::max(0.5, std::abs(comp) * 0.01);

            TVector3 pPlus = currentPos;
            TVector3 pMinus = currentPos;
            if (k == 0) { pPlus.SetX(comp + dp); pMinus.SetX(comp - dp); }
            if (k == 1) { pPlus.SetY(comp + dp); pMinus.SetY(comp - dp); }
            if (k == 2) { pPlus.SetZ(comp + dp); pMinus.SetZ(comp - dp); }

            double tmp = 0.0;
            double lossPlus = computeLoss(pPlus, tmp);
            double lossMinus = computeLoss(pMinus, tmp);
            double g = (lossPlus - lossMinus) / (2.0 * dp);

            if (k == 0) grad.SetX(g);
            if (k == 1) grad.SetY(g);
            if (k == 2) grad.SetZ(g);
        }

        TVector3 nextPos = currentPos - grad * learningRate;

        // [EN] If loss increases, shrink step size. / [CN] 若损失上升则缩小步长。
        double tmpMinDist = 0.0;
        double nextLoss = computeLoss(nextPos, tmpMinDist);
        if (nextLoss > currentLoss) {
            nextPos = currentPos - grad * (learningRate * 0.5);
        }

        currentPos = nextPos;
    }

    // [EN] Build final trajectory from best start position. / [CN] 用最优出射点位置构建最终轨迹。
    double bestEnergy = std::sqrt(fixedMomentum.Mag2() + mass * mass);
    TLorentzVector bestP4(fixedMomentum.X(), fixedMomentum.Y(), fixedMomentum.Z(), bestEnergy);
    ParticleTrajectory traj(fMagField);
    traj.SetStepSize(fTrajectoryStepSize);
    std::vector<ParticleTrajectory::TrajectoryPoint> finalTraj =
        traj.CalculateTrajectory(bestPos, bestP4, charge, mass);

    if (!finalTraj.empty()) {
        result.bestMomentum = bestP4;
        result.finalDistance = bestMinDist;
        if (saveTrajectories) {
            result.bestTrajectory.clear();
            for (const auto& pt : finalTraj) {
                TrajectoryPoint tp;
                tp.position = pt.position;
                tp.momentum = pt.momentum;
                tp.time = pt.time;
                result.bestTrajectory.push_back(tp);
            }
        }
    }

    return result;
}

double TargetReconstructor::CalculateThreePointLoss(const TVector3& startPos,
                                                    const TVector3& initialMomentum,
                                                    const TVector3& targetPos,
                                                    const TVector3& pdc1,
                                                    const TVector3& pdc2,
                                                    double pdcSigma,
                                                    double targetSigma,
                                                    double* outTargetDist,
                                                    double* outPdc1Dist,
                                                    double* outPdc2Dist,
                                                    std::vector<ParticleTrajectory::TrajectoryPoint>* outTrajectory) const {
    const double kLargePenalty = 1.0e8;

    if (!fMagField) return kLargePenalty;
    if (initialMomentum.Mag() < 1.0e-6) return kLargePenalty;
    if (pdcSigma <= 0.0 || targetSigma <= 0.0) return kLargePenalty;

    const double charge = +1.0;
    const double mass = fProtonMass;
    const double energy = std::sqrt(initialMomentum.Mag2() + mass * mass);
    TLorentzVector p4(initialMomentum.X(), initialMomentum.Y(), initialMomentum.Z(), energy);

    ParticleTrajectory traj(fMagField);
    traj.SetStepSize(fTrajectoryStepSize);
    const double farDist = std::max({(targetPos - startPos).Mag(), (pdc1 - startPos).Mag(), (pdc2 - startPos).Mag()});
    traj.SetMaxDistance(std::max(1500.0, farDist + 1500.0));
    traj.SetMaxTime(400.0);
    traj.SetMinMomentum(5.0);

    std::vector<ParticleTrajectory::TrajectoryPoint> points = traj.CalculateTrajectory(startPos, p4, charge, mass);
    if (points.empty()) return kLargePenalty;

    auto minDistTo = [&](const TVector3& point) {
        double minDist = std::numeric_limits<double>::max();
        for (const auto& trajPt : points) {
            const double dist = (trajPt.position - point).Mag();
            if (dist < minDist) minDist = dist;
        }
        return minDist;
    };

    const double dTarget = minDistTo(targetPos);
    const double dPdc1 = minDistTo(pdc1);
    const double dPdc2 = minDistTo(pdc2);

    if (outTargetDist) *outTargetDist = dTarget;
    if (outPdc1Dist) *outPdc1Dist = dPdc1;
    if (outPdc2Dist) *outPdc2Dist = dPdc2;
    if (outTrajectory) *outTrajectory = points;

    const double spatialChi2 =
        ((dTarget / targetSigma) * (dTarget / targetSigma) +
         (dPdc1 / pdcSigma) * (dPdc1 / pdcSigma) +
         (dPdc2 / pdcSigma) * (dPdc2 / pdcSigma)) / 3.0;

    // [EN] Vertex prior keeps emission point inside target-scale region (mm-level). / [CN] 顶点先验将发射点限制在靶尺度区域（毫米级）。
    const double vxDx = (startPos.X() - fgThreeVertexPrior.X()) / std::max(1.0e-6, fgThreeVertexSigmaXY);
    const double vxDy = (startPos.Y() - fgThreeVertexPrior.Y()) / std::max(1.0e-6, fgThreeVertexSigmaXY);
    const double vxDz = (startPos.Z() - fgThreeVertexPrior.Z()) / std::max(1.0e-6, fgThreeVertexSigmaZ);
    const double vertexChi2 = vxDx * vxDx + vxDy * vxDy + vxDz * vxDz;

    const double pMag = initialMomentum.Mag();
    const double pSigmaSafe = std::max(1.0e-6, fgThreeMomentumSigma);
    // [EN] Momentum prior suppresses unrealistically small/large rigidity solutions. / [CN] 动量先验抑制不现实的刚度解（过小/过大）。
    const double momentumChi2 = ((pMag - fgThreeMomentumPrior) / pSigmaSafe) *
                                ((pMag - fgThreeMomentumPrior) / pSigmaSafe);

    double directionChi2 = 0.0;
    if (pMag > 1.0e-6 && fgThreeDirectionPrior.Mag() > 1.0e-6) {
        const TVector3 pDir = initialMomentum.Unit();
        const TVector3 refDir = fgThreeDirectionPrior.Unit();
        const double cosTheta = std::clamp(pDir.Dot(refDir), -1.0, 1.0);
        const double theta = std::acos(cosTheta);
        const double sigmaTheta = std::max(1.0e-6, fgThreeDirectionSigmaRad);
        // [EN] Direction prior forces momentum orientation close to measured PDC track line. / [CN] 方向先验将动量方向约束到PDC测得轨迹方向附近。
        directionChi2 = (theta / sigmaTheta) * (theta / sigmaTheta);
    }

    const double totalChi2 = spatialChi2
                           + fgThreeVertexWeight * vertexChi2
                           + fgThreeMomentumWeight * momentumChi2
                           + fgThreeDirectionWeight * directionChi2;
    if (!std::isfinite(totalChi2)) return kLargePenalty;
    return std::sqrt(std::max(0.0, totalChi2));
}

void TargetReconstructor::MinuitFunctionThreePointFree(Int_t& npar,
                                                       Double_t* grad,
                                                       Double_t& result,
                                                       Double_t* parameters,
                                                       Int_t flag) {
    (void)npar;
    (void)grad;

    const double kLargePenalty = 1.0e8;
    if (!fgCurrentInstance) {
        result = kLargePenalty;
        return;
    }

    const TVector3 startPos(parameters[0], parameters[1], parameters[2]);
    const TVector3 momentum(parameters[3], parameters[4], parameters[5]);
    const double pMag = momentum.Mag();

    if (!std::isfinite(pMag) || pMag < fgThreePMin || pMag > fgThreePMax) {
        const double under = std::max(0.0, fgThreePMin - pMag);
        const double over = std::max(0.0, pMag - fgThreePMax);
        const double penalty = (under * under + over * over) * 1.0e4;
        result = kLargePenalty + penalty;
        return;
    }

    result = fgCurrentInstance->CalculateThreePointLoss(startPos, momentum,
                                                        fgThreeTargetPos, fgThreePDC1, fgThreePDC2,
                                                        fgThreePdcSigma, fgThreeTargetSigma,
                                                        nullptr, nullptr, nullptr, nullptr);

    if (!std::isfinite(result)) {
        result = kLargePenalty;
        return;
    }

    if (fgRecordSteps && fgResultPtr && flag == 4) {
        fgResultPtr->optimizationSteps_P.push_back(pMag);
        fgResultPtr->optimizationSteps_Loss.push_back(result);
        fgResultPtr->totalIterations++;
    }
}

// TMinuit 回调函数：计算目标函数值 (最小化与目标点的距离)
void TargetReconstructor::MinuitFunction(Int_t& npar, Double_t* grad, Double_t& result, 
                                        Double_t* parameters, Int_t flag) {
    // parameters[0] 是动量大小
    double momentum = parameters[0];
    
    // 计算到目标点的最小距离
    if (fgCurrentInstance) {
        result = fgCurrentInstance->CalculateMinimumDistance(momentum, fgStartPos, fgDirection,
                                                           fgTargetPos, fgCharge, fgMass);
        
        // 记录优化步骤（仅当 fgRecordSteps 为 true 时）
        // flag == 4 表示 TMinuit 正在进行正常的函数评估
        if (fgRecordSteps && fgResultPtr && flag == 4) {
            fgResultPtr->optimizationSteps_P.push_back(momentum);
            fgResultPtr->optimizationSteps_Loss.push_back(result);
            fgResultPtr->totalIterations++;
        }
    } else {
        result = 1e6; // 如果实例无效，返回大值
    }
}

// TMinuit 优化方法实现
TargetReconstructionResult TargetReconstructor::ReconstructAtTargetMinuit(
    const RecoTrack& track,
    const TVector3& targetPos,
    bool saveTrajectories,
    double pInit,
    double tol,
    int maxIterations,
    bool recordSteps) const {
    
    TargetReconstructionResult result;
    result.success = false;
    result.finalDistance = std::numeric_limits<double>::max();
    result.totalIterations = 0;  // 初始化迭代计数
    
    // 设置是否记录优化步骤（仅调试时启用）
    fgRecordSteps = recordSteps;
    
    // 设置静态指针以便在回调函数中记录步骤（仅当 recordSteps 为 true 时）
    if (recordSteps) {
        fgResultPtr = &result;
    } else {
        fgResultPtr = nullptr;
    }
    
    // Calculate distances from both PDC points to target
    double distStart = (track.start - targetPos).Mag();
    double distEnd = (track.end - targetPos).Mag();
    
    // Choose the PDC point farther from target as starting point
    bool useEndPoint = (distEnd > distStart);
    TVector3 startPos = useEndPoint ? track.end : track.start;
    TVector3 otherPos = useEndPoint ? track.start : track.end;
    
    // Calculate backward direction
    TVector3 backwardDirection = (otherPos - startPos);
    if (backwardDirection.Mag() < 1e-6) {
        std::cerr << "TargetReconstructor (TMinuit): track has zero length" << std::endl;
        return result;
    }
    
    std::cout << "TargetReconstructor (TMinuit): Starting from " 
              << (useEndPoint ? "PDC2" : "PDC1") 
              << " at (" << startPos.X() << ", " << startPos.Y() << ", " << startPos.Z() 
              << ") mm, target at (" << targetPos.X() << ", " << targetPos.Y() 
              << ", " << targetPos.Z() << ") mm" << std::endl;
    
    double mass = fProtonMass;
    double charge = -1.0; // Use negative charge for backward propagation
    
    // 设置静态变量供回调函数使用
    fgCurrentInstance = const_cast<TargetReconstructor*>(this);
    fgStartPos = startPos;
    fgDirection = backwardDirection;
    fgTargetPos = targetPos;
    fgCharge = charge;
    fgMass = mass;
    
    // 创建 TMinuit 实例
    TMinuit minuit(1); // 1个参数 (动量大小)
    minuit.SetPrintLevel(-1); // 减少输出
    
    // 设置目标函数
    minuit.SetFCN(MinuitFunction);
    
    // 定义参数：momentum
    Int_t ierflg = 0;
    minuit.mnparm(0, "momentum", pInit, 10.0, 50.0, 5000.0, ierflg);
    
    if (ierflg != 0) {
        std::cerr << "TMinuit parameter setup failed" << std::endl;
        return result;
    }
    
    // 设置策略和容差 (使用正确的TMinuit命令)
    Double_t arglist[10];
    arglist[0] = 2; // 策略 2 = 最精确的策略
    minuit.mnexcm("SET STR", arglist, 1, ierflg); 
    
    arglist[0] = tol * tol; // 设置误差定义
    minuit.mnexcm("SET ERR", arglist, 1, ierflg);
    
    // 设置最大迭代次数
    arglist[0] = maxIterations;
    minuit.mnexcm("SET LIM", arglist, 1, ierflg);
    
    std::cout << "Starting TMinuit MIGRAD optimization..." << std::endl;
    std::cout << "Initial momentum: " << pInit << " MeV/c" << std::endl;
    
    // 运行 MIGRAD 算法 (拟牛顿法)
    Double_t args[2];
    args[0] = maxIterations; // 最大迭代次数
    args[1] = tol;          // 容差
    minuit.mnexcm("MIGRAD", args, 2, ierflg);
    
    if (ierflg != 0) {
        std::cout << "MIGRAD failed with error code: " << ierflg << std::endl;
        // 尝试 SIMPLEX 算法作为备选
        std::cout << "Trying SIMPLEX algorithm..." << std::endl;
        minuit.mnexcm("SIMPLEX", args, 2, ierflg);
    }
    
    // 获取优化结果
    Double_t bestP, errorP;
    minuit.GetParameter(0, bestP, errorP);
    
    // 获取最小函数值 (最小距离)
    Double_t amin, edm, errdef;
    Int_t nvpar, nparx, icstat;
    minuit.mnstat(amin, edm, errdef, nvpar, nparx, icstat);
    
    std::cout << "TMinuit optimization completed:" << std::endl;
    std::cout << "  Best momentum: " << bestP << " ± " << errorP << " MeV/c" << std::endl;
    std::cout << "  Final distance: " << sqrt(amin) << " mm" << std::endl;
    std::cout << "  Convergence status: " << icstat << " (3=converged)" << std::endl;
    std::cout << "  EDM (estimated distance to minimum): " << edm << std::endl;
    
    // 验证收敛
    if (icstat >= 3 || sqrt(amin) <= tol) {
        result.success = true;
    }
    result.finalDistance = sqrt(amin);
    
    // 计算最终轨迹和动量
    TVector3 pDirection = backwardDirection.Unit();
    TVector3 initialP = bestP * pDirection;
    double energy = sqrt(initialP.Mag2() + mass * mass);
    TLorentzVector initialP4(initialP.X(), initialP.Y(), initialP.Z(), energy);

    ParticleTrajectory traj(fMagField);
    traj.SetStepSize(fTrajectoryStepSize);
    std::vector<ParticleTrajectory::TrajectoryPoint> finalTraj = traj.CalculateTrajectory(
        startPos, initialP4, charge, mass
    );
    
    if (!finalTraj.empty()) {
        // 找到最接近目标的点
        ParticleTrajectory::TrajectoryPoint closestPt;
        double minDist = std::numeric_limits<double>::max();
        for (const auto& pt : finalTraj) {
            double dist = (pt.position - targetPos).Mag();
            if (dist < minDist) {
                minDist = dist;
                closestPt = pt;
            }
        }
        
        // Convert back to original particle momentum (reverse the charge flip)
        result.bestMomentum.SetXYZT(-closestPt.momentum.X(), -closestPt.momentum.Y(), 
                                   -closestPt.momentum.Z(), 
                                   sqrt(closestPt.momentum.Mag2() + mass * mass));
        
        // 保存最佳轨迹
        if (saveTrajectories) {
            result.bestTrajectory.clear();
            for (const auto& pt : finalTraj) {
                TrajectoryPoint tp;
                tp.position = pt.position;
                tp.momentum = pt.momentum;
                tp.time = pt.time;
                result.bestTrajectory.push_back(tp);
            }
        }
    }
    
    // 清理静态变量
    fgCurrentInstance = nullptr;
    fgResultPtr = nullptr;
    fgRecordSteps = false;
    
    return result;
}

TargetReconstructionResult TargetReconstructor::ReconstructAtTargetThreePointFreeMinuit(
    const RecoTrack& track,
    const TVector3& targetPos,
    bool saveTrajectories,
    const TVector3& startPosInit,
    const TVector3& momentumInit,
    double targetSigma,
    double pdcSigma,
    double tol,
    int maxIterations,
    bool recordSteps) const {

    TargetReconstructionResult result;
    result.success = false;
    result.finalDistance = std::numeric_limits<double>::max();
    result.finalLoss = std::numeric_limits<double>::infinity();
    result.totalIterations = 0;

    const TVector3 pdc1 = track.start;
    const TVector3 pdc2 = track.end;
    if ((pdc2 - pdc1).Mag() < 1.0e-6) {
        std::cerr << "TargetReconstructor (3-point free Minuit): track has zero length" << std::endl;
        return result;
    }
    if (targetSigma <= 0.0 || pdcSigma <= 0.0) {
        std::cerr << "TargetReconstructor (3-point free Minuit): sigma must be positive" << std::endl;
        return result;
    }

    auto isFiniteVec = [](const TVector3& vec) {
        return std::isfinite(vec.X()) && std::isfinite(vec.Y()) && std::isfinite(vec.Z());
    };

    TVector3 initStart = startPosInit;
    if (!isFiniteVec(initStart)) {
        initStart = targetPos;
    }

    TVector3 initMomentum = momentumInit;
    if (!isFiniteVec(initMomentum) || initMomentum.Mag() < 1.0e-6) {
        TVector3 dir = pdc2 - pdc1;
        if (dir.Mag() < 1.0e-6) dir = TVector3(0.0, 0.0, 1.0);
        initMomentum = dir.Unit() * 627.0;
    }
    const TVector3 lineDirection = (pdc2 - pdc1).Unit();
    const double pExpected = std::max(50.0, initMomentum.Mag());

    fgCurrentInstance = const_cast<TargetReconstructor*>(this);
    fgThreeTargetPos = targetPos;
    fgThreePDC1 = pdc1;
    fgThreePDC2 = pdc2;
    fgThreePdcSigma = pdcSigma;
    fgThreeTargetSigma = targetSigma;
    // [EN] Apply physical priors to regularize ill-conditioned free 6D fit. / [CN] 对病态6维自由拟合引入物理先验正则。
    fgThreeVertexPrior = targetPos;
    fgThreeVertexSigmaXY = 2.0;   // mm-level transverse vertex prior
    fgThreeVertexSigmaZ = 15.0;   // target thickness scale (~30 mm full thickness)
    fgThreeMomentumPrior = pExpected;
    fgThreeMomentumSigma = std::max(25.0, 0.10 * pExpected);
    fgThreeDirectionPrior = lineDirection;
    fgThreeDirectionSigmaRad = 6.0 * TMath::DegToRad();
    fgThreeVertexWeight = 1.0;
    fgThreeMomentumWeight = 1.0;
    fgThreeDirectionWeight = 1.0;
    fgThreePMin = std::max(50.0, fgThreeMomentumPrior - 3.0 * fgThreeMomentumSigma);
    fgThreePMax = fgThreeMomentumPrior + 3.0 * fgThreeMomentumSigma;

    fgRecordSteps = recordSteps;
    fgResultPtr = recordSteps ? &result : nullptr;

    TMinuit minuit(6);
    minuit.SetPrintLevel(-1);
    minuit.SetFCN(MinuitFunctionThreePointFree);

    Int_t ierflg = 0;
    const double posWindowXY = 4.0;
    const double posWindowZ = 18.0;
    const double momentumWindow = std::max(800.0, fgThreeMomentumPrior + 250.0);

    minuit.mnparm(0, "x0", initStart.X(), 1.0, targetPos.X() - posWindowXY, targetPos.X() + posWindowXY, ierflg);
    minuit.mnparm(1, "y0", initStart.Y(), 1.0, targetPos.Y() - posWindowXY, targetPos.Y() + posWindowXY, ierflg);
    minuit.mnparm(2, "z0", initStart.Z(), 1.0, targetPos.Z() - posWindowZ, targetPos.Z() + posWindowZ, ierflg);
    minuit.mnparm(3, "px0", initMomentum.X(), std::max(2.0, std::abs(initMomentum.X()) * 0.03), -momentumWindow, momentumWindow, ierflg);
    minuit.mnparm(4, "py0", initMomentum.Y(), std::max(2.0, std::abs(initMomentum.Y()) * 0.03), -momentumWindow, momentumWindow, ierflg);
    minuit.mnparm(5, "pz0", initMomentum.Z(), std::max(2.0, std::abs(initMomentum.Z()) * 0.03), -momentumWindow, momentumWindow, ierflg);

    if (ierflg != 0) {
        std::cerr << "TargetReconstructor (3-point free Minuit): parameter setup failed" << std::endl;
        fgCurrentInstance = nullptr;
        fgResultPtr = nullptr;
        fgRecordSteps = false;
        return result;
    }

    Double_t arglist[10];
    arglist[0] = 2;
    minuit.mnexcm("SET STR", arglist, 1, ierflg);
    arglist[0] = tol * tol;
    minuit.mnexcm("SET ERR", arglist, 1, ierflg);
    arglist[0] = maxIterations;
    minuit.mnexcm("SET LIM", arglist, 1, ierflg);

    Double_t args[2];
    args[0] = maxIterations;
    args[1] = tol;
    minuit.mnexcm("MIGRAD", args, 2, ierflg);
    if (ierflg != 0) {
        minuit.mnexcm("SIMPLEX", args, 2, ierflg);
    }

    Double_t amin = std::numeric_limits<double>::infinity();
    Double_t edm = std::numeric_limits<double>::infinity();
    Double_t errdef = 0.0;
    Int_t nvpar = 0;
    Int_t nparx = 0;
    Int_t icstat = 0;
    minuit.mnstat(amin, edm, errdef, nvpar, nparx, icstat);

    Double_t x0 = initStart.X(), y0 = initStart.Y(), z0 = initStart.Z();
    Double_t px0 = initMomentum.X(), py0 = initMomentum.Y(), pz0 = initMomentum.Z();
    Double_t err = 0.0;
    minuit.GetParameter(0, x0, err);
    minuit.GetParameter(1, y0, err);
    minuit.GetParameter(2, z0, err);
    minuit.GetParameter(3, px0, err);
    minuit.GetParameter(4, py0, err);
    minuit.GetParameter(5, pz0, err);

    const TVector3 bestStartPos(x0, y0, z0);
    const TVector3 bestInitialMomentum(px0, py0, pz0);

    std::vector<ParticleTrajectory::TrajectoryPoint> bestTrajectory;
    double dTarget = std::numeric_limits<double>::infinity();
    double dPdc1 = std::numeric_limits<double>::infinity();
    double dPdc2 = std::numeric_limits<double>::infinity();
    const double finalLoss = CalculateThreePointLoss(bestStartPos, bestInitialMomentum,
                                                     targetPos, pdc1, pdc2,
                                                     pdcSigma, targetSigma,
                                                     &dTarget, &dPdc1, &dPdc2, &bestTrajectory);

    result.bestStartPos = bestStartPos;
    result.bestInitialMomentum = bestInitialMomentum;
    result.finalLoss = finalLoss;
    result.minDistTarget = dTarget;
    result.minDistPDC1 = dPdc1;
    result.minDistPDC2 = dPdc2;
    result.fitStatus = icstat;
    result.edm = edm;
    result.finalDistance = std::max({dTarget, dPdc1, dPdc2});

    if (!bestTrajectory.empty()) {
        std::size_t bestIdx = 0;
        double minTarget = std::numeric_limits<double>::max();
        for (std::size_t i = 0; i < bestTrajectory.size(); ++i) {
            const double dist = (bestTrajectory[i].position - targetPos).Mag();
            if (dist < minTarget) {
                minTarget = dist;
                bestIdx = i;
            }
        }
        const TVector3 recoMomentumAtTarget = bestTrajectory[bestIdx].momentum;
        const double energy = std::sqrt(recoMomentumAtTarget.Mag2() + fProtonMass * fProtonMass);
        result.bestMomentum.SetXYZT(recoMomentumAtTarget.X(), recoMomentumAtTarget.Y(), recoMomentumAtTarget.Z(), energy);

        if (saveTrajectories) {
            result.bestTrajectory.clear();
            result.bestTrajectory.reserve(bestTrajectory.size());
            for (const auto& pt : bestTrajectory) {
                TrajectoryPoint tp;
                tp.position = pt.position;
                tp.momentum = pt.momentum;
                tp.time = pt.time;
                result.bestTrajectory.push_back(tp);
            }
        }
    } else {
        const double pMag = bestInitialMomentum.Mag();
        if (pMag > 1.0e-6) {
            const double energy = std::sqrt(pMag * pMag + fProtonMass * fProtonMass);
            result.bestMomentum.SetXYZT(bestInitialMomentum.X(), bestInitialMomentum.Y(), bestInitialMomentum.Z(), energy);
        }
    }

    result.success = ((icstat >= 3) || (std::isfinite(finalLoss) && finalLoss <= tol));

    fgCurrentInstance = nullptr;
    fgResultPtr = nullptr;
    fgRecordSteps = false;

    return result;
}
