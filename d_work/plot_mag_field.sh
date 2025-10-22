#!/bin/bash
# 绘制磁场分布图

cd "$(dirname "$0")"

echo "=========================================="
echo "  磁场分布可视化工具"
echo "=========================================="
echo ""

# 检查库是否存在
if [ ! -f "sources/build/libPDCAnalysisTools.so" ]; then
    echo "❌ 错误: 找不到 libPDCAnalysisTools.so"
    echo "请先编译:"
    echo "  cd sources/build && cmake .. && make -j4"
    exit 1
fi

# 获取旋转角度参数(默认为30度)
ROTATION_ANGLE=${1:-30}

echo "✅ 库文件存在"
echo "🔄 旋转角度: ${ROTATION_ANGLE} 度"
echo ""

# 检查磁场文件或ROOT文件是否存在
if [ -f "magnetic_field_test.root" ]; then
    echo "✅ 发现已保存的磁场ROOT文件，将快速加载"
elif [ -f "geometry/filed_map/180626-1,20T-3000.table" ]; then
    echo "✅ 发现原始磁场文件"
    echo "⚠️  首次运行需要几分钟来加载磁场数据"
else
    echo "❌ 错误: 找不到磁场文件"
    exit 1
fi

echo ""
echo "正在启动磁场绘制..."
echo "这将生成4个子图:"
echo "  1. XZ平面磁场强度分布"
echo "  2. XY平面磁场强度分布" 
echo "  3. By分量分布"
echo "  4. 沿X轴磁场分布曲线"
echo ""

# 运行绘制脚本
root -l <<EOF
.x macros/load_magnetic_field_lib.C
.x macros/plot_magnetic_field_func.C
plot_magnetic_field_func($ROTATION_ANGLE)
EOF

echo ""
echo "=========================================="
echo "  磁场绘制完成!"
echo "=========================================="
echo ""
echo "输出文件:"
echo "  - magnetic_field_${ROTATION_ANGLE}deg.png"
echo "  - magnetic_field_${ROTATION_ANGLE}deg.pdf"
echo ""
echo "使用方法:"
echo "  $0 [角度]     # 默认30度"
echo "  $0 0          # 0度 (无旋转)"
echo "  $0 45         # 45度旋转"
echo ""