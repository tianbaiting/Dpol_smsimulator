#!/bin/bash
# =========================================================================
# [EN] Generate Geant4 macro files for 0-degree target configuration
# [CN] 生成0度靶点配置的Geant4宏文件
# =========================================================================
# Target position: (0, 0, -400) cm = (0, 0, -4000) mm
# Target angle: 0 deg (no rotation)
# =========================================================================

SMSIMDIR="${SMSIMDIR:-/home/tian/workspace/dpol/smsimulator5.5}"
VIZDIR="${SMSIMDIR}/configs/simulation/DbeamTest/Angle0deg"
TRAJVIZDIR="${SMSIMDIR}/configs/simulation/DbeamTest/trajectory_viz"
ROOTFILE="${VIZDIR}/rootfiles/pn_8tracks_0deg.root"

# [EN] Magnetic field strengths to scan / [CN] 扫描的磁场强度
FIELDS="080 100 120 140 160 180 200"

echo "Generating Geant4 macro files for 0-degree target..."
echo ""

for field in $FIELDS; do
    macfile="${VIZDIR}/run_pn_B${field}T_0deg.mac"
    
    cat > "$macfile" << MACEOF
# =========================================================================
# [EN] Proton + Neutron trajectory visualization at 0-degree target
# [CN] 0度靶点位置的质子+中子轨迹可视化
# =========================================================================
# Field: B=${field:0:1}.${field:1}T
# Target position: (0, 0, -400) cm = (0, 0, -4m)
# Target angle: 0 deg
# Tracks: 4 protons + 4 neutrons = 8 total
# =========================================================================

# 1. Visualization setup
/vis/open VRML2FILE

# 2. Set Target position and angle (FIXED at 0 deg)
/samurai/geometry/Target/Position 0 0 -400 cm
/samurai/geometry/Target/Angle 0 deg

# 3. Load Geometry
/control/execute ${TRAJVIZDIR}/geometry_B${field}T.mac

/action/file/OverWrite y
/action/file/RunName pn_B${field}T_0deg
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

/vis/viewer/set/autoRefresh false
/vis/verbose warnings

# ==========================================
# Load proton+neutron data from ROOT tree
# ==========================================
/action/gun/Type Tree
/action/gun/tree/InputFileName ${ROOTFILE}
/action/gun/tree/TreeName tree
/action/gun/tree/beamOn 8

# ==========================================
# Refresh and flush
# ==========================================
/vis/viewer/set/autoRefresh true
/vis/viewer/refresh
/vis/viewer/update
/vis/viewer/flush

exit
MACEOF
    
    echo "Generated: run_pn_B${field}T_0deg.mac"
done

echo ""
echo "Done. Total: $(echo $FIELDS | wc -w) macro files"
echo "ROOT tree: ${ROOTFILE}"
