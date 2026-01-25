#!/bin/bash
# =========================================================================
# [EN] Batch run proton trajectory visualization with Tree input
# [CN] 批量运行基于Tree输入的质子轨迹可视化
# =========================================================================

SMSIMDIR="${SMSIMDIR:-/home/tian/workspace/dpol/smsimulator5.5}"
VIZDIR="${SMSIMDIR}/configs/simulation/DbeamTest/track_vis_useTree"
OUTDIR="${VIZDIR}/output"
SIM_EXEC="${SMSIMDIR}/build/bin/sim_deuteron"

# Default: all fields and angles
FIELDS="080,100,120,160,200"
ANGLES="2.0,4.0,6.0,8.0,10.0"
FORMAT="vrml"  # vrml or png

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo "Options:"
    echo "  --fields \"080,100\"    Comma-separated field strengths"
    echo "  --angles \"4.0,6.0\"    Comma-separated beam angles"
    echo "  --format vrml|png      Output format (default: vrml)"
    echo "  --help                 Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 --fields \"100\" --angles \"6.0\""
    echo "  $0 --format vrml"
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --fields) FIELDS="$2"; shift 2 ;;
        --angles) ANGLES="$2"; shift 2 ;;
        --format) FORMAT="$2"; shift 2 ;;
        --help) usage; exit 0 ;;
        *) echo "Unknown option: $1"; usage; exit 1 ;;
    esac
done

mkdir -p "${OUTDIR}"
cd "${VIZDIR}"

IFS=',' read -ra FIELD_ARR <<< "$FIELDS"
IFS=',' read -ra ANGLE_ARR <<< "$ANGLES"

for field in "${FIELD_ARR[@]}"; do
    for angle in "${ANGLE_ARR[@]}"; do
        macfile="run_protons_B${field}T_${angle}deg.mac"
        base="protons_B${field}T_${angle}deg"
        rootfile="rootfiles/protons_B${field}T_${angle}deg.root"
        
        if [[ ! -f "$macfile" ]]; then
            echo "[SKIP] $macfile not found"
            continue
        fi
        
        if [[ ! -f "$rootfile" ]]; then
            echo "[SKIP] $rootfile not found - run generate_all_trees.sh first"
            continue
        fi
        
        echo "========================================"
        echo "[RUN] B=${field}T, angle=${angle}deg"
        echo "========================================"
        
        tmpfile=$(mktemp)
        
        if [[ "$FORMAT" == "vrml" ]]; then
            # VRML export (headless)
            sed 's|/vis/open OGLIQt.*|/vis/open VRML2FILE|g' "$macfile" > "$tmpfile"
            echo "/vis/viewer/flush" >> "$tmpfile"
            echo "exit" >> "$tmpfile"
            
            echo "[VRML] Generating ${base}.wrl ..."
            timeout 180 "${SIM_EXEC}" "$tmpfile" 2>&1 | grep -E "(Error|error|beamOn|tree)" | head -10
            
            if [[ -f "g4_00.wrl" ]]; then
                mv "g4_00.wrl" "${OUTDIR}/${base}.wrl"
                echo "[OK] VRML: ${OUTDIR}/${base}.wrl"
            fi
        else
            # PNG export (requires display)
            cp "$macfile" "$tmpfile"
            echo "/vis/ogl/export ${OUTDIR}/${base}.png" >> "$tmpfile"
            echo "exit" >> "$tmpfile"
            
            echo "[PNG] Generating ${base}.png ..."
            timeout 180 "${SIM_EXEC}" "$tmpfile" 2>&1 | grep -E "(Error|error|beamOn|export)" | head -10
            
            if [[ -f "${OUTDIR}/${base}.png" ]]; then
                echo "[OK] PNG: ${OUTDIR}/${base}.png"
            fi
        fi
        
        rm -f "$tmpfile"
    done
done

echo ""
echo "========================================"
echo "Export complete. Output: ${OUTDIR}/"
ls -la "${OUTDIR}/" 2>/dev/null | tail -20
