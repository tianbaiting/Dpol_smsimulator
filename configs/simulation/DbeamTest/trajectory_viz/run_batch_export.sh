#!/bin/bash
# [EN] Batch export trajectory visualizations (VRML + PNG)
# [CN] 批量导出轨迹可视化 (VRML + PNG)

VIZDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTDIR="${VIZDIR}/output"
SMSIMDIR="${SMSIMDIR:-/home/tian/workspace/dpol/smsimulator5.5}"
SIM_EXEC="${SMSIMDIR}/build/bin/sim_deuteron"

# Default: all fields and angles
FIELDS="080,100,120,160,200"
ANGLES="2.0,4.0,6.0,8.0,10.0"
FORMAT="vrml"  # vrml, png, or both

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo "Options:"
    echo "  --fields \"080,100\"    Comma-separated field strengths"
    echo "  --angles \"4.0,6.0\"    Comma-separated beam angles"
    echo "  --format vrml|png|both  Output format (default: vrml)"
    echo "  --help                  Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 --fields \"100\" --angles \"6.0\" --format both"
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
        macfile="run_trajectory_B${field}T_${angle}deg.mac"
        base="trajectory_B${field}T_${angle}deg"
        
        if [[ ! -f "$macfile" ]]; then
            echo "[SKIP] $macfile not found"
            continue
        fi
        
        echo "========================================"
        echo "[RUN] B=${field}T, angle=${angle}deg"
        echo "========================================"
        
        # Create temporary macro for batch export
        tmpfile=$(mktemp)
        
        if [[ "$FORMAT" == "vrml" ]] || [[ "$FORMAT" == "both" ]]; then
            # VRML export version (headless)
            sed 's|/vis/open OGLIQt.*|/vis/open VRML2FILE|g' "$macfile" > "$tmpfile"
            echo "" >> "$tmpfile"
            echo "/vis/viewer/flush" >> "$tmpfile"
            echo "exit" >> "$tmpfile"
            
            echo "[VRML] Generating ${base}.wrl ..."
            timeout 120 "${SIM_EXEC}" "$tmpfile" 2>&1 | grep -E "(Error|error|WARNING|beamOn)" | head -10
            
            if [[ -f "g4_00.wrl" ]]; then
                mv "g4_00.wrl" "${OUTDIR}/${base}.wrl"
                echo "[OK] VRML: ${OUTDIR}/${base}.wrl"
            fi
        fi
        
        if [[ "$FORMAT" == "png" ]] || [[ "$FORMAT" == "both" ]]; then
            # PNG export (requires display)
            cp "$macfile" "$tmpfile"
            echo "" >> "$tmpfile"
            echo "/vis/ogl/export ${OUTDIR}/${base}.png" >> "$tmpfile"
            echo "exit" >> "$tmpfile"
            
            echo "[PNG] Generating ${base}.png ..."
            timeout 120 "${SIM_EXEC}" "$tmpfile" 2>&1 | grep -E "(Error|error|WARNING|beamOn|export)" | head -10
            
            if [[ -f "${OUTDIR}/${base}.png" ]]; then
                echo "[OK] PNG: ${OUTDIR}/${base}.png"
            else
                echo "[WARN] PNG export may require X display"
            fi
        fi
        
        rm -f "$tmpfile"
    done
done

echo ""
echo "========================================"
echo "Export complete. Output: ${OUTDIR}/"
ls -la "${OUTDIR}/" 2>/dev/null | tail -20
