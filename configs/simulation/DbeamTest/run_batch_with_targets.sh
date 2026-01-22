#!/bin/bash
# ============================================================================
# [EN] Comprehensive batch run script for sim_deuteron with:
#      - Multiple magnetic field configurations
#      - Multiple beam deflection angles
#      - Corresponding target positions
#      - Trajectory and energy deposition visualization
# [CN] sim_deuteron综合批量运行脚本，支持:
#      - 多种磁场配置
#      - 多种束流偏转角度
#      - 对应的靶点位置
#      - 轨迹和能量沉积可视化
# ============================================================================
# Usage: ./run_batch_with_targets.sh [OPTIONS]
#   --fields    Comma-separated field codes (default: 080,100,120)
#   --angles    Comma-separated beam angles in deg (default: 0,4,8)
#   --events    Number of events per configuration (default: 5)
#   --energy    Deuteron beam energy in MeV (default: 380)
#   --output    Output directory (default: ./trajectory_output)
#   --dry-run   Print what would be done without executing
#   --help      Show this help message
# ============================================================================

set -e

# ============================================================================
# [EN] Default Parameters / [CN] 默认参数
# ============================================================================
FIELD_LIST="080,100,120"                    # [EN] 0.8T, 1.0T, 1.2T / [CN] 磁场
ANGLE_LIST="0,4,8"                          # [EN] Beam deflection angles / [CN] 偏转角
N_EVENTS=5                                  # [EN] Events per config / [CN] 事件数
BEAM_ENERGY=380                             # [EN] MeV / [CN] 能量
OUTPUT_DIR="./trajectory_output"
DRY_RUN=false

# ============================================================================
# [EN] Parse Arguments / [CN] 解析参数
# ============================================================================
while [[ $# -gt 0 ]]; do
    case $1 in
        --fields)    FIELD_LIST="$2"; shift 2 ;;
        --angles)    ANGLE_LIST="$2"; shift 2 ;;
        --events)    N_EVENTS="$2"; shift 2 ;;
        --energy)    BEAM_ENERGY="$2"; shift 2 ;;
        --output)    OUTPUT_DIR="$2"; shift 2 ;;
        --dry-run)   DRY_RUN=true; shift ;;
        --help|-h)   head -25 "$0" | tail -20; exit 0 ;;
        *)           echo "Unknown option: $1"; exit 1 ;;
    esac
done

# ============================================================================
# [EN] Environment Setup / [CN] 环境设置
# ============================================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ -z "$SMSIMDIR" ]; then
    export SMSIMDIR="/home/tian/workspace/dpol/smsimulator5.5"
fi

SIM_BIN="$SMSIMDIR/build/bin/sim_deuteron"

if [ ! -x "$SIM_BIN" ] && [ "$DRY_RUN" = false ]; then
    echo "[ERROR] sim_deuteron not found: $SIM_BIN"
    exit 1
fi

# [EN] First generate target configs if not present / [CN] 如果没有靶点配置则先生成
if [ ! -f "$SCRIPT_DIR/target/angle0deg.mac" ]; then
    echo "[INFO] Generating target configurations..."
    ./generate_target_configs.sh
fi

mkdir -p "$OUTPUT_DIR"

IFS=',' read -ra FIELDS <<< "$FIELD_LIST"
IFS=',' read -ra ANGLES <<< "$ANGLE_LIST"

# ============================================================================
# [EN] Print Configuration / [CN] 打印配置
# ============================================================================
TOTAL_CONFIGS=$(( ${#FIELDS[@]} * ${#ANGLES[@]} ))
echo "============================================================================"
echo "[EN] Batch Trajectory Simulation / [CN] 批量轨迹模拟"
echo "============================================================================"
printf "  %-20s %s\n" "Magnetic Fields:" "${FIELDS[*]/%/T}"
printf "  %-20s %s deg\n" "Beam Angles:" "${ANGLES[*]}"
printf "  %-20s %d\n" "Events/config:" "$N_EVENTS"
printf "  %-20s %d MeV\n" "Beam Energy:" "$BEAM_ENERGY"
printf "  %-20s %s\n" "Output:" "$OUTPUT_DIR"
printf "  %-20s %d\n" "Total configs:" "$TOTAL_CONFIGS"
if [ "$DRY_RUN" = true ]; then
    echo "  [DRY RUN MODE - no simulations will be executed]"
fi
echo "============================================================================"
echo ""

# ============================================================================
# [EN] Main Loop / [CN] 主循环
# ============================================================================
CURRENT=0
START_TIME=$(date +%s)

SUMMARY_FILE="$OUTPUT_DIR/trajectory_summary.txt"
echo "# Trajectory Simulation Summary" > "$SUMMARY_FILE"
echo "# Generated: $(date)" >> "$SUMMARY_FILE"
echo "# Format: Field[T] Angle[deg] TargetX[cm] TargetZ[cm] Energy[MeV] N_Events VRML_File" >> "$SUMMARY_FILE"
echo "#" >> "$SUMMARY_FILE"

for field in "${FIELDS[@]}"; do
    FIELD_T="$(echo "scale=2; $field/100" | bc)"
    GEO_MAC="$SCRIPT_DIR/nopdc/B${field}T.mac"
    
    if [ ! -f "$GEO_MAC" ]; then
        echo "[WARN] Geometry macro not found: $GEO_MAC (skipping field $field)"
        continue
    fi
    
    for angle in "${ANGLES[@]}"; do
        CURRENT=$((CURRENT + 1))
        
        # [EN] Check if target config exists / [CN] 检查靶点配置是否存在
        TARGET_MAC="$SCRIPT_DIR/target/angle${angle}deg.mac"
        if [ ! -f "$TARGET_MAC" ]; then
            echo "[WARN] Target config not found: $TARGET_MAC (skipping)"
            continue
        fi
        
        # [EN] Extract target position from macro / [CN] 从宏文件提取靶点位置
        TARGET_POS=$(grep "Position" "$TARGET_MAC" | head -1 | awk '{print $2, $3, $4}')
        TARGET_X=$(echo "$TARGET_POS" | awk '{print $1}')
        TARGET_Z=$(echo "$TARGET_POS" | awk '{print $3}')
        
        CONFIG_NAME="B${field}T_angle${angle}deg"
        ANGLE_RAD=$(echo "scale=6; $angle * 3.14159265359 / 180" | bc)
        
        echo "[$CURRENT/$TOTAL_CONFIGS] Field=${FIELD_T}T, Angle=${angle}deg, Target=(${TARGET_X}, 0, ${TARGET_Z})cm"
        
        if [ "$DRY_RUN" = true ]; then
            echo "  [DRY RUN] Would create macro and run simulation"
            echo "${FIELD_T} ${angle} ${TARGET_X} ${TARGET_Z} ${BEAM_ENERGY} ${N_EVENTS} ${CONFIG_NAME}.wrl" >> "$SUMMARY_FILE"
            continue
        fi
        
        # [EN] Create combined macro / [CN] 创建组合宏文件
        TEMP_MAC="$OUTPUT_DIR/${CONFIG_NAME}.mac"
        cat > "$TEMP_MAC" << EOF
# ============================================================================
# [EN] Combined config: Field=${FIELD_T}T, Angle=${angle}deg
# [CN] 组合配置: 磁场=${FIELD_T}T, 偏转角=${angle}度
# ============================================================================

# [EN] Load geometry (magnet + detectors) / [CN] 加载几何配置(磁铁+探测器)
/control/execute ${GEO_MAC}

# [EN] Load target configuration / [CN] 加载靶点配置
/control/execute ${TARGET_MAC}

# [EN] Output settings / [CN] 输出设置
/action/file/OverWrite y
/action/file/RunName ${CONFIG_NAME}
/action/file/SaveDirectory ${OUTPUT_DIR}

# [EN] Trajectory storage / [CN] 轨迹存储
/tracking/storeTrajectory 1

# [EN] VRML visualization / [CN] VRML可视化
/vis/open VRML2FILE
/vis/drawVolume

# [EN] Coordinate axes / [CN] 坐标轴
/vis/scene/add/axes 0 0 0 6000 mm

# [EN] Trajectory visualization / [CN] 轨迹可视化
/vis/scene/endOfEventAction accumulate
/vis/scene/add/trajectories smooth
/vis/modeling/trajectories/create/drawByCharge
/vis/modeling/trajectories/drawByCharge-0/default/setDrawStepPts true
/vis/modeling/trajectories/drawByCharge-0/default/setStepPtsSize 2

# [EN] Energy deposition hits / [CN] 能量沉积点
/vis/scene/add/hits

# ============================================================================
# [EN] Deuteron beam / [CN] 氘核束流
# ============================================================================
/action/gun/Type Pencil
/action/gun/SetBeamParticleName deuteron
/action/gun/Position 0 0 -6 m
/action/gun/Energy ${BEAM_ENERGY} MeV
/action/gun/AngleY ${ANGLE_RAD} rad

# [EN] Run simulation / [CN] 运行模拟
/run/beamOn ${N_EVENTS}
/vis/viewer/flush
EOF

        # [EN] Execute simulation / [CN] 执行模拟
        if $SIM_BIN "$TEMP_MAC" > "$OUTPUT_DIR/${CONFIG_NAME}.log" 2>&1; then
            echo "  [OK] Simulation completed"
        else
            echo "  [WARN] Finished with warnings"
        fi
        
        # [EN] Collect VRML output / [CN] 收集VRML输出
        VRML_OUT="${CONFIG_NAME}.wrl"
        if [ -f "g4_00.wrl" ]; then
            mv "g4_00.wrl" "$OUTPUT_DIR/$VRML_OUT"
            echo "  [OK] VRML: $OUTPUT_DIR/$VRML_OUT"
        elif [ -f "g4_01.wrl" ]; then
            mv "g4_01.wrl" "$OUTPUT_DIR/$VRML_OUT"
            echo "  [OK] VRML: $OUTPUT_DIR/$VRML_OUT"
        else
            echo "  [WARN] No VRML output"
            VRML_OUT="(none)"
        fi
        rm -f g4_*.wrl
        
        echo "${FIELD_T} ${angle} ${TARGET_X} ${TARGET_Z} ${BEAM_ENERGY} ${N_EVENTS} ${VRML_OUT}" >> "$SUMMARY_FILE"
        
        # [EN] Progress / [CN] 进度
        ELAPSED=$(($(date +%s) - START_TIME))
        REMAINING=$(( (TOTAL_CONFIGS - CURRENT) * ELAPSED / CURRENT ))
        echo "  [TIME] Elapsed: ${ELAPSED}s, Remaining: ~${REMAINING}s"
    done
done

# ============================================================================
# [EN] Summary / [CN] 汇总
# ============================================================================
END_TIME=$(date +%s)
TOTAL_TIME=$((END_TIME - START_TIME))

echo ""
echo "============================================================================"
echo "[EN] Batch Complete / [CN] 批量完成"
echo "============================================================================"
echo "  Total time: ${TOTAL_TIME}s"
echo "  Configs:    $CURRENT"
echo "  Summary:    $SUMMARY_FILE"
echo ""
echo "  VRML files:"
ls -lh "$OUTPUT_DIR"/*.wrl 2>/dev/null | head -10 || echo "  (none)"
echo "============================================================================"
