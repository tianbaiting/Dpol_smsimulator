#!/bin/bash

##############################################################################
# 项目重构后的使用指南生成脚本
##############################################################################

cat > MIGRATION_GUIDE.md << 'EOF'
# SMSimulator 项目重构迁移指南

## 概述

本指南帮助你完成从旧项目结构到新项目结构的迁移。

## 迁移步骤

### 1. 运行迁移脚本

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
chmod +x migrate_to_new_structure.sh
./migrate_to_new_structure.sh
```

这将创建一个新的目录 `smsimulator5.5_new/` 包含重组后的项目。

### 2. 复制 CMake 文件到新项目

迁移脚本运行完成后，需要将生成的 CMakeLists.txt 文件复制到对应位置：

```bash
# 进入新项目目录
cd smsimulator5.5_new

# 复制主 CMakeLists.txt
cp ../cmake_new/libs_CMakeLists.txt libs/CMakeLists.txt

# 复制 libs/ 下的 CMakeLists.txt
cp ../cmake_new/libs_smg4lib_CMakeLists.txt libs/smg4lib/CMakeLists.txt
cp ../cmake_new/libs_smg4lib_data_CMakeLists.txt libs/smg4lib/src/data/CMakeLists.txt
cp ../cmake_new/libs_smg4lib_action_CMakeLists.txt libs/smg4lib/src/action/CMakeLists.txt
cp ../cmake_new/libs_smg4lib_construction_CMakeLists.txt libs/smg4lib/src/construction/CMakeLists.txt
cp ../cmake_new/libs_smg4lib_physics_CMakeLists.txt libs/smg4lib/src/physics/CMakeLists.txt
cp ../cmake_new/libs_sim_deuteron_core_CMakeLists.txt libs/sim_deuteron_core/CMakeLists.txt
cp ../cmake_new/libs_analysis_CMakeLists.txt libs/analysis/CMakeLists.txt

# 复制 apps/ 下的 CMakeLists.txt
cp ../cmake_new/apps_CMakeLists.txt apps/CMakeLists.txt
cp ../cmake_new/apps_sim_deuteron_CMakeLists.txt apps/sim_deuteron/CMakeLists.txt
cp ../cmake_new/apps_run_reconstruction_CMakeLists.txt apps/run_reconstruction/CMakeLists.txt
cp ../cmake_new/apps_tools_CMakeLists.txt apps/tools/CMakeLists.txt

# 复制 tests/ 下的 CMakeLists.txt
cp ../cmake_new/tests_CMakeLists.txt tests/CMakeLists.txt
cp ../cmake_new/tests_unit_CMakeLists.txt tests/unit/CMakeLists.txt
cp ../cmake_new/tests_integration_CMakeLists.txt tests/integration/CMakeLists.txt

# 复制 cmake/ 模块
mkdir -p cmake
cp ../cmake_new/FindANAROOT.cmake cmake/
cp ../cmake_new/FindXercesC.cmake cmake/

# 复制主 CMakeLists.txt
cp ../CMakeLists_NEW.txt CMakeLists.txt
```

### 3. 调整头文件包含路径

由于项目结构变化，需要检查和更新源码中的 `#include` 路径。

**迁移前：**
```cpp
#include "SimData.hh"
#include "../../smg4lib/data/include/SimData.hh"
```

**迁移后：**
```cpp
#include "SimData.hh"  // CMake 已经配置好包含路径
```

大部分情况下 CMake 已经正确配置，但如果遇到编译错误，请检查包含路径。

### 4. 设置环境并编译

```bash
# 确保环境变量正确设置
source setup.sh

# 创建构建目录
mkdir build && cd build

# 配置（可以关闭不需要的选项）
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=ON \
    -DBUILD_ANALYSIS=ON \
    -DWITH_ANAROOT=ON

# 编译
make -j$(nproc)

# 可选：运行测试
ctest --output-on-failure
```

### 5. 安装（可选）

```bash
# 安装到系统目录
sudo make install

# 或安装到自定义目录
cmake .. -DCMAKE_INSTALL_PREFIX=/path/to/install
make install
```

## 新项目结构说明

```
smsimulator5.5_new/
├── CMakeLists.txt          # 主构建文件
├── cmake/                  # CMake 查找模块
│   ├── FindANAROOT.cmake
│   └── FindXercesC.cmake
├── libs/                   # 所有库
│   ├── smg4lib/           # Geant4 基础库
│   ├── sim_deuteron_core/ # 氘核模拟核心
│   └── analysis/          # 分析重建库
├── apps/                   # 可执行程序
│   ├── sim_deuteron/      # 主模拟程序
│   ├── run_reconstruction/# 重建程序
│   └── tools/             # 小工具
├── configs/                # 配置文件
│   ├── simulation/        # 模拟配置
│   └── reconstruction/    # 重建配置
├── data/                   # 数据目录
│   ├── input/             # 输入数据
│   ├── magnetic_field/    # 磁场数据
│   ├── simulation/        # 模拟输出
│   └── reconstruction/    # 重建输出
├── scripts/                # Python 脚本
├── tests/                  # 测试代码
└── docs/                   # 文档
```

## 运行程序

### 模拟程序

```bash
# 从构建目录
cd build/bin
./sim_deuteron ../../configs/simulation/macros/simulation.mac

# 或从项目根目录
./bin/sim_deuteron configs/simulation/macros/simulation.mac
```

### 分析程序

```bash
# 使用 ROOT
cd build/bin
root -l
.L ../../libs/analysis/src/PDCSimAna.cc+
// 运行你的分析宏
```

### Python 脚本

```bash
# 安装依赖
pip install -r scripts/requirements.txt

# 运行批处理
cd scripts/batch
python batch_run_ypol.py
```

## CMake 选项

编译时可以使用以下选项：

- `BUILD_TESTS`：构建测试（默认：ON）
- `BUILD_APPS`：构建应用程序（默认：ON）
- `BUILD_ANALYSIS`：构建分析库（默认：ON）
- `WITH_ANAROOT`：启用 ANAROOT 支持（默认：ON）
- `WITH_GEANT4_UIVIS`：启用 Geant4 UI/Vis（默认：ON）

示例：
```bash
cmake .. -DBUILD_TESTS=OFF -DWITH_ANAROOT=OFF
```

## 常见问题

### 1. 找不到 ANAROOT

确保设置了 `TARTSYS` 环境变量：
```bash
export TARTSYS=/path/to/anaroot
```

或在 CMake 配置时禁用：
```bash
cmake .. -DWITH_ANAROOT=OFF
```

### 2. 找不到 Geant4

确保 Geant4 已正确安装并设置环境：
```bash
source /path/to/geant4-install/bin/geant4.sh
```

### 3. 编译错误：找不到头文件

检查 CMakeLists.txt 中的 `target_include_directories` 是否正确配置。

### 4. 链接错误

确保库的依赖顺序正确。CMake 会自动处理大部分依赖。

## 回滚到旧版本

如果新结构有问题，旧项目仍然保留在原位置：

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
# 继续使用旧的构建方式
```

## 下一步

1. 阅读 `docs/` 中的文档
2. 运行示例模拟验证配置
3. 根据需要调整配置文件
4. 开始开发新功能

## 反馈

如有问题，请检查：
1. 编译输出日志
2. CMake 配置摘要
3. 环境变量设置

EOF

cat > QUICK_START.md << 'EOF'
# 快速开始指南

## 一键迁移和构建

```bash
# 1. 运行迁移
./migrate_to_new_structure.sh

# 2. 进入新项目
cd smsimulator5.5_new

# 3. 复制 CMake 文件（一次性脚本）
cat > copy_cmake.sh << 'SCRIPT'
#!/bin/bash
cd ..
cp cmake_new/libs_CMakeLists.txt smsimulator5.5_new/libs/CMakeLists.txt
cp cmake_new/libs_smg4lib_CMakeLists.txt smsimulator5.5_new/libs/smg4lib/CMakeLists.txt
cp cmake_new/libs_smg4lib_data_CMakeLists.txt smsimulator5.5_new/libs/smg4lib/src/data/CMakeLists.txt
cp cmake_new/libs_smg4lib_action_CMakeLists.txt smsimulator5.5_new/libs/smg4lib/src/action/CMakeLists.txt
cp cmake_new/libs_smg4lib_construction_CMakeLists.txt smsimulator5.5_new/libs/smg4lib/src/construction/CMakeLists.txt
cp cmake_new/libs_smg4lib_physics_CMakeLists.txt smsimulator5.5_new/libs/smg4lib/src/physics/CMakeLists.txt
cp cmake_new/libs_sim_deuteron_core_CMakeLists.txt smsimulator5.5_new/libs/sim_deuteron_core/CMakeLists.txt
cp cmake_new/libs_analysis_CMakeLists.txt smsimulator5.5_new/libs/analysis/CMakeLists.txt
cp cmake_new/apps_CMakeLists.txt smsimulator5.5_new/apps/CMakeLists.txt
cp cmake_new/apps_sim_deuteron_CMakeLists.txt smsimulator5.5_new/apps/sim_deuteron/CMakeLists.txt
cp cmake_new/apps_run_reconstruction_CMakeLists.txt smsimulator5.5_new/apps/run_reconstruction/CMakeLists.txt
cp cmake_new/apps_tools_CMakeLists.txt smsimulator5.5_new/apps/tools/CMakeLists.txt
cp cmake_new/tests_CMakeLists.txt smsimulator5.5_new/tests/CMakeLists.txt
cp cmake_new/tests_unit_CMakeLists.txt smsimulator5.5_new/tests/unit/CMakeLists.txt
cp cmake_new/tests_integration_CMakeLists.txt smsimulator5.5_new/tests/integration/CMakeLists.txt
mkdir -p smsimulator5.5_new/cmake
cp cmake_new/FindANAROOT.cmake smsimulator5.5_new/cmake/
cp cmake_new/FindXercesC.cmake smsimulator5.5_new/cmake/
cp CMakeLists_NEW.txt smsimulator5.5_new/CMakeLists.txt
SCRIPT
chmod +x copy_cmake.sh
./copy_cmake.sh

# 4. 快速构建
cd smsimulator5.5_new
./quick_build.sh
```

## 验证安装

```bash
# 检查可执行文件
ls -l bin/sim_deuteron

# 运行帮助
./bin/sim_deuteron --help

# 运行简单测试
cd build
ctest
```

完成！
EOF

echo "指南文档已生成："
echo "  - MIGRATION_GUIDE.md (详细迁移指南)"
echo "  - QUICK_START.md (快速开始)"
