#!/usr/bin/env bash
# Driver: ensure SSHFS mount, run merge + plot steps.
# Usage: ./run_compare.sh [smoke|full]
set -euo pipefail

MODE="${1:-full}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$REPO_ROOT"

RK_BASE="/home/tian/workspace/sshDir/spana03/Dpol_smsimulator/data/reconstruction/dbreak_elastic"
NN_BASE="data/reconstruction/breakup_nn_20260503"
TAG="$(date +%Y%m%d)"
if [[ "$MODE" == "smoke" ]]; then
    OUT_DIR="build/test_output/rk_vs_nn_breakup/smoke"
else
    OUT_DIR="build/test_output/rk_vs_nn_breakup/${TAG}"
fi
mkdir -p "$OUT_DIR"

MNT_SCRIPT="/home/tian/workspace/sshDir/mnt"
MOUNT_POINT="/home/tian/workspace/sshDir/spana03/Dpol_smsimulator"

if ! mountpoint -q "$MOUNT_POINT"; then
    echo "[run_compare] SSHFS mount not active; calling ${MNT_SCRIPT} spana03 Dpol_smsimulator"
    if [[ ! -x "$MNT_SCRIPT" ]]; then
        echo "[run_compare] ERROR: mnt script not found or not executable: $MNT_SCRIPT" >&2
        exit 1
    fi
    "$MNT_SCRIPT" spana03 Dpol_smsimulator
    mountpoint -q "$MOUNT_POINT" || { echo "[run_compare] mount failed"; exit 1; }
fi
echo "[run_compare] SSHFS mount OK"

SCRIPT_DIR="scripts/analysis/rk_vs_nn_breakup"
MERGE_ARGS=(
    "${SCRIPT_DIR}/compare_rk_nn_breakup.py"
    --rk-reco-base "$RK_BASE"
    --nn-reco-base "$NN_BASE"
    --output "$OUT_DIR/merged_events.root"
)
if [[ "$MODE" == "smoke" ]]; then
    MERGE_ARGS+=(
        --cells d+Sn112E190g050ynp,d+Sn124E190g080ypn
        --zpol-cells d+Sn112E190g050znp:b01,d+Sn124E190g080zpn:b10
    )
fi
python "${MERGE_ARGS[@]}"

python "${SCRIPT_DIR}/make_figures.py" \
    --input "$OUT_DIR/merged_events.root" \
    --output-dir "$OUT_DIR/"

echo "[run_compare] done. Outputs in $OUT_DIR"
