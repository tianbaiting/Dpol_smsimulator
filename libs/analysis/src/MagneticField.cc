#include "MagneticField.hh"
#include "SMLogger.hh"
#include "TFile.h"
#include "TArrayI.h"
#include "TArrayD.h"
#include "TMath.h"
#include <fstream>
#include <sstream>
#include <cmath>

ClassImp(MagneticField)

MagneticField::MagneticField() 
    : fNx(0), fNy(0), fNz(0), fTotalPoints(0),
      fXmin(0), fXmax(0), fYmin(0), fYmax(0), fZmin(0), fZmax(0),
      fXstep(0), fYstep(0), fZstep(0),
      fRotationAngle(30.0), fCosTheta(0), fSinTheta(0)  // 默认30度
{
    SetRotationAngle(30.0);  // 设置默认旋转角度
}

MagneticField::MagneticField(const std::string& filename) 
    : fNx(0), fNy(0), fNz(0), fTotalPoints(0),
      fXmin(0), fXmax(0), fYmin(0), fYmax(0), fZmin(0), fZmax(0),
      fXstep(0), fYstep(0), fZstep(0),
      fRotationAngle(30.0), fCosTheta(0), fSinTheta(0)  // 默认30度
{
    SetRotationAngle(30.0);  // 设置默认旋转角度
    LoadFieldMap(filename);
}

MagneticField::~MagneticField() 
{
    fBx.clear();
    fBy.clear();
    fBz.clear();
}

bool MagneticField::LoadFieldMap(const std::string& filename) 
{
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        SM_ERROR("MagneticField::LoadFieldMap: 无法打开文件 {}", filename);
        return false;
    }
    
    SM_INFO("MagneticField::LoadFieldMap: 正在加载磁场文件 {}", filename);
    
    // [EN] Read grid dimensions to map linearized field arrays to 3D indices. / [CN] 读取网格尺寸以将线性磁场数组映射到三维索引。
    // 读取第一行: Nx Ny Nz NFields
    int nfields;
    if (!(file >> fNx >> fNy >> fNz >> nfields)) {
        SM_ERROR("MagneticField::LoadFieldMap: 无法读取网格尺寸");
        return false;
    }
    
    fTotalPoints = fNx * fNy * fNz;
    SM_INFO("网格尺寸: {} x {} x {} = {} 点", fNx, fNy, fNz, fTotalPoints);
    
    // [EN] Skip header lines so numerical parsing starts at the field table. / [CN] 跳过表头行以便直接读取磁场数值表。
    // 跳过列标题行 (6行) 和分隔符行 (1行)
    std::string line;
    std::getline(file, line); // 读取第一行剩余部分
    for (int i = 0; i < 7; i++) {
        std::getline(file, line);
    }
    
    // 重置数据向量
    fBx.clear(); fBy.clear(); fBz.clear();
    fBx.resize(fTotalPoints);
    fBy.resize(fTotalPoints);
    fBz.resize(fTotalPoints);
    
    // 初始化范围变量
    bool firstPoint = true;
    
    // [EN] Stream all grid points to preserve cache-friendly contiguous storage. / [CN] 顺序读取网格点以保持缓存友好的连续存储。
    // 读取数据点
    double x, y, z, bx, by, bz;
    int pointCount = 0;
    
    while (file >> x >> y >> z >> bx >> by >> bz && pointCount < fTotalPoints) {
        // 更新空间范围
        if (firstPoint) {
            fXmin = fXmax = x;
            fYmin = fYmax = y;
            fZmin = fZmax = z;
            firstPoint = false;
        } else {
            if (x < fXmin) fXmin = x;
            if (x > fXmax) fXmax = x;
            if (y < fYmin) fYmin = y;
            if (y > fYmax) fYmax = y;
            if (z < fZmin) fZmin = z;
            if (z > fZmax) fZmax = z;
        }
        
        // 存储磁场数据
        fBx[pointCount] = bx;
        fBy[pointCount] = by;
        fBz[pointCount] = bz;
        
        pointCount++;
        
        if (pointCount % 100000 == 0) {
            SM_DEBUG("已读取 {} / {} 点...", pointCount, fTotalPoints);
        }
    }
    
    file.close();
    
    if (pointCount != fTotalPoints) {
        SM_WARN("读取的点数 ({}) 与预期不符 ({})", pointCount, fTotalPoints);
        fTotalPoints = pointCount;
    }
    
    // [EN] Step size derives from bounds to support trilinear interpolation. / [CN] 由范围计算步长以支持三线性插值。
    // 计算网格步长
    fXstep = (fNx > 1) ? (fXmax - fXmin) / (fNx - 1) : 0;
    fYstep = (fNy > 1) ? (fYmax - fYmin) / (fNy - 1) : 0;
    fZstep = (fNz > 1) ? (fZmax - fZmin) / (fNz - 1) : 0;
    
    PrintInfo();
    SM_INFO("MagneticField::LoadFieldMap: 磁场加载完成!");
    
    return true;
}

void MagneticField::SetRotationAngle(double angle) 
{
    fRotationAngle = angle;
    // [EN] Cache sin/cos to avoid recomputation during per-step field queries. / [CN] 预缓存正弦余弦以避免每步查询时重复计算。
    // 磁铁绕Y轴旋转angle度（从上往下看顺时针为正）
    // 这意味着磁铁+Z方向在实验室中偏向-X方向
    double angleRad = angle * TMath::Pi() / 180.0;  // 使用正角度
    fCosTheta = TMath::Cos(angleRad);
    fSinTheta = TMath::Sin(angleRad);
    
    SM_INFO("MagneticField: 设置旋转角度为 {} 度 (磁铁+Z偏向实验室-X)", angle);
}

TVector3 MagneticField::RotateToMagnetFrame(const TVector3& labPos) const 
{
    // 将实验室坐标系的位置转换到磁铁坐标系
    // 逆旋转：R_y(-θ)
    // x_m =  x_lab * cos(θ) + z_lab * sin(θ)
    // z_m = -x_lab * sin(θ) + z_lab * cos(θ)
    double x = labPos.X() * fCosTheta + labPos.Z() * fSinTheta;
    double y = labPos.Y();
    double z = -labPos.X() * fSinTheta + labPos.Z() * fCosTheta;
    
    return TVector3(x, y, z);
}

TVector3 MagneticField::RotateToLabFrame(const TVector3& magnetField) const 
{
    // 将磁铁坐标系的矢量转换到实验室坐标系
    // 正旋转：R_y(θ)
    // x_lab = x_m * cos(θ) - z_m * sin(θ)
    // z_lab = x_m * sin(θ) + z_m * cos(θ)
    // 
    // 验证：磁铁(0,0,1000)应该变为实验室(-sin(θ)*1000, 0, cos(θ)*1000)
    //       即对于θ=30°: (-500, 0, 866) - 磁铁+Z偏向实验室-X ✓
    double bx = magnetField.X() * fCosTheta - magnetField.Z() * fSinTheta;
    double by = magnetField.Y();
    double bz = magnetField.X() * fSinTheta + magnetField.Z() * fCosTheta;
    
    return TVector3(bx, by, bz);
}

TVector3 MagneticField::GetField(double x, double y, double z) const 
{
    // 将实验室坐标转换到磁铁坐标系
    TVector3 labPos(x, y, z);
    TVector3 magnetPos = RotateToMagnetFrame(labPos);
    
    // 在磁铁坐标系中获取磁场
    TVector3 magnetField = GetFieldRaw(magnetPos.X(), magnetPos.Y(), magnetPos.Z());
    
    // 将磁场转换回实验室坐标系
    // std::cout<<"Debug: Magnet Frame Field = (" << magnetField.X() << ", " << magnetField.Y() << ", " << magnetField.Z() << ")" << std::endl;

    // std::cout<<"Debug: Lab Pos = (" << labPos.X() << ", " << labPos.Y() << ", " << labPos.Z() << ")" << std::endl;

    // std::cout<<"--"<<std::endl;


    return RotateToLabFrame(magnetField);
}

TVector3 MagneticField::GetField(const TVector3& position) const 
{
    return GetField(position.X(), position.Y(), position.Z());
}

TVector3 MagneticField::GetFieldRaw(double x, double y, double z) const 
{
    // 使用对称性处理所有坐标
    return GetFieldWithSymmetry(x, y, z);
}

TVector3 MagneticField::GetFieldRaw(const TVector3& position) const 
{
    return GetFieldRaw(position.X(), position.Y(), position.Z());
}

TVector3 MagneticField::GetFieldWithSymmetry(double x, double y, double z) const 
{
    // [EN] Use mirror symmetry to extend limited field maps without duplicating data. / [CN] 利用镜像对称扩展有限磁场表而无需复制数据。
    // 处理坐标对称性，将所有坐标映射到原始数据覆盖的区域 (x>=0, z>=0)
    double map_x = x;
    double map_z = z;
    
    // 用于跟踪是否需要翻转磁场分量
    bool flip_bx = false;  // x坐标翻转时需要翻转Bx
    bool flip_bz = false;  // z坐标翻转时需要翻转Bz
    
    // 处理x坐标对称性
    if (x < 0) {
        map_x = -x;      // 映射到正x区域
        flip_bx = true;  // 需要翻转Bx分量
    }
    
    // 处理z坐标对称性  
    if (z < 0) {
        map_z = -z;      // 映射到正z区域
        flip_bz = true;  // 需要翻转Bz分量
    }
    
    // 检查映射后的坐标是否在数据范围内
    if (!IsInRange(map_x, y, map_z)) {
        return TVector3(0, 0, 0);
    }
    
    // 在映射坐标处进行三线性插值
    double bx = InterpolateTrilinear(fBx, map_x, y, map_z);
    double by = InterpolateTrilinear(fBy, map_x, y, map_z);
    double bz = InterpolateTrilinear(fBz, map_x, y, map_z);
    
    // 根据对称性规律调整磁场分量符号
    if (flip_bx) bx = -bx;  // x → -x: Bx → -Bx
    if (flip_bz) bz = -bz;  // z → -z: Bz → -Bz
    // By 分量在x和z翻转时都保持不变
    
    return TVector3(bx, by, bz);
}

double MagneticField::InterpolateTrilinear(const std::vector<double>& data, 
                                         double x, double y, double z) const 
{
    // [EN] Trilinear weights ensure smooth forces for stable Runge-Kutta integration. / [CN] 三线性权重保证力场平滑以稳定Runge-Kutta积分。
    // 将坐标转换为网格索引
    double fx = (x - fXmin) / fXstep;
    double fy = (y - fYmin) / fYstep;
    double fz = (z - fZmin) / fZstep;
    
    // 获取相邻网格点的整数索引
    int ix = static_cast<int>(std::floor(fx));
    int iy = static_cast<int>(std::floor(fy));
    int iz = static_cast<int>(std::floor(fz));
    
    // 确保索引在有效范围内
    ix = std::max(0, std::min(ix, fNx - 2));
    iy = std::max(0, std::min(iy, fNy - 2));
    iz = std::max(0, std::min(iz, fNz - 2));
    
    // 计算插值权重
    double dx = fx - ix;
    double dy = fy - iy;
    double dz = fz - iz;
    
    // 确保权重在 [0,1] 范围内
    dx = std::max(0.0, std::min(1.0, dx));
    dy = std::max(0.0, std::min(1.0, dy));
    dz = std::max(0.0, std::min(1.0, dz));
    
    // 插值
    double result = 0.0;
    
    for (int i = 0; i < 2; i++) {
        double wx = (i == 0) ? (1.0 - dx) : dx;
        for (int j = 0; j < 2; j++) {
            double wy = (j == 0) ? (1.0 - dy) : dy;
            for (int k = 0; k < 2; k++) {
                double wz = (k == 0) ? (1.0 - dz) : dz;
                
                int idx = GetIndex(ix + i, iy + j, iz + k);
                if (idx >= 0 && idx < static_cast<int>(data.size())) {
                    result += wx * wy * wz * data[idx];
                }
            }
        }
    }
    
    return result;
}

int MagneticField::GetIndex(int ix, int iy, int iz) const 
{
    if (!IsValidIndex(ix, iy, iz)) {
        return -1;
    }
    return ix * fNy * fNz + iy * fNz + iz;
}

bool MagneticField::IsValidIndex(int ix, int iy, int iz) const 
{
    return (ix >= 0 && ix < fNx && 
            iy >= 0 && iy < fNy && 
            iz >= 0 && iz < fNz);
}

void MagneticField::GetGridIndices(int index, int& ix, int& iy, int& iz) const 
{
    ix = index / (fNy * fNz);
    int temp = index % (fNy * fNz);
    iy = temp / fNz;
    iz = temp % fNz;
}

bool MagneticField::IsInRange(double x, double y, double z) const 
{
    return (x >= fXmin && x <= fXmax &&
            y >= fYmin && y <= fYmax &&
            z >= fZmin && z <= fZmax);
}

bool MagneticField::IsInRange(const TVector3& position) const 
{
    return IsInRange(position.X(), position.Y(), position.Z());
}

TVector3 MagneticField::GetGridPosition(int ix, int iy, int iz) const 
{
    double x = fXmin + ix * fXstep;
    double y = fYmin + iy * fYstep;
    double z = fZmin + iz * fZstep;
    return TVector3(x, y, z);
}

void MagneticField::SaveAsROOTFile(const std::string& filename, const std::string& objectName) const 
{
    TFile file(filename.c_str(), "RECREATE");
    if (!file.IsOpen()) {
        SM_ERROR("MagneticField::SaveAsROOTFile: 无法创建文件 {}", filename);
        return;
    }
    
    // 保存网格信息
    TArrayI gridInfo(3);
    gridInfo[0] = fNx; gridInfo[1] = fNy; gridInfo[2] = fNz;
    
    // 保存空间范围
    TArrayD rangeInfo(6);
    rangeInfo[0] = fXmin; rangeInfo[1] = fXmax;
    rangeInfo[2] = fYmin; rangeInfo[3] = fYmax;
    rangeInfo[4] = fZmin; rangeInfo[5] = fZmax;
    
    // 保存磁场数据
    TArrayD bxData(fTotalPoints);
    TArrayD byData(fTotalPoints);
    TArrayD bzData(fTotalPoints);
    
    for (int i = 0; i < fTotalPoints; i++) {
        bxData[i] = fBx[i];
        byData[i] = fBy[i];
        bzData[i] = fBz[i];
    }
    
    file.WriteObject(&gridInfo, "GridInfo");
    file.WriteObject(&rangeInfo, "RangeInfo");
    file.WriteObject(&bxData, "Bx");
    file.WriteObject(&byData, "By");
    file.WriteObject(&bzData, "Bz");
    
    file.Close();
    SM_INFO("MagneticField::SaveAsROOTFile: 已保存到 {}", filename);
}

bool MagneticField::LoadFromROOTFile(const std::string& filename, const std::string& objectName) 
{
    TFile file(filename.c_str(), "READ");
    if (!file.IsOpen()) {
        SM_ERROR("MagneticField::LoadFromROOTFile: 无法打开文件 {}", filename);
        return false;
    }
    
    // 读取网格信息
    TArrayI* gridInfo = (TArrayI*)file.GetObjectChecked("GridInfo", "TArrayI");
    TArrayD* rangeInfo = (TArrayD*)file.GetObjectChecked("RangeInfo", "TArrayD");
    TArrayD* bxData = (TArrayD*)file.GetObjectChecked("Bx", "TArrayD");
    TArrayD* byData = (TArrayD*)file.GetObjectChecked("By", "TArrayD");
    TArrayD* bzData = (TArrayD*)file.GetObjectChecked("Bz", "TArrayD");
    
    if (!gridInfo || !rangeInfo || !bxData || !byData || !bzData) {
        SM_ERROR("MagneticField::LoadFromROOTFile: 数据不完整");
        file.Close();
        return false;
    }
    
    // 设置参数
    fNx = gridInfo->At(0); fNy = gridInfo->At(1); fNz = gridInfo->At(2);
    fTotalPoints = fNx * fNy * fNz;
    
    fXmin = rangeInfo->At(0); fXmax = rangeInfo->At(1);
    fYmin = rangeInfo->At(2); fYmax = rangeInfo->At(3);
    fZmin = rangeInfo->At(4); fZmax = rangeInfo->At(5);
    
    fXstep = (fNx > 1) ? (fXmax - fXmin) / (fNx - 1) : 0;
    fYstep = (fNy > 1) ? (fYmax - fYmin) / (fNy - 1) : 0;
    fZstep = (fNz > 1) ? (fZmax - fZmin) / (fNz - 1) : 0;
    
    // 复制磁场数据
    fBx.resize(fTotalPoints); fBy.resize(fTotalPoints); fBz.resize(fTotalPoints);
    for (int i = 0; i < fTotalPoints; i++) {
        fBx[i] = bxData->At(i);
        fBy[i] = byData->At(i);
        fBz[i] = bzData->At(i);
    }
    
    file.Close();
    
    PrintInfo();
    SM_INFO("MagneticField::LoadFromROOTFile: 从 {} 加载完成!", filename);
    
    return true;
}

void MagneticField::PrintInfo() const 
{
    SM_INFO("=== 磁场信息 ===");
    SM_INFO("网格尺寸: {} x {} x {} (总点数: {})", fNx, fNy, fNz, fTotalPoints);
    SM_INFO("X 范围: [{}, {}] mm, 步长: {} mm", fXmin, fXmax, fXstep);
    SM_INFO("Y 范围: [{}, {}] mm, 步长: {} mm", fYmin, fYmax, fYstep);
    SM_INFO("Z 范围: [{}, {}] mm, 步长: {} mm", fZmin, fZmax, fZstep);
    
    // 计算磁场统计信息
    if (!fBx.empty()) {
        double bxMax = *std::max_element(fBx.begin(), fBx.end());
        double byMax = *std::max_element(fBy.begin(), fBy.end());
        double bzMax = *std::max_element(fBz.begin(), fBz.end());
        double bxMin = *std::min_element(fBx.begin(), fBx.end());
        double byMin = *std::min_element(fBy.begin(), fBy.end());
        double bzMin = *std::min_element(fBz.begin(), fBz.end());
        
        SM_INFO("Bx 范围: [{}, {}] T", bxMin, bxMax);
        SM_INFO("By 范围: [{}, {}] T", byMin, byMax);
        SM_INFO("Bz 范围: [{}, {}] T", bzMin, bzMax);
    }
    SM_INFO("================");
}
