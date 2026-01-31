# Trajectory Visualization / 轨迹可视化

## Quick Start / 快速开始

### 1. 交互式运行 (推荐用于查看和调试)
```bash
cd /home/tian/workspace/dpol/smsimulator5.5/configs/simulation/DbeamTest/trajectory_viz

# 启动 sim_deuteron
$SMSIMDIR/build/bin/sim_deuteron

# 在 Geant4 界面中加载配置
/control/execute run_trajectory_B100T_6.0deg.mac

# 导出 PNG
/vis/ogl/export trajectory_B100T_6.0deg.png
```

### 2. 批量导出 VRML (无需显示器)
```bash
cd /home/tian/workspace/dpol/smsimulator5.5/configs/simulation/DbeamTest/trajectory_viz

# 运行所有配置
./run_batch_export.sh

# 运行指定配置
./run_batch_export.sh --fields "080,100" --angles "4.0,6.0"

# 查看帮助
./run_batch_export.sh --help
```

### 3. 可用配置

| 磁场强度 | 角度 | 配置文件 |
|---------|------|---------|
| 0.8T | 2°, 4°, 6°, 8°, 10° | `run_trajectory_B080T_*.mac` |
| 1.0T | 2°, 4°, 6°, 8°, 10° | `run_trajectory_B100T_*.mac` |
| 1.2T | 2°, 4°, 6°, 8°, 10° | `run_trajectory_B120T_*.mac` |
| 1.6T | 2°, 4°, 6°, 8°, 10° | `run_trajectory_B160T_*.mac` |
| 2.0T | 2°, 4°, 6°, 8°, 10° | `run_trajectory_B200T_*.mac` |

### 4. 粒子参数

| 粒子 | 动能 | 起始位置 | 方向 |
|------|------|----------|------|
| 氘核 | 380 MeV | (0, 0, -4m) | 沿Z轴 |
| 质子+ | 195 MeV | 靶点位置 | AngleY = +9.07° |
| 质子- | 195 MeV | 靶点位置 | AngleY = -9.07° |

质子动量: p = (  ±100,  0, 627) MeV/c

### 5. PDC 固定位置
```
/samurai/geometry/PDC/Angle 65 deg
/samurai/geometry/PDC/Position1 0 0 400 cm
/samurai/geometry/PDC/Position2 0 0 500 cm
```

### 6. 重新生成配置
```bash
./generate_all_configs.sh
```

### 7. VRML 转 PNG
输出的 VRML 文件可以使用多种工具转换为 PNG：
- Blender (导入 VRML, 渲染导出)
- view3dscene
- meshlab
./run_batch_export.sh --format vrml   # VRML (无显示器)
./run_batch_export.sh --format png    # PNG (需要X显示)
./run_batch_export.sh --format both   # 两种都导出