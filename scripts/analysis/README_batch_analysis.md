# 批处理重建分析使用说明

## 概述
这套工具用于批处理分析 Geant4 输出文件，进行 PDC 和 NEBULA 重建，并保存原始粒子动量信息以供后续分析。

## 文件说明

### 1. `batch_analysis.C` - 主要批处理脚本
- **功能**: 批量处理 Geant4 输出文件，执行 PDC 和 NEBULA 重建
- **输入**: Geant4 模拟输出的 ROOT 文件
- **输出**: 包含重建结果和原始粒子数据的 ROOT 文件

#### 保存的数据结构:
- `RecoEvent`: PDC 和 NEBULA 重建结果
  - PDC 击中点和重建轨迹
  - NEBULA 重建中子信息（位置、能量、动量等）
- `OriginalParticleData`: 原始 Geant4 粒子数据
  - 原始质子动量和位置
  - 原始中子动量和位置

### 2. `test_batch_analysis.C` - 快速测试脚本
- **功能**: 测试批处理分析是否正常运行并显示基本统计
- **用途**: 验证重建流程和查看基本结果

### 3. `analyze_reco_performance.C` - 详细性能分析脚本
- **功能**: 深入分析重建性能，对比原始和重建数据
- **输出**: 
  - 原始粒子动量分布图
  - 重建结果统计图
  - 原始vs重建动量对比图
  - 重建分辨率分析

## 使用方法

### 1. 基本用法 - 使用默认文件
```cpp
root -l
.L batch_analysis.C
batch_analysis()  // 使用默认输入文件
```

### 2. 指定输入输出文件
```cpp
root -l
.L batch_analysis.C
batch_analysis("/path/to/input.root", "/path/to/output.root")
```

### 3. 快速测试
```cpp
root -l
.L test_batch_analysis.C
quick_test()  // 运行批处理并测试结果
```

### 4. 详细性能分析
```cpp
root -l
.L analyze_reco_performance.C
analyze_reconstruction_performance("reco_output.root")
```

## 环境要求

### 环境变量
确保设置了 `SMSIMDIR` 环境变量：
```bash
export SMSIMDIR=/path/to/smsimulator5.5
```

### 依赖库
需要加载以下库：
- `libPDCAnalysisTools.so` (包含所有重建类)

### 输入文件格式
输入文件应包含：
- `TBeamSimData` 分支：原始粒子数据
- 探测器击中数据分支

## 输出文件内容

### 重建事件树 (`recoTree`)
每个事件包含：

#### `RecoEvent` 分支:
```cpp
struct RecoEvent {
    Long64_t eventID;                     // 事件ID
    std::vector<RecoHit> rawHits;         // PDC原始击中
    std::vector<RecoHit> smearedHits;     // PDC模糊击中
    std::vector<RecoTrack> tracks;        // PDC重建轨迹
    std::vector<RecoNeutron> neutrons;    // NEBULA重建中子
}
```

#### `OriginalParticleData` 分支:
```cpp
struct OriginalParticleData {
    bool hasProton;                       // 是否有质子
    TLorentzVector protonMomentum;        // 质子四动量
    TVector3 protonPosition;              // 质子位置
    
    bool hasNeutron;                      // 是否有中子
    TLorentzVector neutronMomentum;       // 中子四动量
    TVector3 neutronPosition;             // 中子位置
}
```

### 统计信息
文件还包含处理统计信息：
- `InputFile`: 输入文件路径
- `TotalEvents`: 总事件数
- `ProcessedEvents`: 处理的事件数
- `PDCHitEvents`: PDC击中事件数
- `NEBULAHitEvents`: NEBULA击中事件数
- `OriginalDataEvents`: 包含原始粒子数据的事件数

## 分析示例

### 获取重建中子动量
```cpp
for (const auto& neutron : recoEvent->neutrons) {
    TVector3 momentum = neutron.GetMomentum();  // MeV/c
    double energy = neutron.energy;             // MeV
    double beta = neutron.beta;
    double tof = neutron.timeOfFlight;          // ns
}
```

### 获取原始粒子动量
```cpp
if (originalData->hasProton) {
    TLorentzVector p_proton = originalData->protonMomentum;
    // p_proton.Px(), p_proton.Py(), p_proton.Pz(), p_proton.E()
}

if (originalData->hasNeutron) {
    TLorentzVector p_neutron = originalData->neutronMomentum;
    // p_neutron.Px(), p_neutron.Py(), p_neutron.Pz(), p_neutron.E()
}
```

### 计算重建分辨率
```cpp
if (originalData->hasNeutron && !recoEvent->neutrons.empty()) {
    double orig_p = originalData->neutronMomentum.Vect().Mag();
    double reco_p = recoEvent->neutrons[0].GetMomentum().Mag();
    double resolution = (reco_p - orig_p) / orig_p;
}
```

## 注意事项

1. **内存管理**: 处理大文件时注意内存使用
2. **文件路径**: 确保输入文件路径正确
3. **几何配置**: 确保几何配置文件存在并正确
4. **库依赖**: 确保所有必需的库都已正确编译和加载

## 故障排除

### 常见问题:

1. **库加载失败**
   ```
   错误: 无法加载 libPDCAnalysisTools.so 库
   ```
   解决: 检查库是否编译成功，路径是否正确

2. **几何文件未找到**
   ```
   错误: 无法加载几何配置文件
   ```
   解决: 检查 `SMSIMDIR` 环境变量和几何文件路径

3. **输入文件格式错误**
   ```
   错误: 无法打开输入文件
   ```
   解决: 检查文件路径和文件格式

4. **内存不足**
   - 处理小批量文件
   - 增加系统内存
   - 优化代码减少内存使用

## 输出图表说明

### `analyze_reco_performance.C` 生成的图表:

1. **`original_momentum_distributions.png`**
   - 原始质子和中子的动量分布
   - 用于验证输入数据的物理合理性

2. **`reconstruction_distributions.png`**
   - 重建结果的统计分布
   - PDC轨迹数、中子能量、β值等

3. **`momentum_comparison.png`**
   - 原始vs重建动量的二维对比图
   - 重建分辨率分布直方图

这些工具为后续的物理分析提供了完整的数据基础。