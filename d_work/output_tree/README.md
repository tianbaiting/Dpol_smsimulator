# Output Tree 使用说明

## 概述
`output_tree`目录包含仿真运行后生成的ROOT文件，用于存储氘核实验的蒙特卡罗仿真数据。

## 文件格式
- **文件名格式**: `d+Pb208E190g050xyz0000.root`
  - `d+Pb208`: 氘核+铅208靶
  - `E190`: 束流能量190 MeV
  - `g050`: geometry参数
  - `xyz0000`: 配置标识
  - `.root`: ROOT数据格式

## 数据结构

### 主要分支 (Branches)
ROOT文件包含一个名为`tree`的TTree，共243个分支，主要包括：

#### 1. 束流数据 (beam)
- `beam.fZ`, `beam.fA`: 原子序数和质量数
- `beam.fCharge`, `beam.fMass`: 电荷和质量
- `beam.fMomentum`: 四动量矢量 (TLorentzVector)
- `beam.fPosition`: 位置矢量 (TVector3)
- `beam.fTime`: 时间信息

#### 2. 碎片仿真数据 (FragSimData)
- `FragSimData.fTrackID`: 径迹ID
- `FragSimData.fZ`, `FragSimData.fA`: 碎片原子序数和质量数
- `FragSimData.fPreMomentum`, `FragSimData.fPostMomentum`: 反应前后动量
- `FragSimData.fPrePosition`, `FragSimData.fPostPosition`: 反应前后位置
- `FragSimData.fEnergyDeposit`: 能量损失
- `FragSimData.fDetectorName`: 探测器名称

#### 3. 靶重建数据 (target_*)
- `target_x`, `target_y`, `target_z`: 靶点坐标
- `target_*_err`: 重建误差

#### 4. PDC径迹数据 (PDC1*, PDC2*)
- `PDC1*_x`, `PDC1*_y`: PDC1层位置信息
- `PDC2*_x`, `PDC2*_y`: PDC2层位置信息
- 包含U, X, V三个方向的信息

#### 5. NEBULA探测器数据 (NEBULAPla)
- `NEBULAPla.fCharge[120]`: 各探测器单元电荷
- `NEBULAPla.fTime[120]`: 时间信息
- `NEBULAPla.fGPos[120]`: 全局位置
- 共120个NEBULA塑闪探测器单元

## 使用方法

### 1. 基本ROOT命令
```bash
# 进入输出目录
cd /path/to/output_tree

# 打开ROOT文件
root -l d+Pb208E190g050xyz0000.root

# 在ROOT中查看树结构
tree->Print()
tree->Show(0)  # 显示第0个事件
```

### 2. 数据分析示例
```cpp
// ROOT宏示例
TFile* f = TFile::Open("d+Pb208E190g050xyz0000.root");
TTree* tree = (TTree*)f->Get("tree");

// 读取束流信息
TClonesArray* beam = 0;
tree->SetBranchAddress("beam", &beam);

// 读取碎片数据  
TClonesArray* fragData = 0;
tree->SetBranchAddress("FragSimData", &fragData);

// 事件循环
for(int i = 0; i < tree->GetEntries(); i++) {
    tree->GetEntry(i);
    // 分析代码...
}
```

### 3. Python分析 (使用uproot)
```python
import uproot
import numpy as np

# 读取ROOT文件
file = uproot.open("d+Pb208E190g050xyz0000.root")
tree = file["tree"]

# 获取分支数据
beam_data = tree["beam.fMomentum"].array()
frag_data = tree["FragSimData.fEnergyDeposit"].array()
```

## 数据特征
- **事件数量**: 100个事件
- **文件大小**: ~23.8 KB
- **压缩比**: 4.29
- **数据完整性**: 包含完整的粒子径迹、探测器响应和重建信息

## 注意事项
1. 文件使用ROOT格式，需要ROOT环境或兼容库
2. 某些类定义可能需要加载相应的共享库
3. 分析时注意数组分支的长度可能不固定
4. NEBULA数据包含120个探测器单元的完整信息

## 相关文件
- 仿真配置: `../simulation.mac`
- 几何定义: `../geometry/`
- 分析脚本: 可在此目录创建ROOT宏或Python脚本
