#!/bin/bash
# Run the Air-off MS ablation experiment (v1).
# Two conditions (baseline / no_air) over 3 truth points × ENSEMBLE_SIZE seeds.
set -euo pipefail

SMSIM_DIR=${SMSIM_DIR:-/home/tian/workspace/dpol/smsimulator5.5}
BUILD_DIR=${BUILD_DIR:-${SMSIM_DIR}/build}
ENSEMBLE_SIZE=${ENSEMBLE_SIZE:-50}
SEED_A=${SEED_A:-20260421}
SEED_B=${SEED_B:-20260422}
OUT_ROOT=${OUT_ROOT:-${BUILD_DIR}/test_output/ms_ablation_air}
RK_FIT_MODE=fixed-target-pdc-only

MAC_BASELINE=${SMSIM_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227.mac
MAC_NOAIR=${SMSIM_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_noair.mac
FIELD_MAP=${SMSIM_DIR}/configs/simulation/geometry/filed_map/180703-1,15T-3000.table
# [EN] nn-model-json is required by test_pdc_target_momentum_e2e argparse even when --backend=rk.
# Any valid NN model JSON works; we reuse the one from run_ensemble_coverage.sh baseline.
NN_MODEL_JSON=${SMSIM_DIR}/data/nn_target_momentum/formal_B115T3deg_qmdwindow/20260227_223007/model/model_cpp.json

# Sanity
for f in "$MAC_BASELINE" "$MAC_NOAIR" "$FIELD_MAP" "$NN_MODEL_JSON" \
         "$BUILD_DIR/bin/test_pdc_target_momentum_e2e" \
         "$BUILD_DIR/bin/sim_deuteron" \
         "$BUILD_DIR/bin/reconstruct_target_momentum" \
         "$BUILD_DIR/bin/evaluate_target_momentum_reco"; do
    [[ -e "$f" ]] || { echo "MISSING: $f" >&2; exit 1; }
done

echo "=== MS ablation Air v1 ==="
echo "git HEAD:      $(cd "$SMSIM_DIR" && git rev-parse HEAD)"
echo "ENSEMBLE_SIZE: $ENSEMBLE_SIZE"
echo "SEEDS:         A=$SEED_A B=$SEED_B"
echo "OUT_ROOT:      $OUT_ROOT"
echo

run_condition() {
    local label=$1
    local mac=$2
    local outdir=${OUT_ROOT}/${label}
    local simdir=${outdir}/sim

    mkdir -p "$simdir"
    echo "--- [$label] simulating with $(basename "$mac") ---"

    "$BUILD_DIR/bin/test_pdc_target_momentum_e2e" \
        --backend rk \
        --smsimdir "$SMSIM_DIR" \
        --output-dir "$simdir" \
        --sim-bin "$BUILD_DIR/bin/sim_deuteron" \
        --reco-bin "$BUILD_DIR/bin/reconstruct_target_momentum" \
        --eval-bin "$BUILD_DIR/bin/evaluate_target_momentum_reco" \
        --geometry-macro "$mac" \
        --field-map "$FIELD_MAP" \
        --nn-model-json "$NN_MODEL_JSON" \
        --threshold-px 500 --threshold-py 500 --threshold-pz 500 \
        --seed-a "$SEED_A" --seed-b "$SEED_B" \
        --rk-fit-mode "$RK_FIT_MODE" \
        --require-gate-pass 0 --require-min-matched 1 \
        --ensemble-size "$ENSEMBLE_SIZE"

    local reco_root=${simdir}/reco/pdc_truth_grid_rk_${RK_FIT_MODE//-/_}_reco.root
    [[ -f "$reco_root" ]] || { echo "MISSING reco root: $reco_root" >&2; exit 1; }

    echo "--- [$label] computing M1+M2 metrics ---"
    micromamba run -n anaroot-env python3 \
        "$SMSIM_DIR/scripts/analysis/ms_ablation/compute_metrics.py" \
        --reco-root "$reco_root" \
        --out-dir "$outdir"
}

run_condition "baseline" "$MAC_BASELINE"
run_condition "no_air"   "$MAC_NOAIR"

echo
echo "=== Comparing baseline vs no_air ==="
micromamba run -n anaroot-env python3 \
    "$SMSIM_DIR/scripts/analysis/ms_ablation/compare_ablation.py" \
    --baseline-dir "$OUT_ROOT/baseline" \
    --noair-dir    "$OUT_ROOT/no_air" \
    --out-dir      "$OUT_ROOT"

echo
echo "=== Done. Outputs under: $OUT_ROOT ==="
ls -l "$OUT_ROOT"
