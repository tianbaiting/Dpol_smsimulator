#ifndef BEAM_DEFLECTION_CALCULATOR_HH
#define BEAM_DEFLECTION_CALCULATOR_HH

#include "TVector3.h"
#include "TLorentzVector.h"
#include "MagneticField.hh"
#include "ParticleTrajectory.hh"
#include <vector>
#include <string>

/**
 * @class BeamDeflectionCalculator
 * @brief 计算束流在磁场中的偏转，确定target位置
 * 
 * 氘核束流从入射点(0, 0, -4000)mm 沿+Z方向入射，
 * 经过磁场偏转后，在特定偏转角度时的位置即为target位置
 * 
 * 工作原理：
 * 1. 从入射点发射氘核(190 MeV/u)，初始方向沿+Z
 * 2. 使用RK4方法追踪粒子在磁场中的运动轨迹
 * 3. 计算轨迹上每一点相对于入射方向的偏转角度
 * 4. 找到偏转角度达到目标值(如5°、10°)时的位置
 */
class BeamDeflectionCalculator {
public:
    struct TargetPosition {
        double deflectionAngle;  // 偏转角度 [度]
        TVector3 position;       // Target位置 [mm]
        TVector3 beamDirection;  // 束流在该位置的方向（单位矢量）
        double rotationAngle;    // Target旋转角度 [度]，用于PDC计算
        double fieldStrength;    // 磁场强度标识
        std::string fieldMapFile; // 磁场文件名
        
        TargetPosition() : deflectionAngle(0), position(0,0,-4000), 
                          beamDirection(0,0,1), rotationAngle(0), fieldStrength(0) {}
    };

private:
    MagneticField* fMagField;
    ParticleTrajectory* fTrajectory;
    
    // 束流参数 (Deuteron @ 190 MeV/u)
    static const double kDeuteronMass;      // 1875.612928 MeV/c²
    static const double kDeuteronCharge;    // +1 e
    static const double kBeamEnergyPerNucleon; // 190 MeV/u
    
    // 入射位置（束流起始点）
    TVector3 fBeamEntryPoint;  // 束流入射点 (0, 0, -4000) mm
    TVector3 fInitialDirection; // 初始方向 (0, 0, 1) 沿+Z
    
    // 计算参数
    double fStepSize;           // RK步长 [mm]
    double fAngleTolerance;     // 角度容差 [度]
    
public:
    BeamDeflectionCalculator();
    ~BeamDeflectionCalculator();
    
    // 设置磁场
    void SetMagneticField(MagneticField* magField);
    void LoadFieldMap(const std::string& filename);
    
    // 设置入射点（默认(0, 0, -4000) mm）
    void SetBeamEntryPoint(const TVector3& pos) { fBeamEntryPoint = pos; }
    void SetBeamEntryPoint(double x, double y, double z) { 
        fBeamEntryPoint.SetXYZ(x, y, z); 
    }
    
    // 设置初始方向（默认沿+Z）
    void SetInitialDirection(const TVector3& dir) { fInitialDirection = dir.Unit(); }
    
    // 计算给定偏转角度对应的target位置
    // 从入射点追踪氘核轨迹，找到偏转达到指定角度时的位置
    TargetPosition CalculateTargetPosition(double deflectionAngle);
    
    // 批量计算不同角度的target位置
    std::vector<TargetPosition> CalculateTargetPositions(
        const std::vector<double>& deflectionAngles
    );
    
    // 计算完整的束流轨迹（从入射点开始）
    std::vector<ParticleTrajectory::TrajectoryPoint> CalculateFullBeamTrajectory();
    
    // 获取束流初始动量 (Deuteron @ 190 MeV/u)
    TLorentzVector GetBeamMomentum(const TVector3& direction) const;
    
    // 工具函数
    double CalculateBeamMomentum() const;  // |p| [MeV/c]
    double CalculateBeamEnergy() const;    // E_total [MeV]
    double CalculateBeamBeta() const;      // v/c
    double CalculateBeamGamma() const;     // γ
    
    // 设置参数
    void SetStepSize(double step) { fStepSize = step; }
    void SetAngleTolerance(double tol) { fAngleTolerance = tol; }
    
    // 获取入射点
    TVector3 GetBeamEntryPoint() const { return fBeamEntryPoint; }
    
    // 打印信息
    void PrintBeamInfo() const;
    void PrintTargetPosition(const TargetPosition& target) const;
    
private:
    // 计算轨迹上某点相对于初始方向的偏转角度
    double CalculateDeflectionAngle(const TVector3& momentum) const;
};

#endif // BEAM_DEFLECTION_CALCULATOR_HH
