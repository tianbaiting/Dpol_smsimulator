#!/bin/bash
# =========================================================================
# [EN] Combine deuteron and proton+neutron trajectory WRL files
# [CN] 合并氘核与质子+中子轨迹 WRL 文件
# =========================================================================

SMSIMDIR="${SMSIMDIR:-/home/tian/workspace/dpol/smsimulator5.5}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTDIR="${SCRIPT_DIR}/combined_output"

DEUTERON_DIR="${SCRIPT_DIR}/deuteron_output"
PN_DIR="${SCRIPT_DIR}/output"

FIELDS="100,105,110,115,120"
ANGLES="0.0,1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -f, --fields \"100,105\"   Comma-separated field strengths (default: all)"
    echo "  -a, --angles \"0.0,1.0\"   Comma-separated angles (default: 0.0-10.0)"
    echo "  -l, --list                List available files"
    echo "  -h, --help                Show this help"
}

list_files() {
    echo "[DEUTERON FILES] in ${DEUTERON_DIR}/"
    ls -1 "${DEUTERON_DIR}/"*.wrl 2>/dev/null | xargs -I{} basename {} || echo "(none)"
    echo ""
    echo "[PN FILES] in ${PN_DIR}/"
    ls -1 "${PN_DIR}/"*.wrl 2>/dev/null | xargs -I{} basename {} || echo "(none)"
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

extract_polylines_colored() {
    local wrlfile="$1"
    local color="$2"

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
        pn_file="${PN_DIR}/protons_B${field}T_${angle}deg.wrl"
        output_file="${OUTDIR}/combined_B${field}T_${angle}deg.wrl"

        if [[ ! -f "$deuteron_file" ]]; then
            echo "[SKIP] Deuteron not found: B${field}T"
            continue
        fi

        if [[ ! -f "$pn_file" ]]; then
            echo "[SKIP] PN not found: B${field}T_${angle}deg"
            continue
        fi

        echo "[COMBINE] B=${field}T @ ${angle}deg"

        {
            cat "$pn_file"
            echo ""
            echo "#=========================================="
            echo "# DEUTERON TRAJECTORIES (Magenta 1 0 1)"
            echo "#=========================================="
            extract_polylines_colored "$deuteron_file" "1 0 1"
        } > "$output_file"

        echo "  -> $(basename "$output_file") ($(du -h "$output_file" | cut -f1))"
    done
done
