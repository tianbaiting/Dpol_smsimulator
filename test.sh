#!/bin/bash
set -e

if [ ! -d "build" ]; then
    echo "错误: build 目录不存在，请先运行 ./build.sh"
    exit 1
fi

cd build
echo "运行测试..."
ctest --output-on-failure
