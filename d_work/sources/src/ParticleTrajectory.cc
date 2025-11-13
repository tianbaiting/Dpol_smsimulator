// 计算单位

// 动量单位 MeV/C
// 质量单位 MeV/c²  
// 时间单位 ns
// 长度单位 mm
// 电荷单位 e


#include "ParticleTrajectory.hh"
#include "TMath.h"
#include <iostream>
#include <iomanip>

// Physics constants
const double ParticleTrajectory::kSpeedOfLight = 299.792458; // mm/ns (c in natural units)

ParticleTrajectory::ParticleTrajectory(MagneticField* magField)
    : fMagField(magField), fStepSize(1.0), fMaxTime(100.0), 
      fMaxDistance(5000.0), fMinMomentum(1.0)
{
    if (!fMagField) {
        std::cerr << "ERROR: ParticleTrajectory - MagneticField pointer is null!" << std::endl;
    }
}

ParticleTrajectory::~ParticleTrajectory()
{
    // MagneticField is not owned by this class, don't delete it
}

std::vector<ParticleTrajectory::TrajectoryPoint> 
ParticleTrajectory::CalculateTrajectory(const TVector3& initialPosition,
                                       const TLorentzVector& initialMomentum,
                                       double charge, double mass) const
{
    std::vector<TrajectoryPoint> trajectory;
    
    if (!fMagField) {
        std::cerr << "ERROR: No magnetic field object available!" << std::endl;
        return trajectory;
    }
    
    if (TMath::Abs(charge) < 1e-6) {
        std::cout << "INFO: Neutral particle - no magnetic deflection" << std::endl;
        // For neutral particles, just straight line
        TrajectoryPoint start(initialPosition, initialMomentum.Vect(), 0.0, TVector3(0,0,0));
        trajectory.push_back(start);
        
        // Calculate straight line endpoint
        TVector3 direction = initialMomentum.Vect().Unit();
        TVector3 endPosition = initialPosition + direction * fMaxDistance;
        double endTime = fMaxDistance / (initialMomentum.Beta() * kSpeedOfLight);
        TrajectoryPoint end(endPosition, initialMomentum.Vect(), endTime, TVector3(0,0,0));
        trajectory.push_back(end);
        
        return trajectory;
    }
    
    // Initialize trajectory
    TVector3 bField = fMagField->GetField(initialPosition);
    TrajectoryPoint currentPoint(initialPosition, initialMomentum.Vect(), 0.0, bField);
    trajectory.push_back(currentPoint);
    
    std::cout << "Starting trajectory calculation:" << std::endl;
    std::cout << "  Initial position: (" << initialPosition.X() << ", " 
              << initialPosition.Y() << ", " << initialPosition.Z() << ") mm" << std::endl;
    std::cout << "  Initial momentum: (" << initialMomentum.Px() << ", " 
              << initialMomentum.Py() << ", " << initialMomentum.Pz() << ") MeV/c" << std::endl;
    std::cout << "  Charge: " << charge << " e" << std::endl;
    std::cout << "  Mass: " << mass << " MeV/c²" << std::endl;
    
    // Integration parameters
    double dt = fStepSize / (initialMomentum.Beta() * kSpeedOfLight); // Time step [ns]
    
    // Main integration loop
    int stepCount = 0;
    const int maxSteps = static_cast<int>(fMaxTime / dt);
    
    while (stepCount < maxSteps && IsValidStep(currentPoint)) {
        // Runge-Kutta integration step
        TrajectoryPoint nextPoint = RungeKuttaStep(currentPoint, charge, mass, dt);
        
        // Update magnetic field at new position
        nextPoint.bField = fMagField->GetField(nextPoint.position);
        
        trajectory.push_back(nextPoint);
        currentPoint = nextPoint;
        stepCount++;
        
        // Print progress every 100 steps
        // if (stepCount % 100 == 0) {
        //     std::cout << "  Step " << stepCount << ": position=(" 
        //               << currentPoint.position.X() << ", " << currentPoint.position.Y() 
        //               << ", " << currentPoint.position.Z() << ") mm, |p|=" 
        //               << currentPoint.momentum.Mag() << " MeV/c" << std::endl;
        // }
    }
    
    // std::cout << "Trajectory calculation completed with " << trajectory.size() 
            //   << " points over " << currentPoint.time << " ns" << std::endl;
    
    return trajectory;
}

bool ParticleTrajectory::IsValidStep(const TrajectoryPoint& point) const
{
    // Check time limit
    if (point.time > fMaxTime) return false;
    
    // Check distance limit
    if (point.position.Mag() > fMaxDistance) return false;
    
    // Check momentum threshold
    if (point.momentum.Mag() < fMinMomentum) return false;
    
    return true;
}

TVector3 ParticleTrajectory::CalculateForce(const TVector3& position, 
                                          const TVector3& momentum, 
                                          double charge,double E) const
{
    if (!fMagField) return TVector3(0, 0, 0);
    
    // Get magnetic field at position
    TVector3 B = fMagField->GetField(position);
    
    double momentumMag = momentum.Mag();
    if (momentumMag < 1e-6) return TVector3(0, 0, 0);
    

    // dp/dt = q * (v × B)，其中 v = pc²/E
    // 
    // 对于我们的单位系统 (mm, ns, MeV/c, Tesla)，c= 299.8 ~300 mm/ns
    // 使用经验转换系数，这是从实验中确定的标准值
    // （e*1*c）/（MeV/c/ns）= 89.87551787



    const double physics_constant = 89.87551787; // 经验物理常数
    
    // 洛伦兹力: F = q * (p × B) * 常数 / |p|
    // 这里的常数包含了所有必要的单位转换
    TVector3 force = momentum.Cross(B) * (charge * physics_constant  / E);


    // std::cout << "CalculateForce: position=(" << position.X() << ", " 
    //           << position.Y() << ", " << position.Z() << ") mm, "
    //           << "momentum=(" << momentum.X() << ", " << momentum.Y() 
    //           << ", " << momentum.Z() << ") MeV/c, "
    //           << "B=(" << B.X() << ", " << B.Y() << ", " << B.Z() << ") T, "
    //           << "force=(" << force.X() << ", " << force.Y() 
    //           << ", " << force.Z() << ") MeV/c/ns" << std::endl;
    
    return force;
}

ParticleTrajectory::TrajectoryPoint 
ParticleTrajectory::RungeKuttaStep(const TrajectoryPoint& current, 
                                  double charge, double mass, double dt) const
{
    // 4th order Runge-Kutta integration for relativistic motion
    
    TVector3 r0 = current.position;
    TVector3 p0 = current.momentum;
    double t0 = current.time;
    
    // Calculate energy: E = sqrt(p²c² + m²c⁴) = sqrt(p² + m²) in natural units
    double E0 = TMath::Sqrt(p0.Mag2() + mass*mass);
    
    // K1: derivatives at t0
    // velocity = pc²/E, in our units: v [mm/ns] = p [MeV/c] × c² [mm²/ns²] / E [MeV]
    TVector3 k1_r = p0 * (kSpeedOfLight / E0);
    TVector3 k1_p = CalculateForce(r0, p0, charge,E0);

    
    
    // K2: derivatives at t0 + dt/2
    TVector3 r1 = r0 + k1_r * (dt/2);
    TVector3 p1 = p0 + k1_p * (dt/2);
    double E1 = TMath::Sqrt(p1.Mag2() + mass*mass);
    TVector3 k2_r = p1 * (kSpeedOfLight / E1);
    TVector3 k2_p = CalculateForce(r1, p1, charge,E1);
    
    // K3: derivatives at t0 + dt/2 (second estimate)
    TVector3 r2 = r0 + k2_r * (dt/2);
    TVector3 p2 = p0 + k2_p * (dt/2);
    double E2 = TMath::Sqrt(p2.Mag2() + mass*mass);
    TVector3 k3_r = p2 * (kSpeedOfLight / E2);
    TVector3 k3_p = CalculateForce(r2, p2, charge,E2);
    
    // K4: derivatives at t0 + dt
    TVector3 r3 = r0 + k3_r * dt;
    TVector3 p3 = p0 + k3_p * dt;
    double E3 = TMath::Sqrt(p3.Mag2() + mass*mass);
    TVector3 k4_r = p3 * ( kSpeedOfLight / E3);
    TVector3 k4_p = CalculateForce(r3, p3, charge,E3);
    
    // Final step
    TVector3 r_new = r0 + (k1_r + 2*k2_r + 2*k3_r + k4_r) * (dt/6);
    TVector3 p_new = p0 + (k1_p + 2*k2_p + 2*k3_p + k4_p) * (dt/6);
    double t_new = t0 + dt;
    
    return TrajectoryPoint(r_new, p_new, t_new, TVector3(0,0,0));
}

void ParticleTrajectory::GetTrajectoryPoints(const std::vector<TrajectoryPoint>& trajectory,
                                           std::vector<double>& x, std::vector<double>& y,
                                           std::vector<double>& z) const
{
    x.clear();
    y.clear();
    z.clear();
    
    x.reserve(trajectory.size());
    y.reserve(trajectory.size());
    z.reserve(trajectory.size());
    
    for (const auto& point : trajectory) {
        x.push_back(point.position.X());
        y.push_back(point.position.Y());
        z.push_back(point.position.Z());
    }
}

void ParticleTrajectory::PrintTrajectoryInfo(const std::vector<TrajectoryPoint>& trajectory) const
{
    if (trajectory.empty()) {
        std::cout << "Empty trajectory!" << std::endl;
        return;
    }
    
    const TrajectoryPoint& start = trajectory.front();
    const TrajectoryPoint& end = trajectory.back();
    
    // std::cout << "\n=== Trajectory Summary ===" << std::endl;
    // std::cout << "Number of points: " << trajectory.size() << std::endl;
    // std::cout << "Total time: " << end.time << " ns" << std::endl;
    // std::cout << "Total distance: " << (end.position - start.position).Mag() << " mm" << std::endl;
    
    // std::cout << "Start: position=(" << start.position.X() << ", " << start.position.Y() 
    //           << ", " << start.position.Z() << ") mm" << std::endl;
    // std::cout << "       momentum=(" << start.momentum.X() << ", " << start.momentum.Y() 
    //           << ", " << start.momentum.Z() << ") MeV/c" << std::endl;
    
    // std::cout << "End:   position=(" << end.position.X() << ", " << end.position.Y() 
    //           << ", " << end.position.Z() << ") mm" << std::endl;
    // std::cout << "       momentum=(" << end.momentum.X() << ", " << end.momentum.Y() 
    //           << ", " << end.momentum.Z() << ") MeV/c" << std::endl;
    
    double initialMomentum = start.momentum.Mag();
    double finalMomentum = end.momentum.Mag();
    // std::cout << "Momentum change: " << ((finalMomentum - initialMomentum)/initialMomentum * 100) 
    //           << "%" << std::endl;
    // std::cout << "=========================" << std::endl;
}