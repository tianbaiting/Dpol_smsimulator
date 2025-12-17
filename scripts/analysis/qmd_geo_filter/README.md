# QMD Geometry Filter Module

这个模块用于对 QMD 数据应用动量 cut 和几何接受度筛选。
（注意：这只是几何接受度筛选，没有包含重建过程）

## 目录结构

```
scripts/analysis/qmd_geo_filter/
├── run_qmd_geo_filter.py    # Python 主脚本
├── run_analysis.sh          # Shell 运行脚本
├── full_analysis_config.json  # 完整分析配置
└── README.md               # 本文件

libs/qmd_geo_filter/
├── __init__.py             # 模块初始化
├── qmd_data_loader.py      # QMD 数据加载器
├── momentum_cut.py         # 动量 cut 库
├── geometry_filter.py      # 几何接受度筛选
├── plot_utils.py           # 绑定工具
└── analysis_runner.py      # 分析运行器
```

## 快速开始

### 1. 设置环境变量

```bash
export SMSIMDIR=/home/tian/workspace/dpol/smsimulator5.5
export PYTHONPATH=$SMSIMDIR/libs:$PYTHONPATH
```

### 2. 运行快速测试

```bash
cd $SMSIMDIR/scripts/analysis/qmd_geo_filter
./run_analysis.sh --test
```

### 3. 查看可用数据

```bash
./run_analysis.sh --list-data
```

### 4. 使用默认配置运行

```bash
./run_analysis.sh
```

### 5. 使用自定义配置

```bash
# 生成配置模板
./run_analysis.sh --generate-config my_config.json

# 编辑配置文件
vim my_config.json

# 运行分析
./run_analysis.sh --config my_config.json
```

## 配置参数说明

| 参数 | 说明 | 示例值 |
|------|------|--------|
| `field_strengths` | 磁场强度列表 [T] | `[0.8, 1.0, 1.2, 1.4]` |
| `deflection_angles` | 束流偏转角度列表 [度] | `[0.0, 5.0, 10.0]` |
| `targets` | 靶材料列表 | `["Pb208", "Sn124"]` |
| `pol_types` | 极化类型 | `["zpol", "ypol"]` |
| `gamma_values` | γ 参数列表 | `["050", "060", "070"]` |
| `b_range` | 碰撞参数范围 [fm] | `[5.0, 10.0]` |
| `momentum_resolutions` | 动量分辨率列表 | `[0.0, 0.02, 0.05]` |

### 动量 Cut 参数

**注意**: zpol 和 ypol 使用不同的 cut 条件！

#### zpol Cut 参数 (所有条件同时满足)

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `pz_sum_threshold` | (pzp + pzn) 阈值 [MeV/c] | 1150.0 |
| `phi_threshold` | \|π - \|φ\|\| 阈值 [rad] | 0.5 |
| `delta_pz_threshold` | \|pzp - pzn\| 阈值 [MeV/c] | 150.0 |
| `px_sum_threshold` | (pxp + pxn) 阈值 [MeV/c] | 200.0 |
| `pt_sum_sq_threshold` | (pxp+pxn)² + (pyp+pyn)² 阈值 | 2500.0 |

#### ypol Cut 参数 (分两步)

第一步:
| 参数 | 说明 | 默认值 |
|------|------|--------|
| `delta_py_threshold` | \|pyp - pyn\| 阈值 [MeV/c] | 150.0 |
| `pt_sum_sq_threshold` | (pxp+pxn)² + (pyp+pyn)² 阈值 | 2500.0 |

第二步 (旋转后):
| 参数 | 说明 | 默认值 |
|------|------|--------|
| `px_sum_after_rotation` | 旋转后 (pxp + pxn) 阈值 [MeV/c] | 200.0 |
| `phi_threshold` | \|π - \|φ\|\| 阈值 [rad] | 0.2 |

## 输出目录结构

```
results/qmd_geo_filter/
├── [磁场]T/
│   ├── [角度]deg/
│   │   ├── ratio_comparison.pdf    # 不同靶子的 ratio 对比图
│   │   ├── ratio_comparison.html   # 交互式版本
│   │   ├── [target]/
│   │   │   ├── [pol_type]/
│   │   │   │   ├── config.txt           # 配置信息
│   │   │   │   ├── geometry_config.json # 几何配置
│   │   │   │   ├── gamma_050/
│   │   │   │   │   ├── 3d_momentum_before_cuts.pdf
│   │   │   │   │   ├── 3d_momentum_before_cuts.html
│   │   │   │   │   ├── 3d_momentum_after_cuts.pdf/html
│   │   │   │   │   ├── 3d_momentum_after_geometry.pdf/html
│   │   │   │   │   ├── pxp_pxn_distribution_before_cut.pdf/html
│   │   │   │   │   ├── pxp_pxn_distribution_after_cut.pdf/html
│   │   │   │   │   └── pxp_pxn_distribution_after_geometry.pdf/html
│   │   │   │   │   ├── 3d_momentum_after_cuts.pdf
│   │   │   │   │   ├── 3d_momentum_after_cuts.html
│   │   │   │   │   ├── 3d_momentum_after_geometry.pdf
│   │   │   │   │   ├── 3d_momentum_after_geometry.html
│   │   │   │   │   └── pxp_pxn_distribution.pdf/html
│   │   │   │   ├── gamma_060/
│   │   │   │   │   └── ...
```

## API 使用示例

### Python 代码中使用

```python
import sys
sys.path.insert(0, '/home/tian/workspace/dpol/smsimulator5.5/libs')

from qmd_geo_filter import (
    QMDDataLoader, MomentumCut, GeometryFilter, PlotUtils, AnalysisRunner
)
from qmd_geo_filter.analysis_runner import AnalysisConfig

# 1. 加载数据
loader = QMDDataLoader()
dataset = loader.load_zpol_data("Pb208", "050", "znp", b_range=(5, 10))

# 2. 应用动量 cut
cut = MomentumCut(delta_py_threshold=150.0)
pxp, pyp, pzp, pxn, pyn, pzn = dataset.get_momentum_arrays()
result = cut.apply_cut(pxp, pyp, pzp, pxn, pyn, pzn)
print(f"Cut efficiency: {result.efficiency:.2%}")

# 3. 应用几何筛选
geo_filter = GeometryFilter(field_strength=1.0, deflection_angle=5.0)
geo_result = geo_filter.apply_geometry_filter(
    result.pxp_rotated[result.mask], result.pyp_rotated[result.mask], 
    result.pzp_rotated[result.mask], result.pxn_rotated[result.mask],
    result.pyn_rotated[result.mask], result.pzn_rotated[result.mask]
)

# 4. 计算 ratio
plotter = PlotUtils()
ratio_info = plotter.calculate_ratio(
    result.pxp_rotated[result.mask], 
    result.pxn_rotated[result.mask]
)
print(f"Ratio: {ratio_info['ratio']:.4f} ± {ratio_info['error']:.4f}")

# 5. 运行完整分析
config = AnalysisConfig(
    targets=["Pb208"],
    gamma_values=["050", "060"]
)
runner = AnalysisRunner(config)
results = runner.run_full_analysis()
```

### 动量模糊测试

```python
# 测试不同分辨率下的影响
resolutions = [0.0, 0.02, 0.05, 0.10]

for res in resolutions:
    cut = MomentumCut(enable_momentum_smearing=True, momentum_resolution=res)
    result = cut.apply_cut(pxp, pyp, pzp, pxn, pyn, pzn)
    ratio = plotter.calculate_ratio(
        result.pxp_rotated[result.mask],
        result.pxn_rotated[result.mask]
    )
    print(f"Resolution {res*100:.0f}%: Ratio = {ratio['ratio']:.4f}")
```

## 依赖项

- Python 3.8+
- numpy
- matplotlib
- plotly (可选，用于交互式 HTML 图)

安装依赖:
```bash
pip install numpy matplotlib plotly
```

## 注意事项

1. **数据格式差异**: z_pol 和 y_pol 数据格式略有不同:
   - z_pol: 按 b 值分文件 (dbreakb01.dat, dbreakb02.dat, ...)
   - y_pol: 单个文件包含 b 和 rpphi 列

2. **旋转坐标系**: 动量 cut 中会将动量旋转到反应平面坐标系

3. **几何接受度**: 简化实现基于 Px 范围和角度范围，更精确的计算需要调用 C++ 后端

4. **仅几何筛选**: 本模块只包含几何接受度筛选，不包含探测器响应重建过程

## 联系方式

如有问题，请联系项目维护者。
