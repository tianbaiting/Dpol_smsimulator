#ifndef PARTICLE_TRAJECTORY_H
#define PARTICLE_TRAJECTORY_H

#include "TVector3.h"
#include "TLorentzVector.h"
#include "MagneticField.hh"
#include <vector>

/**
 * @class ParticleTrajectory
 * @brief Calculate charged particle trajectory in magnetic field
 * 
 * This class implements Runge-Kutta integration to calculate
 * the trajectory of charged particles in a non-uniform magnetic field.
 */
class ParticleTrajectory {
public:
    struct TrajectoryPoint {
        TVector3 position;      // Position [mm]
        TVector3 momentum;      // Momentum [MeV/c]
        double time;            // Time [ns]
        TVector3 bField;        // Magnetic field at this point [T]
        
        TrajectoryPoint() : position(0,0,0), momentum(0,0,0), time(0), bField(0,0,0) {}
        TrajectoryPoint(const TVector3& pos, const TVector3& mom, double t, const TVector3& b)
            : position(pos), momentum(mom), time(t), bField(b) {}
    };

private:
    MagneticField* fMagField;           // Magnetic field object
    double fStepSize;                   // Integration step size [mm]
    double fMaxTime;                    // Maximum integration time [ns]
    double fMaxDistance;                // Maximum distance from origin [mm]
    double fMinMomentum;                // Minimum momentum threshold [MeV/c]
    
    // Physics constants
    static const double kSpeedOfLight;  // Speed of light [mm/ns]
    static const double kChargeUnit;    // Elementary charge unit for B-field calculation
    
public:
    ParticleTrajectory(MagneticField* magField);
    ~ParticleTrajectory();
    
    // Configuration methods
    void SetStepSize(double stepSize) { fStepSize = stepSize; }
    void SetMaxTime(double maxTime) { fMaxTime = maxTime; }
    void SetMaxDistance(double maxDist) { fMaxDistance = maxDist; }
    void SetMinMomentum(double minMom) { fMinMomentum = minMom; }
    
    double GetStepSize() const { return fStepSize; }
    double GetMaxTime() const { return fMaxTime; }
    double GetMaxDistance() const { return fMaxDistance; }
    double GetMinMomentum() const { return fMinMomentum; }
    
    // Trajectory calculation
    std::vector<TrajectoryPoint> CalculateTrajectory(
        const TVector3& initialPosition,    // Initial position [mm]
        const TLorentzVector& initialMomentum, // Initial 4-momentum [MeV]
        double charge,                      // Particle charge [e]
        double mass                         // Particle mass [MeV/c²]
    ) const;
    
    // Utility methods
    bool IsValidStep(const TrajectoryPoint& point) const;
    TVector3 CalculateForce(const TVector3& position, const TVector3& momentum, 
                           double charge) const;
    
    // Runge-Kutta integration step
    TrajectoryPoint RungeKuttaStep(const TrajectoryPoint& current, 
                                  double charge, double mass, double dt) const;
    
    // Get trajectory endpoints for event display
    void GetTrajectoryPoints(const std::vector<TrajectoryPoint>& trajectory,
                           std::vector<double>& x, std::vector<double>& y, 
                           std::vector<double>& z) const;
    
    // Print trajectory information
    void PrintTrajectoryInfo(const std::vector<TrajectoryPoint>& trajectory) const;
};

#endif // PARTICLE_TRAJECTORY_H