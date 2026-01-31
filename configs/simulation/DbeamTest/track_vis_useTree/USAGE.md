# Proton Trajectory Visualization (Tree Input) / 质子轨迹可视化

## 工作原理

使用 **单个ROOT树文件** + **Target参数** 控制粒子入射：

```
DeutPrimaryGeneratorAction.cc:
if (fUseTargetParameters) {
    pos_lab.set(fTargetPosition);      // 使用 Target/Position
    dir.rotateY(-angle_tgt);           // 按 Target/Angle 旋转动量
}
```

## 快速开始

### Step 1: 设置
```bash
cd $SMSIMDIR/configs/simulation/DbeamTest/track_vis_useTree
./setup.sh
```

或手动：
```bash
# 生成单个ROOT树（4个质子）
root -l -b -q 'GenProtonTree_Single.C("rootfiles/protons_4tracks.root")'

# 生成25个宏文件
./generate_all_macros_v2.sh
```

### Step 2: 运行

**交互式运行:**
```bash
$SMSIMDIR/build/bin/sim_deuteron
# 在Geant4界面中:
/control/execute run_protons_B100T_6.0deg.mac
# 导出:
/vis/ogl/export protons_B100T_6.0deg.png
```

**批量运行:**
```bash
./run_batch_v2.sh                                # 全部25个 (VRML)
./run_batch_v2.sh --fields "100" --angles "6.0"  # 指定配置
./run_batch_v2.sh --format png                   # PNG格式 (需要X显示)
```

## 目录结构

```
track_vis_useTree/
├── setup.sh                 # 一键设置
├── GenProtonTree_Single.C   # 生成单个ROOT树
├── generate_all_macros_v2.sh # 生成Geant4宏
├── run_batch_v2.sh          # 批量运行
├── rootfiles/
│   └── protons_4tracks.root # 单个ROOT树（4个质子）
├── run_protons_B*T_*deg.mac # 25个Geant4宏文件
└── output/                  # 输出目录
```

## 质子参数

| Px (MeV/c) | Py | Pz (MeV/c) | |P| (MeV/c) | T (MeV) |
|------------|-----|------------|------------|---------|
| +100 | 0 | 627 | 634.9 | 195 |
| -100 | 0 | 627 | 634.9 | 195 |
| +150 | 0 | 627 | 644.7 | 204 |
| -150 | 0 | 627 | 644.7 | 204 |

## 关键Geant4命令

```g4macro
# 设置靶点位置（粒子起始位置）
/samurai/geometry/Target/Position X Y Z cm

# 设置束流偏转角度（动量旋转）
/samurai/geometry/Target/Angle ANGLE deg

# 加载ROOT树
/action/gun/tree/InputFileName protons_4tracks.root
/action/gun/tree/TreeName tree
/action/gun/tree/beamOn 0   # 0=运行所有entries
```

## PDC固定位置

```
/samurai/geometry/PDC/Angle 65 deg
/samurai/geometry/PDC/Position1 0 0 400 cm
/samurai/geometry/PDC/Position2 0 0 500 cm
```
