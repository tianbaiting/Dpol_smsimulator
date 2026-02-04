#!/bin/bash
# =========================================================================
# [EN] Batch run deuteron beam simulation with detailed fields (VRML)
# [CN] 使用细分磁场批量运行氘束模拟（VRML输出）
# =========================================================================

SMSIMDIR="${SMSIMDIR:-/home/tian/workspace/dpol/smsimulator5.5}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTDIR="${SCRIPT_DIR}/deuteron_output"
SIM_EXEC="${SMSIMDIR}/build/bin/sim_deuteron"

FIELDS="100,105,110,115,120"
TIMEOUT_SEC=300

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -f, --fields \"100,105\"   Comma-separated field strengths (default: all)"
    echo "  -a, --angles \"0.0,1.0\"   (unused) kept for compatibility"
    echo "  -t, --timeout SEC         Timeout per run in seconds (default: 300)"
    echo "  -h, --help                Show this help"
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -f|--fields) FIELDS="$2"; shift 2 ;;
    -a|--angles) shift 2 ;;
        -t|--timeout) TIMEOUT_SEC="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1"; usage; exit 1 ;;
    esac
done

if [[ ! -x "$SIM_EXEC" ]]; then
    echo "[ERROR] Simulator not found: $SIM_EXEC"
    exit 1
fi

mkdir -p "${OUTDIR}"

IFS=',' read -ra FIELD_ARR <<< "$FIELDS"

for field in "${FIELD_ARR[@]}"; do
    macfile="${SCRIPT_DIR}/run_deuteron_B${field}T.mac"
    base="deuteron_B${field}T_380MeV"

    if [[ ! -f "$macfile" ]]; then
        echo "[SKIP] Macro not found: $macfile"
        continue
    fi

    tmpfile=$(mktemp --suffix=.mac)
    python3 - << PY
import io
src_path = r"""$macfile"""
dst_path = r"""$tmpfile"""
with open(src_path, "r", encoding="utf-8", errors="ignore") as f:
    data = f.read()
data = data.replace("/vis/open OGLIQt 1280x720", "/vis/open VRML2FILE")
lines = [line for line in data.splitlines() if "/vis/viewer/save" not in line]
with open(dst_path, "w", encoding="utf-8") as f:
    f.write("\\n".join(lines) + "\\n")
PY
    echo "/vis/viewer/flush" >> "$tmpfile"
    echo "exit" >> "$tmpfile"

    echo "[RUN] B=${field}T (fixed deuteron)"
    timeout ${TIMEOUT_SEC} "${SIM_EXEC}" "$tmpfile" 2>&1 | grep -E "(Error|error|beamOn|WARNING|VRML)" | head -15

    wrlfile=$(ls -t g4_*.wrl 2>/dev/null | head -1)
    if [[ -n "$wrlfile" && -f "$wrlfile" ]]; then
        mv "$wrlfile" "${OUTDIR}/${base}.wrl"
        echo "[OK] ${OUTDIR}/${base}.wrl"
    else
        echo "[WARN] No VRML file generated for B=${field}T"
    fi

    rm -f "$tmpfile"
    rm -f g4_*.wrl 2>/dev/null
done
