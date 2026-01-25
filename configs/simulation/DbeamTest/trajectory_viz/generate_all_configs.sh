#!/bin/bash
# [EN] Generate all trajectory visualization macros with correct order
# [CN] 生成所有轨迹可视化宏文件（修正命令顺序）

VIZDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Target positions from target_summary files (X, Y, Z in mm)
declare -A TARGET_POS
TARGET_POS["080"]="19.3064 0.0075 -436.9073"
TARGET_POS["100"]="-33.7681 0.0131 -762.6098"
TARGET_POS["120"]="-79.5513 0.0184 -1056.1679"
TARGET_POS["160"]="-152.7034 0.0283 -1571.5049"
TARGET_POS["200"]="-207.8011 0.0371 -2009.1295"

FIELDS=("080" "100" "120" "160" "200")
ANGLES=("2.0" "4.0" "6.0" "8.0" "10.0")

for field in "${FIELDS[@]}"; do
    pos="${TARGET_POS[$field]}"
    read -r tx ty tz <<< "$pos"
    
    for angle in "${ANGLES[@]}"; do
        fname="run_trajectory_B${field}T_${angle}deg.mac"
        cat > "${VIZDIR}/${fname}" << EOF
# =========================================================================
# [EN] Trajectory visualization for B=${field:0:1}.${field:1}T, beam angle=${angle} deg
# [CN] 轨迹可视化: B=${field:0:1}.${field:1}T, 束流偏转角=${angle}度
# =========================================================================
# Target position: X=${tx}mm, Y=${ty}mm, Z=${tz}mm
# Deuteron: 380 MeV from (0, 0, -4m)
# Proton: p=(0, ±100, 627) MeV/c, T=195 MeV, AngleY=±9.07°
# =========================================================================

# 1. Open visualization
/vis/open OGLIQt 1280x720

# 2. Load Geometry - B=${field:0:1}.${field:1}T
/control/execute ${VIZDIR}/geometry_B${field}T.mac

/action/file/OverWrite y
/action/file/RunName trajectory_B${field}T_${angle}deg
/action/file/SaveDirectory /home/tian/workspace/dpol/smsimulator5.5/data/simulation/g4output

# 3. Draw volume FIRST (required before adding scene elements)
/vis/drawVolume

# 4. Set Z-X plane view (top view)
/vis/viewer/set/upVector 1 0 0
/vis/viewer/set/viewpointThetaPhi 90. 90.

# 5. Add coordinate axes
/vis/scene/add/axes 0 0 0 6000 mm

# ==========================================
# [CRITICAL] Trajectory settings - BEFORE beamOn
# ==========================================
/vis/scene/add/trajectories smooth
/vis/modeling/trajectories/create/drawByCharge
/vis/modeling/trajectories/drawByCharge-0/default/setDrawStepPts true
/vis/modeling/trajectories/drawByCharge-0/default/setStepPtsSize 2
/vis/scene/endOfEventAction accumulate
/vis/scene/endOfRunAction accumulate

# Disable auto refresh during shooting
/vis/viewer/set/autoRefresh false
/vis/verbose warnings

# ==========================================
# 1. Deuteron: 380 MeV from (0, 0, -4m)
# ==========================================
/action/gun/Type Pencil
/action/gun/SetBeamParticleName deuteron
/action/gun/Position 0 0 -4000 mm
/action/gun/Energy 380 MeV
/action/gun/AngleX 0 deg
/action/gun/AngleY 0 deg
/run/beamOn 1

# ==========================================
# 2. Proton Py=+100 MeV/c from target
# ==========================================
/action/gun/SetBeamParticleName proton
/action/gun/Position ${tx} ${ty} ${tz} mm
/action/gun/Energy 195 MeV
/action/gun/AngleX 0 deg
/action/gun/AngleY 9.07 deg
/run/beamOn 1

# ==========================================
# 3. Proton Py=-100 MeV/c from target
# ==========================================
/action/gun/SetBeamParticleName proton
/action/gun/Position ${tx} ${ty} ${tz} mm
/action/gun/Energy 195 MeV
/action/gun/AngleX 0 deg
/action/gun/AngleY -9.07 deg
/run/beamOn 1

# ==========================================
# Refresh and Export
# ==========================================
/vis/viewer/set/autoRefresh true
/vis/viewer/refresh
/vis/viewer/update
/vis/viewer/zoomTo 0.3

# Export: /vis/ogl/export trajectory_B${field}T_${angle}deg.png
# Vector: /vis/ogl/printEPS
EOF
        echo "Generated: ${fname}"
    done
done

echo "All 25 macro files regenerated with correct trajectory order."
