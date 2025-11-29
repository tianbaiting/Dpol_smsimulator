
# 几何接受度分析库 (Geometry Acceptance Library)

此代码用于从几何上分析，不同磁场、不同target位置下，PDC的摆放最佳位置，以及最佳接受度。

## 功能概述

### 主要功能
1. **束流偏转计算**: 根据不同磁场大小和Deuteron束流偏转角度，计算对应的target位置
2. **PDC最佳位置优化**: 找出最佳PDC摆放位置和接受度
3. **NEBULA接受度计算**: 计算固定位置NEBULA的接受度
4. **完整分析报告**: 生成不同配置下的接受度统计

### 物理参数
- **束流**: 氘核(Deuteron)束流 @ 190 MeV/u
- **反应数据**: `/home/tian/workspace/dpol/smsimulator5.5/data/qmdrawdata`
- **磁场配置**: `smsimulator5.5/configs/simulation/geometry/filed_map/`
- **束流偏转角度**: 0°, 5°, 10°
- **参考target位置** (0°): (0, 0, -4000) mm  其余通过代码计算。 

### PDC优化标准
- PDC平面垂直于Pz=600 MeV/c质子的轨迹方向
- 横向动量接受范围: Px = ±100 MeV/c
- PDC实际尺寸 (来自PDCConstruction.cc和detector_geometry.gdml):
  - 外壳: 1700×800×190 mm
  - 有效探测区: 1680×780 mm (U/X/V三层)

### NEBULA配置
- 位置: 固定 (0, 0, 5000) mm
- 阵列尺寸 (来自NEBULA_Detectors_Dayone.csv和TNEBULASimParameter.cc):
  - 宽度(X): ~3600 mm
  - 高度(Y): ~1800 mm
  - 深度(Z): ~600 mm (多层结构)
  - 单个Neut探测器: 120×1800×120 mm

## 代码结构

```
geo_accepentce/
├── CMakeLists.txt                    # CMake构建配置
├── readme.md                         # 本文档
├── include/
│   ├── BeamDeflectionCalculator.hh   # 束流偏转计算类
│   ├── DetectorAcceptanceCalculator.hh # 探测器接受度计算类
│   └── GeoAcceptanceManager.hh       # 分析管理主类
└── src/
    ├── BeamDeflectionCalculator.cc   # 束流偏转实现
    ├── DetectorAcceptanceCalculator.cc # 探测器接受度实现
    ├── GeoAcceptanceManager.cc       # 分析管理实现
    └── RunGeoAcceptanceAnalysis.cc   # 主程序
```

## 依赖关系

### 内部依赖
- `libs/analysis/`: 磁场类(MagneticField)和粒子轨迹类(ParticleTrajectory)
  - 使用Runge-Kutta方法计算带电粒子在磁场中的轨迹

### 外部依赖
- ROOT: 数据处理和可视化
- CMake: 构建系统

## 编译方法

```bash
# 在项目根目录
cd smsimulator5.5

# 创建并进入build目录
mkdir -p build
cd build

# 配置和编译
cmake ..
make GeoAcceptance

# 或编译所有
make -j4
```

## 使用方法

### 1. 基本用法

```bash
# 使用默认配置运行
./bin/RunGeoAcceptanceAnalysis

# 指定磁场文件和参数
./bin/RunGeoAcceptanceAnalysis \
  --fieldmap configs/simulation/geometry/filed_map/141114-0,8T-6000/field.dat --field 0.8 \
  --fieldmap configs/simulation/geometry/filed_map/180626-1,00T-6000/field.dat --field 1.0 \
  --angle 0 --angle 5 --angle 10 \
  --qmd data/qmdrawdata \
  --output ./acceptance_results
```

### 2. 命令行选项

- `--fieldmap <file>`: 磁场文件路径 (可多次指定)
- `--field <value>`: 磁场强度(Tesla) (对应每个fieldmap)
- `--angle <value>`: 偏转角度(度) (可多次指定)
- `--qmd <path>`: QMD数据目录路径
- `--output <path>`: 结果输出目录
- `--help`: 显示帮助信息
