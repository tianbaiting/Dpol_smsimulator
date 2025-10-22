#!/bin/bash
# 测试 MagneticField 类

cd "$(dirname "$0")"

echo "=========================================="
echo "  测试 MagneticField 类"
echo "=========================================="
echo ""

# 检查库是否存在
if [ ! -f "sources/build/libPDCAnalysisTools.so" ]; then
    echo "❌ 错误: 找不到 libPDCAnalysisTools.so"
    echo "请先编译:"
    echo "  cd sources/build && cmake .. && make -j4"
    exit 1
fi

# 检查磁场文件是否存在
FIELD_FILE="geometry/filed_map/180626-1,20T-3000.table"
if [ ! -f "$FIELD_FILE" ]; then
    echo "❌ 错误: 找不到磁场文件 $FIELD_FILE"
    exit 1
fi

echo "✅ 库文件存在"
echo "✅ 磁场文件存在"
echo ""

# 运行测试
echo "正在运行磁场测试..."
echo ""

root -l <<EOF
.x macros/load_magnetic_field_lib.C
.x macros/test_magnetic_field_func.C
test_magnetic_field_func()
.q
EOF