#!/bin/bash
# 测试日志重定向功能

set -e

# 设置环境
cd "$(dirname "$0")"
export SMSIMDIR=$(pwd)

# 清理旧的构建
echo "清理旧的构建..."
cd build
make clean || true

# 重新编译
echo "重新编译..."
cmake .. && make -j4

# 运行测试（单个场文件）
echo -e "\n=== 测试1: 单个场文件 ==="
./bin/test_pdc_position ../configs/simulation/geometry/filed_map/180626-1,00T-6000.table 30

# 检查输出
echo -e "\n=== 检查输出文件 ==="
OUTPUT_DIR="$SMSIMDIR/results/test_pdc_position"
if [ -f "$OUTPUT_DIR/test_pdc_position.log" ]; then
    echo "✓ 日志文件已创建: $OUTPUT_DIR/test_pdc_position.log"
    echo "  文件大小: $(du -h $OUTPUT_DIR/test_pdc_position.log | cut -f1)"
else
    echo "✗ 日志文件未创建!"
    exit 1
fi

if [ -f "$OUTPUT_DIR/test_pdc_position.png" ]; then
    echo "✓ 图像文件已创建: $OUTPUT_DIR/test_pdc_position.png"
else
    echo "✗ 图像文件未创建!"
fi

# 显示日志文件前20行
echo -e "\n=== 日志文件前20行 ==="
head -n 20 "$OUTPUT_DIR/test_pdc_position.log"

echo -e "\n=== 测试成功 ==="
echo "所有输出已保存到: $OUTPUT_DIR"
echo "查看完整日志: cat $OUTPUT_DIR/test_pdc_position.log"
