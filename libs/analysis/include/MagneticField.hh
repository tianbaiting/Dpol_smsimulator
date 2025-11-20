#ifndef MagneticField_H
#define MagneticField_H

#include "TVector3.h"
#include "TObject.h"
#include <vector>
#include <string>

/**
 * @class MagneticField
 * @brief 用于读取和插值磁场数据的类
 * 
 * 磁场文件格式:
 * Line 1: Nx Ny Nz NFields (e.g., 301 81 301 2)
 * Line 2-7: 列标题描述 (X[MM], Y[MM], Z[MM], BX[1], BY[1], BZ[1])
 * Line 8: 0 (分隔符)
 * Line 9+: X Y Z BX BY BZ 数据
 */
class MagneticField : public TObject {
private:
    // 网格尺寸
    int fNx, fNy, fNz;      // 网格点数
    int fTotalPoints;       // 总点数 = Nx * Ny * Nz
    
    // 空间范围
    double fXmin, fXmax;    // X 范围 [mm]
    double fYmin, fYmax;    // Y 范围 [mm] 
    double fZmin, fZmax;    // Z 范围 [mm]
    
    // 网格步长
    double fXstep, fYstep, fZstep;
    
    // 磁场数据
    std::vector<double> fBx;  // X方向磁场 [Tesla]
    std::vector<double> fBy;  // Y方向磁场 [Tesla]
    std::vector<double> fBz;  // Z方向磁场 [Tesla]
    
    // 旋转参数
    double fRotationAngle;    // 绕Y轴负方向的旋转角度 [度]
    double fCosTheta, fSinTheta;  // 旋转角度的余弦和正弦值
    
    // 内部辅助函数
    int GetIndex(int ix, int iy, int iz) const;
    bool IsValidIndex(int ix, int iy, int iz) const;
    void GetGridIndices(int index, int& ix, int& iy, int& iz) const;
    
    // 坐标旋转函数
    TVector3 RotateToMagnetFrame(const TVector3& labPos) const;
    TVector3 RotateToLabFrame(const TVector3& magnetField) const;
    
    // 对称性处理函数
    TVector3 GetFieldWithSymmetry(double x, double y, double z) const;

public:
    // 构造函数和析构函数
    MagneticField();
    MagneticField(const std::string& filename);
    virtual ~MagneticField();
    
    // 加载磁场文件
    bool LoadFieldMap(const std::string& filename);
    
    // 设置磁铁旋转角度
    void SetRotationAngle(double angle);  // 角度，绕Y轴负方向旋转
    double GetRotationAngle() const { return fRotationAngle; }
    
    // 获取磁场（在实验室坐标系中）
    TVector3 GetField(double x, double y, double z) const;
    TVector3 GetField(const TVector3& position) const;
    
    // 获取原始磁场（磁铁坐标系，无旋转）
    TVector3 GetFieldRaw(double x, double y, double z) const;
    TVector3 GetFieldRaw(const TVector3& position) const;
    
    // 三线性插值
    double InterpolateTrilinear(const std::vector<double>& data, 
                               double x, double y, double z) const;
    
    // 获取网格信息
    int GetNx() const { return fNx; }
    int GetNy() const { return fNy; }
    int GetNz() const { return fNz; }
    int GetTotalPoints() const { return fTotalPoints; }
    
    // 获取空间范围
    double GetXmin() const { return fXmin; }
    double GetXmax() const { return fXmax; }
    double GetYmin() const { return fYmin; }
    double GetYmax() const { return fYmax; }
    double GetZmin() const { return fZmin; }
    double GetZmax() const { return fZmax; }
    
    // 获取步长
    double GetXstep() const { return fXstep; }
    double GetYstep() const { return fYstep; }
    double GetZstep() const { return fZstep; }
    
    // 检查点是否在磁场范围内
    bool IsInRange(double x, double y, double z) const;
    bool IsInRange(const TVector3& position) const;
    
    // 获取网格点位置
    TVector3 GetGridPosition(int ix, int iy, int iz) const;
    
    // 获取原始磁场数据（调试用）
    double GetBx(int index) const { return (index >= 0 && index < fTotalPoints) ? fBx[index] : 0.0; }
    double GetBy(int index) const { return (index >= 0 && index < fTotalPoints) ? fBy[index] : 0.0; }
    double GetBz(int index) const { return (index >= 0 && index < fTotalPoints) ? fBz[index] : 0.0; }
    
    // 保存和加载ROOT文件
    void SaveAsROOTFile(const std::string& filename, const std::string& objectName = "MagField") const;
    bool LoadFromROOTFile(const std::string& filename, const std::string& objectName = "MagField");
    
    // 打印磁场信息
    void PrintInfo() const;
    
    ClassDef(MagneticField, 1)
};

#endif // MagneticField_H