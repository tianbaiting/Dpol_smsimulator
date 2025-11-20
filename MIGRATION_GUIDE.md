# 项目结构重构迁移指南

## 快速开始

```bash
# 1. 运行迁移脚本
./migrate_to_new_structure.sh

# 2. 进入新项目目录
cd smsimulator5.5_new

# 3. 检查文件是否正确迁移
ls -la libs/
ls -la data/

# 4. 编译项目
./quick_build.sh
```

## 重要说明

### 文件迁移策略

1. **源代码和配置文件**: 使用 `cp` 复制
   - `sources/` → `libs/`
   - `d_work/macros/` → `configs/`
   - 脚本文件等

2. **数据文件**: 使用 `mv` 移动（节省磁盘空间）
   - `*.root` 文件
   - `*.dat` 文件
   - `QMDdata/`
   - `d_work/rootfiles/`
   - `d_work/output_tree/`
   - `d_work/plots/`

⚠️ **警告**: 数据文件将从原项目移走，不会复制！

### 迁移后的目录结构

```
smsimulator5.5_new/
├── libs/                    # C++ 库
│   ├── smg4lib/            # Geant4 核心库
│   ├── sim_deuteron_core/  # 氘核模拟
│   └── analysis/           # 数据分析
├── apps/                    # 可执行程序
│   ├── sim_deuteron/       # 主模拟程序
│   ├── run_reconstruction/ # 重建程序
│   └── tools/              # 工具程序
├── configs/                 # 配置文件
│   ├── simulation/         # G4 宏和几何
│   ├── reconstruction/     # 重建参数
│   └── batch/              # 批处理配置
├── data/                    # 数据文件（已移动）
│   ├── input/              # QMD 输入
│   ├── magnetic_field/     # 磁场数据
│   ├── simulation/         # 模拟输出
│   └── reconstruction/     # 重建结果
├── scripts/                 # Python 脚本
└── docs/                    # 文档
```

## 编译说明

### 方法 1: 使用快速编译脚本（推荐）

```bash
./quick_build.sh
```

### 方法 2: 手动编译

```bash
source setup.sh
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 方法 3: 使用新的 CMake 文件

如果要使用新生成的 CMake 文件：

```bash
# 将 cmake_new/ 目录中的 CMakeLists.txt 复制到对应位置
cp cmake_new/CMakeLists.txt ./
cp cmake_new/libs/smg4lib/CMakeLists.txt libs/smg4lib/
# ... 复制其他 CMakeLists.txt

# 然后编译
./quick_build.sh
```

## 验证迁移

### 检查源代码

```bash
# 检查库文件
ls -la libs/smg4lib/src/
ls -la libs/sim_deuteron_core/src/
ls -la libs/analysis/src/

# 检查头文件
ls -la libs/smg4lib/include/
```

### 检查数据文件

```bash
# 检查数据是否已移动
ls -la data/magnetic_field/
ls -la data/input/
ls -la data/simulation/

# 检查原目录（数据文件应该已被移走）
ls -la ../d_work/
```

### 检查配置文件

```bash
ls -la configs/simulation/macros/
ls -la configs/simulation/geometry/
```

## 回滚操作

如果需要回到原始结构：

```bash
# 数据文件需要手动移回
mv smsimulator5.5_new/data/* smsimulator5.5/d_work/

# 删除新项目（源代码在原项目中仍然存在）
rm -rf smsimulator5.5_new
```

## 常见问题

### Q: 磁场数据文件找不到？
A: 检查 `data/magnetic_field/` 目录，文件已从 `d_work/` 移动过来。

### Q: 编译失败，找不到头文件？
A: 确保使用了新的 CMakeLists.txt，并正确设置了 include 路径。

### Q: 原项目的数据文件被删除了？
A: 数据文件被移动（mv）到新项目，不是删除。可以从 `smsimulator5.5_new/data/` 找回。

### Q: 想保留原项目的数据怎么办？
A: 在运行迁移脚本前，先备份数据目录：
```bash
cp -r d_work/rootfiles d_work/rootfiles_backup
cp d_work/magnetic_field_test.root d_work/magnetic_field_test.root.backup
```

## 技术支持

如遇问题，请检查：
1. 迁移脚本的输出日志
2. 新项目的文件完整性
3. CMake 配置是否正确

---

**最后更新**: 2025-11-20
