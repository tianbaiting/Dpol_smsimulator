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
FORMAT="vrml"  # vrml or png

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo "Options:"
    echo "  --fields \"080,100\"    Comma-separated field strengths"
    echo "  --angles \"4.0,6.0\"    Comma-separated beam angles"
    echo "  --format vrml|png      Output format (default: vrml)"
    echo "  --help                 Show this help"
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
        
        tmpfile=$(mktemp --suffix=.mac)
        
        if [[ "$FORMAT" == "vrml" ]]; then
            # VRML export (headless)
            sed 's|/vis/open OGLIQt.*|/vis/open VRML2FILE|g' "$macfile" > "$tmpfile"
            # Remove /vis/viewer/save (not supported in VRML)
            sed -i '/\/vis\/viewer\/save/d' "$tmpfile"
            echo "/vis/viewer/flush" >> "$tmpfile"
            echo "exit" >> "$tmpfile"
            
            echo "[VRML] Generating ${base}.wrl ..."
            timeout 180 "${SIM_EXEC}" "$tmpfile" 2>&1 | grep -E "(Error|error|beamOn|tree|WARNING)" | head -10
            
            # Find and move VRML file
            wrlfile=$(ls -t g4_*.wrl 2>/dev/null | head -1)
            if [[ -n "$wrlfile" && -f "$wrlfile" ]]; then
                mv "$wrlfile" "${OUTDIR}/${base}.wrl"
                echo "[OK] VRML: ${OUTDIR}/${base}.wrl"
            else
                echo "[WARN] No VRML file generated"
            fi
        else
            # PNG export (requires X display)
            cp "$macfile" "$tmpfile"
            # Replace /vis/viewer/save with /vis/ogl/export
            sed -i "s|/vis/viewer/save.*|/vis/ogl/export ${OUTDIR}/${base}.png|g" "$tmpfile"
            echo "exit" >> "$tmpfile"
            
            echo "[PNG] Generating ${base}.png ..."
            timeout 180 "${SIM_EXEC}" "$tmpfile" 2>&1 | grep -E "(Error|error|beamOn|export)" | head -10
            
            if [[ -f "${OUTDIR}/${base}.png" ]]; then
                echo "[OK] PNG: ${OUTDIR}/${base}.png"
            else
                echo "[WARN] PNG export requires X display"
            fi
        fi
        
        rm -f "$tmpfile"
        # Clean up any stray VRML files
        rm -f g4_*.wrl 2>/dev/null
    done
done

echo ""
echo "========================================"
echo "Batch complete. Output: ${OUTDIR}/"
ls -la "${OUTDIR}/" 2>/dev/null | tail -20
