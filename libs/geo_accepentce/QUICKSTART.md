# 几何接受度分析 - 快速入门指南

## 快速开始

### 1. 编译

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
mkdir -p build && cd build
cmake .. -DBUILD_ANALYSIS=ON
make GeoAcceptance -j4
```

### 2. 准备磁场数据

```bash
# 下载并解压磁场文件
cd configs/simulation/geometry/filed_map
python3 downloadFormSamurai.py -d -u

# 检查解压后的文件
ls -lh
```

### 3. 运行分析

#### 方法A: 使用默认配置

```bash
cd build
./bin/RunGeoAcceptanceAnalysis
```

#### 方法B: 指定参数

```bash
./bin/RunGeoAcceptanceAnalysis \
  --fieldmap ../configs/simulation/geometry/filed_map/141114-0,8T-6000/field.dat \
  --field 0.8 \
  --angle 0 --angle 5 --angle 10 \
  --output ./acceptance_results
```

### 4. 查看结果

```bash
# 查看文本报告
cat acceptance_results/acceptance_report.txt

# 用ROOT查看详细数据
root acceptance_results/acceptance_results.root
```

## 在ROOT中分析

```cpp
// 打开结果文件
TFile* f = TFile::Open("acceptance_results.root");
TTree* tree = (TTree*)f->Get("acceptance");

// 查看数据结构
tree->Print();

// 绘制接受度vs角度
tree->Draw("pdcAcceptance:deflectionAngle", "fieldStrength==0.8");

// 绘制接受度vs磁场强度
tree->Draw("pdcAcceptance:fieldStrength", "deflectionAngle==0");

// 创建2D图
TCanvas* c1 = new TCanvas("c1", "Acceptance vs Field and Angle", 800, 600);
tree->Draw("pdcAcceptance:fieldStrength:deflectionAngle", "", "colz");
```

## 示例脚本

### Python脚本分析结果

```python
import uproot
import pandas as pd
import matplotlib.pyplot as plt

# 读取ROOT文件
file = uproot.open("acceptance_results.root")
tree = file["acceptance"]
df = tree.arrays(library="pd")

# 绘图
fig, axes = plt.subplots(1, 3, figsize=(15, 4))

# PDC接受度
axes[0].plot(df['deflectionAngle'], df['pdcAcceptance'], 'o-')
axes[0].set_xlabel('Deflection Angle (deg)')
axes[0].set_ylabel('PDC Acceptance (%)')
axes[0].grid(True)

# NEBULA接受度
axes[1].plot(df['deflectionAngle'], df['nebulaAcceptance'], 's-')
axes[1].set_xlabel('Deflection Angle (deg)')
axes[1].set_ylabel('NEBULA Acceptance (%)')
axes[1].grid(True)

# 符合接受度
axes[2].plot(df['deflectionAngle'], df['coincidenceAcceptance'], '^-')
axes[2].set_xlabel('Deflection Angle (deg)')
axes[2].set_ylabel('Coincidence Acceptance (%)')
axes[2].grid(True)

plt.tight_layout()
plt.savefig('acceptance_comparison.png', dpi=150)
plt.show()
```

## 常见问题

### Q1: 找不到磁场文件

**A:** 确保已下载并解压磁场数据:
```bash
cd configs/simulation/geometry/filed_map
python3 downloadFormSamurai.py -d -u
```

### Q2: 编译错误

**A:** 确保已启用BUILD_ANALYSIS选项:
```bash
cmake .. -DBUILD_ANALYSIS=ON
```

### Q3: QMD数据加载失败

**A:** 检查数据路径是否正确:
```bash
ls -lh data/qmdrawdata/
```

如果数据不存在，需要先运行QMD模拟生成数据。

### Q4: 如何修改PDC接受范围？

**A:** 在代码中修改`DetectorAcceptanceCalculator::CalculateOptimalPDCPosition`中的Px范围:
```cpp
optimalConfig.pxMin = -150.0;  // 修改为-150 MeV/c
optimalConfig.pxMax = 150.0;   // 修改为+150 MeV/c
```

## 性能优化

### 减少计算时间

1. **增大RK步长** (牺牲精度):
```cpp
fTrajectory->SetStepSize(10.0);  // 默认5.0 mm
```

2. **减少偏转角度数量**:
```bash
./RunGeoAcceptanceAnalysis --angle 0 --angle 10  # 只计算2个角度
```

3. **使用更粗的磁场网格** (如果有的话)

### 并行化（未来实现）

目前版本是串行的，未来可以使用OpenMP并行化粒子循环。

## 下一步

- 查看详细文档: `libs/geo_accepentce/readme.md`
- 理解物理原理: 阅读代码注释
- 修改配置: 调整探测器参数
- 可视化结果: 使用ROOT或Python绘图

## 技术支持

如有问题，请检查:
1. 编译日志
2. 运行时输出
3. README文档
4. 代码注释
