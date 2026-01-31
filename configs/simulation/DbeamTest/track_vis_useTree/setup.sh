#!/bin/bash
# =========================================================================
# [EN] Setup script for proton trajectory visualization
# [CN] 质子轨迹可视化设置脚本
# =========================================================================

SMSIMDIR="${SMSIMDIR:-/home/tian/workspace/dpol/smsimulator5.5}"
VIZDIR="${SMSIMDIR}/configs/simulation/DbeamTest/track_vis_useTree"

echo "=========================================="
echo "Setup Proton Trajectory Visualization"
echo "=========================================="

# Step 1: Create directories
mkdir -p "${VIZDIR}/rootfiles"
mkdir -p "${VIZDIR}/output"

# Step 2: Generate single ROOT tree with 8 tracks (4 protons + 4 neutrons)
echo ""
echo "[Step 1] Generating ROOT tree with 8 tracks (4p + 4n)..."
cd "${VIZDIR}"

# Correct ROOT command syntax
root -l -b -q 'GenProtonTree_Single.C("rootfiles/pn_8tracks.root")' 2>&1

if [[ -f "${VIZDIR}/rootfiles/pn_8tracks.root" ]]; then
    echo "[OK] ROOT tree generated: rootfiles/pn_8tracks.root"
else
    echo "[ERROR] Failed to generate ROOT tree"
    exit 1
fi

# Step 3: Generate all macro files
echo ""
echo "[Step 2] Generating Geant4 macro files..."
./generate_all_macros_v2.sh 2>&1 | tail -5

echo ""
echo "=========================================="
echo "Setup complete!"
echo "=========================================="
echo ""
echo "Usage:"
echo "  Interactive: \$SMSIMDIR/build/bin/sim_deuteron"
echo "               /control/execute run_protons_B100T_6.0deg.mac"
echo ""
echo "  Batch:       ./run_batch_v2.sh --fields \"100\" --angles \"6.0\""
