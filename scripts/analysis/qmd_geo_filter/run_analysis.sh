#!/bin/bash
# QMD 几何接受度筛选运行脚本
#
# 用法:
#   ./run_analysis.sh              # 使用默认配置
#   ./run_analysis.sh --test       # 快速测试
#   ./run_analysis.sh --full       # 完整分析
#   ./run_analysis.sh --config config.json  # 使用配置文件

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SMSIMDIR="${SMSIMDIR:-/home/tian/workspace/dpol/smsimulator5.5}"

# 设置环境变量
export SMSIMDIR
export PYTHONPATH="${SMSIMDIR}/libs:${PYTHONPATH}"

# 切换到脚本目录
cd "$SCRIPT_DIR"

# 运行分析
echo "=============================================="
echo "  QMD Geometry Filter Analysis"
echo "=============================================="
echo "SMSIMDIR: $SMSIMDIR"
echo ""

# 检查参数
if [[ "$1" == "--full" ]]; then
    echo "Running full analysis..."
    python3 run_qmd_geo_filter.py --config full_analysis_config.json
elif [[ "$1" == "--test" ]]; then
    echo "Running quick test..."
    python3 run_qmd_geo_filter.py --test
elif [[ "$1" == "--generate-config" ]]; then
    echo "Generating configuration template..."
    python3 run_qmd_geo_filter.py --generate-config "${2:-full_analysis_config.json}"
elif [[ "$1" == "--config" ]]; then
    if [[ -z "$2" ]]; then
        echo "Error: Please specify a configuration file"
        exit 1
    fi
    echo "Using configuration: $2"
    python3 run_qmd_geo_filter.py --config "$2"
elif [[ "$1" == "--list-data" ]]; then
    python3 run_qmd_geo_filter.py --list-data
elif [[ "$1" == "--help" ]] || [[ "$1" == "-h" ]]; then
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --test              Run quick test with minimal configuration"
    echo "  --full              Run full analysis with all configurations"
    echo "  --config FILE       Use specified configuration file"
    echo "  --generate-config   Generate a configuration template"
    echo "  --list-data         List available data files"
    echo "  --help, -h          Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 --test"
    echo "  $0 --config my_config.json"
    echo "  $0 --generate-config my_config.json"
else
    echo "Running with default configuration..."
    python3 run_qmd_geo_filter.py
fi

echo ""
echo "Done!"
