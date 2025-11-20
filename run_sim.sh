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
