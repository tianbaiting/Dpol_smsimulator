#!/bin/bash
# ============================================================================
# [EN] Batch run script for sim_deuteron with different magnetic fields and 
#      beam deflection angles. Generates trajectory plots and energy 
#      deposition visualization in VRML format.
# [CN] sim_deuteron批量运行脚本，支持不同磁场和束流偏转角度组合。
#      生成轨迹图和能量沉积的VRML格式可视化。
# ============================================================================
# Usage: ./run_all_fields.sh [OPTIONS]
#   --fields    Comma-separated list of fields (default: 080,100,120,140)
#   --angles    Comma-separated list of beam angles in deg (default: 0,4,8)
#   --events    Number of events per configuration (default: 5)
#   --energy    Beam energy in MeV (default: 380)
#   --output    Output directory (default: ./batch_output)
#   --help      Show this help message
# ============================================================================

set -e

# ============================================================================
# [EN] Default Parameters / [CN] 默认参数
# ============================================================================
FIELD_LIST="080,100,120,140,160,180,200"   # [EN] Magnetic field values in 0.01T / [CN] 磁场值(0.01T单位)
ANGLE_LIST="0,4,8"                          # [EN] Beam deflection angles in deg / [CN] 束流偏转角(度)
N_EVENTS=5                                  # [EN] Events per configuration / [CN] 每配置事件数
BEAM_ENERGY=380                             # [EN] Beam energy in MeV / [CN] 束流能量(MeV)
OUTPUT_DIR="./batch_output"                 # [EN] Output directory / [CN] 输出目录

# ============================================================================
# [EN] Parse Arguments / [CN] 解析参数
# ============================================================================
while [[ $# -gt 0 ]]; do
    case $1 in
        --fields)
            FIELD_LIST="$2"
            shift 2
            ;;
        --angles)
            ANGLE_LIST="$2"
            shift 2
            ;;
        --events)
            N_EVENTS="$2"
            shift 2
            ;;
        --energy)
            BEAM_ENERGY="$2"
            shift 2
            ;;
        --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --help|-h)
            head -20 "$0" | tail -15
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# ============================================================================
# [EN] Environment Setup / [CN] 环境设置
# ============================================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ -z "$SMSIMDIR" ]; then
    export SMSIMDIR="/home/tian/workspace/dpol/smsimulator5.5"
    echo "[INFO] Setting SMSIMDIR=$SMSIMDIR"
fi

SIM_BIN="$SMSIMDIR/build/bin/sim_deuteron"

if [ ! -x "$SIM_BIN" ]; then
    echo "[ERROR] sim_deuteron not found or not executable: $SIM_BIN"
    echo "[ERROR] Please build the project first: cd \$SMSIMDIR/build && make -j4"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

# [EN] Convert comma-separated lists to arrays / [CN] 将逗号分隔的列表转换为数组
IFS=',' read -ra FIELDS <<< "$FIELD_LIST"
IFS=',' read -ra ANGLES <<< "$ANGLE_LIST"

# ============================================================================
# [EN] Print Configuration / [CN] 打印配置
# ============================================================================
echo "============================================================================"
echo "[EN] Batch Simulation Configuration / [CN] 批量模拟配置"
echo "============================================================================"
echo "  Magnetic Fields (T): ${FIELDS[*]/%/T}"
echo "  Beam Angles (deg):   ${ANGLES[*]}"
echo "  Events per config:   $N_EVENTS"
echo "  Beam Energy (MeV):   $BEAM_ENERGY"
echo "  Output Directory:    $OUTPUT_DIR"
echo "  Total configs:       $(( ${#FIELDS[@]} * ${#ANGLES[@]} ))"
echo "============================================================================"
echo ""

# ============================================================================
# [EN] Batch Run Loop / [CN] 批量运行循环
# ============================================================================
TOTAL_CONFIGS=$(( ${#FIELDS[@]} * ${#ANGLES[@]} ))
CURRENT=0
START_TIME=$(date +%s)

# [EN] Create summary file / [CN] 创建汇总文件
SUMMARY_FILE="$OUTPUT_DIR/run_summary.txt"
echo "# Batch Simulation Summary" > "$SUMMARY_FILE"
echo "# Generated: $(date)" >> "$SUMMARY_FILE"
echo "# Format: Field_T Angle_deg Energy_MeV N_Events Output_File" >> "$SUMMARY_FILE"
echo "#" >> "$SUMMARY_FILE"

for field in "${FIELDS[@]}"; do
    # [EN] Map field code to table file / [CN] 将磁场代码映射到表文件
    FIELD_VALUE="$(echo "scale=2; $field/100" | bc)"
    FIELD_CODE="B${field}T"
    GEO_MAC="$SCRIPT_DIR/nopdc/${FIELD_CODE}.mac"
    
    if [ ! -f "$GEO_MAC" ]; then
        echo "[WARN] Geometry macro not found: $GEO_MAC (skipping)"
        continue
    fi
    
    for angle in "${ANGLES[@]}"; do
        CURRENT=$((CURRENT + 1))
        CONFIG_NAME="${FIELD_CODE}_angle${angle}deg"
        
        # [EN] Calculate beam direction from angle / [CN] 从角度计算束流方向
        # AngleY in radians for /action/gun/AngleY command
        ANGLE_RAD=$(echo "scale=6; $angle * 3.14159265359 / 180" | bc)
        
        echo ""
        echo "[$CURRENT/$TOTAL_CONFIGS] Running: Field=${FIELD_VALUE}T, Angle=${angle}deg"
        echo "--------------------------------------------------------------"
        
        # [EN] Create temporary macro file / [CN] 创建临时宏文件
        TEMP_MAC="$OUTPUT_DIR/temp_${CONFIG_NAME}.mac"
        cat > "$TEMP_MAC" << EOF
# ============================================================================
# [EN] Auto-generated macro for Field=${FIELD_VALUE}T, Angle=${angle}deg
# [CN] 自动生成的宏文件: 磁场=${FIELD_VALUE}T, 偏转角=${angle}度
# ============================================================================

# [EN] Load geometry configuration / [CN] 加载几何配置
/control/execute ${GEO_MAC}

# [EN] Output configuration / [CN] 输出配置
/action/file/OverWrite y
/action/file/RunName ${CONFIG_NAME}
/action/file/SaveDirectory ${OUTPUT_DIR}

# [EN] Store trajectories for visualization / [CN] 存储轨迹用于可视化
/tracking/storeTrajectory 1

# [EN] VRML visualization (headless mode) / [CN] VRML可视化(无头模式)
/vis/open VRML2FILE
/vis/drawVolume

# [EN] Add coordinate axes (6m) / [CN] 添加坐标轴(6米)
/vis/scene/add/axes 0 0 0 6000 mm

# [EN] Trajectory visualization settings / [CN] 轨迹可视化设置
/vis/scene/endOfEventAction accumulate
/vis/scene/add/trajectories smooth
/vis/modeling/trajectories/create/drawByCharge
/vis/modeling/trajectories/drawByCharge-0/default/setDrawStepPts true
/vis/modeling/trajectories/drawByCharge-0/default/setStepPtsSize 2

# [EN] Add energy deposition hits / [CN] 添加能量沉积点
/vis/scene/add/hits

# ============================================================================
# [EN] Deuteron beam configuration / [CN] 氘核束流配置
# ============================================================================
/action/gun/Type Pencil
/action/gun/SetBeamParticleName deuteron
/action/gun/Position 0 0 -4 m
/action/gun/Energy ${BEAM_ENERGY} MeV
/action/gun/AngleY ${ANGLE_RAD} rad

# [EN] Run simulation / [CN] 运行模拟
/run/beamOn ${N_EVENTS}
/vis/viewer/flush

# ============================================================================
# [EN] Additional proton track for comparison (optional)
# [CN] 用于对比的质子轨迹(可选)
# ============================================================================
# /action/gun/SetBeamParticleName proton
# /action/gun/Energy 190 MeV
# /run/beamOn 3
# /vis/viewer/flush
EOF

        # [EN] Run simulation / [CN] 运行模拟
        if $SIM_BIN "$TEMP_MAC" > "$OUTPUT_DIR/${CONFIG_NAME}.log" 2>&1; then
            echo "  [OK] Simulation completed"
        else
            echo "  [WARN] Simulation finished with warnings (check log)"
        fi
        
        # [EN] Rename VRML output file / [CN] 重命名VRML输出文件
        VRML_OUTPUT="${CONFIG_NAME}.wrl"
        if [ -f "g4_00.wrl" ]; then
            mv "g4_00.wrl" "$OUTPUT_DIR/$VRML_OUTPUT"
            echo "  [OK] VRML exported: $OUTPUT_DIR/$VRML_OUTPUT"
        elif [ -f "g4_01.wrl" ]; then
            mv "g4_01.wrl" "$OUTPUT_DIR/$VRML_OUTPUT"
            echo "  [OK] VRML exported: $OUTPUT_DIR/$VRML_OUTPUT"
        else
            echo "  [WARN] No VRML file generated"
            VRML_OUTPUT="(none)"
        fi
        
        # [EN] Clean up stray VRML files / [CN] 清理多余的VRML文件
        rm -f g4_*.wrl
        
        # [EN] Record to summary / [CN] 记录到汇总
        echo "$FIELD_VALUE $angle $BEAM_ENERGY $N_EVENTS $VRML_OUTPUT" >> "$SUMMARY_FILE"
        
        # [EN] Remove temp macro (optional - keep for debugging)
        # rm -f "$TEMP_MAC"
        
        # [EN] Progress estimate / [CN] 进度估计
        ELAPSED=$(($(date +%s) - START_TIME))
        if [ $CURRENT -gt 0 ]; then
            AVG_TIME=$((ELAPSED / CURRENT))
            REMAINING=$(( (TOTAL_CONFIGS - CURRENT) * AVG_TIME ))
            echo "  [INFO] Elapsed: ${ELAPSED}s, Est. remaining: ${REMAINING}s"
        fi
    done
done

# ============================================================================
# [EN] Cleanup and Summary / [CN] 清理和汇总
# ============================================================================
END_TIME=$(date +%s)
TOTAL_TIME=$((END_TIME - START_TIME))

echo ""
echo "============================================================================"
echo "[EN] Batch Simulation Complete / [CN] 批量模拟完成"
echo "============================================================================"
echo "  Total time:    ${TOTAL_TIME}s"
echo "  Configs run:   $CURRENT"
echo "  Output dir:    $OUTPUT_DIR"
echo ""
echo "  Generated files:"
ls -la "$OUTPUT_DIR"/*.wrl 2>/dev/null | head -20 || echo "  (no VRML files)"
echo ""
echo "  Summary file:  $SUMMARY_FILE"
echo "============================================================================"
echo ""
echo "[EN] To convert VRML to PNG, use: python3 vrml_to_png.py <file.wrl>"
echo "[CN] 若要将VRML转换为PNG，请使用: python3 vrml_to_png.py <file.wrl>"
echo "============================================================================"
