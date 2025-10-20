#!/bin/bash

# 安全构建脚本 - 确保不删除源码
# Safe build script - ensures source code is not deleted

echo "=== SMSimulator 安全构建脚本 ==="
echo "=== SMSimulator Safe Build Script ==="

# 检查是否在源码目录中
if [ -f "CMakeLists.txt" ] && [ -d "sources" ]; then
    echo "✓ 检测到SMSimulator源码目录"
    echo "✓ SMSimulator source directory detected"
else
    echo "✗ 错误：请在SMSimulator根目录运行此脚本"
    echo "✗ Error: Please run this script from SMSimulator root directory"
    exit 1
fi

# 创建独立的构建目录
BUILD_DIR="build"
if [ -d "$BUILD_DIR" ]; then
    echo "⚠ 构建目录已存在，将清理旧的构建文件"
    echo "⚠ Build directory exists, will clean old build files"
    read -p "继续吗? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm -rf "$BUILD_DIR"
    else
        echo "构建已取消"
        echo "Build cancelled"
        exit 0
    fi
fi

# 创建构建目录
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo ""
echo "=== 配置项目 ==="
echo "=== Configuring project ==="

# 运行CMake配置
if cmake ..; then
    echo "✓ CMake配置成功"
    echo "✓ CMake configuration successful"
else
    echo "✗ CMake配置失败"
    echo "✗ CMake configuration failed"
    exit 1
fi

echo ""
echo "=== 开始构建 ==="
echo "=== Starting build ==="

# 构建项目
if make -j$(nproc); then
    echo ""
    echo "✓ 构建成功！"
    echo "✓ Build successful!"
    echo ""
    echo "可执行文件位置："
    echo "Executable location:"
    echo "  - Build目录: $PWD/sources/sim_deuteron/sim_deuteron"
    echo "  - Bin目录:   $(dirname $PWD)/bin/sim_deuteron"
    echo ""
    echo "注意：源码完全保持不变，没有任何文件被删除。"
    echo "Note: Source code remains completely unchanged, no files were deleted."
else
    echo "✗ 构建失败"
    echo "✗ Build failed"
    exit 1
fi

echo ""
echo "=== 构建完成 ==="
echo "=== Build complete ==="