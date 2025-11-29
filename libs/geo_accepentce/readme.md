
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
- **参考target位置** (0°): (0, 0, -4000) mm

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

### 3. 在ROOT中使用

```cpp
// 加载库
gSystem->Load("lib/libAnalysis.so");
gSystem->Load("lib/libGeoAcceptance.so");

// 创建管理器
GeoAcceptanceManager* manager = new GeoAcceptanceManager();

// 添加磁场配置
manager->AddFieldMap("configs/simulation/geometry/filed_map/141114-0,8T-6000/field.dat", 0.8);
manager->AddFieldMap("configs/simulation/geometry/filed_map/180626-1,00T-6000/field.dat", 1.0);

// 添加偏转角度
manager->AddDeflectionAngle(0.0);
manager->AddDeflectionAngle(5.0);
manager->AddDeflectionAngle(10.0);

// 设置路径
manager->SetQMDDataPath("data/qmdrawdata");
manager->SetOutputPath("./results");

// 运行分析
manager->RunFullAnalysis();

// 查看结果
manager->PrintResults();
```

## 计算流程

### 1. 束流偏转与Target位置计算

使用`BeamDeflectionCalculator`类:

```cpp
BeamDeflectionCalculator* beamCalc = new BeamDeflectionCalculator();
beamCalc->LoadFieldMap(fieldMapFile);

// 计算不同偏转角度的target位置
auto targetPos_0deg = beamCalc->CalculateTargetPosition(0.0);   // (0, 0, -4000) mm
auto targetPos_5deg = beamCalc->CalculateTargetPosition(5.0);   
auto targetPos_10deg = beamCalc->CalculateTargetPosition(10.0);

// 打印target信息
beamCalc->PrintTargetPosition(targetPos_5deg);
```

**物理过程**:
- 氘核从target位置出射，初始方向沿+Z轴
- 通过磁场时受洛伦兹力偏转
- 使用RK4方法数值积分求解运动方程
- 迭代搜索使得最终偏转角度匹配目标值的target位置

### 2. PDC最佳位置计算

使用`DetectorAcceptanceCalculator`类:

```cpp
DetectorAcceptanceCalculator* detCalc = new DetectorAcceptanceCalculator();
detCalc->SetMagneticField(magField);

// 计算最佳PDC配置
auto pdcConfig = detCalc->CalculateOptimalPDCPosition(targetPos);
```

**优化标准**:
- 追踪参考质子(Pz=600 MeV/c, Px=0)的轨迹
- PDC平面垂直于该轨迹在对应点的切线方向
- 接受Px ∈ [-100, 100] MeV/c的质子

### 3. 接受度计算

```cpp
// 加载QMD反应数据
detCalc->LoadQMDDataFromDirectory(qmdDataPath);

// 计算接受度
auto result = detCalc->CalculateAcceptance();

// 打印报告
detCalc->PrintAcceptanceReport(result, deflectionAngle, fieldStrength);
```

**计算内容**:
- 读取np动量分布数据
- 对每个粒子追踪轨迹
- 检查是否打到PDC或NEBULA
- 统计接受度百分比

### 4. 完整分析

使用`GeoAcceptanceManager`进行批量分析:

```cpp
GeoAcceptanceManager* manager = new GeoAcceptanceManager();

// 配置
auto config = GeoAcceptanceManager::CreateDefaultConfig();
manager->SetConfig(config);

// 运行完整分析
manager->RunFullAnalysis();

// 生成报告
manager->GenerateSummaryTable();
manager->GenerateTextReport("acceptance_report.txt");
manager->GenerateROOTFile("acceptance_results.root");
```

## 输出结果

### 1. 终端输出

程序会在终端显示:
- 各步骤计算进度
- Target位置信息
- PDC配置信息
- 接受度统计表格

示例输出:
```
╔══════════════════════════════════════════════════════════════════════════════════╗
║                        ACCEPTANCE SUMMARY TABLE                                  ║
╠═══════╦═══════╦════════════╦════════════╦════════════╦═══════════════════════════╣
║ Field ║ Angle ║    PDC     ║   NEBULA   ║ Coincidence║      Target Position      ║
║  (T)  ║ (deg) ║    (%)     ║    (%)     ║    (%)     ║        (X, Y, Z) mm       ║
╠═══════╬═══════╬════════════╬════════════╬════════════╬═══════════════════════════╣
║  0.80 ║  0.00 ║      45.23 ║      67.89 ║      38.45 ║ (  0.00,   0.00, -4000.00) ║
║  0.80 ║  5.00 ║      43.12 ║      65.34 ║      36.78 ║ (-120.45,   0.00, -4000.00) ║
...
╚═══════╩═══════╩════════════╩════════════╩════════════╩═══════════════════════════╝
```

### 2. 文本报告 (acceptance_report.txt)

包含详细的配置参数和结果:
- Target位置坐标
- PDC配置(位置、法向量、Px范围)
- NEBULA配置
- 接受度统计

### 3. ROOT文件 (acceptance_results.root)

包含TTree "acceptance"，分支:
- `fieldStrength`: 磁场强度 [T]
- `deflectionAngle`: 偏转角度 [度]
- `targetX, targetY, targetZ`: Target位置 [mm]
- `pdcX, pdcY, pdcZ`: PDC位置 [mm]
- `totalEvents`: 总事例数
- `pdcHits, nebulaHits, bothHits`: 各探测器命中数
- `pdcAcceptance, nebulaAcceptance, coincidenceAcceptance`: 接受度 [%]

可用ROOT进行后续分析和绘图。

## 类接口说明

### BeamDeflectionCalculator

**主要方法**:
- `SetMagneticField(MagneticField*)`: 设置磁场
- `CalculateTargetPosition(double angle)`: 计算给定偏转角的target位置
- `GetBeamMomentum(TVector3 direction)`: 获取束流4动量
- `PrintBeamInfo()`: 打印束流参数

### DetectorAcceptanceCalculator  

**主要方法**:
- `SetMagneticField(MagneticField*)`: 设置磁场
- `LoadQMDData(string file)`: 加载QMD数据
- `CalculateOptimalPDCPosition(TVector3 targetPos)`: 计算最佳PDC位置
- `CheckPDCHit(ParticleInfo)`: 检查粒子是否打到PDC
- `CheckNEBULAHit(ParticleInfo)`: 检查粒子是否打到NEBULA
- `CalculateAcceptance()`: 计算接受度

### GeoAcceptanceManager

**主要方法**:
- `AddFieldMap(string file, double strength)`: 添加磁场配置
- `AddDeflectionAngle(double angle)`: 添加偏转角度
- `RunFullAnalysis()`: 执行完整分析
- `GenerateSummaryTable()`: 生成汇总表
- `GenerateTextReport(string file)`: 生成文本报告
- `GenerateROOTFile(string file)`: 生成ROOT文件

## 注意事项

1. **QMD数据格式**: 需要根据实际数据格式实现`LoadQMDData()`函数
2. **磁场文件**: 确保磁场文件已解压到正确位置
3. **内存管理**: 大量粒子数据可能占用较多内存
4. **计算时间**: RK积分和轨迹追踪较耗时，建议使用粗步长或并行化

## 未来改进

- [ ] 实现QMD数据读取接口
- [ ] 添加并行计算支持
- [ ] 优化RK积分步长自适应
- [ ] 添加角度分布分析
- [ ] 实现PDC尺寸优化
- [ ] 添加可视化工具(事例显示)
- [ ] 支持更多探测器配置

## 参考

- 磁场和轨迹计算: `libs/analysis/`
- 磁场数据: http://ribf.riken.jp/~hsato/fieldmaps/180703-1,40T-6000.zip 类似
- QMD反应数据: `data/qmdrawdata/`

## 作者与维护

创建日期: 2025-11-30
依赖项目: smsimulator5.5 

