#!/bin/bash
# =========================================================================
# [EN] Generate Geant4 macro files for tree-based trajectory visualization
# [CN] 生成基于Tree的轨迹可视化Geant4宏文件
# =========================================================================

SMSIMDIR="${SMSIMDIR:-/home/tian/workspace/dpol/smsimulator5.5}"
VIZDIR="${SMSIMDIR}/configs/simulation/DbeamTest/track_vis_useTree"
TRAJVIZDIR="${SMSIMDIR}/configs/simulation/DbeamTest/trajectory_viz"

# [EN] Target positions: FIELD ANGLE TX TY TZ ROTANGLE
# [CN] 靶点位置: 磁场 角度 X Y Z 旋转角
read -r -d '' TARGET_DATA << 'ENDDATA'
080 2.0 5.2866 0.0052 -599.4389 -2.0
080 4.0 12.2155 0.0064 -513.6437 -4.0
080 6.0 19.3064 0.0075 -436.9073 -6.0
080 8.0 26.1312 0.0084 -367.4178 -8.0
080 10.0 32.4076 0.0094 -304.0095 -10.0
100 2.0 -7.7287 0.0075 -1151.8117 -2.0
100 4.0 -18.9698 0.0105 -932.5705 -4.0
100 6.0 -33.7681 0.0131 -762.6098 -6.0
100 8.0 -52.6612 0.0157 -608.5314 -8.0
100 10.0 -76.0454 0.0186 -460.8262 -10.0
120 2.0 -20.3977 0.0097 -1697.9595 -2.0
120 4.0 -49.2207 0.0142 -1351.9449 -4.0
120 6.0 -79.5513 0.0184 -1056.1679 -6.0
120 8.0 -110.2287 0.0224 -802.4392 -8.0
120 10.0 -140.5538 0.0264 -587.9127 -10.0
160 2.0 -45.6012 0.0137 -2775.5735 -2.0
160 4.0 -108.4693 0.0212 -2158.6699 -4.0
160 6.0 -152.7034 0.0283 -1571.5049 -6.0
160 8.0 -172.5082 0.0349 -1100.8568 -8.0
160 10.0 -191.4073 0.0418 -696.2632 -10.0
200 2.0 -71.7287 0.0176 -3775.2148 -2.0
200 4.0 -152.3698 0.0275 -2907.8705 -4.0
200 6.0 -207.8011 0.0371 -2009.1295 -6.0
200 8.0 -238.0612 0.0461 -1305.5314 -8.0
200 10.0 -259.0454 0.0551 -714.8262 -10.0
ENDDATA

echo "Generating Geant4 macro files for tree-based visualization..."
echo ""

while IFS=' ' read -r field angle tx ty tz rotangle; do
    [[ -z "$field" ]] && continue
    
    macfile="${VIZDIR}/run_protons_B${field}T_${angle}deg.mac"
    rootfile="${VIZDIR}/rootfiles/protons_B${field}T_${angle}deg.root"
    
    cat > "$macfile" << EOF
# =========================================================================
# [EN] Proton trajectory visualization using Tree input
# [CN] 使用Tree输入的质子轨迹可视化
# =========================================================================
# Field: B=${field:0:1}.${field:1}T, Beam angle=${angle} deg
# Target position: X=${tx}mm, Y=${ty}mm, Z=${tz}mm
# Proton momenta: Px = ±100, ±150 MeV/c, Py = 0, Pz = 627 MeV/c
# =========================================================================

# 1. Visualization setup
/vis/open OGLIQt 1280x720

# 2. Load Geometry from trajectory_viz
/control/execute ${TRAJVIZDIR}/geometry_B${field}T.mac

# 3. Set Target angle for momentum direction
/samurai/geometry/Target/Angle ${rotangle} deg

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
# [CRITICAL] Trajectory visualization settings
# ==========================================
/vis/scene/add/trajectories smooth
/vis/modeling/trajectories/create/drawByCharge
/vis/modeling/trajectories/drawByCharge-0/default/setDrawStepPts true
/vis/modeling/trajectories/drawByCharge-0/default/setStepPtsSize 3
/vis/scene/endOfEventAction accumulate
/vis/scene/endOfRunAction accumulate

/vis/viewer/set/autoRefresh false
/vis/verbose warnings

# ==========================================
# [EN] Load proton data from ROOT tree
# [CN] 从ROOT树加载质子数据
# ==========================================
/action/gun/tree/InputFileName ${rootfile}
/action/gun/tree/TreeName tree
/action/gun/tree/beamOn 0

# ==========================================
# Refresh and Export
# ==========================================
/vis/viewer/set/autoRefresh true
/vis/viewer/refresh
/vis/viewer/update
/vis/viewer/zoomTo 0.3

# Export commands (run manually):
# /vis/ogl/export protons_B${field}T_${angle}deg.png
# /vis/ogl/printEPS
EOF
    
    echo "Generated: run_protons_B${field}T_${angle}deg.mac"
done <<< "$TARGET_DATA"

echo ""
echo "Done. Macro files in: ${VIZDIR}/"
ls -la "${VIZDIR}/"*.mac 2>/dev/null | wc -l
echo "total macro files generated"
