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

// TMinuit 回调函数：计算目标函数值 (最小化与目标点的距离)
void TargetReconstructor::MinuitFunction(Int_t& npar, Double_t* grad, Double_t& result, 
                                        Double_t* parameters, Int_t flag) {
    // parameters[0] 是动量大小
    double momentum = parameters[0];
    
    // 计算到目标点的最小距离
    if (fgCurrentInstance) {
        result = fgCurrentInstance->CalculateMinimumDistance(momentum, fgStartPos, fgDirection,
                                                           fgTargetPos, fgCharge, fgMass);
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
    
    return result;
}