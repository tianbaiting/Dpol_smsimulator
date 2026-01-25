#!/bin/bash
# [EN] Generate all trajectory visualization macros using /control/loop for 3 tracks
# [CN] 使用 /control/loop 方式生成所有轨迹可视化宏文件，一个 run 运行三条轨迹
# [EN] All paths use {SMSIMDIR} environment variable for portability
# [CN] 所有路径使用 {SMSIMDIR} 环境变量以保证可移植性

VIZDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# [EN] Relative path from SMSIMDIR to this directory
# [CN] 从 SMSIMDIR 到本目录的相对路径
VIZDIR_REL="configs/simulation/DbeamTest/trajectory_viz"

# Target positions from target_summary files (X, Y, Z in mm)
declare -A TARGET_POS
TARGET_POS["080"]="19.3064 0.0075 -436.9073"
TARGET_POS["100"]="-33.7681 0.0131 -762.6098"
TARGET_POS["120"]="-79.5513 0. -1056.1679"
TARGET_POS["160"]="-152.7034 0.0283 -1571.5049"
TARGET_POS["200"]="-207.8011 0.0371 -2009.1295"

FIELDS=("080" "100" "120" "160" "200")
ANGLES=("2.0" "4.0" "6.0" "8.0" "10.0")

# =========================================================================
# [EN] Generate the common shoot_track.mac (called by /control/loop)
# [CN] 生成通用的 shoot_track.mac（被 /control/loop 调用）
# =========================================================================
cat > "${VIZDIR}/shoot_track.mac" << 'SHOOTEOF'
# =========================================================================
# [EN] Single track shooter - called by /control/loop with PARTICLE alias
# [CN] 单轨迹发射器 - 被 /control/loop 以 PARTICLE 别名调用
# =========================================================================
# Usage: /control/loop shoot_track.mac PARTICLE deuteron proton_up proton_down
# Each call sets gun parameters based on {PARTICLE} and fires one event
# =========================================================================

/control/getEnv PARTICLE
/control/execute {VIZDIR}/tracks/{PARTICLE}.mac
/run/beamOn 1
SHOOTEOF
echo "Generated: shoot_track.mac"

# =========================================================================
# [EN] Generate particle definition macros in tracks/ subdirectory
# [CN] 在 tracks/ 子目录生成各粒子定义宏
# =========================================================================
mkdir -p "${VIZDIR}/tracks"

# [EN] Deuteron track: 380 MeV from (0, 0, -4m)
# [CN] 氘核轨迹: 380 MeV，从 (0, 0, -4m) 发射
cat > "${VIZDIR}/tracks/deuteron.mac" << 'EOF'
# [EN] Deuteron: 380 MeV from upstream / [CN] 氘核: 380 MeV 从上游发射
/action/gun/Type Pencil
/action/gun/SetBeamParticleName deuteron
/action/gun/Position 0 0 -4000 mm
/action/gun/Energy 380 MeV
/action/gun/AngleX 0 deg
/action/gun/AngleY 0 deg
EOF
echo "Generated: tracks/deuteron.mac"

# =========================================================================
# [EN] Generate field-specific proton macros (target position varies)
# [CN] 生成各磁场强度对应的质子宏（靶位置不同）
# =========================================================================
for field in "${FIELDS[@]}"; do
    pos="${TARGET_POS[$field]}"
    read -r tx ty tz <<< "$pos"
    
    # [EN] Proton up: Py=+100 MeV/c, AngleX=-9.07 deg
    # [CN] 质子上行: Py=+100 MeV/c, AngleX=-9.07 度
    cat > "${VIZDIR}/tracks/proton_up_B${field}T.mac" << EOF
# [EN] Proton Py=+100 MeV/c from target / [CN] 质子 Py=+100 MeV/c 从靶发射
/action/gun/Type Pencil
/action/gun/SetBeamParticleName proton
/action/gun/Position ${tx} ${ty} ${tz} mm
/action/gun/Energy 195 MeV
/action/gun/AngleX -9.07 deg
/action/gun/AngleY 0 deg
EOF
    
    # [EN] Proton down: Py=-100 MeV/c, AngleX=+9.07 deg
    # [CN] 质子下行: Py=-100 MeV/c, AngleX=+9.07 度
    cat > "${VIZDIR}/tracks/proton_down_B${field}T.mac" << EOF
# [EN] Proton Py=-100 MeV/c from target / [CN] 质子 Py=-100 MeV/c 从靶发射
/action/gun/Type Pencil
/action/gun/SetBeamParticleName proton
/action/gun/Position ${tx} ${ty} ${tz} mm
/action/gun/Energy 195 MeV
/action/gun/AngleX +9.07 deg
/action/gun/AngleY 0 deg
EOF
    echo "Generated: tracks/proton_up_B${field}T.mac, tracks/proton_down_B${field}T.mac"
done

# =========================================================================
# [EN] Generate main trajectory visualization macros with /control/loop
# [CN] 生成使用 /control/loop 的主轨迹可视化宏
# =========================================================================
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
# [EN] Uses /control/loop to shoot 3 tracks in sequence:
#      1. Deuteron (380 MeV from upstream)
#      2. Proton Py=+100 MeV/c (AngleX=-9.07 deg)
#      3. Proton Py=-100 MeV/c (AngleX=+9.07 deg)
# [CN] 使用 /control/loop 依次发射3条轨迹:
#      1. 氘核 (380 MeV 从上游)
#      2. 质子 Py=+100 MeV/c (AngleX=-9.07度)
#      3. 质子 Py=-100 MeV/c (AngleX=+9.07度)
# =========================================================================
# Target position: X=${tx}mm, Y=${ty}mm, Z=${tz}mm
# =========================================================================

# 1. Open visualization
/vis/open OGLIQt 1280x720

# 2. Load Geometry - B=${field:0:1}.${field:1}T
/control/execute {SMSIMDIR}/${VIZDIR_REL}/geometry_B\${field}T.mac

/action/file/OverWrite y
/action/file/RunName trajectory_B\${field}T_\${angle}deg
/action/file/SaveDirectory {SMSIMDIR}/data/simulation/g4output

# 3. Draw volume FIRST (required before adding scene elements)
/vis/drawVolume

# 4. Set Z-X plane view (top view)
/vis/viewer/set/upVector 1 0 0
/vis/viewer/set/viewpointThetaPhi 90. 90.

# 5. Add coordinate axes
/vis/scene/add/axes 0 0 0 6000 mm

# ==========================================
# [CRITICAL] Trajectory settings - BEFORE beamOn
# [EN] Must configure trajectory visualization before any beamOn
# [CN] 必须在任何 beamOn 之前配置轨迹可视化
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
# [EN] Shoot 3 tracks using /control/loop
# [CN] 使用 /control/loop 发射3条轨迹
# ==========================================
# [EN] /control/loop iterates over particle list and executes macro for each
# [CN] /control/loop 遍历粒子列表，为每个粒子执行宏
/action/gun/Type Pencil

/control/loop {SMSIMDIR}/${VIZDIR_REL}/shoot_3tracks_B\${field}T.mac TRACK 1 3 1

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
    
    # =========================================================================
    # [EN] Generate shoot_3tracks macro for this field strength
    # [CN] 为该磁场强度生成 shoot_3tracks 宏
    # =========================================================================
    cat > "${VIZDIR}/shoot_3tracks_B${field}T.mac" << EOF
# =========================================================================
# [EN] 3-track shooter for B=${field:0:1}.${field:1}T - called by /control/loop
# [CN] B=${field:0:1}.${field:1}T 的3轨迹发射器 - 被 /control/loop 调用
# =========================================================================
# [EN] TRACK=1: Deuteron, TRACK=2: Proton up, TRACK=3: Proton down
# [CN] TRACK=1: 氘核, TRACK=2: 质子上行, TRACK=3: 质子下行
# =========================================================================

# [EN] Use /control/if to select particle based on TRACK value
# [CN] 使用 /control/if 根据 TRACK 值选择粒子

/control/doif {TRACK} == 1 /control/execute {SMSIMDIR}/${VIZDIR_REL}/tracks/deuteron.mac
/control/doif {TRACK} == 2 /control/execute {SMSIMDIR}/${VIZDIR_REL}/tracks/proton_up_B\${field}T.mac
/control/doif {TRACK} == 3 /control/execute {SMSIMDIR}/${VIZDIR_REL}/tracks/proton_down_B\${field}T.mac

/run/beamOn 1
EOF
    echo "Generated: shoot_3tracks_B${field}T.mac"
done

echo ""
echo "==========================================================================="
echo "[EN] All 25 main macros + 5 shoot_3tracks macros + 11 particle macros generated"
echo "[CN] 已生成 25 个主宏 + 5 个 shoot_3tracks 宏 + 11 个粒子宏"
echo "==========================================================================="
echo "[EN] Structure: Main macro -> /control/loop -> shoot_3tracks -> particle mac"
echo "[CN] 结构: 主宏 -> /control/loop -> shoot_3tracks -> 粒子宏"
echo "==========================================================================="/
