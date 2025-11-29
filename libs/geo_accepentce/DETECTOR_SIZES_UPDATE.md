# 探测器尺寸修正说明

## 修正内容

已根据实际的Geant4几何配置更新了几何接受度分析库中的探测器尺寸。

## PDC尺寸（已修正）

### 数据来源
- `libs/smg4lib/src/construction/src/PDCConstruction.cc`
- `detector_geometry.gdml`

### 实际尺寸
```cpp
// PDC外壳 (enclosure)
X: 1700 mm (全尺寸)
Y: 800 mm (全尺寸)
Z: 190 mm (全尺寸)

// PDC有效探测区域 (active area)
X: 1680 mm (全尺寸, 2×840 mm)
Y: 780 mm (全尺寸, 2×390 mm)

// 各层厚度
U层: 8 mm
X层: 16 mm
V层: 16 mm
```

### 代码中的更新
- `DetectorAcceptanceCalculator.hh`: PDCConfiguration结构体
  - width = 1680 mm (有效宽度)
  - height = 780 mm (有效高度)
  - depth = 190 mm (总厚度)

## NEBULA尺寸（已修正）

### 数据来源
- `libs/smg4lib/src/data/src/TNEBULASimParameter.cc`
- `configs/simulation/geometry/NEBULA_Detectors_Dayone.csv`

### 单个探测器尺寸
```cpp
// 从 TNEBULASimParameter.cc
Neut探测器: 12 × 180 × 12 cm = 120 × 1800 × 120 mm
Veto探测器: 32 × 1 × 190 cm
```

### NEBULA阵列总体尺寸
```cpp
// 从配置文件分析
X方向宽度: ~3600 mm (从-1647.8到+1901.8 mm)
Y方向高度: ~1800 mm (单层高度)
Z方向深度: ~600 mm (多层结构估计)
```

### 代码中的更新
- `DetectorAcceptanceCalculator.hh`: NEBULAConfiguration结构体
  - width = 3600 mm (X方向)
  - height = 1800 mm (Y方向)
  - depth = 600 mm (Z方向)

## 修改的文件清单

### 头文件
1. `/home/tian/workspace/dpol/smsimulator5.5/libs/geo_accepentce/include/DetectorAcceptanceCalculator.hh`
   - 更新了PDCConfiguration默认值
   - 更新了NEBULAConfiguration默认值
   - 添加了详细的尺寸注释

### 源文件
2. `/home/tian/workspace/dpol/smsimulator5.5/libs/geo_accepentce/src/DetectorAcceptanceCalculator.cc`
   - 更新了构造函数中的NEBULA配置
   - 更新了CalculateOptimalPDCPosition中的PDC尺寸
   - 添加了尺寸来源说明

### 文档
3. `/home/tian/workspace/dpol/smsimulator5.5/libs/geo_accepentce/readme.md`
   - 更新了PDC和NEBULA的尺寸描述
   - 添加了数据来源引用

## 验证方法

### 1. PDC尺寸验证
```bash
# 查看GDML文件
grep -A5 "PDCenc_box" detector_geometry.gdml
# 输出: x="1700" y="800" z="190"

grep -A5 "U_box\|X_box\|V_box" detector_geometry.gdml
# 输出: x="1680" y="780" z="8/16/16"
```

### 2. NEBULA尺寸验证
```bash
# 查看TNEBULASimParameter.cc中的默认值
grep "fNeutSize\|fVetoSize" libs/smg4lib/src/data/src/TNEBULASimParameter.cc

# 查看NEBULA配置文件
head -50 configs/simulation/geometry/NEBULA_Detectors_Dayone.csv
```

## 影响分析

### 接受度计算
- PDC接受度计算将更准确，使用实际的1680×780 mm有效面积
- NEBULA接受度计算考虑了更宽的X方向覆盖范围(3600 mm)

### 最佳位置计算
- PDC最佳位置优化时使用正确的物理尺寸
- 避免了之前1000×1000 mm估计值带来的误差

## 注意事项

1. **NEBULA位置**: 代码中默认NEBULA位置为(0, 0, 5000) mm，这需要根据实验实际配置确认

2. **NEBULA深度**: 600 mm的深度是估计值，基于多层结构的典型配置

3. **坐标系**: 所有尺寸都基于Geant4的全局坐标系
   - X: 横向
   - Y: 竖直
   - Z: 束流方向

## 后续工作

- [ ] 验证NEBULA的实际Z位置
- [ ] 确认NEBULA的准确深度（多层结构）
- [ ] 从实验数据验证接受度计算结果

## 参考文件

- PDC几何: `libs/smg4lib/src/construction/src/PDCConstruction.cc`
- NEBULA几何: `libs/smg4lib/src/construction/src/NEBULAConstruction.cc`
- NEBULA参数: `libs/smg4lib/src/data/src/TNEBULASimParameter.cc`
- GDML导出: `detector_geometry.gdml`
- NEBULA配置: `configs/simulation/geometry/NEBULA_Detectors_Dayone.csv`

---
更新日期: 2025-11-30
更新者: GitHub Copilot
