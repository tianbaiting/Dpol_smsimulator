#!/bin/bash
# =========================================================================
# [EN] Batch run trajectory visualization and export VRML
# [CN] 批量运行轨迹可视化并导出VRML
# Usage: ./run_batch_export.sh [OPTIONS]
# =========================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

FIELDS="080,100,120,160,200"
ANGLES="2.0,4.0,6.0,8.0,10.0"
OUTPUT_DIR="$SCRIPT_DIR/output_png"
DRY_RUN=false
SMSIMDIR="/home/tian/workspace/dpol/smsimulator5.5"

while [[ $# -gt 0 ]]; do
    case $1 in
        --dry-run) DRY_RUN=true; shift ;;
        --fields) FIELDS="$2"; shift 2 ;;
        --angles) ANGLES="$2"; shift 2 ;;
        --output) OUTPUT_DIR="$2"; shift 2 ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "  --fields FIELDS   Comma-separated field codes"
            echo "  --angles ANGLES   Comma-separated angles"
            echo "  --output DIR      Output directory"
            echo "  --dry-run         Print without executing"
            exit 0 ;;
        *) echo "Unknown: $1"; exit 1 ;;
    esac
done

SIM_BIN="$SMSIMDIR/build/bin/sim_deuteron"
mkdir -p "$OUTPUT_DIR"

if [ ! -x "$SIM_BIN" ]; then
    echo "[ERROR] sim_deuteron not found: $SIM_BIN"
    exit 1
fi

IFS=',' read -ra FIELD_ARR <<< "$FIELDS"
IFS=',' read -ra ANGLE_ARR <<< "$ANGLES"

echo "============================================"
echo "Batch Trajectory Export"
echo "Fields: ${FIELD_ARR[*]}"
echo "Angles: ${ANGLE_ARR[*]}"
echo "Output: $OUTPUT_DIR"
echo "DryRun: $DRY_RUN"
echo "============================================"

for FIELD in "${FIELD_ARR[@]}"; do
    for ANGLE in "${ANGLE_ARR[@]}"; do
        MAC_FILE="$SCRIPT_DIR/run_trajectory_B${FIELD}T_${ANGLE}deg.mac"
        
        if [ ! -f "$MAC_FILE" ]; then
            echo "[SKIP] $MAC_FILE not found"
            continue
        fi
        
        echo "[RUN] B=${FIELD}T, Angle=${ANGLE}deg"
        
        if [ "$DRY_RUN" = true ]; then
            echo "  [DRY-RUN] Would run: $SIM_BIN"
            continue
        fi
        
        # Create batch macro with VRML output
        BATCH_MAC=$(mktemp --suffix=.mac)
        cat > "$BATCH_MAC" << EOF
# Batch macro for VRML export
/vis/open VRML2FILE
/vis/viewer/set/autoRefresh false

# Load geometry
/control/execute $SCRIPT_DIR/geometry_B${FIELD}T.mac

/action/file/OverWrite y
/action/file/RunName trajectory_B${FIELD}T_${ANGLE}deg
/action/file/SaveDirectory $SMSIMDIR/data/simulation/g4output

# View settings
/vis/viewer/set/upVector 1 0 0
/vis/viewer/set/viewpointThetaPhi 90. 90.
/vis/drawVolume
/vis/scene/add/axes 0 0 0 6000 mm

# Trajectory accumulation
/vis/scene/endOfEventAction accumulate
/vis/scene/add/trajectories smooth
/vis/modeling/trajectories/create/drawByCharge
/vis/modeling/trajectories/drawByCharge-0/default/setDrawStepPts true
/vis/modeling/trajectories/drawByCharge-0/default/setStepPtsSize 1
/vis/scene/endOfRunAction accumulate

# Run particles from main config
/control/execute $MAC_FILE
/vis/viewer/flush
exit
EOF
        
        # But the main config already has /vis/open, so let's create a simpler approach
        # Just run the main macro but replace OGLIQt with VRML2FILE
        sed 's/OGLIQt/VRML2FILE/g' "$MAC_FILE" > "$BATCH_MAC"
        echo "exit" >> "$BATCH_MAC"
        
        # Run simulation
        "$SIM_BIN" "$BATCH_MAC" > "${OUTPUT_DIR}/log_B${FIELD}T_${ANGLE}deg.txt" 2>&1 || true
        
        # Move VRML output
        for wrl in g4_*.wrl G4Data*.wrl; do
            if [ -f "$wrl" ]; then
                mv "$wrl" "${OUTPUT_DIR}/trajectory_B${FIELD}T_${ANGLE}deg.wrl"
                echo "  [OK] VRML: trajectory_B${FIELD}T_${ANGLE}deg.wrl"
                break
            fi
        done
        
        rm -f "$BATCH_MAC"
    done
done

echo ""
echo "============================================"
echo "Done! Files saved to: $OUTPUT_DIR"
echo "============================================"
