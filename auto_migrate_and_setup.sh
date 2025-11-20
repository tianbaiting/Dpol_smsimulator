#!/bin/bash

##############################################################################
# 一键完成整个迁移和配置过程
# 使用方法: ./auto_migrate_and_setup.sh
##############################################################################

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "${SCRIPT_DIR}"

echo "═══════════════════════════════════════════════════════════════"
echo "   SMSimulator 项目自动迁移和配置脚本"
echo "═══════════════════════════════════════════════════════════════"
echo ""

##############################################################################
# 步骤 1: 生成指南文档
##############################################################################

log_step "步骤 1/6: 生成迁移指南文档..."
chmod +x generate_guides.sh
./generate_guides.sh

##############################################################################
# 步骤 2: 运行迁移脚本
##############################################################################

log_step "步骤 2/6: 执行项目结构迁移..."
chmod +x migrate_to_new_structure.sh

# 自动确认
echo "y" | ./migrate_to_new_structure.sh

if [ ! -d "smsimulator5.5_new" ]; then
    log_error "迁移失败：新项目目录未创建"
    exit 1
fi

log_info "✓ 项目结构迁移完成"

##############################################################################
# 步骤 3: 复制 CMake 文件
##############################################################################

log_step "步骤 3/6: 复制 CMake 配置文件..."

NEW_DIR="smsimulator5.5_new"

# 创建必要的目录
mkdir -p "${NEW_DIR}/cmake"
mkdir -p "${NEW_DIR}/libs/smg4lib/src/data"
mkdir -p "${NEW_DIR}/libs/smg4lib/src/action"
mkdir -p "${NEW_DIR}/libs/smg4lib/src/construction"
mkdir -p "${NEW_DIR}/libs/smg4lib/src/physics"

# 复制主 CMakeLists.txt
cp CMakeLists_NEW.txt "${NEW_DIR}/CMakeLists.txt"

# 复制 cmake 模块
cp cmake_new/FindANAROOT.cmake "${NEW_DIR}/cmake/"
cp cmake_new/FindXercesC.cmake "${NEW_DIR}/cmake/"

# 复制 libs
cp cmake_new/libs_CMakeLists.txt "${NEW_DIR}/libs/CMakeLists.txt"
cp cmake_new/libs_smg4lib_CMakeLists.txt "${NEW_DIR}/libs/smg4lib/CMakeLists.txt"
cp cmake_new/libs_smg4lib_data_CMakeLists.txt "${NEW_DIR}/libs/smg4lib/src/data/CMakeLists.txt"
cp cmake_new/libs_smg4lib_action_CMakeLists.txt "${NEW_DIR}/libs/smg4lib/src/action/CMakeLists.txt"
cp cmake_new/libs_smg4lib_construction_CMakeLists.txt "${NEW_DIR}/libs/smg4lib/src/construction/CMakeLists.txt"
cp cmake_new/libs_smg4lib_physics_CMakeLists.txt "${NEW_DIR}/libs/smg4lib/src/physics/CMakeLists.txt"
cp cmake_new/libs_sim_deuteron_core_CMakeLists.txt "${NEW_DIR}/libs/sim_deuteron_core/CMakeLists.txt"
cp cmake_new/libs_analysis_CMakeLists.txt "${NEW_DIR}/libs/analysis/CMakeLists.txt"

# 复制 apps
cp cmake_new/apps_CMakeLists.txt "${NEW_DIR}/apps/CMakeLists.txt"
cp cmake_new/apps_sim_deuteron_CMakeLists.txt "${NEW_DIR}/apps/sim_deuteron/CMakeLists.txt"
cp cmake_new/apps_run_reconstruction_CMakeLists.txt "${NEW_DIR}/apps/run_reconstruction/CMakeLists.txt"
cp cmake_new/apps_tools_CMakeLists.txt "${NEW_DIR}/apps/tools/CMakeLists.txt"

# 复制 tests
cp cmake_new/tests_CMakeLists.txt "${NEW_DIR}/tests/CMakeLists.txt"
cp cmake_new/tests_unit_CMakeLists.txt "${NEW_DIR}/tests/unit/CMakeLists.txt"
cp cmake_new/tests_integration_CMakeLists.txt "${NEW_DIR}/tests/integration/CMakeLists.txt"

# 复制指南到新目录
cp MIGRATION_GUIDE.md "${NEW_DIR}/"
cp QUICK_START.md "${NEW_DIR}/"

log_info "✓ CMake 文件复制完成"

##############################################################################
# 步骤 4: 创建辅助脚本
##############################################################################

log_step "步骤 4/6: 创建辅助脚本..."

# 创建编译脚本
cat > "${NEW_DIR}/build.sh" << 'BUILDSCRIPT'
#!/bin/bash
set -e

# 设置环境
if [ -f "setup.sh" ]; then
    source setup.sh
fi

# 清理旧的构建
rm -rf build bin lib

# 创建构建目录
mkdir -p build
cd build

# CMake 配置
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=ON \
    -DBUILD_APPS=ON \
    -DBUILD_ANALYSIS=ON \
    -DWITH_ANAROOT=ON \
    -DWITH_GEANT4_UIVIS=ON

# 编译
make -j$(nproc)

echo ""
echo "✓ 编译完成！"
echo ""
echo "可执行文件位于: ../bin/sim_deuteron"
BUILDSCRIPT

chmod +x "${NEW_DIR}/build.sh"

# 创建快速测试脚本
cat > "${NEW_DIR}/test.sh" << 'TESTSCRIPT'
#!/bin/bash
set -e

if [ ! -d "build" ]; then
    echo "错误: build 目录不存在，请先运行 ./build.sh"
    exit 1
fi

cd build
ctest --output-on-failure
TESTSCRIPT

chmod +x "${NEW_DIR}/test.sh"

# 创建清理脚本
cat > "${NEW_DIR}/clean.sh" << 'CLEANSCRIPT'
#!/bin/bash
rm -rf build bin lib
echo "清理完成"
CLEANSCRIPT

chmod +x "${NEW_DIR}/clean.sh"

log_info "✓ 辅助脚本创建完成"

##############################################################################
# 步骤 5: 创建示例配置文件
##############################################################################

log_step "步骤 5/6: 创建示例配置文件..."

# 创建简单的测试宏
cat > "${NEW_DIR}/configs/simulation/macros/test.mac" << 'TESTMAC'
# Simple test macro for integration testing
/run/verbose 0
/event/verbose 0
/tracking/verbose 0

# Initialize
/run/initialize

# Generate a few events
/run/beamOn 10
TESTMAC

log_info "✓ 示例配置创建完成"

##############################################################################
# 步骤 6: 生成摘要
##############################################################################

log_step "步骤 6/6: 生成项目摘要..."

cat > "${NEW_DIR}/README.md" << 'README'
# SMSimulator - 重构版本 v5.5

## 快速开始

### 编译项目

```bash
./build.sh
```

### 运行模拟

```bash
./bin/sim_deuteron configs/simulation/macros/simulation.mac
```

### 运行测试

```bash
./test.sh
```

### 清理构建

```bash
./clean.sh
```

## 项目结构

- `libs/` - 核心 C++ 库
  - `smg4lib/` - Geant4 基础库
  - `sim_deuteron_core/` - 氘核模拟核心
  - `analysis/` - 数据分析库
- `apps/` - 可执行程序
- `configs/` - 配置文件
- `data/` - 数据文件
- `scripts/` - Python 工具
- `tests/` - 测试代码
- `docs/` - 文档

## 文档

- [迁移指南](MIGRATION_GUIDE.md)
- [快速开始](QUICK_START.md)
- [详细文档](docs/)

## 编译选项

编辑 `build.sh` 来修改 CMake 选项：

- `BUILD_TESTS` - 构建测试
- `BUILD_ANALYSIS` - 构建分析库
- `WITH_ANAROOT` - ANAROOT 支持
- `WITH_GEANT4_UIVIS` - Geant4 可视化

## 环境要求

- CMake >= 3.16
- ROOT >= 6.0
- Geant4 >= 10.0
- GCC/Clang 支持 C++17
- (可选) ANAROOT

## 联系方式

项目主页: [GitHub](https://github.com/tianbaiting/Dpol_smsimulator)
README

log_info "✓ 项目摘要创建完成"

##############################################################################
# 完成
##############################################################################

echo ""
echo "═══════════════════════════════════════════════════════════════"
echo -e "${GREEN}迁移和配置完成！${NC}"
echo "═══════════════════════════════════════════════════════════════"
echo ""
echo "新项目位置: ${SCRIPT_DIR}/${NEW_DIR}"
echo ""
echo "下一步操作:"
echo ""
echo "  1. 进入新项目目录:"
echo "     cd ${NEW_DIR}"
echo ""
echo "  2. 编译项目:"
echo "     ./build.sh"
echo ""
echo "  3. 运行测试:"
echo "     ./test.sh"
echo ""
echo "  4. 查看文档:"
echo "     cat MIGRATION_GUIDE.md"
echo "     cat QUICK_START.md"
echo ""
echo "═══════════════════════════════════════════════════════════════"
echo ""

# 创建快捷方式脚本
cat > go_to_new_project.sh << 'GOSCRIPT'
#!/bin/bash
cd smsimulator5.5_new
exec $SHELL
GOSCRIPT
chmod +x go_to_new_project.sh

log_info "提示: 运行 ./go_to_new_project.sh 快速进入新项目目录"
echo ""
