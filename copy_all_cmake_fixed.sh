#!/bin/bash
set -e

echo "复制 CMake 文件到新项目..."

OLD_DIR="/home/tian/workspace/dpol/smsimulator5.5"
NEW_DIR="/home/tian/workspace/dpol/smsimulator5.5_new"

cd "${OLD_DIR}"

# 复制主 CMakeLists.txt
cp CMakeLists_NEW.txt "${NEW_DIR}/CMakeLists.txt"
echo "✓ 主 CMakeLists.txt"

# 创建并复制 cmake 模块
mkdir -p "${NEW_DIR}/cmake"
cp cmake_new/FindANAROOT.cmake "${NEW_DIR}/cmake/"
cp cmake_new/FindXercesC.cmake "${NEW_DIR}/cmake/"
echo "✓ cmake 模块"

# 复制 libs CMakeLists
cp cmake_new/libs_CMakeLists.txt "${NEW_DIR}/libs/CMakeLists.txt"
echo "✓ libs/CMakeLists.txt"

# 复制 smg4lib CMakeLists
cp cmake_new/libs_smg4lib_CMakeLists.txt "${NEW_DIR}/libs/smg4lib/CMakeLists.txt"
mkdir -p "${NEW_DIR}/libs/smg4lib/src/data"
mkdir -p "${NEW_DIR}/libs/smg4lib/src/action"
mkdir -p "${NEW_DIR}/libs/smg4lib/src/construction"
mkdir -p "${NEW_DIR}/libs/smg4lib/src/physics"
cp cmake_new/libs_smg4lib_data_CMakeLists.txt "${NEW_DIR}/libs/smg4lib/src/data/CMakeLists.txt"
cp cmake_new/libs_smg4lib_action_CMakeLists.txt "${NEW_DIR}/libs/smg4lib/src/action/CMakeLists.txt"
cp cmake_new/libs_smg4lib_construction_CMakeLists.txt "${NEW_DIR}/libs/smg4lib/src/construction/CMakeLists.txt"
cp cmake_new/libs_smg4lib_physics_CMakeLists.txt "${NEW_DIR}/libs/smg4lib/src/physics/CMakeLists.txt"
echo "✓ smg4lib CMakeLists"

# 复制其他 libs
cp cmake_new/libs_sim_deuteron_core_CMakeLists.txt "${NEW_DIR}/libs/sim_deuteron_core/CMakeLists.txt"
cp cmake_new/libs_analysis_CMakeLists.txt "${NEW_DIR}/libs/analysis/CMakeLists.txt"
echo "✓ sim_deuteron_core 和 analysis CMakeLists"

# 复制 apps CMakeLists
mkdir -p "${NEW_DIR}/apps/sim_deuteron"
mkdir -p "${NEW_DIR}/apps/run_reconstruction"
mkdir -p "${NEW_DIR}/apps/tools"
cp cmake_new/apps_CMakeLists.txt "${NEW_DIR}/apps/CMakeLists.txt"
cp cmake_new/apps_sim_deuteron_CMakeLists.txt "${NEW_DIR}/apps/sim_deuteron/CMakeLists.txt"
cp cmake_new/apps_run_reconstruction_CMakeLists.txt "${NEW_DIR}/apps/run_reconstruction/CMakeLists.txt"
cp cmake_new/apps_tools_CMakeLists.txt "${NEW_DIR}/apps/tools/CMakeLists.txt"
echo "✓ apps CMakeLists"

# 复制 tests CMakeLists
mkdir -p "${NEW_DIR}/tests/unit"
mkdir -p "${NEW_DIR}/tests/integration"
cp cmake_new/tests_CMakeLists.txt "${NEW_DIR}/tests/CMakeLists.txt"
cp cmake_new/tests_unit_CMakeLists.txt "${NEW_DIR}/tests/unit/CMakeLists.txt"
cp cmake_new/tests_integration_CMakeLists.txt "${NEW_DIR}/tests/integration/CMakeLists.txt"
echo "✓ tests CMakeLists"

echo ""
echo "✅ 所有 CMake 文件已复制完成！"
echo ""
echo "现在创建构建脚本..."

# 创建 build.sh
cat > "${NEW_DIR}/build.sh" << 'BUILDSCRIPT'
#!/bin/bash
set -e

echo "════════════════════════════════════════════════════════════════"
echo "  SMSimulator 编译脚本"
echo "════════════════════════════════════════════════════════════════"
echo ""

# 设置环境
if [ -f "setup.sh" ]; then
    echo "设置环境变量..."
    source setup.sh
fi

# 清理旧的构建
echo "清理旧构建..."
rm -rf build bin lib

# 创建构建目录
mkdir -p build
cd build

# CMake 配置
echo ""
echo "配置 CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=ON \
    -DBUILD_APPS=ON \
    -DBUILD_ANALYSIS=ON \
    -DWITH_ANAROOT=ON \
    -DWITH_GEANT4_UIVIS=ON

# 编译
echo ""
echo "编译中..."
make -j$(nproc)

echo ""
echo "════════════════════════════════════════════════════════════════"
echo "✅ 编译完成！"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "可执行文件位于: ../bin/sim_deuteron"
echo ""
echo "运行方法:"
echo "  ./bin/sim_deuteron configs/simulation/macros/simulation.mac"
echo ""
BUILDSCRIPT

chmod +x "${NEW_DIR}/build.sh"
echo "✓ build.sh"

# 创建 test.sh
cat > "${NEW_DIR}/test.sh" << 'TESTSCRIPT'
#!/bin/bash
set -e

if [ ! -d "build" ]; then
    echo "错误: build 目录不存在，请先运行 ./build.sh"
    exit 1
fi

cd build
echo "运行测试..."
ctest --output-on-failure
TESTSCRIPT

chmod +x "${NEW_DIR}/test.sh"
echo "✓ test.sh"

# 创建 clean.sh
cat > "${NEW_DIR}/clean.sh" << 'CLEANSCRIPT'
#!/bin/bash
echo "清理构建文件..."
rm -rf build bin lib
echo "✓ 清理完成"
CLEANSCRIPT

chmod +x "${NEW_DIR}/clean.sh"
echo "✓ clean.sh"

# 创建 run_sim.sh
cat > "${NEW_DIR}/run_sim.sh" << 'RUNSCRIPT'
#!/bin/bash

if [ ! -f "bin/sim_deuteron" ]; then
    echo "错误: sim_deuteron 未找到，请先运行 ./build.sh"
    exit 1
fi

# 默认宏文件
MAC_FILE=${1:-"configs/simulation/macros/simulation.mac"}

if [ ! -f "$MAC_FILE" ]; then
    echo "错误: 宏文件 $MAC_FILE 不存在"
    echo "用法: $0 [宏文件路径]"
    exit 1
fi

echo "运行模拟: $MAC_FILE"
./bin/sim_deuteron "$MAC_FILE"
RUNSCRIPT

chmod +x "${NEW_DIR}/run_sim.sh"
echo "✓ run_sim.sh"

echo ""
echo "════════════════════════════════════════════════════════════════"
echo "✅ 全部完成！"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "下一步："
echo "  cd ${NEW_DIR}"
echo "  ./build.sh"
echo ""
