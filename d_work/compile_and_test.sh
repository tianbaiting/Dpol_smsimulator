#!/bin/bash
# 编译和测试脚本

cd /home/tian/workspace/dpol/smsimulator5.5/d_work/sources/build

echo "========================================="
echo "  重新编译 PDCAnalysisTools 库"
echo "========================================="
make clean
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "编译失败！"
    exit 1
fi

echo ""
echo "========================================="
echo "  运行旋转调试测试"
echo "========================================="
cd ..
root -l -b -q macros/debug_particle_rotation.C

echo ""
echo "========================================="
echo "  测试完成"
echo "========================================="
