#!/bin/bash
# ============================================================================
# PDC位置计算测试运行脚本
# ============================================================================
# 
# [EN] This script runs PDC position calculation tests with different magnetic
#      field configurations. The generated plots include:
#      - Deuteron track (beam trajectory through magnet)
#      - Center proton track (Px=0)
#      - Edge proton tracks (Px=±100, ±150 MeV/c)
# 
# [CN] 此脚本运行不同磁场配置的PDC位置计算测试。生成的图包括：
#      - 氘核轨迹（穿过磁铁的束流轨迹）
#      - 中心质子轨迹 (Px=0)
#      - 边缘质子轨迹 (Px=±100, ±150 MeV/c)
#
# Output location / 输出位置: $SMSIMDIR/results/test_pdc_position/
#
# Available field maps / 可用磁场文件:
#   0.8T: 141114-0,8T-3000.table
#   1.0T: 180626-1,00T-3000.table
#   1.2T: 180626-1,20T-3000.table
#   1.4T: 180703-1,40T-3000.table
#   1.6T: 180626-1,60T-3000.table
#   1.8T: 180703-1,80T-3000.table
#   2.0T: 180627-2,00T-3000.table
#   2.2T: 180628-2,20T-3000.table
#   2.4T: 180702-2,40T-3000.table
#   2.6T: 180703-2,60T-3000.table
#   2.8T: 180628-2,80T-3000.table
#   3.0T: 180629-3,00T-3000.table
# ============================================================================

# 检查环境变量
if [ -z "$SMSIMDIR" ]; then
    echo "ERROR: SMSIMDIR environment variable not set"
    echo "Please run: source setup.sh"
    exit 1
fi

FIELD_MAP_DIR="$SMSIMDIR/configs/simulation/geometry/filed_map"
TEST_BIN="$SMSIMDIR/build/bin/test_pdc_position"
OUTPUT_DIR="$SMSIMDIR/results/test_pdc_position"

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║         PDC Position Calculation Test Suite                   ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""
echo "Output directory: $OUTPUT_DIR"
echo ""

# 创建输出目录
mkdir -p "$OUTPUT_DIR"

# 批量运行所有3000.table磁场文件
echo "Running tests with all magnetic field configurations..."
echo ""

# 测试 0.8T 磁场
echo "[1/12] Testing 0.8T field..."
$TEST_BIN "$FIELD_MAP_DIR/141114-0,8T-3000.table"

# 测试 1.0T 磁场（默认）
echo "[2/12] Testing 1.0T field..."
$TEST_BIN "$FIELD_MAP_DIR/180626-1,00T-3000.table"

# 测试 1.2T 磁场
echo "[3/12] Testing 1.2T field..."
$TEST_BIN "$FIELD_MAP_DIR/180626-1,20T-3000.table"

# 测试 1.4T 磁场
echo "[4/12] Testing 1.4T field..."
$TEST_BIN "$FIELD_MAP_DIR/180703-1,40T-3000.table"

# 测试 1.6T 磁场
echo "[5/12] Testing 1.6T field..."
$TEST_BIN "$FIELD_MAP_DIR/180626-1,60T-3000.table"

# 测试 1.8T 磁场
echo "[6/12] Testing 1.8T field..."
$TEST_BIN "$FIELD_MAP_DIR/180703-1,80T-3000.table"

# 测试 2.0T 磁场
echo "[7/12] Testing 2.0T field..."
$TEST_BIN "$FIELD_MAP_DIR/180627-2,00T-3000.table"

# 测试 2.2T 磁场
echo "[8/12] Testing 2.2T field..."
$TEST_BIN "$FIELD_MAP_DIR/180628-2,20T-3000.table"

# 测试 2.4T 磁场
echo "[9/12] Testing 2.4T field..."
$TEST_BIN "$FIELD_MAP_DIR/180702-2,40T-3000.table"

# 测试 2.6T 磁场
echo "[10/12] Testing 2.6T field..."
$TEST_BIN "$FIELD_MAP_DIR/180703-2,60T-3000.table"

# 测试 2.8T 磁场
echo "[11/12] Testing 2.8T field..."
$TEST_BIN "$FIELD_MAP_DIR/180628-2,80T-3000.table"

# 测试 3.0T 磁场
echo "[12/12] Testing 3.0T field..."
$TEST_BIN "$FIELD_MAP_DIR/180629-3,00T-3000.table"

echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║                   All tests completed!                        ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""
echo "Generated plots show:"
echo "  - Green line: Deuteron (beam) trajectory"
echo "  - Red line: Center proton track (Px=0)"
echo "  - Blue/Magenta lines: Edge proton tracks (Px=±100, ±150 MeV/c)"
echo ""
echo "Check output at: $OUTPUT_DIR"
echo ""