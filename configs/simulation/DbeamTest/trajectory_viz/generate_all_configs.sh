#!/bin/bash
# =========================================================================
# [EN] Generate trajectory visualization configs for all field/angle combos
# [CN] 生成所有磁场强度和偏转角组合的轨迹可视化配置
# =========================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Target positions from summary files
declare -A TARGET_B080T=(
    ["2.0"]="-8.1743:0.0046:-1093.7313"
    ["4.0"]="-20.6120:0.0063:-851.8761"
    ["6.0"]="-37.8391:0.0077:-654.3163"
    ["8.0"]="-60.6090:0.0088:-468.7377"
    ["10.0"]="-89.3315:0.0100:-287.3597"
)

declare -A TARGET_B100T=(
    ["2.0"]="-7.7287:0.0075:-1151.8117"
    ["4.0"]="-18.9698:0.0105:-932.5705"
    ["6.0"]="-33.7681:0.0131:-762.6098"
    ["8.0"]="-52.6612:0.0157:-608.5314"
    ["10.0"]="-76.0454:0.0186:-460.8262"
)

declare -A TARGET_B120T=(
    ["2.0"]="-7.4478:0.0082:-1193.9326"
    ["4.0"]="-17.9688:0.0107:-988.3352"
    ["6.0"]="-31.3605:0.0133:-834.3431"
    ["8.0"]="-47.9353:0.0150:-699.0843"
    ["10.0"]="-68.0180:0.0169:-572.1932"
)

declare -A TARGET_B160T=(
    ["2.0"]="-7.0397:0.0098:-1256.1967"
    ["4.0"]="-16.6920:0.0118:-1067.2291"
    ["6.0"]="-28.5461:0.0144:-930.7559"
    ["8.0"]="-42.6748:0.0158:-815.3566"
    ["10.0"]="-59.2219:0.0167:-710.7437"
)

declare -A TARGET_B200T=(
    ["2.0"]="-6.6894:0.0134:-1309.1564"
    ["4.0"]="-15.6985:0.0153:-1132.5691"
    ["6.0"]="-26.5470:0.0178:-1007.5765"
    ["8.0"]="-39.2217:0.0192:-904.0010"
    ["10.0"]="-53.7761:0.0193:-811.9539"
)

PROTON_ENERGY="195"
PROTON_ANGLE="9.07"
DEUTERON_ENERGY="380"

generate_config() {
    local FIELD=$1
    local FIELD_T=$2
    local ANGLE=$3
    local TARGET_X=$4
    local TARGET_Y=$5
    local TARGET_Z=$6
    
    local OUTFILE="run_trajectory_B${FIELD}T_${ANGLE}deg.mac"
    local VIZ_DIR="/home/tian/workspace/dpol/smsimulator5.5/configs/simulation/DbeamTest/trajectory_viz"
    
    cat > "$OUTFILE" << EOF
# =========================================================================
# [EN] Trajectory visualization for B=${FIELD_T}T, beam angle=${ANGLE} deg
# [CN] 轨迹可视化: B=${FIELD_T}T, 束流偏转角=${ANGLE}度
# =========================================================================
# Target position: X=${TARGET_X}mm, Y=${TARGET_Y}mm, Z=${TARGET_Z}mm
# Deuteron: 380 MeV from (0, 0, -4m)
# Proton: p=(0, ±100, 627) MeV/c, T=${PROTON_ENERGY} MeV, AngleY=±${PROTON_ANGLE}°
# =========================================================================

# Visualization Setup
/vis/open OGLIQt
/vis/viewer/set/autoRefresh false
/vis/verbose errors

# Load Geometry - B=${FIELD_T}T
/control/execute ${VIZ_DIR}/geometry_B${FIELD}T.mac

/action/file/OverWrite y
/action/file/RunName trajectory_B${FIELD}T_${ANGLE}deg
/action/file/SaveDirectory /home/tian/workspace/dpol/smsimulator5.5/data/simulation/g4output

# Z-X plane view (top view)
/vis/viewer/set/upVector 1 0 0
/vis/viewer/set/viewpointThetaPhi 90. 90.
/vis/drawVolume
/vis/scene/add/axes 0 0 0 6000 mm

# Trajectory accumulation across runs
/vis/scene/endOfEventAction accumulate
/vis/scene/add/trajectories smooth
/vis/modeling/trajectories/create/drawByCharge
/vis/modeling/trajectories/drawByCharge-0/default/setDrawStepPts true
/vis/modeling/trajectories/drawByCharge-0/default/setStepPtsSize 1
/vis/scene/endOfRunAction accumulate

# ==========================================
# 1. Deuteron: 380 MeV from (0, 0, -4m)
# ==========================================
/action/gun/Type Pencil
/action/gun/SetBeamParticleName deuteron
/action/gun/Position 0 0 -4000 mm
/action/gun/Energy ${DEUTERON_ENERGY} MeV
/action/gun/AngleX 0 deg
/action/gun/AngleY 0 deg
/run/beamOn 1

# ==========================================
# 2. Proton Py=+100 MeV/c from target
# ==========================================
/action/gun/Type Pencil
/action/gun/SetBeamParticleName proton
/action/gun/Position ${TARGET_X} ${TARGET_Y} ${TARGET_Z} mm
/action/gun/Energy ${PROTON_ENERGY} MeV
/action/gun/AngleX 0 deg
/action/gun/AngleY ${PROTON_ANGLE} deg
/run/beamOn 1

# ==========================================
# 3. Proton Py=-100 MeV/c from target
# ==========================================
/action/gun/Type Pencil
/action/gun/SetBeamParticleName proton
/action/gun/Position ${TARGET_X} ${TARGET_Y} ${TARGET_Z} mm
/action/gun/Energy ${PROTON_ENERGY} MeV
/action/gun/AngleX 0 deg
/action/gun/AngleY -${PROTON_ANGLE} deg
/run/beamOn 1

# Refresh display
/vis/viewer/set/autoRefresh true
/vis/viewer/refresh

# Save: /vis/ogl/export trajectory_B${FIELD}T_${ANGLE}deg.png
EOF

    echo "  Generated: $OUTFILE"
}

# Generate all configs
echo "Generating trajectory visualization configs..."

for ANGLE in "2.0" "4.0" "6.0" "8.0" "10.0"; do
    echo "Angle: ${ANGLE}deg"
    
    IFS=':' read -r X Y Z <<< "${TARGET_B080T[$ANGLE]}"
    generate_config "080" "0.8" "$ANGLE" "$X" "$Y" "$Z"
    
    IFS=':' read -r X Y Z <<< "${TARGET_B100T[$ANGLE]}"
    generate_config "100" "1.0" "$ANGLE" "$X" "$Y" "$Z"
    
    IFS=':' read -r X Y Z <<< "${TARGET_B120T[$ANGLE]}"
    generate_config "120" "1.2" "$ANGLE" "$X" "$Y" "$Z"
    
    IFS=':' read -r X Y Z <<< "${TARGET_B160T[$ANGLE]}"
    generate_config "160" "1.6" "$ANGLE" "$X" "$Y" "$Z"
    
    IFS=':' read -r X Y Z <<< "${TARGET_B200T[$ANGLE]}"
    generate_config "200" "2.0" "$ANGLE" "$X" "$Y" "$Z"
done

echo ""
echo "Done! Generated $(ls run_trajectory_*.mac 2>/dev/null | wc -l) macro files."
