#!/bin/bash
# =========================================================================
# [EN] Combine deuteron and proton trajectory WRL files
# [CN] 合并氘核和质子轨迹 WRL 文件
# [EN] Deuteron trajectories will be colored magenta (1 0 1)
# [CN] 氘核轨迹将被着色为洋红色 (1 0 1)
# =========================================================================

SMSIMDIR="${SMSIMDIR:-/home/tian/workspace/dpol/smsimulator5.5}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTDIR="${SCRIPT_DIR}/output"

# [EN] Source directories / [CN] 源目录
DEUTERON_DIR="${SMSIMDIR}/configs/simulation/DbeamTest/nopdc/output"
PROTON_DIR="${SMSIMDIR}/configs/simulation/DbeamTest/track_vis_useTree/output"

# [EN] Default configurations / [CN] 默认配置
FIELDS="080,100,120,160,200"
ANGLES="2.0,4.0,6.0,8.0,10.0"  # [EN] All proton angles / [CN] 所有质子角度

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -f, --fields \"080,100,120\"   Comma-separated field strengths (default: 080,100,120,160,200)"
    echo "  -a, --angles \"2.0,4.0,6.0\"   Comma-separated proton angles (default: all 2.0-10.0)"
    echo "  -l, --list                   List available files"
    echo "  -h, --help                   Show this help"
    echo ""
    echo "Examples:"
    echo "  $0                           # Combine all fields and all angles"
    echo "  $0 -f 080,100 -a 4.0,6.0     # Combine 0.8T, 1.0T with 4.0 and 6.0deg protons"
    echo "  $0 -l                        # List available input files"
}

list_files() {
    echo "========================================"
    echo "[DEUTERON FILES] in ${DEUTERON_DIR}/"
    echo "========================================"
    ls -1 "${DEUTERON_DIR}/"*.wrl 2>/dev/null | xargs -I{} basename {} || echo "(none)"
    
    echo ""
    echo "========================================"
    echo "[PROTON FILES] in ${PROTON_DIR}/"
    echo "========================================"
    ls -1 "${PROTON_DIR}/"*.wrl 2>/dev/null | xargs -I{} basename {} || echo "(none)"
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -f|--fields) FIELDS="$2"; shift 2 ;;
        -a|--angles) ANGLES="$2"; shift 2 ;;
        -l|--list) list_files; exit 0 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1"; usage; exit 1 ;;
    esac
done

mkdir -p "${OUTDIR}"

echo "=========================================="
echo "[CONFIG] Fields: $FIELDS"
echo "[CONFIG] Angles: ${ANGLES}deg"
echo "[CONFIG] Output: ${OUTDIR}/"
echo "=========================================="

# =========================================================================
# [EN] Function: Extract POLYLINE sections from WRL file and change color
# [CN] 函数：从 WRL 文件中提取 POLYLINE 部分并修改颜色
# =========================================================================
extract_polylines_colored() {
    local wrlfile="$1"
    local color="$2"
    
    # [EN] Extract POLYLINE sections and replace color / [CN] 提取 POLYLINE 并替换颜色
    awk -v newcolor="$color" '
    BEGIN { in_polyline = 0 }
    /^#---------- POLYLINE/ { in_polyline = 1 }
    in_polyline {
        if (newcolor != "") {
            gsub(/diffuseColor [0-9.]+ [0-9.]+ [0-9.]+/, "diffuseColor " newcolor)
            gsub(/emissiveColor [0-9.]+ [0-9.]+ [0-9.]+/, "emissiveColor " newcolor)
        }
        print
        if (/^}$/) in_polyline = 0
    }
    ' "$wrlfile"
}

IFS=',' read -ra FIELD_ARR <<< "$FIELDS"
IFS=',' read -ra ANGLE_ARR <<< "$ANGLES"

for field in "${FIELD_ARR[@]}"; do
    for angle in "${ANGLE_ARR[@]}"; do
        deuteron_file="${DEUTERON_DIR}/deuteron_B${field}T_380MeV.wrl"
        proton_file="${PROTON_DIR}/protons_B${field}T_${angle}deg.wrl"
        output_file="${OUTDIR}/combined_B${field}T_${angle}deg.wrl"
        
        # [EN] Check input files / [CN] 检查输入文件
        if [[ ! -f "$deuteron_file" ]]; then
            echo "[SKIP] Deuteron not found: B${field}T"
            continue
        fi
        
        if [[ ! -f "$proton_file" ]]; then
            echo "[SKIP] Proton not found: B${field}T_${angle}deg"
            continue
        fi
        
        echo "[COMBINE] B=${field}T @ ${angle}deg"
        
        # =========================================================================
        # [EN] Simple merge: proton file + deuteron POLYLINE (magenta)
        # [CN] 简单合并：质子文件 + 氘核POLYLINE（洋红色）
        # =========================================================================
        {
            cat "$proton_file"
            echo ""
            echo "#=========================================="
            echo "# DEUTERON TRAJECTORIES (Magenta 1 0 1)"
            echo "#=========================================="
            extract_polylines_colored "$deuteron_file" "1 0 1"
        } > "$output_file"
        
        echo "  -> $(basename "$output_file") ($(du -h "$output_file" | cut -f1))"
    done
done

echo ""
echo "[DONE] Generated $(ls "${OUTDIR}/"*.wrl 2>/dev/null | wc -l) files in ${OUTDIR}/"
