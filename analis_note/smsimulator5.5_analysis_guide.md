# SMSIMULATOR5.5 使用分析指南

## 项目概述

SMSIMULATOR5.5 是用于 SAMURAI 实验的 Geant4 仿真器，专门用于模拟重离子+中子反应实验。该仿真器由 R. Tanaka、Y. Kondo 和 M. Yasuda 开发，版本 5.0 兼容 Geant4.10.x。

## 系统环境要求

### 必需软件
1. **Geant4** (版本 10.x)
2. **ANAROOT** 库 (可从 RIBFDAQ 网页下载)
3. **ROOT** 框架
4. **GNU Make**

### 当前系统配置
- Geant4 安装路径: `/home/tbt/Software/geant4/install/`
- ANAROOT 安装路径: `/home/tbt/Software/anaroot/install`
- 项目路径: `/home/tbt/workspace/dpol/smsimulator5.5`

## 项目结构分析

```
smsimulator5.5/
├── README                    # 详细使用说明
├── setup.sh                 # 环境设置脚本
├── bin/                     # 可执行文件目录
│   └── sim_deuteron        # 氘核仿真主程序
├── lib/                     # 库文件目录
├── smg4lib/                 # 主要仿真库
├── sim_deuteron/            # 氘核仿真模块
├── geant4_sim/              # Geant4 仿真配置
├── inputtree/               # 输入数据树
└── analis_note/            # 分析说明文档 (此目录)
```

## 使用流程

### 第一步：环境配置

1. **加载环境设置**
```bash
cd /home/tbt/workspace/dpol/smsimulator5.5
source setup.sh
```

2. **验证环境变量**
```bash
echo $SMSIMDIR
echo $TARTSYS
echo $G4SMLIBDIR
```

### 第二步：编译系统

1. **编译主库**
```bash
cd smg4lib
make clean
make
```

2. **编译氘核仿真模块**
```bash
cd ../sim_deuteron
make clean
make
```

### 第三步：几何配置

项目包含多个几何配置文件，位于 `geant4_sim/geometry/` 目录：

- `5deg_1.2T.mac` - 5度角度，1.2T 磁场配置
- `8deg_1.2T.mac` - 8度角度，1.2T 磁场配置  
- `10deg_1.2T.mac` - 10度角度，1.2T 磁场配置

#### 几何配置要素：

1. **磁场设置**
   - 磁场数据文件: `/nas/data/Bryan/field_map/180626-1,20T-3000.bin`
   - 磁场因子: 1
   - 偶极子角度: 30度

2. **目标设置**
   - 材料: Sn (锡)
   - 尺寸: 80×80×30 mm
   - 角度: 5度

3. **PDC (Proportional Drift Chamber) 探测器**
   - 角度: 62度
   - 位置1: (40, 0, 300) cm
   - 位置2: (40, 0, 400) cm

4. **NEBULA 中子探测器**
   - 参数文件: `NEBULA_Dayone.csv`
   - 探测器参数文件: `NEBULA_Detectors_Dayone.csv`

### 第四步：粒子束流输入配置

SMSIMULATOR5.5 支持多种粒子束流输入方式，通过 `PrimaryGeneratorActionBasic` 基类实现。

#### 4.1 束流类型 (BeamType)

支持以下五种束流类型：

1. **Pencil Beam (束状光束)**
   ```
   /action/gun/BeamType Pencil
   ```
   - 单一能量，单一方向的理想光束
   - 适用于简单的几何测试

2. **Gaussian Beam (高斯分布光束)**
   ```
   /action/gun/BeamType Gaus
   ```
   - 位置和角度具有高斯分布
   - 更真实的光束模拟

3. **Isotropic Beam (各向同性光束)**
   ```
   /action/gun/BeamType Isotropic
   ```
   - 各向同性发射
   - 用于背景研究

4. **Tree Input (树状输入)**
   ```
   /action/gun/BeamType Tree
   ```
   - 从 ROOT 树文件读取粒子信息
   - **当前项目主要使用的方式**

5. **Geantino Beam**
   ```
   /action/gun/BeamType Geantino
   ```
   - 用于几何调试的虚拟粒子

#### 4.2 Tree Input 详细配置

当前项目使用 Tree 输入方式，通过读取预计算的反应产物数据：

##### 输入数据结构 (TBeamSimData)

```cpp
class TBeamSimData {
public:
  Int_t          fPrimaryParticleID;  // 主粒子ID
  Int_t          fZ;                  // 原子序数
  Int_t          fA;                  // 质量数
  Int_t          fPDGCode;           // PDG编码
  TString        fParticleName;      // 粒子名称
  Double_t       fCharge;            // 电荷
  Double_t       fMass;              // 质量 (MeV)
  TLorentzVector fMomentum;          // 四动量 (MeV)
  TVector3       fPosition;          // 位置 (mm)
  Double_t       fTime;              // 时间 (ns)
  Bool_t         fIsAccepted;        // 是否被接受
};
```

##### 输入文件配置

```
/action/gun/TreeInputFile inputtree/unpol/d+Sn124E190g070xyz.root
/action/gun/TreeName tree
```

##### 当前可用的输入数据

输入数据位于 `inputtree/unpol/` 目录：

**反应类型：**
- **d+Pb208** (氘核+铅208) - 能量190 MeV，角度范围50-100度
- **d+Sn112** (氘核+锡112) - 能量190 MeV，角度范围50-100度  
- **d+Sn124** (氘核+锡124) - 能量190 MeV，角度范围50-100度
- **d+Xe130** (氘核+氙130) - 能量190 MeV，角度100度

**文件命名规则:** `d+核素E能量g角度xyz.root`

#### 4.3 束流参数设置

##### 粒子设定
```
/action/gun/BeamA 2              # 质量数 (氘核)
/action/gun/BeamZ 1              # 原子序数 (氘核)
/action/gun/BeamEnergy 190 MeV   # 束流能量
```

##### 位置和角度设定
```
/action/gun/BeamPosition 0 0 -1000 mm        # 束流起始位置
/action/gun/BeamPositionXSigma 2 mm          # X方向位置分布
/action/gun/BeamPositionYSigma 2 mm          # Y方向位置分布
/action/gun/BeamAngleX 0 deg                 # X方向角度
/action/gun/BeamAngleY 0 deg                 # Y方向角度
/action/gun/BeamAngleXSigma 1 mrad           # X方向角度分布
/action/gun/BeamAngleYSigma 1 mrad           # Y方向角度分布
```

##### 粒子过滤设置
```
/action/gun/SkipNeutron false     # 是否跳过中子
/action/gun/SkipHeavyIon false    # 是否跳过重离子
/action/gun/SkipGamma false       # 是否跳过γ射线
```

### 第五步：运行仿真

#### 5.1 基本运行流程

1. **准备工作目录**
```bash
cd /home/tbt/workspace/dpol/smsimulator5.5/geant4_sim
```

2. **运行仿真程序**
```bash
../bin/sim_deuteron
```

3. **在 Geant4 交互模式中执行以下命令**

##### 基本设置命令序列：

```bash
# 加载几何配置
/control/execute geometry/5deg_1.2T.mac

# 设置束流类型为 Tree 输入
/action/gun/BeamType Tree

# 设置输入文件
/action/gun/TreeInputFile ../inputtree/unpol/d+Sn124E190g070xyz.root
/action/gun/TreeName tree

# 设置输出文件
/action/output/Filename output_d+Sn124_070deg.root

# 设置事件数
/action/gun/TreeBeamOn 1000

# 开始仿真
/run/beamOn 1000
```

#### 5.2 完整的宏文件示例

创建一个完整的宏文件 `run_simulation.mac`：

```bash
# ===================================
# SMSIMULATOR5.5 运行脚本示例
# ===================================

# 基本设置
/control/suppressAbortion 1
/control/verbose 1
/run/verbose 1

# 加载几何配置
/control/execute geometry/5deg_1.2T.mac

# 束流设置
/action/gun/BeamType Tree
/action/gun/TreeInputFile ../inputtree/unpol/d+Sn124E190g070xyz.root
/action/gun/TreeName tree

# 粒子过滤设置
/action/gun/SkipNeutron false
/action/gun/SkipHeavyIon false  
/action/gun/SkipGamma false

# 探测器数据设置
/action/data/NEBULA/StoreSteps true
/action/data/NEBULA/Convert true

# 输出设置
/action/output/Filename output_d+Sn124_070deg.root

# 运行设置
/action/gun/TreeBeamOn 10000

# 执行仿真
/run/beamOn 10000

# 保存并退出
/action/output/Save
exit
```

#### 5.3 批处理模式运行

对于大规模仿真，建议使用批处理模式：

```bash
# 直接运行宏文件
../bin/sim_deuteron run_simulation.mac

# 或者使用重定向
../bin/sim_deuteron < run_simulation.mac > simulation.log 2>&1
```

#### 5.4 不同几何配置的运行

针对不同的角度配置：

```bash
# 5度配置
/control/execute geometry/5deg_1.2T.mac
/action/gun/TreeInputFile ../inputtree/unpol/d+Sn124E190g050xyz.root

# 8度配置  
/control/execute geometry/8deg_1.2T.mac
/action/gun/TreeInputFile ../inputtree/unpol/d+Sn124E190g080xyz.root

# 10度配置
/control/execute geometry/10deg_1.2T.mac
/action/gun/TreeInputFile ../inputtree/unpol/d+Sn124E190g100xyz.root
```

#### 5.5 可视化模式

对于调试和验证，可以使用可视化模式：

```bash
# 启动可视化
/control/execute vis.mac

# 显示几何
/vis/drawVolume

# 设置视角
/vis/viewer/set/viewpointThetaPhi 90 0 deg

# 运行少量事件以查看轨迹
/run/beamOn 1
```

### 第六步：探测器数据记录与分析

#### 6.1 NEBULA 中子探测器数据记录

NEBULA (NEutron-detection system for Breakup of Unstable-nuclei with Large Acceptance) 是专门用于中子探测的大接受角探测器系统。

##### NEBULA 探测器结构
- **中子探测器** (Neutron Detector): 塑料闪烁体阵列
- **反符合探测器** (Veto Detector): 用于排除带电粒子的薄层探测器

##### NEBULA 数据记录 (TNEBULASimData)

每次粒子与 NEBULA 探测器发生相互作用时，会记录以下信息：

```cpp
class TNEBULASimData {
public:
  Int_t    fPrimaryParticleID;    // 主粒子ID
  Int_t    fParentID;             // 父轨迹ID
  Int_t    fTrackID;              // 当前轨迹ID
  Int_t    fStepNo;               // 步数
  Int_t    fZ;                    // 原子序数
  Int_t    fA;                    // 质量数
  Int_t    fPDGCode;              // PDG编码
  Int_t    fModuleID;             // 模块ID
  Int_t    fID;                   // 探测器ID
  TString  fParticleName;         // 粒子名称
  TString  fDetectorName;         // 探测器名称
  TString  fModuleName;           // 模块名称
  TString  fProcessName;          // 物理过程名称
  Double_t fCharge;               // 电荷
  Double_t fMass;                 // 质量 (MeV)
  Double_t fPreKineticEnergy;     // 步前动能 (MeV)
  Double_t fPostKineticEnergy;    // 步后动能 (MeV)
  TVector3 fPrePosition;          // 步前位置 (mm)
  TVector3 fPostPosition;         // 步后位置 (mm)
  TLorentzVector fPreMomentum;    // 步前四动量 (MeV)
  TLorentzVector fPostMomentum;   // 步后四动量 (MeV)
  Double_t fEnergyDeposit;        // 能量沉积 (MeV)
  Double_t fPreTime;              // 步前时间 (ns)
  Double_t fPostTime;             // 步后时间 (ns)
  Bool_t   fIsAccepted;           // 是否被接受
};
```

##### NEBULA 几何配置

几何参数由两个 CSV 文件定义：

1. **NEBULA_Dayone.csv** - 整体系统参数
```
Position, 0, 0, 7249.72,        # 探测器位置 (mm)
NeutSize, 120, 1800, 120,       # 中子探测器尺寸 (mm)
VetoSize, 320, 1900, 10,        # 反符合探测器尺寸 (mm)
Angle, 0, 0, 0,                 # 旋转角度 (deg)
```

2. **NEBULA_Detectors_Dayone.csv** - 各个探测器位置

##### NEBULA 数据控制命令

```
/action/data/NEBULA/StoreSteps true/false     # 是否存储步骤数据
/action/data/NEBULA/Convert true/false        # 是否转换为观测量
```

#### 6.2 PDC (Proportional Drift Chamber) 数据记录

PDC 是用于重离子轨迹测量的多丝漂移室探测器。

##### PDC 探测器结构

PDC 系统包含三个测量层：
- **U 层**: 倾斜导线层，测量一个倾斜坐标
- **X 层**: 水平导线层，测量 X 坐标
- **V 层**: 另一个倾斜导线层，测量另一个倾斜坐标

##### PDC 敏感探测器设置

```cpp
// 在 DeutDetectorConstruction.cc 中
fPDCSD_U = new FragmentSD("/PDC_U");  // U层敏感探测器
fPDCSD_X = new FragmentSD("/PDC_X");  // X层敏感探测器
fPDCSD_V = new FragmentSD("/PDC_V");  // V层敏感探测器
```

##### PDC 几何参数 (5deg_1.2T.mac)

```
/samurai/geometry/PDC/Angle 62 deg           # PDC 旋转角度
/samurai/geometry/PDC/Position1 40 0 300 cm  # 第一个PDC位置
/samurai/geometry/PDC/Position2 40 0 400 cm  # 第二个PDC位置
```

##### PDC 数据记录 (TFragSimData)

PDC 使用 `FragmentSD` 敏感探测器，记录重离子的轨迹信息：

```cpp
class TFragSimData {
public:
  Int_t    fPrimaryParticleID;    // 主粒子ID
  Int_t    fTrackID;              // 轨迹ID
  Int_t    fZ;                    // 原子序数
  Int_t    fA;                    // 质量数
  TString  fParticleName;         // 粒子名称
  TString  fDetectorName;         // 探测器名称
  Double_t fCharge;               // 电荷
  Double_t fMass;                 // 质量
  Double_t fKineticEnergy;        // 动能
  TVector3 fPosition;             // 位置
  TVector3 fMomentum;             // 动量
  Double_t fTime;                 // 时间
  Double_t fEnergyDeposit;        // 能量沉积
};
```

#### 6.3 数据分析流程

##### 输出数据结构

输出的 ROOT 文件包含以下 Tree 结构：
1. **SimulationTree** - 主要数据树
   - NEBULASimData 分支 - NEBULA 探测器数据
   - FragSimData 分支 - PDC 和其他探测器数据
   - BeamSimData 分支 - 输入束流数据
   - FragSimParameter 分支 - 仿真参数

2. **ParameterTree** - 参数树
   - 几何配置参数
   - 场强设置
   - 其他仿真设置

##### 数据转换

从 Geant4 步骤数据转换为物理观测量：

1. **能量沉积转换为光产额**
   ```cpp
   // 在 NEBULASimDataConverter 中
   lightOutput = energyDeposit * lightYield;
   ```

2. **时间信息校正**
   ```cpp
   correctedTime = rawTime - flightTime + timeOffset;
   ```

3. **位置重建**
   ```cpp
   // 从多个击中点重建粒子轨迹
   reconstructedPosition = weightedAverage(hitPositions, energyDeposits);
   ```

##### 分析示例脚本

位于各模块的 `macros/examples/` 目录：

```cpp
// analysis_example.cc 示例
void analysis_example() {
    TFile *file = new TFile("output.root");
    TTree *tree = (TTree*)file->Get("SimulationTree");
    
    TClonesArray *nebulaArray = 0;
    tree->SetBranchAddress("NEBULASimData", &nebulaArray);
    
    for (int i = 0; i < tree->GetEntries(); i++) {
        tree->GetEntry(i);
        
        for (int j = 0; j < nebulaArray->GetEntries(); j++) {
            TNEBULASimData *data = (TNEBULASimData*)nebulaArray->At(j);
            
            // 分析中子击中
            if (data->fParticleName == "neutron") {
                Double_t tof = data->fPreTime;          // 飞行时间
                Double_t energy = data->fEnergyDeposit; // 沉积能量
                TVector3 pos = data->fPrePosition;      // 击中位置
                
                // 进行中子能量和角度分析
                // ...
            }
        }
    }
}
```

## 重要配置参数

### 当前项目特定配置 (5deg_1.2T.mac)

```plaintext
# 偶极子磁场
/samurai/geometry/Dipole/Angle 30 deg
/samurai/geometry/Dipole/FieldFile /nas/data/Bryan/field_map/180626-1,20T-3000.bin
/samurai/geometry/Dipole/FieldFactor 1

# 目标
/samurai/geometry/Target/SetTarget false
/samurai/geometry/Target/Material Sn
/samurai/geometry/Target/Size 80 80 30 mm
/samurai/geometry/Target/Position -2.43 0.00 -90.79 cm
/samurai/geometry/Target/Angle 5.00 deg

# PDC 探测器
/samurai/geometry/PDC/Angle 62 deg
/samurai/geometry/PDC/Position1 40 0 300 cm
/samurai/geometry/PDC/Position2 40 0 400 cm

# NEBULA 中子探测器
/samurai/geometry/NEBULA/ParameterFileName geometry/NEBULA_Dayone.csv
/samurai/geometry/NEBULA/DetectorParameterFileName geometry/NEBULA_Detectors_Dayone.csv
```

## 性能优化建议

### 7.1 内存和存储优化

1. **控制步骤数据存储** (大幅减少文件大小)
```
/action/data/NEBULA/StoreSteps false     # 不存储详细步骤数据
```

2. **选择性粒子记录**
```
/action/gun/SkipGamma true               # 跳过γ射线
/action/data/NEBULA/EnergyThreshold 0.1  # 设置能量阈值 (MeV)
```

3. **文件压缩设置**
```cpp
// 在 ROOT 文件中设置压缩级别
file->SetCompressionLevel(9);
```

### 7.2 仿真性能优化

1. **防止仿真中止**
```
/control/suppressAbortion 1
```

2. **物理过程优化**
```
/physics/SetCuts 0.1 mm                  # 设置产生次级粒子的最小距离
/physics/SetRegionCuts detector 0.05 mm  # 在探测器区域使用更精细的切割
```

3. **并行处理**
```bash
# 将大任务分割为多个小任务并行运行
for angle in 050 060 070 080 090 100; do
    ../bin/sim_deuteron run_${angle}.mac &
done
wait
```

### 7.3 大规模仿真的批处理脚本

创建批处理脚本 `batch_simulation.sh`：

```bash
#!/bin/bash

# 设置环境
source ../setup.sh

# 仿真参数
REACTIONS=("d+Pb208" "d+Sn112" "d+Sn124")
ANGLES=("050" "060" "070" "080" "090" "100")
GEOMETRIES=("5deg_1.2T" "8deg_1.2T" "10deg_1.2T")
EVENTS=10000

# 创建输出目录
mkdir -p output_data

for reaction in "${REACTIONS[@]}"; do
    for angle in "${ANGLES[@]}"; do
        for geom in "${GEOMETRIES[@]}"; do
            
            # 生成宏文件
            cat > batch_${reaction}_${angle}_${geom}.mac << EOF
/control/suppressAbortion 1
/control/execute geometry/${geom}.mac
/action/gun/BeamType Tree
/action/gun/TreeInputFile ../inputtree/unpol/${reaction}E190g${angle}xyz.root
/action/gun/TreeName tree
/action/data/NEBULA/StoreSteps false
/action/output/Filename output_data/${reaction}_${angle}_${geom}.root
/action/gun/TreeBeamOn ${EVENTS}
/run/beamOn ${EVENTS}
exit
EOF
            
            # 后台运行
            echo "Starting simulation: ${reaction} ${angle} ${geom}"
            ../bin/sim_deuteron batch_${reaction}_${angle}_${geom}.mac > \
                logs/${reaction}_${angle}_${geom}.log 2>&1 &
            
            # 控制并行任务数量
            while [ $(jobs -r | wc -l) -ge 4 ]; do
                sleep 10
            done
        done
    done
done

# 等待所有任务完成
wait
echo "All simulations completed!"
```

## 故障排除

### 8.1 常见编译错误

1. **Geant4 路径问题**
```bash
# 检查 Geant4 环境
echo $G4INSTALL
echo $G4INCLUDE
source /home/tbt/Software/geant4/install/share/Geant4/geant4make/geant4make.sh
```

2. **ANAROOT 链接错误**
```bash
# 检查 ANAROOT 环境
echo $TARTSYS
export LD_LIBRARY_PATH=$TARTSYS/lib:$LD_LIBRARY_PATH
```

3. **ROOT 版本兼容性**
```bash
# 检查 ROOT 版本
root-config --version
# 确保 ROOT 版本与 ANAROOT 兼容
```

### 8.2 运行时错误

1. **磁场文件找不到**
```
错误: Cannot open field file: /nas/data/Bryan/field_map/180626-1,20T-3000.bin
解决: 检查磁场文件路径，或在几何文件中修改路径
```

2. **输入树文件错误**
```
错误: Cannot open input tree file
解决: 检查输入文件路径和文件完整性
```

3. **内存不足**
```
错误: std::bad_alloc
解决: 减少事件数量或开启 StoreSteps false
```

### 8.3 输出文件问题

1. **文件过大**
```bash
# 检查文件大小
ls -lh output*.root

# 如果文件过大，修改设置
/action/data/NEBULA/StoreSteps false
/action/data/NEBULA/EnergyThreshold 1.0  # 提高阈值
```

2. **数据缺失**
```bash
# 检查 ROOT 文件内容
root -l output.root
root [0] .ls
root [1] SimulationTree->Show(0)
```

3. **权限问题**
```bash
# 确认输出目录权限
chmod 755 output_directory
```

### 8.4 性能监控

1. **监控系统资源**
```bash
# 实时监控
htop

# 内存使用
free -h

# 磁盘使用
df -h
```

2. **仿真进度监控**
```bash
# 监控日志文件
tail -f simulation.log

# 检查输出文件增长
watch -n 30 'ls -lh output*.root'
```

## 下一步开发方向

根据 README 中的 TODO 部分：

1. **需要添加的功能**
   - 重离子分辨率模拟
   - 目标中的能量损失差异效应
   - 仿真器有效性评估文档

2. **可能的改进**
   - 优化中子仿真精度
   - 增加更多探测器类型支持
   - 改进用户界面

## 联系信息

- 开发者: R. Tanaka, Y. Kondo, M. Yasuda
- 更多信息: http://be.nucl.ap.titech.ac.jp/~ryuki/iroiro/geant4/
- RIBFDAQ 网页: http://ribf.riken.jp/RIBFDAQ/

---

*最后更新: 2025年7月16日*
*版本: SMSIMULATOR 5.5*
