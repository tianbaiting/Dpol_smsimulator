#!/bin/bash
#
# ============================================================================
# run_3d_display.sh - 真实数据3D事件显示脚本
# ============================================================================
#
# 功能：
#   - 运行ROOT宏显示真实数据的3D重建结果
#   - 支持指定事件ID和数据文件
#   - 自动检查环境变量和文件存在性
#
# 使用方法：
#   ./run_3d_display.sh [event_id] [data_file]
#
# 示例：
#   ./run_3d_display.sh 0                         # 默认文件，事件0
#   ./run_3d_display.sh 5                         # 默认文件，事件5
#   ./run_3d_display.sh 0 /path/to/data.root     # 自定义文件
#
# 前提条件：
#   1. 已运行：source setup.sh
#   2. 环境变量 SMSIMDIR 已设置
#   3. 数据文件存在且可读
#
# 输出：
#   - 3D交互式显示窗口
#   - PNG图像：test_output/reconstruction_realdata_event<N>/event_display_3d.png
#
# 故障排查：
#   - "SMSIMDIR 未设置" → 运行 source setup.sh
#   - "数据文件不存在" → 检查文件路径
#   - "ROOT宏文件不存在" → 检查脚本目录
#
# ============================================================================

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 获取事件 ID（默认为 0）
EVENT_ID=${1:-0}

# 获取数据文件路径（默认值）
DATA_FILE=${2:-"/home/tian/workspace/dpol/smsimulator5.5/data/simulation/output_tree/testry0000.root"}

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}真实数据3D事件显示${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "事件 ID: ${GREEN}${EVENT_ID}${NC}"
echo -e "数据文件: ${GREEN}${DATA_FILE}${NC}"
echo ""

# 检查数据文件是否存在
if [ ! -f "${DATA_FILE}" ]; then
    echo -e "${RED}错误: 数据文件不存在: ${DATA_FILE}${NC}"
    exit 1
fi

# 检查环境变量
if [ -z "$SMSIMDIR" ]; then
    echo -e "${RED}错误: 环境变量 SMSIMDIR 未设置${NC}"
    exit 1
fi

# 获取脚本所在目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT_MACRO="${SCRIPT_DIR}/run_event_display_realdata.C"

# 检查ROOT宏文件是否存在
if [ ! -f "${ROOT_MACRO}" ]; then
    echo -e "${RED}错误: ROOT宏文件不存在: ${ROOT_MACRO}${NC}"
    exit 1
fi

echo -e "${YELLOW}启动ROOT 3D显示...${NC}"
echo ""

# 运行ROOT宏（交互模式）
cd ${SCRIPT_DIR}/../../build || exit 1

root -l <<EOF
.L ${ROOT_MACRO}+
run_event_display_realdata(${EVENT_ID}, "${DATA_FILE}")
EOF

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}✓ 显示完成${NC}"
echo -e "${GREEN}========================================${NC}"
