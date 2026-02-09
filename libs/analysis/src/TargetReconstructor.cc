#include "TargetReconstructor.hh"
#include "ParticleTrajectory.hh"
#include "TMinuit.h"
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

TargetReconstructor::TargetReconstructor(MagneticField* magField)
    : fMagField(magField), fProtonMass(938.272) {
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
