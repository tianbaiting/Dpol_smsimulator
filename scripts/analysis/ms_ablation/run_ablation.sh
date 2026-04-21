#!/bin/bash
# Run the MS ablation experiment v2 (stage A').
# Three conditions:
#   - all_air         : FillAir=true,  BeamLineVacuum=false (= stage A baseline)
#   - beamline_vacuum : FillAir=true,  BeamLineVacuum=true  (NEW; experimentally real)
#   - all_vacuum      : FillAir=false                        (= stage A no_air)
# 3 truth points × ENSEMBLE_SIZE seeds per condition.
set -euo pipefail

SMSIM_DIR=${SMSIM_DIR:-/home/tian/workspace/dpol/smsimulator5.5}
BUILD_DIR=${BUILD_DIR:-${SMSIM_DIR}/build}
ENSEMBLE_SIZE=${ENSEMBLE_SIZE:-50}
SEED_A=${SEED_A:-20260421}
SEED_B=${SEED_B:-20260422}
OUT_ROOT=${OUT_ROOT:-${BUILD_DIR}/test_output/ms_ablation_air}
RK_FIT_MODE=${RK_FIT_MODE:-fixed-target-pdc-only}

# Normalize to absolute paths.
if [[ "${OUT_ROOT}" != /* ]]; then OUT_ROOT="$(pwd)/${OUT_ROOT}"; fi
if [[ "${BUILD_DIR}" != /* ]]; then BUILD_DIR="$(pwd)/${BUILD_DIR}"; fi

MAC_ALL_AIR=${SMSIM_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac
MAC_MIXED=${SMSIM_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg_mixed.mac
MAC_ALL_VAC=${SMSIM_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg_noair.mac
FIELD_MAP=${SMSIM_DIR}/configs/simulation/geometry/filed_map/180703-1,15T-3000.table
NN_MODEL_JSON=${NN_MODEL_JSON:-${SMSIM_DIR}/data/nn_target_momentum/domain_matched_retrain/20260228_002757/model/model_cpp.json}

for f in "$MAC_ALL_AIR" "$MAC_MIXED" "$MAC_ALL_VAC" "$FIELD_MAP" "$NN_MODEL_JSON" \
         "$BUILD_DIR/bin/test_pdc_target_momentum_e2e" \
         "$BUILD_DIR/bin/sim_deuteron" \
         "$BUILD_DIR/bin/reconstruct_target_momentum" \
         "$BUILD_DIR/bin/evaluate_target_momentum_reco"; do
    [[ -e "$f" ]] || { echo "MISSING: $f" >&2; exit 1; }
done

echo "=== MS ablation Air v2 (stage A') ==="
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

run_condition "all_air"         "$MAC_ALL_AIR"
run_condition "beamline_vacuum" "$MAC_MIXED"
run_condition "all_vacuum"      "$MAC_ALL_VAC"

echo
echo "=== Comparing three conditions ==="
micromamba run -n anaroot-env python3 \
    "$SMSIM_DIR/scripts/analysis/ms_ablation/compare_ablation.py" \
    --all-air-dir         "$OUT_ROOT/all_air" \
    --beamline-vacuum-dir "$OUT_ROOT/beamline_vacuum" \
    --all-vacuum-dir      "$OUT_ROOT/all_vacuum" \
    --out-dir             "$OUT_ROOT"

echo
echo "=== Done. Outputs under: $OUT_ROOT ==="
ls -l "$OUT_ROOT"
