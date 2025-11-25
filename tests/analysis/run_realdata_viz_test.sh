#!/bin/bash
#
# 真实数据重建可视化测试脚本
# 
# 用法:
#   ./run_realdata_viz_test.sh [event_id]
#
# 示例:
#   ./run_realdata_viz_test.sh 0     # 分析第0个事件
#   ./run_realdata_viz_test.sh       # 默认分析第0个事件

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 检查是否在 build 目录
if [ ! -f "bin/test_TargetReconstructor_RealData" ]; then
    echo -e "${RED}错误: 请在 build 目录下运行此脚本${NC}"
    echo "示例: cd /home/tian/workspace/dpol/smsimulator5.5/build && ../tests/analysis/run_realdata_viz_test.sh"
    exit 1
fi

# 获取事件 ID（默认为 0）
EVENT_ID=${1:-0}

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}真实数据重建可视化测试（带MC Truth对比）${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "事件 ID: ${GREEN}${EVENT_ID}${NC}"
echo -e "可视化: ${GREEN}已启用${NC}"
echo -e "MC Truth: ${GREEN}包含${NC}"
echo ""

# 创建输出目录
OUTPUT_DIR="test_output/reconstruction_realdata_event${EVENT_ID}"
mkdir -p "${OUTPUT_DIR}"

# 运行测试
echo -e "${YELLOW}正在运行测试...${NC}"
echo ""

SM_TEST_VISUALIZATION=ON ./bin/test_TargetReconstructor_RealData \
    '--gtest_filter=*Single*' 2>&1 | tee test_realdata_viz.log

# 检查是否成功
if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}✓ 测试成功完成！${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    
    # 检查生成的文件
    if [ -f "${OUTPUT_DIR}/c_opt_path.png" ]; then
        echo -e "${GREEN}✓ Loss Function 图已生成:${NC}"
        echo "  ${OUTPUT_DIR}/c_opt_path.png"
        echo "  ${OUTPUT_DIR}/c_opt_path.root"
        echo ""
    fi
    
    if [ -f "${OUTPUT_DIR}/c_traj_3d.png" ]; then
        echo -e "${GREEN}✓ 3D 轨迹图已生成:${NC}"
        echo "  ${OUTPUT_DIR}/c_traj_3d.png"
        echo "  ${OUTPUT_DIR}/c_traj_3d.root"
        echo ""
    fi
    
    # 提示如何查看
    echo -e "${BLUE}查看图像:${NC}"
    echo "  # PNG 图像"
    echo "  eog ${OUTPUT_DIR}/c_opt_path.png"
    echo "  eog ${OUTPUT_DIR}/c_traj_3d.png"
    echo ""
    echo "  # ROOT 文件（交互式）"
    echo "  root -l ${OUTPUT_DIR}/c_opt_path.root"
    echo "  root -l ${OUTPUT_DIR}/c_traj_3d.root"
    echo ""
    
    # 提示日志位置
    echo -e "${BLUE}完整日志:${NC}"
    echo "  test_realdata_viz.log"
    echo ""
    
else
    echo ""
    echo -e "${RED}========================================${NC}"
    echo -e "${RED}✗ 测试失败${NC}"
    echo -e "${RED}========================================${NC}"
    echo ""
    echo "请查看日志文件: test_realdata_viz.log"
    exit 1
fi
