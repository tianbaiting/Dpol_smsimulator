# SMSimulator 5.5 运行指南

## 快速开始

### 1. 环境配置
```bash
cd /home/tian/workspace/dpol/smsimulator5.5_new
source setup.sh
```

### 2. 运行模拟

#### 方式一：使用 Pencil 束流（简单测试，不需要输入文件）
```bash
./bin/sim_deuteron configs/simulation/macros/test_pencil.mac
```

#### 方式二：使用 Tree 束流（从ROOT文件读取粒子）
需要先准备输入ROOT文件到 `data/input/` 目录，然后修改配置文件中的路径。

## 可用的配置文件

### 测试配置
- `configs/simulation/macros/test_pencil.mac` - Pencil束流测试（10个事件）
- `configs/simulation/macros/test_vis.mac` - 可视化测试（需要显示器）

### 几何配置
- `configs/simulation/geometry/5deg_1.2T.mac` - 5度，1.2T磁场
- `configs/simulation/geometry/10deg_1.2T.mac` - 10度，1.2T磁场
- `configs/simulation/geometry/geom_deuteron.mac` - 标准氘核几何

## 束流类型

程序支持以下束流类型（通过 `/action/gun/Type` 设置）：

1. **Pencil** - 平行束流
   ```
   /action/gun/Type Pencil
   /action/gun/Position 0 0 -200 cm
   /action/gun/SetBeamParticleName deuteron
   /action/gun/Energy 190 MeV
   /run/beamOn 10
   ```

2. **Gaus** - 高斯分布束流
   ```
   /action/gun/Type Gaus
   /action/gun/AngleX 0 deg
   /action/gun/AngleY 0 deg
   /action/gun/PositionXSigma 1 cm
   /action/gun/PositionYSigma 1 cm
   /action/gun/SetBeamParticleName proton
   /action/gun/Energy 190 MeV
   ```

3. **Isotropic** - 各向同性发射

4. **Tree** - 从ROOT文件读取
   ```
   /action/gun/Type Tree
   /action/gun/tree/InputFileName data/input/your_file.root
   /action/gun/tree/TreeName tree
   /action/gun/tree/beamOn 100
   ```

5. **Geantino** - 用于截面计算

## 输出文件

模拟结果保存在以下目录结构中：

```
data/
├── simulation/
│   ├── output_tree/        # 模拟输出的ROOT文件
│   ├── rootfiles/          # 输入用的ROOT文件
│   └── temp_dat_files/     # 临时数据文件
├── reconstruction/         # 重建结果
├── input/                  # 原始输入数据
├── magnetic_field/         # 磁场数据
└── calibration/            # 刻度参数
```

模拟输出保存在：`data/simulation/output_tree/`

重建结果保存在：`data/reconstruction/`

输出ROOT文件包含：
- 粒子轨迹信息
- 探测器响应
- 能量沉积
- PDC击中信息

## 常用命令

### 查看ROOT文件内容
```bash
root -l results/simulation/test_pencil_run0000.root
root [0] tree->Print()
root [1] tree->Draw("pdcID")
```

### 重新编译
```bash
./build.sh
```

### 清理编译文件
```bash
./clean.sh
```

## 目录结构说明

```
smsimulator5.5_new/
├── bin/                    # 可执行文件
│   └── sim_deuteron       # 主程序
├── libs/                   # 源代码（库）
│   ├── smg4lib/           # Geant4核心库
│   ├── sim_deuteron_core/ # 氘核模拟核心
│   └── analysis/          # 分析工具
├── apps/                   # 应用程序源码
├── configs/                # 配置文件
│   ├── simulation/        # 模拟配置
│   │   ├── macros/       # 宏文件(.mac)
│   │   └── geometry/     # 几何配置
│   └── reconstruction/    # 重建配置
├── data/                   # 数据文件
│   ├── input/             # 输入数据
│   ├── magnetic_field/    # 磁场图
│   └── calibration/       # 刻度参数
├── results/                # 结果输出
│   ├── simulation/        # 模拟结果
│   ├── reconstruction/    # 重建结果
│   └── plots/             # 图表
└── scripts/                # 辅助脚本
```

## 故障排除

### 问题：找不到磁场文件
```
fail to get magnetic field data from: .../filed_map/180626-1,2T-3000.bin
```
**解决**：检查 `d_work/geometry/filed_map/` 目录是否存在磁场数据文件。

### 问题：找不到NEBULA配置文件
```
File open error "geometry/NEBULA_Dayone.csv"
```
**解决**：这是警告信息，如果不使用NEBULA探测器可以忽略。

### 问题：可视化无法打开
```
/vis/open OGL 失败
```
**解决**：确保系统支持OpenGL，或使用无头模式（不加 /vis/open 命令）。

## 高级用法

### 批处理模式
创建批处理配置文件放在 `configs/batch/` 目录：
```bash
./bin/sim_deuteron configs/batch/run_100events.mac
```

### 使用环境变量
配置文件中可以使用 `$SMSIMDIR` 环境变量（由setup.sh设置）：
```
/action/gun/tree/InputFileName $SMSIMDIR/data/input/mydata.root
```

## 参考资料

- Geant4 用户指南: http://geant4.org/
- ROOT 文档: https://root.cern/
- 项目文档: `docs/` 目录
