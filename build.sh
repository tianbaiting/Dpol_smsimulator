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
