#!/bin/bash
# =============================================================================
# QMD Geometry Filter Analysis Runner
# =============================================================================
# 
# 用法:
#   ./run_analysis.sh [OPTIONS]
#
# 选项:
#   --full              运行完整分析（所有配置）
#   --quick             快速测试（最少配置）
#   --custom            自定义配置（需要设置下面的变量）
#   --help              显示帮助信息
#
# 自定义配置示例（修改下面的变量）:
#   FIELD_STRENGTHS=(1.0 1.2)
#   DEFLECTION_ANGLES=(5 10)
#   TARGETS=("Pb208")
#   POL_TYPES=("zpol" "ypol")
#   GAMMA_VALUES=("050" "060" "070" "080")
#
# =============================================================================

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# =============================================================================
# 配置区域 - 在这里自定义你的分析参数
# =============================================================================

# 磁场强度列表 [T]
# 可用: 0.8, 1.0, 1.2, 1.4
FIELD_STRENGTHS_FULL=(0.8 1.0 1.2 1.4)
FIELD_STRENGTHS_QUICK=(1.0)
FIELD_STRENGTHS_CUSTOM=(1.0)

# 束流偏转角度列表 [度]
DEFLECTION_ANGLES_FULL=(0 5 10)
DEFLECTION_ANGLES_QUICK=(5)
DEFLECTION_ANGLES_CUSTOM=(5 10)

# 靶材料列表
TARGETS_FULL=("Pb208" "Sn124" "Sn112")
TARGETS_QUICK=("Pb208")
TARGETS_CUSTOM=("Pb208")

# 极化类型列表
POL_TYPES_FULL=("zpol" "ypol")
POL_TYPES_QUICK=("zpol")
POL_TYPES_CUSTOM=("zpol" "ypol")

# Gamma 值列表
GAMMA_VALUES_FULL=("050" "055" "060" "065" "070" "075" "080" "085")
GAMMA_VALUES_QUICK=("050" "060")
GAMMA_VALUES_CUSTOM=("050" "060" "070" "080")

# =============================================================================
# 路径配置
# =============================================================================

# 获取脚本所在目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# SMSIMDIR 路径（自动检测）
if [ -z "$SMSIMDIR" ]; then
    SMSIMDIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
fi

# 可执行文件路径
EXECUTABLE="$SMSIMDIR/build/bin/RunQMDGeoFilter"

# 输出目录
OUTPUT_DIR="$SMSIMDIR/results/qmd_geo_filter"

# =============================================================================
# 函数定义
# =============================================================================

print_header() {
    echo -e "${BLUE}============================================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}============================================================${NC}"
}

print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_executable() {
    if [ ! -f "$EXECUTABLE" ]; then
        print_error "可执行文件不存在: $EXECUTABLE"
        print_info "请先编译: cd $SMSIMDIR/build && make RunQMDGeoFilter"
        exit 1
    fi
    
    if [ ! -x "$EXECUTABLE" ]; then
        print_error "文件不可执行: $EXECUTABLE"
        exit 1
    fi
    
    print_info "找到可执行文件: $EXECUTABLE"
}

show_help() {
    cat << EOF
QMD Geometry Filter Analysis Runner

用法:
    $0 [MODE] [OPTIONS]

模式:
    --full              运行完整分析（所有配置）
    --quick             快速测试（最少配置）
    --custom            自定义配置（修改脚本中的 CUSTOM 变量）
    --help              显示此帮助信息

选项:
    --dry-run           只显示将要运行的命令，不实际执行
    --output DIR        指定输出目录（默认: $OUTPUT_DIR）

环境变量:
    SMSIMDIR            SMSimulator 根目录（默认: $SMSIMDIR）

示例:
    # 快速测试
    $0 --quick

    # 完整分析
    $0 --full

    # 自定义配置（需要修改脚本中的 CUSTOM 变量）
    $0 --custom

    # 预览命令但不执行
    $0 --full --dry-run

自定义配置:
    编辑此脚本，修改以下变量来自定义你的分析:
    - FIELD_STRENGTHS_CUSTOM    磁场强度列表
    - DEFLECTION_ANGLES_CUSTOM  偏转角度列表
    - TARGETS_CUSTOM            靶材料列表
    - POL_TYPES_CUSTOM          极化类型列表
    - GAMMA_VALUES_CUSTOM       Gamma 值列表

EOF
}

build_command() {
    local fields=("$@")
    shift $#
    local angles=("$@")
    shift $#
    local targets=("$@")
    shift $#
    local pols=("$@")
    shift $#
    local gammas=("$@")
    
    CMD="$EXECUTABLE"
    
    # 添加磁场参数
    for field in "${FIELD_STRENGTHS[@]}"; do
        CMD="$CMD --field $field"
    done
    
    # 添加角度参数
    for angle in "${DEFLECTION_ANGLES[@]}"; do
        CMD="$CMD --angle $angle"
    done
    
    # 添加靶材料参数
    for target in "${TARGETS[@]}"; do
        CMD="$CMD --target $target"
    done
    
    # 添加极化类型参数
    for pol in "${POL_TYPES[@]}"; do
        CMD="$CMD --pol $pol"
    done
    
    # 添加 gamma 值参数
    for gamma in "${GAMMA_VALUES[@]}"; do
        CMD="$CMD --gamma $gamma"
    done
    
    # 添加输出目录
    CMD="$CMD --output $OUTPUT_DIR"
    
    echo "$CMD"
}

print_configuration() {
    echo ""
    print_header "分析配置"
    echo -e "  ${GREEN}磁场强度:${NC}    ${FIELD_STRENGTHS[*]}"
    echo -e "  ${GREEN}偏转角度:${NC}    ${DEFLECTION_ANGLES[*]}"
    echo -e "  ${GREEN}靶材料:${NC}      ${TARGETS[*]}"
    echo -e "  ${GREEN}极化类型:${NC}    ${POL_TYPES[*]}"
    echo -e "  ${GREEN}Gamma值:${NC}     ${GAMMA_VALUES[*]}"
    echo -e "  ${GREEN}输出目录:${NC}    $OUTPUT_DIR"
    echo ""
    
    # 计算总配置数
    local n_configs=$((${#FIELD_STRENGTHS[@]} * ${#DEFLECTION_ANGLES[@]} * ${#TARGETS[@]} * ${#POL_TYPES[@]}))
    local n_gamma=${#GAMMA_VALUES[@]}
    echo -e "  ${YELLOW}总配置数:${NC}    $n_configs 个配置 × $n_gamma 个 gamma 值"
    echo ""
}

run_analysis() {
    print_configuration
    
    # 构建命令
    local cmd=$(build_command)
    
    print_header "运行命令"
    echo "$cmd"
    echo ""
    
    if [ "$DRY_RUN" = true ]; then
        print_warning "Dry-run 模式，不实际执行"
        return 0
    fi
    
    # 确认执行
    read -p "$(echo -e ${YELLOW}是否继续执行? [y/N]:${NC} )" -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_warning "用户取消执行"
        exit 0
    fi
    
    # 记录开始时间
    START_TIME=$(date +%s)
    
    # 执行命令
    print_info "开始分析..."
    eval $cmd
    local exit_code=$?
    
    # 记录结束时间
    END_TIME=$(date +%s)
    ELAPSED=$((END_TIME - START_TIME))
    
    echo ""
    if [ $exit_code -eq 0 ]; then
        print_header "分析完成"
        print_info "耗时: ${ELAPSED} 秒"
        print_info "结果保存在: $OUTPUT_DIR"
    else
        print_error "分析失败，退出码: $exit_code"
        exit $exit_code
    fi
}

# =============================================================================
# 主程序
# =============================================================================

# 默认参数
MODE="custom"
DRY_RUN=false

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        --full)
            MODE="full"
            shift
            ;;
        --quick)
            MODE="quick"
            shift
            ;;
        --custom)
            MODE="custom"
            shift
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            print_error "未知选项: $1"
            show_help
            exit 1
            ;;
    esac
done

# 根据模式设置配置
case $MODE in
    full)
        FIELD_STRENGTHS=("${FIELD_STRENGTHS_FULL[@]}")
        DEFLECTION_ANGLES=("${DEFLECTION_ANGLES_FULL[@]}")
        TARGETS=("${TARGETS_FULL[@]}")
        POL_TYPES=("${POL_TYPES_FULL[@]}")
        GAMMA_VALUES=("${GAMMA_VALUES_FULL[@]}")
        ;;
    quick)
        FIELD_STRENGTHS=("${FIELD_STRENGTHS_QUICK[@]}")
        DEFLECTION_ANGLES=("${DEFLECTION_ANGLES_QUICK[@]}")
        TARGETS=("${TARGETS_QUICK[@]}")
        POL_TYPES=("${POL_TYPES_QUICK[@]}")
        GAMMA_VALUES=("${GAMMA_VALUES_QUICK[@]}")
        ;;
    custom)
        FIELD_STRENGTHS=("${FIELD_STRENGTHS_CUSTOM[@]}")
        DEFLECTION_ANGLES=("${DEFLECTION_ANGLES_CUSTOM[@]}")
        TARGETS=("${TARGETS_CUSTOM[@]}")
        POL_TYPES=("${POL_TYPES_CUSTOM[@]}")
        GAMMA_VALUES=("${GAMMA_VALUES_CUSTOM[@]}")
        ;;
esac

# 显示标题
print_header "QMD Geometry Filter Analysis"
echo -e "  模式: ${GREEN}$MODE${NC}"
echo ""

# 检查可执行文件
check_executable

# 运行分析
run_analysis
