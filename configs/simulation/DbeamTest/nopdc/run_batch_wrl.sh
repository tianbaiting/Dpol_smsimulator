#!/bin/bash
# =========================================================================
# [EN] Batch run deuteron beam simulation with multiple magnetic fields
# [CN] 批量运行不同磁场强度下的氘束模拟
# [EN] Output: VRML (.wrl) trajectory visualization files
# [CN] 输出: VRML (.wrl) 轨迹可视化文件
# =========================================================================

SMSIMDIR="${SMSIMDIR:-/home/tian/workspace/dpol/smsimulator5.5}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTDIR="${SCRIPT_DIR}/output"
SIM_EXEC="${SMSIMDIR}/build/bin/sim_deuteron"

# [EN] Default: all magnetic field configurations / [CN] 默认: 所有磁场配置
FIELDS="080,100,120,140,160,180,200"
ENERGY="380"  # MeV
TIMEOUT_SEC=300

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -f, --fields \"080,100,120\"  Comma-separated field strengths (default: all 080-200)"
    echo "  -e, --energy MeV             Beam energy in MeV (default: 380)"
    echo "  -t, --timeout SEC            Timeout per run in seconds (default: 300)"
    echo "  -h, --help                   Show this help"
    echo ""
    echo "Examples:"
    echo "  $0                           # Run all fields 0.8T-2.0T"
    echo "  $0 -f 080,100,120            # Run only 0.8T, 1.0T, 1.2T"
    echo "  $0 -f 200 -e 250             # Run 2.0T at 250 MeV"
}

while [[ $# -gt 0 ]]; do
    case $1 in
        -f|--fields) FIELDS="$2"; shift 2 ;;
        -e|--energy) ENERGY="$2"; shift 2 ;;
        -t|--timeout) TIMEOUT_SEC="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1"; usage; exit 1 ;;
    esac
done

# [EN] Verify simulator exists / [CN] 验证模拟器是否存在
if [[ ! -x "$SIM_EXEC" ]]; then
    echo "[ERROR] Simulator not found: $SIM_EXEC"
    echo "Please build the project first: cd $SMSIMDIR && ./build.sh"
    exit 1
fi

mkdir -p "${OUTDIR}"

echo "=========================================="
echo "[CONFIG] Fields: $FIELDS"
echo "[CONFIG] Energy: ${ENERGY} MeV"
echo "[CONFIG] Output: ${OUTDIR}/"
echo "=========================================="

IFS=',' read -ra FIELD_ARR <<< "$FIELDS"

for field in "${FIELD_ARR[@]}"; do
    geofile="${SCRIPT_DIR}/B${field}T.mac"
    base="deuteron_B${field}T_${ENERGY}MeV"
    
    if [[ ! -f "$geofile" ]]; then
        echo "[SKIP] Geometry file not found: $geofile"
        continue
    fi
    
    echo ""
    echo "========================================"
    echo "[RUN] B=${field}T @ ${ENERGY} MeV"
    echo "========================================"
    
    # [EN] Create temporary macro for VRML output / [CN] 创建用于 VRML 输出的临时宏文件
    tmpfile=$(mktemp --suffix=.mac)
    
    cat > "$tmpfile" << MACRO
# =========================================================================
# [EN] Auto-generated macro for VRML output / [CN] 自动生成的 VRML 输出宏
# [EN] Magnetic Field: ${field}T / [CN] 磁场强度: ${field}T
# =========================================================================

# [EN] Use VRML2FILE driver for headless output / [CN] 使用 VRML2FILE 驱动进行无头输出
/vis/open VRML2FILE

/control/getEnv SMSIMDIR

/action/file/OverWrite y
/action/file/RunName ${base}
/action/file/SaveDirectory ${OUTDIR}

# [EN] Load geometry configuration / [CN] 加载几何配置
/control/execute ${geofile}

# ==========================================
# [EN] Visualization settings / [CN] 可视化设置
# ==========================================
/vis/viewer/set/autoRefresh false
/vis/verbose errors

# [EN] Set Z-X view (top/side view) / [CN] 设置 Z-X 视图 (俯视/侧视)
/vis/viewer/set/upVector 1 0 0
/vis/viewer/set/viewpointThetaPhi 90. 90.
/vis/drawVolume
/vis/scene/add/axes 0 0 0 6000 mm

# [EN] Trajectory accumulation / [CN] 轨迹累积
/vis/scene/endOfEventAction accumulate
/vis/scene/add/trajectories smooth
/vis/modeling/trajectories/create/drawByCharge
/vis/modeling/trajectories/drawByCharge-0/default/setDrawStepPts true
/vis/modeling/trajectories/drawByCharge-0/default/setStepPtsSize 1
/vis/scene/endOfRunAction accumulate

# ==========================================
# [EN] Beam setup / [CN] 束流设置
# ==========================================
/action/gun/Type Pencil
/action/gun/SetBeamParticleName deuteron
/action/gun/Position 0 0 -4000 mm
/action/gun/Energy ${ENERGY} MeV

# [EN] Run simulation / [CN] 运行模拟
/run/beamOn 1

# [EN] Flush visualization and exit / [CN] 刷新可视化并退出
/vis/viewer/set/autoRefresh true
/vis/viewer/flush
exit
MACRO

    echo "[INFO] Running simulation..."
    cd "${SCRIPT_DIR}"
    timeout ${TIMEOUT_SEC} "${SIM_EXEC}" "$tmpfile" 2>&1 | grep -E "(Error|error|beamOn|WARNING|VRML)" | head -15
    
    # [EN] Find and rename VRML output file / [CN] 查找并重命名 VRML 输出文件
    wrlfile=$(ls -t g4_*.wrl 2>/dev/null | head -1)
    if [[ -n "$wrlfile" && -f "$wrlfile" ]]; then
        mv "$wrlfile" "${OUTDIR}/${base}.wrl"
        echo "[OK] Generated: ${OUTDIR}/${base}.wrl"
    else
        echo "[WARN] No VRML file generated for B=${field}T"
    fi
    
    # [EN] Cleanup / [CN] 清理
    rm -f "$tmpfile"
    rm -f g4_*.wrl 2>/dev/null
done

echo ""
echo "========================================"
echo "[DONE] Batch complete"
echo "========================================"
echo "Output directory: ${OUTDIR}/"
ls -lh "${OUTDIR}/"*.wrl 2>/dev/null || echo "(No .wrl files found)"
