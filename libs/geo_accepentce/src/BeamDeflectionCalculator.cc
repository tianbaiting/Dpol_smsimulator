#include "BeamDeflectionCalculator.hh"
#include "SMLogger.hh"
#include "TMath.h"
#include <iostream>
#include <iomanip>

// Deuteron 物理常数
const double BeamDeflectionCalculator::kDeuteronMass = 1875.612928;  // MeV/c²
const double BeamDeflectionCalculator::kDeuteronCharge = 1.0;        // e
const double BeamDeflectionCalculator::kBeamEnergyPerNucleon = 190.0; // MeV/u

BeamDeflectionCalculator::BeamDeflectionCalculator()
    : fMagField(nullptr), fTrajectory(nullptr),
      fBeamEntryPoint(0, 0, -4000),  // 入射点: (0, 0, -4000) mm
      fInitialDirection(0, 0, 1),     // 初始方向: 沿+Z
      fStepSize(1.0), fAngleTolerance(0.5)
{
}

BeamDeflectionCalculator::~BeamDeflectionCalculator()
{
    if (fTrajectory) {
        delete fTrajectory;
        fTrajectory = nullptr;
    }
    // fMagField is managed externally, don't delete
}

void BeamDeflectionCalculator::SetMagneticField(MagneticField* magField)
{
    fMagField = magField;
    
    // 重新创建trajectory计算器
    if (fTrajectory) {
        delete fTrajectory;
    }
    fTrajectory = new ParticleTrajectory(fMagField);
    fTrajectory->SetStepSize(fStepSize);
    fTrajectory->SetMaxDistance(10000.0); // 10m
    fTrajectory->SetMaxTime(200.0);       // 200 ns
}

void BeamDeflectionCalculator::LoadFieldMap(const std::string& filename)
{
    if (fMagField) {
        fMagField->LoadFieldMap(filename);
    } else {
        fMagField = new MagneticField(filename);
        SetMagneticField(fMagField);
    }
}

double BeamDeflectionCalculator::CalculateBeamMomentum() const
{
    // E_kinetic = 190 MeV/u * 2 = 380 MeV
    double E_kin = kBeamEnergyPerNucleon * 2.0;
    double E_total = E_kin + kDeuteronMass;
    double p = TMath::Sqrt(E_total * E_total - kDeuteronMass * kDeuteronMass);
    return p;
}

double BeamDeflectionCalculator::CalculateBeamEnergy() const
{
    double E_kin = kBeamEnergyPerNucleon * 2.0;
    return E_kin + kDeuteronMass;
}

double BeamDeflectionCalculator::CalculateBeamBeta() const
{
    double E_total = CalculateBeamEnergy();
    double p = CalculateBeamMomentum();
    return p / E_total;
}

double BeamDeflectionCalculator::CalculateBeamGamma() const
{
    double E_total = CalculateBeamEnergy();
    return E_total / kDeuteronMass;
}

TLorentzVector BeamDeflectionCalculator::GetBeamMomentum(const TVector3& direction) const
{
    double p_mag = CalculateBeamMomentum();
    TVector3 p_vec = direction.Unit() * p_mag;
    double E = CalculateBeamEnergy();
    
    return TLorentzVector(p_vec.X(), p_vec.Y(), p_vec.Z(), E);
}

double BeamDeflectionCalculator::CalculateDeflectionAngle(const TVector3& momentum) const
{
    // 计算当前动量方向相对于初始方向的偏转角度
    TVector3 currentDir = momentum.Unit();
    TVector3 initialDir = fInitialDirection.Unit();
    
    // 使用点积计算夹角
    double cosAngle = currentDir.Dot(initialDir);
    // 限制在[-1, 1]范围内，避免浮点误差
    cosAngle = TMath::Max(-1.0, TMath::Min(1.0, cosAngle));
    
    double angle = TMath::ACos(cosAngle) * TMath::RadToDeg();
    return angle;
}

BeamDeflectionCalculator::TargetPosition 
BeamDeflectionCalculator::CalculateTargetPosition(double deflectionAngle)
{
    TargetPosition result;
    result.deflectionAngle = deflectionAngle;
    
    if (!fMagField || !fTrajectory) {
        SM_ERROR("BeamDeflectionCalculator: Magnetic field not set!");
        return result;
    }
    
    SM_INFO("=== Calculating Target Position for {:.1f}° deflection ===", deflectionAngle);
    SM_INFO("  Beam entry point: ({:.1f}, {:.1f}, {:.1f}) mm", 
            fBeamEntryPoint.X(), fBeamEntryPoint.Y(), fBeamEntryPoint.Z());
    SM_INFO("  Initial direction: ({:.3f}, {:.3f}, {:.3f})", 
            fInitialDirection.X(), fInitialDirection.Y(), fInitialDirection.Z());
    
    // 0度情况：target就在入射点，束流沿初始方向
    if (TMath::Abs(deflectionAngle) < 0.01) {
        result.position = fBeamEntryPoint;
        result.beamDirection = fInitialDirection;
        result.rotationAngle = 0;
        SM_INFO("0° deflection: Target at beam entry point");
        PrintTargetPosition(result);
        return result;
    }
    
    // 从入射点发射氘核，追踪完整轨迹
    TLorentzVector p0 = GetBeamMomentum(fInitialDirection);
    auto trajectory = fTrajectory->CalculateTrajectory(fBeamEntryPoint, p0,
                                                        kDeuteronCharge, kDeuteronMass);
    
    if (trajectory.size() < 10) {
        SM_ERROR("Failed to calculate beam trajectory!");
        return result;
    }
    
    SM_DEBUG("Trajectory calculated with {} points", trajectory.size());
    
    // 在轨迹上寻找偏转角度达到目标值的点
    bool found = false;
    double minAngleDiff = 999.0;
    size_t bestIndex = 0;
    
    for (size_t i = 1; i < trajectory.size(); i++) {
        double currentAngle = CalculateDeflectionAngle(trajectory[i].momentum);
        double angleDiff = TMath::Abs(currentAngle - deflectionAngle);
        
        if (angleDiff < minAngleDiff) {
            minAngleDiff = angleDiff;
            bestIndex = i;
        }
        
        // 如果找到足够接近的点
        if (angleDiff < fAngleTolerance) {
            found = true;
        }
        
        // 输出一些调试信息（每100个点输出一次）
        if (i % 100 == 0) {
            SM_DEBUG("  Point {}: pos=({:.1f}, {:.1f}, {:.1f}) mm, angle={:.2f}°",
                     i, trajectory[i].position.X(), trajectory[i].position.Y(),
                     trajectory[i].position.Z(), currentAngle);
        }
    }
    
    if (!found) {
        SM_WARN("Exact deflection angle {:.1f}° not found in trajectory, "
                "using closest point with {:.2f}° difference", 
                deflectionAngle, minAngleDiff);
    }
    
    // 使用线性插值找到更精确的位置
    if (bestIndex > 0 && bestIndex < trajectory.size() - 1) {
        double anglePrev = CalculateDeflectionAngle(trajectory[bestIndex-1].momentum);
        double angleCurr = CalculateDeflectionAngle(trajectory[bestIndex].momentum);
        double angleNext = CalculateDeflectionAngle(trajectory[bestIndex+1].momentum);
        
        // 判断应该在前一段还是后一段插值
        size_t i1, i2;
        double angle1, angle2;
        
        if (TMath::Abs(angleCurr - deflectionAngle) < TMath::Abs(anglePrev - deflectionAngle) &&
            TMath::Abs(angleCurr - deflectionAngle) < TMath::Abs(angleNext - deflectionAngle)) {
            // 当前点最接近，检查应该与前一点还是后一点插值
            if (TMath::Abs(anglePrev - deflectionAngle) < TMath::Abs(angleNext - deflectionAngle)) {
                i1 = bestIndex - 1; i2 = bestIndex;
                angle1 = anglePrev; angle2 = angleCurr;
            } else {
                i1 = bestIndex; i2 = bestIndex + 1;
                angle1 = angleCurr; angle2 = angleNext;
            }
        } else if ((deflectionAngle - anglePrev) * (deflectionAngle - angleCurr) <= 0) {
            i1 = bestIndex - 1; i2 = bestIndex;
            angle1 = anglePrev; angle2 = angleCurr;
        } else {
            i1 = bestIndex; i2 = bestIndex + 1;
            angle1 = angleCurr; angle2 = angleNext;
        }
        
        // 线性插值
        if (TMath::Abs(angle2 - angle1) > 0.001) {
            double t = (deflectionAngle - angle1) / (angle2 - angle1);
            t = TMath::Max(0.0, TMath::Min(1.0, t));  // 限制在[0,1]
            
            result.position = trajectory[i1].position * (1.0 - t) + 
                             trajectory[i2].position * t;
            TVector3 interpMom = trajectory[i1].momentum * (1.0 - t) + 
                                trajectory[i2].momentum * t;
            result.beamDirection = interpMom.Unit();
        } else {
            result.position = trajectory[bestIndex].position;
            result.beamDirection = trajectory[bestIndex].momentum.Unit();
        }
    } else {
        result.position = trajectory[bestIndex].position;
        result.beamDirection = trajectory[bestIndex].momentum.Unit();
    }
    
    // 计算旋转角度（束流方向相对于+Z轴在XZ平面的夹角）
    // 这个角度用于后续PDC位置计算
    result.rotationAngle = TMath::ATan2(result.beamDirection.X(), 
                                        result.beamDirection.Z()) * TMath::RadToDeg();
    
    SM_INFO("Target position found:");
    SM_INFO("  Position: ({:.2f}, {:.2f}, {:.2f}) mm", 
            result.position.X(), result.position.Y(), result.position.Z());
    SM_INFO("  Beam direction: ({:.4f}, {:.4f}, {:.4f})", 
            result.beamDirection.X(), result.beamDirection.Y(), result.beamDirection.Z());
    SM_INFO("  Rotation angle: {:.2f}°", result.rotationAngle);
    SM_INFO("  Angle difference from target: {:.3f}°", minAngleDiff);
    
    PrintTargetPosition(result);
    
    return result;
}

std::vector<BeamDeflectionCalculator::TargetPosition> 
BeamDeflectionCalculator::CalculateTargetPositions(const std::vector<double>& deflectionAngles)
{
    std::vector<TargetPosition> results;
    
    for (double angle : deflectionAngles) {
        TargetPosition pos = CalculateTargetPosition(angle);
        results.push_back(pos);
    }
    
    return results;
}

std::vector<ParticleTrajectory::TrajectoryPoint> 
BeamDeflectionCalculator::CalculateFullBeamTrajectory()
{
    std::vector<ParticleTrajectory::TrajectoryPoint> trajectory;
    
    if (!fTrajectory) {
        SM_ERROR("BeamDeflectionCalculator: No trajectory calculator available!");
        return trajectory;
    }
    
    TLorentzVector p0 = GetBeamMomentum(fInitialDirection);
    trajectory = fTrajectory->CalculateTrajectory(fBeamEntryPoint, p0,
                                                   kDeuteronCharge, kDeuteronMass);
    
    return trajectory;
}

void BeamDeflectionCalculator::PrintBeamInfo() const
{
    SM_INFO("=== Deuteron Beam Parameters ===");
    SM_INFO("  Beam energy: {:.1f} MeV/u", kBeamEnergyPerNucleon);
    SM_INFO("  Total kinetic energy: {:.1f} MeV", kBeamEnergyPerNucleon * 2);
    SM_INFO("  Mass: {:.3f} MeV/c²", kDeuteronMass);
    SM_INFO("  Momentum: {:.2f} MeV/c", CalculateBeamMomentum());
    SM_INFO("  Total energy: {:.2f} MeV", CalculateBeamEnergy());
    SM_INFO("  β = {:.4f}", CalculateBeamBeta());
    SM_INFO("  γ = {:.4f}", CalculateBeamGamma());
    SM_INFO("  Entry point: ({:.1f}, {:.1f}, {:.1f}) mm",
            fBeamEntryPoint.X(), fBeamEntryPoint.Y(), fBeamEntryPoint.Z());
    SM_INFO("=================================");
}

void BeamDeflectionCalculator::PrintTargetPosition(const TargetPosition& target) const
{
    SM_INFO("--- Target Position ---");
    SM_INFO("  Deflection angle: {:.1f}°", target.deflectionAngle);
    SM_INFO("  Position: ({:.2f}, {:.2f}, {:.2f}) mm",
            target.position.X(), target.position.Y(), target.position.Z());
    SM_INFO("  Beam direction: ({:.4f}, {:.4f}, {:.4f})",
            target.beamDirection.X(), target.beamDirection.Y(), target.beamDirection.Z());
    SM_INFO("  Rotation angle: {:.2f}°", target.rotationAngle);
    SM_INFO("------------------------");
}
