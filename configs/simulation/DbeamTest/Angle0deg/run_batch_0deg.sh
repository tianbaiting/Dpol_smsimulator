#!/bin/bash
# =========================================================================
# [EN] Batch run proton+neutron trajectory visualization at 0-degree target
# [CN] 批量运行0度靶点的质子+中子轨迹可视化
# =========================================================================

SMSIMDIR="${SMSIMDIR:-/home/tian/workspace/dpol/smsimulator5.5}"
VIZDIR="${SMSIMDIR}/configs/simulation/DbeamTest/Angle0deg"
OUTDIR="${VIZDIR}/output"
SIM_EXEC="${SMSIMDIR}/build/bin/sim_deuteron"

# [EN] Default: all fields / [CN] 默认：所有磁场
FIELDS="080,100,120,140,160,180,200"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -f, --fields \"080,100\"    Comma-separated field strengths (default: all)"
    echo "  -h, --help                 Show this help"
    echo ""
    echo "Examples:"
    echo "  $0                         # Generate VRML for all field configs"
    echo "  $0 -f 080,100,120          # Generate VRML for B=0.8T, 1.0T, 1.2T"
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -f|--fields) FIELDS="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1"; usage; exit 1 ;;
    esac
done

echo "=========================================="
echo "[CONFIG] Fields: $FIELDS"
echo "[CONFIG] Target: (0, 0, -4m), Angle: 0 deg"
echo "[CONFIG] Tracks: 4 protons + 4 neutrons"
echo "=========================================="

# [EN] Check ROOT tree exists / [CN] 检查ROOT树是否存在
ROOTFILE="${VIZDIR}/rootfiles/pn_8tracks_0deg.root"
if [[ ! -f "$ROOTFILE" ]]; then
    echo "[ERROR] ROOT tree not found: $ROOTFILE"
    echo "Run: root -l -b -q 'GenPNTree_0deg.C(\"rootfiles/pn_8tracks_0deg.root\")'"
    exit 1
fi

mkdir -p "${OUTDIR}"
cd "${VIZDIR}"

IFS=',' read -ra FIELD_ARR <<< "$FIELDS"

for field in "${FIELD_ARR[@]}"; do
    macfile="run_pn_B${field}T_0deg.mac"
    base="pn_B${field}T_0deg"
    
    if [[ ! -f "$macfile" ]]; then
        echo "[SKIP] $macfile not found - run generate_macros_0deg.sh first"
        continue
    fi
    
    echo "========================================"
    echo "[RUN] B=${field}T @ 0deg"
    echo "========================================"
    
    echo "[VRML] Generating ${base}.wrl ..."
    timeout 180 "${SIM_EXEC}" "$macfile" 2>&1 | grep -E "(Error|error|beamOn|tree|WARNING)" | head -10
    
    # [EN] Find and move VRML file / [CN] 查找并移动 VRML 文件
    wrlfile=$(ls -t g4_*.wrl 2>/dev/null | head -1)
    if [[ -n "$wrlfile" && -f "$wrlfile" ]]; then
        mv "$wrlfile" "${OUTDIR}/${base}.wrl"
        echo "[OK] VRML: ${OUTDIR}/${base}.wrl"
    else
        echo "[WARN] No VRML file generated"
    fi
    rm -f g4_*.wrl 2>/dev/null
done

echo ""
echo "[DONE] Generated $(ls "${OUTDIR}/"*.wrl 2>/dev/null | wc -l) VRML files in ${OUTDIR}/"
