#include "TargetReconstructor.hh"
#include "ParticleTrajectory.hh"
#include <limits>
#include <cmath>
#include <iostream>

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
    
    TVector3 direction = (track.end - track.start);
    if (direction.Mag() < 1e-6) {
        std::cerr << "TargetReconstructor: track has zero length" << std::endl;
        return result;
    }

    // Determine which PDC point is farther from target
    double distStart = (track.start - targetPos).Mag();
    double distEnd = (track.end - targetPos).Mag();
    TVector3 startPos = (distEnd > distStart) ? track.end : track.start;
    
    // For backward propagation: reverse both charge and direction
    double charge = -1.0; // negative charge for time-reversed propagation
    double mass = fProtonMass;
    
    // Reverse direction for backward propagation (from PDC to target)
    TVector3 backwardDirection = -direction;

    std::cout << "TargetReconstructor: Starting from " 
              << (distEnd > distStart ? "PDC2" : "PDC1") 
              << " at (" << startPos.X() << ", " << startPos.Y() << ", " << startPos.Z() 
              << ") mm, target at (" << targetPos.X() << ", " << targetPos.Y() 
              << ", " << targetPos.Z() << ") mm" << std::endl;

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