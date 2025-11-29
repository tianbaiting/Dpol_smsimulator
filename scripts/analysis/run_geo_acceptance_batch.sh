#!/bin/bash
###############################################################################
# 批量几何接受度分析脚本
# Batch Geometry Acceptance Analysis Script
#
# 自动分析多个磁场配置和偏转角度下的PDC位置和几何接受度
#
# 使用方法:
#   ./run_geo_acceptance_batch.sh [OUTPUT_DIR]
#
# 示例:
#   ./run_geo_acceptance_batch.sh                    # 使用默认输出目录
#   ./run_geo_acceptance_batch.sh ./my_results       # 指定输出目录
#
# 作者: Auto-generated
# 日期: 2025-11-30
###############################################################################

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 项目根目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/../.." && pwd )"
BUILD_DIR="$PROJECT_ROOT/build"
FIELD_MAP_DIR="$PROJECT_ROOT/configs/simulation/geometry/filed_map"
QMD_DATA_DIR="$PROJECT_ROOT/data/qmdrawdata"

# 默认输出目录
OUTPUT_DIR="${1:-./geo_acceptance_batch_results}"

# 磁场配置
# 注意: 磁场文件后缀是 .table 不是 .dat（直接用table文件，不是目录内的field.dat）
declare -a FIELD_NAMES=("0.8T" "1.0T" "1.2T" "1.4T")
declare -a FIELD_FILES=(
    "141114-0,8T-6000.table"
    "180626-1,00T-6000.table"
    "180626-1,20T-3000.table"
    "180703-1,40T-6000.table"
)
declare -a FIELD_STRENGTHS=("0.8" "1.0" "1.2" "1.4")

# 偏转角度
DEFLECTION_ANGLES="0 5 10"

###############################################################################
# 函数定义
###############################################################################

print_banner() {
    echo ""
    echo "======================================================================"
    echo "                  几何接受度批量分析"
    echo "           Geometry Acceptance Batch Analysis"
    echo "======================================================================"
    echo "时间: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "======================================================================"
    echo ""
}

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_prerequisites() {
    print_info "检查前置条件..."
    
    # 检查可执行文件
    local exe_path="$BUILD_DIR/bin/RunGeoAcceptanceAnalysis"
    if [ ! -f "$exe_path" ]; then
        print_error "找不到可执行文件: $exe_path"
        print_error "请先编译项目:"
        echo "    cd $BUILD_DIR"
        echo "    cmake .. -DBUILD_ANALYSIS=ON"
        echo "    make GeoAcceptance -j4"
        return 1
    fi
    print_success "找到可执行文件: $exe_path"
    
    # 检查QMD数据
    if [ ! -d "$QMD_DATA_DIR" ]; then
        print_warning "QMD数据目录不存在: $QMD_DATA_DIR"
        print_warning "将使用空数据运行（仅计算几何配置）"
    else
        print_success "QMD数据目录: $QMD_DATA_DIR"
    fi
    
    # 检查磁场文件
    print_info "检查磁场文件:"
    local missing_count=0
    for i in "${!FIELD_NAMES[@]}"; do
        local field_path="$FIELD_MAP_DIR/${FIELD_FILES[$i]}"
        if [ -f "$field_path" ]; then
            echo "  ✓ ${FIELD_NAMES[$i]}: $field_path"
        else
            echo "  ✗ ${FIELD_NAMES[$i]}: $field_path (不存在)"
            ((missing_count++))
        fi
    done
    
    if [ $missing_count -gt 0 ]; then
        print_warning "$missing_count 个磁场文件不存在"
        print_warning "请先下载磁场数据:"
        echo "    cd $FIELD_MAP_DIR"
        echo "    python3 downloadFormSamurai.py -d -u"
        return 1
    fi
    
    print_success "所有前置条件满足"
    echo ""
    return 0
}

create_output_directories() {
    print_info "创建输出目录: $OUTPUT_DIR"
    
    mkdir -p "$OUTPUT_DIR"
    mkdir -p "$OUTPUT_DIR/logs"
    mkdir -p "$OUTPUT_DIR/results"
    mkdir -p "$OUTPUT_DIR/plots"
    
    print_success "输出目录已创建"
}

run_single_analysis() {
    local field_name=$1
    local field_file=$2
    local field_strength=$3
    
    echo ""
    echo "======================================================================"
    echo "分析磁场配置: $field_name ($field_strength T)"
    echo "磁场文件: $FIELD_MAP_DIR/$field_file"
    echo "偏转角度: $DEFLECTION_ANGLES"
    echo "======================================================================"
    echo ""
    
    local exe_path="$BUILD_DIR/bin/RunGeoAcceptanceAnalysis"
    local result_dir="$OUTPUT_DIR/results/$field_name"
    local log_file="$OUTPUT_DIR/logs/${field_name}.log"
    
    mkdir -p "$result_dir"
    
    # 构建命令
    local cmd="$exe_path"
    cmd="$cmd --fieldmap $FIELD_MAP_DIR/$field_file"
    cmd="$cmd --field $field_strength"
    cmd="$cmd --qmd $QMD_DATA_DIR"
    cmd="$cmd --output $result_dir"
    
    # 添加角度参数
    for angle in $DEFLECTION_ANGLES; do
        cmd="$cmd --angle $angle"
    done
    
    print_info "运行命令: $cmd"
    print_info "日志文件: $log_file"
    echo ""
    
    # 运行并记录日志
    {
        echo "# 几何接受度分析日志"
        echo "# 磁场: $field_name ($field_strength T)"
        echo "# 时间: $(date)"
        echo "# 命令: $cmd"
        echo ""
        $cmd 2>&1
    } | tee "$log_file"
    
    local exit_code=${PIPESTATUS[0]}
    
    if [ $exit_code -eq 0 ]; then
        print_success "分析成功: $field_name"
        print_success "结果保存在: $result_dir"
        return 0
    else
        print_error "分析失败: $field_name (退出码: $exit_code)"
        return 1
    fi
}

generate_summary_report() {
    print_info "生成汇总报告..."
    
    local summary_file="$OUTPUT_DIR/SUMMARY_REPORT.txt"
    
    {
        echo "======================================================================"
        echo "                   几何接受度分析汇总报告"
        echo "              Geometry Acceptance Summary Report"
        echo "======================================================================"
        echo ""
        echo "分析时间: $(date '+%Y-%m-%d %H:%M:%S')"
        echo "项目目录: $PROJECT_ROOT"
        echo "输出目录: $OUTPUT_DIR"
        echo ""
        echo "分析配置:"
        echo "----------------------------------------------------------------------"
        echo "磁场配置: ${#FIELD_NAMES[@]} 个"
        for i in "${!FIELD_NAMES[@]}"; do
            echo "  - ${FIELD_NAMES[$i]}: ${FIELD_STRENGTHS[$i]} T"
        done
        echo ""
        echo "偏转角度: $DEFLECTION_ANGLES°"
        echo ""
        echo "结果文件:"
        echo "----------------------------------------------------------------------"
        
        for field_name in "${FIELD_NAMES[@]}"; do
            echo ""
            echo "$field_name:"
            
            local report_file="$OUTPUT_DIR/results/$field_name/acceptance_report.txt"
            local root_file="$OUTPUT_DIR/results/$field_name/acceptance_results.root"
            
            if [ -f "$report_file" ]; then
                echo "  ✓ 文本报告: $report_file"
            else
                echo "  ✗ 文本报告: 不存在"
            fi
            
            if [ -f "$root_file" ]; then
                echo "  ✓ ROOT文件: $root_file"
            else
                echo "  ✗ ROOT文件: 不存在"
            fi
        done
        
        echo ""
        echo "======================================================================"
        echo "分析完成!"
        echo "======================================================================"
    } | tee "$summary_file"
    
    print_success "汇总报告已生成: $summary_file"
}

create_plot_script() {
    print_info "创建绘图脚本..."
    
    local script_file="$OUTPUT_DIR/plot_results.py"
    
    cat > "$script_file" << 'EOF'
#!/usr/bin/env python3
"""绘制几何接受度分析结果"""

import uproot
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

def plot_acceptance_comparison():
    """比较不同磁场的接受度"""
    
    fields = ['0.8T', '1.0T', '1.2T', '1.4T']
    colors = ['blue', 'green', 'red', 'orange']
    
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    axes = axes.flatten()
    
    for i, (field, color) in enumerate(zip(fields, colors)):
        root_file = f"results/{field}/acceptance_results.root"
        
        if not Path(root_file).exists():
            print(f"警告: 文件不存在 {root_file}")
            continue
        
        # 读取ROOT文件
        file = uproot.open(root_file)
        tree = file["acceptance"]
        
        data = tree.arrays(['deflectionAngle', 'pdcAcceptance', 
                           'nebulaAcceptance', 'coincidenceAcceptance'], 
                          library="np")
        
        # 绘制PDC接受度
        axes[i].plot(data['deflectionAngle'], data['pdcAcceptance'], 
                    'o-', color=color, label='PDC', linewidth=2, markersize=8)
        
        # 绘制NEBULA接受度
        axes[i].plot(data['deflectionAngle'], data['nebulaAcceptance'], 
                    's--', color='purple', label='NEBULA', linewidth=2, markersize=8)
        
        # 绘制符合接受度
        axes[i].plot(data['deflectionAngle'], data['coincidenceAcceptance'], 
                    '^:', color='brown', label='Coincidence', linewidth=2, markersize=8)
        
        axes[i].set_xlabel('Deflection Angle (deg)', fontsize=12)
        axes[i].set_ylabel('Acceptance (%)', fontsize=12)
        axes[i].set_title(f'{field} Magnetic Field', fontsize=14, fontweight='bold')
        axes[i].legend(fontsize=10)
        axes[i].grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig('plots/acceptance_comparison.png', dpi=150, bbox_inches='tight')
    print("✓ 图表已保存: plots/acceptance_comparison.png")
    plt.show()

if __name__ == '__main__':
    plot_acceptance_comparison()
EOF
    
    chmod +x "$script_file"
    print_success "绘图脚本已创建: $script_file"
    print_info "运行方法: cd $OUTPUT_DIR && python3 plot_results.py"
}

###############################################################################
# 主程序
###############################################################################

main() {
    # 打印横幅
    print_banner
    
    # 检查前置条件
    if ! check_prerequisites; then
        print_error "前置条件不满足，退出"
        exit 1
    fi
    
    # 创建输出目录
    create_output_directories
    
    # 运行批量分析
    local success_count=0
    local total_count=${#FIELD_NAMES[@]}
    
    print_info "开始批量分析..."
    print_info "将分析 $total_count 个磁场配置"
    echo ""
    
    for i in "${!FIELD_NAMES[@]}"; do
        echo ""
        print_info "[$((i+1))/$total_count] 分析 ${FIELD_NAMES[$i]}..."
        
        if run_single_analysis "${FIELD_NAMES[$i]}" "${FIELD_FILES[$i]}" "${FIELD_STRENGTHS[$i]}"; then
            ((success_count++))
        fi
    done
    
    # 生成汇总报告
    echo ""
    generate_summary_report
    
    # 创建绘图脚本
    echo ""
    create_plot_script
    
    # 最终总结
    echo ""
    echo "======================================================================"
    echo "批量分析完成!"
    echo "成功: $success_count/$total_count"
    echo "输出目录: $OUTPUT_DIR"
    echo "======================================================================"
    echo ""
    
    if [ $success_count -eq $total_count ]; then
        exit 0
    else
        exit 1
    fi
}

# 运行主程序
main "$@"
