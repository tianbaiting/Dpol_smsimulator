#!/bin/bash
# =========================================================================
# [EN] Batch run proton trajectory visualization with single ROOT tree
# [CN] 批量运行质子轨迹可视化（单个ROOT树）
# =========================================================================

SMSIMDIR="${SMSIMDIR:-/home/tian/workspace/dpol/smsimulator5.5}"
VIZDIR="${SMSIMDIR}/configs/simulation/DbeamTest/track_vis_useTree"
OUTDIR="${VIZDIR}/output"
SIM_EXEC="${SMSIMDIR}/build/bin/sim_deuteron"

# Default: all fields and angles
FIELDS="080,100,120,160,200"
ANGLES="2.0,4.0,6.0,8.0,10.0"
FORMAT="vrml"  # vrml, png, or both

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -f, --fields \"080,100\"    Comma-separated field strengths (default: all)"
    echo "  -a, --angles \"4.0,6.0\"    Comma-separated beam angles (default: all)"
    echo "  -o, --format FORMAT        Output format: vrml, png, or both (default: vrml)"
    echo "      --vrml                 Shortcut for --format vrml"
    echo "      --png                  Shortcut for --format png"
    echo "      --both                 Shortcut for --format both"
    echo "  -h, --help                 Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 --vrml                           # Generate VRML for all configs"
    echo "  $0 --png --fields 080,100           # Generate PNG for B=0.8T and 1.0T"
    echo "  $0 --both --angles 4.0,6.0          # Generate both formats for 4° and 6°"
    echo "  $0 -f 080 -a 4.0 -o png             # Single config, PNG output"
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -f|--fields) FIELDS="$2"; shift 2 ;;
        -a|--angles) ANGLES="$2"; shift 2 ;;
        -o|--format) FORMAT="$2"; shift 2 ;;
        --vrml) FORMAT="vrml"; shift ;;
        --png) FORMAT="png"; shift ;;
        --both) FORMAT="both"; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1"; usage; exit 1 ;;
    esac
done

# [EN] Validate format option / [CN] 验证格式选项
if [[ "$FORMAT" != "vrml" && "$FORMAT" != "png" && "$FORMAT" != "both" ]]; then
    echo "[ERROR] Invalid format: $FORMAT (must be vrml, png, or both)"
    exit 1
fi

echo "=========================================="
echo "[CONFIG] Fields: $FIELDS"
echo "[CONFIG] Angles: $ANGLES"
echo "[CONFIG] Format: $FORMAT"
echo "=========================================="

# Check ROOT tree exists
ROOTFILE="${VIZDIR}/rootfiles/protons_4tracks.root"
if [[ ! -f "$ROOTFILE" ]]; then
    echo "[ERROR] ROOT tree not found: $ROOTFILE"
    echo "Run ./setup.sh first"
    exit 1
fi

mkdir -p "${OUTDIR}"
cd "${VIZDIR}"

IFS=',' read -ra FIELD_ARR <<< "$FIELDS"
IFS=',' read -ra ANGLE_ARR <<< "$ANGLES"

for field in "${FIELD_ARR[@]}"; do
    for angle in "${ANGLE_ARR[@]}"; do
        macfile="run_protons_B${field}T_${angle}deg.mac"
        base="protons_B${field}T_${angle}deg"
        
        if [[ ! -f "$macfile" ]]; then
            echo "[SKIP] $macfile not found - run generate_all_macros_v2.sh"
            continue
        fi
        
        echo "========================================"
        echo "[RUN] B=${field}T, angle=${angle}deg"
        echo "========================================"
        
        # ==========================================
        # [EN] Generate VRML if requested
        # [CN] 如果需要则生成 VRML
        # ==========================================
        if [[ "$FORMAT" == "vrml" || "$FORMAT" == "both" ]]; then
            tmpfile=$(mktemp --suffix=.mac)
            # [EN] Replace OpenGL with VRML2FILE driver / [CN] 将 OpenGL 替换为 VRML2FILE 驱动
            sed 's|/vis/open OGLIQt.*|/vis/open VRML2FILE|g' "$macfile" > "$tmpfile"
            # [EN] Remove /vis/viewer/save (not supported in VRML) / [CN] 移除 /vis/viewer/save（VRML 不支持）
            sed -i '/\/vis\/viewer\/save/d' "$tmpfile"
            echo "/vis/viewer/flush" >> "$tmpfile"
            echo "exit" >> "$tmpfile"
            
            echo "[VRML] Generating ${base}.wrl ..."
            timeout 180 "${SIM_EXEC}" "$tmpfile" 2>&1 | grep -E "(Error|error|beamOn|tree|WARNING)" | head -10
            
            # [EN] Find and move VRML file / [CN] 查找并移动 VRML 文件
            wrlfile=$(ls -t g4_*.wrl 2>/dev/null | head -1)
            if [[ -n "$wrlfile" && -f "$wrlfile" ]]; then
                mv "$wrlfile" "${OUTDIR}/${base}.wrl"
                echo "[OK] VRML: ${OUTDIR}/${base}.wrl"
            else
                echo "[WARN] No VRML file generated"
            fi
            rm -f "$tmpfile"
            rm -f g4_*.wrl 2>/dev/null
        fi
        
        # ==========================================
        # [EN] Generate PNG if requested
        # [CN] 如果需要则生成 PNG
        # ==========================================
        if [[ "$FORMAT" == "png" || "$FORMAT" == "both" ]]; then
            tmpfile=$(mktemp --suffix=.mac)
            cp "$macfile" "$tmpfile"
            # [EN] Replace /vis/viewer/save with /vis/ogl/export / [CN] 将 /vis/viewer/save 替换为 /vis/ogl/export
            sed -i "s|/vis/viewer/save.*|/vis/ogl/export ${OUTDIR}/${base}.png|g" "$tmpfile"
            echo "exit" >> "$tmpfile"
            
            echo "[PNG] Generating ${base}.png ..."
            timeout 180 "${SIM_EXEC}" "$tmpfile" 2>&1 | grep -E "(Error|error|beamOn|export)" | head -10
            
            if [[ -f "${OUTDIR}/${base}.png" ]]; then
                echo "[OK] PNG: ${OUTDIR}/${base}.png"
            else
                echo "[WARN] PNG export requires X display"
            fi
            rm -f "$tmpfile"
        fi
    done
done

echo ""
echo "========================================"
echo "Batch complete. Output: ${OUTDIR}/"
echo "Format: $FORMAT"
ls -la "${OUTDIR}/" 2>/dev/null | tail -20
