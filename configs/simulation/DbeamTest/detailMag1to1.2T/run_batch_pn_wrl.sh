#!/bin/bash
# =========================================================================
# [EN] Batch run proton+neutron trajectory visualization (Tree input)
# [CN] 批量运行质子+中子轨迹可视化（Tree输入）
# =========================================================================

SMSIMDIR="${SMSIMDIR:-/home/tian/workspace/dpol/smsimulator5.5}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTDIR="${SCRIPT_DIR}/output"
SIM_EXEC="${SMSIMDIR}/build/bin/sim_deuteron"

FIELDS="100,105,110,115,120"
ANGLES="0.0,1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0"
FORMAT="vrml"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -f, --fields \"100,105\"      Comma-separated field strengths (default: all)"
    echo "  -a, --angles \"0.0,1.0\"      Comma-separated beam angles (default: 0.0-10.0)"
    echo "  -o, --format FORMAT          Output format: vrml, png, or both (default: vrml)"
    echo "      --vrml                   Shortcut for --format vrml"
    echo "      --png                    Shortcut for --format png"
    echo "      --both                   Shortcut for --format both"
    echo "  -h, --help                   Show this help"
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

if [[ "$FORMAT" != "vrml" && "$FORMAT" != "png" && "$FORMAT" != "both" ]]; then
    echo "[ERROR] Invalid format: $FORMAT (must be vrml, png, or both)"
    exit 1
fi

ROOTFILE="${SMSIMDIR}/configs/simulation/DbeamTest/track_vis_useTree/rootfiles/pn_8tracks.root"
if [[ ! -f "$ROOTFILE" ]]; then
    echo "[ERROR] ROOT tree not found: $ROOTFILE"
    exit 1
fi

mkdir -p "${OUTDIR}"
cd "${SCRIPT_DIR}"

IFS=',' read -ra FIELD_ARR <<< "$FIELDS"
IFS=',' read -ra ANGLE_ARR <<< "$ANGLES"

for field in "${FIELD_ARR[@]}"; do
    for angle in "${ANGLE_ARR[@]}"; do
        macfile="run_protons_B${field}T_${angle}deg.mac"
        base="protons_B${field}T_${angle}deg"

        if [[ ! -f "$macfile" ]]; then
            echo "[SKIP] $macfile not found - run generate_detail_macros.sh"
            continue
        fi

        if [[ "$FORMAT" == "vrml" || "$FORMAT" == "both" ]]; then
            tmpfile=$(mktemp --suffix=.mac)
            sed 's|/vis/open OGLIQt.*|/vis/open VRML2FILE|g' "$macfile" > "$tmpfile"
            sed -i '/\/vis\/viewer\/save/d' "$tmpfile"
            echo "/vis/viewer/flush" >> "$tmpfile"
            echo "exit" >> "$tmpfile"

            echo "[VRML] B=${field}T angle=${angle}deg"
            timeout 180 "${SIM_EXEC}" "$tmpfile" 2>&1 | grep -E "(Error|error|beamOn|tree|WARNING)" | head -10

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

        if [[ "$FORMAT" == "png" || "$FORMAT" == "both" ]]; then
            tmpfile=$(mktemp --suffix=.mac)
            cp "$macfile" "$tmpfile"
            sed -i "s|/vis/ogl/export .*|/vis/ogl/export ${OUTDIR}/${base}.png|g" "$tmpfile"
            echo "exit" >> "$tmpfile"

            echo "[PNG] B=${field}T angle=${angle}deg"
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

