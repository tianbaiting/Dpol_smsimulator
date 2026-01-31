#!/bin/bash
# =========================================================================
# [EN] Generate Geant4 macro files using single ROOT tree + Target parameters
# [CN] 使用单个ROOT树 + Target参数生成Geant4宏文件
# =========================================================================
# Key: fUseTargetParameters=true makes sim_deuteron use:
#   - /samurai/geometry/Target/Position for particle start position
#   - /samurai/geometry/Target/Angle for momentum rotation (rotateY(-angle))
# =========================================================================

SMSIMDIR="${SMSIMDIR:-/home/tian/workspace/dpol/smsimulator5.5}"
VIZDIR="${SMSIMDIR}/configs/simulation/DbeamTest/track_vis_useTree"
TRAJVIZDIR="${SMSIMDIR}/configs/simulation/DbeamTest/trajectory_viz"
ROOTFILE="${VIZDIR}/rootfiles/protons_4tracks.root"

# [EN] Target positions: FIELD ANGLE TX TY TZ
# [CN] 靶点位置: 磁场 角度 X Y Z (单位mm, cm需要转换)
read -r -d '' TARGET_DATA << 'ENDDATA'
080 2.0 -8.1743 0.0046 -1093.7313
080 4.0 -20.6120 0.0063 -851.8761
080 6.0 -37.8391 0.0077 -654.3163
080 8.0 -60.6090 0.0088 -468.7377
080 10.0 -89.3315 0.0100 -287.3597
100 2.0 -7.7287 0.0075 -1151.8117
100 4.0 -18.9698 0.0105 -932.5705
100 6.0 -33.7681 0.0131 -762.6098
100 8.0 -52.6612 0.0157 -608.5314
100 10.0 -76.0454 0.0186 -460.8262
120 2.0 -7.4478 0.0082 -1193.9326
120 4.0 -17.9688 0.0107 -988.3352
120 6.0 -31.3605 0.0133 -834.3431
120 8.0 -47.9353 0.0150 -699.0843
120 10.0 -68.0180 0.0169 -572.1932
160 2.0 -7.0397 0.0098 -1256.1967
160 4.0 -16.6920 0.0118 -1067.2291
160 6.0 -28.5461 0.0144 -930.7559
160 8.0 -42.6748 0.0158 -815.3566
160 10.0 -59.2219 0.0167 -710.7437
200 2.0 -6.6894 0.0134 -1309.1564
200 4.0 -15.6985 0.0153 -1132.5691
200 6.0 -26.5470 0.0178 -1007.5765
200 8.0 -39.2217 0.0192 -904.0010
200 10.0 -53.7761 0.0193 -811.9539
ENDDATA

echo "Generating Geant4 macro files (v2 - single ROOT tree)..."
echo ""

while IFS=' ' read -r field angle tx ty tz; do
    [[ -z "$field" ]] && continue
    
    macfile="${VIZDIR}/run_protons_B${field}T_${angle}deg.mac"
    # Convert mm to cm for Target/Position command
    tx_cm=$(echo "scale=4; $tx / 10" | bc)
    ty_cm=$(echo "scale=4; $ty / 10" | bc)
    tz_cm=$(echo "scale=4; $tz / 10" | bc)
    
    cat > "$macfile" << EOF
# =========================================================================
# [EN] Proton trajectory visualization using Tree input + Target parameters
# [CN] 使用Tree输入 + Target参数的质子轨迹可视化
# =========================================================================
# Field: B=${field:0:1}.${field:1}T, Beam angle=${angle} deg
# Target position: X=${tx}mm, Y=${ty}mm, Z=${tz}mm
# Proton momenta: Px = ±100, ±150 MeV/c (4 tracks total)
# =========================================================================

# 1. Visualization setup
/vis/open OGLIQt 1280x720
# /vis/open OGL


# 2. Set Target position and angle (CRITICAL for particle direction)
/samurai/geometry/Target/Position ${tx_cm} ${ty_cm} ${tz_cm} cm
/samurai/geometry/Target/Angle ${angle} deg

# 3. Load Geometry
/control/execute ${TRAJVIZDIR}/geometry_B${field}T.mac


/action/file/OverWrite y
/action/file/RunName protons_B${field}T_${angle}deg
/action/file/SaveDirectory ${SMSIMDIR}/data/simulation/g4output

# 4. Draw volume
/vis/drawVolume

# 5. Set Z-X plane view (top view)
/vis/viewer/set/upVector 1 0 0
/vis/viewer/set/viewpointThetaPhi 90. 90.

# 6. Add coordinate axes
/vis/scene/add/axes 0 0 0 6000 mm

# ==========================================
# Trajectory visualization settings
# ==========================================
/vis/scene/add/trajectories smooth
/vis/modeling/trajectories/create/drawByCharge
/vis/modeling/trajectories/drawByCharge-0/default/setDrawStepPts true
/vis/modeling/trajectories/drawByCharge-0/default/setStepPtsSize 3
/vis/scene/endOfEventAction accumulate
# /vis/scene/endOfRunAction accumulate

/vis/viewer/set/autoRefresh false
/vis/verbose warnings

# ==========================================
# Load proton data from single ROOT tree
# ==========================================
/action/gun/Type Tree
/action/gun/tree/InputFileName ${ROOTFILE}
/action/gun/tree/TreeName tree
/action/gun/tree/beamOn 4

# ==========================================
# Refresh and Save
# ==========================================
/vis/viewer/set/autoRefresh true
/vis/viewer/refresh
/vis/viewer/update


# Save screenshot
# /vis/viewer/save ${VIZDIR}/output/protons_B${field}T_${angle}deg.png  # this is not working

# Geant4 会自动根据后缀 .png 识别格式
#/vis/ogl/export /home/tian/workspace/dpol/smsimulator5.5/configs/simulation/DbeamTest/track_vis_useTree/output/protons_B080T_2.0deg.png

# /vis/ogl/set/exportFormat pdf
# /vis/viewer/set/background white
# /control/shell sleep 1
/vis/ogl/export ${VIZDIR}/output/protons_B${field}T_${angle}deg.png
# Alternative export commands:
# /vis/ogl/export ${VIZDIR}/output/protons_B${field}T_${angle}deg.png
# /vis/ogl/printEPS
EOF
    
    echo "Generated: run_protons_B${field}T_${angle}deg.mac"
done <<< "$TARGET_DATA"

echo ""
echo "Done. Total: 25 macro files"
echo "All use single ROOT tree: ${ROOTFILE}"
