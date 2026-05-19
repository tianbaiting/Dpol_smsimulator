#!/usr/bin/env bash
# Reconstruct the formal-training test split with both NN and RK backends, run the canonical evaluator on each, and emit comparison CSV + JSON for plotting / tex.
#
# Usage: scripts/analysis/nn_vs_rk_targetframe/run_compare.sh <FORMAL_RUN_DIR>
#   <FORMAL_RUN_DIR>: e.g. data/nn_target_momentum/formal_B115T3deg_targetframe/20260516_021328
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <FORMAL_RUN_DIR>" >&2
    exit 2
fi

RUN_DIR="$1"
if [[ ! -d "${RUN_DIR}" ]]; then
    echo "[compare] not a directory: ${RUN_DIR}" >&2
    exit 1
fi
RUN_DIR="$(cd "${RUN_DIR}" && pwd)"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
RECO_BIN="${REPO_DIR}/build/bin/reconstruct_target_momentum"
EVAL_BIN="${REPO_DIR}/build/bin/evaluate_target_momentum_reco"

TEST_SIM=$(ls "${RUN_DIR}/sim_root/"nntrain_*test*.root 2>/dev/null | head -1)
MODEL_JSON="${RUN_DIR}/model/model_cpp.json"
GEOM_MACRO=$(python3 -c "import json; print(json.load(open('${RUN_DIR}/manifest.json'))['geometry']['path'])")
FIELD_MAP="${FIELD_MAP:-${REPO_DIR}/configs/simulation/geometry/filed_map/180703-1,15T-3000.table}"
MAGNET_ROTATION_DEG="${MAGNET_ROTATION_DEG:-30}"

for f in "${TEST_SIM}" "${MODEL_JSON}" "${GEOM_MACRO}" "${RECO_BIN}" "${EVAL_BIN}" "${FIELD_MAP}"; do
    if [[ ! -e "$f" ]]; then
        echo "[compare] missing required input: $f" >&2
        exit 1
    fi
done

OUT_DIR="${RUN_DIR}/nn_vs_rk"
mkdir -p "${OUT_DIR}"

RECO_NN="${OUT_DIR}/reco_nn.root"
RECO_RK="${OUT_DIR}/reco_rk.root"
EVAL_NN_SUM="${OUT_DIR}/eval_nn_summary.csv"
EVAL_RK_SUM="${OUT_DIR}/eval_rk_summary.csv"
EVAL_NN_EVT="${OUT_DIR}/eval_nn_events.csv"
EVAL_RK_EVT="${OUT_DIR}/eval_rk_events.csv"

echo "[compare] test sim:    ${TEST_SIM}"
echo "[compare] model_cpp:   ${MODEL_JSON}"
echo "[compare] geometry:    ${GEOM_MACRO}"
echo "[compare] out dir:     ${OUT_DIR}"

echo "[compare] --- NN backend ---"
"${RECO_BIN}" \
    --input-file "${TEST_SIM}" \
    --output-file "${RECO_NN}" \
    --geometry-macro "${GEOM_MACRO}" \
    --backend nn \
    --nn-model-json "${MODEL_JSON}"

echo "[compare] --- RK backend ---"
"${RECO_BIN}" \
    --input-file "${TEST_SIM}" \
    --output-file "${RECO_RK}" \
    --geometry-macro "${GEOM_MACRO}" \
    --backend rk \
    --rk-fit-mode fixed-target-pdc-only \
    --magnetic-field-map "${FIELD_MAP}" \
    --magnet-rotation-deg "${MAGNET_ROTATION_DEG}"

echo "[compare] --- evaluate NN ---"
"${EVAL_BIN}" --input-file "${RECO_NN}" \
    --summary-csv "${EVAL_NN_SUM}" \
    --event-csv "${EVAL_NN_EVT}"

echo "[compare] --- evaluate RK ---"
"${EVAL_BIN}" --input-file "${RECO_RK}" \
    --summary-csv "${EVAL_RK_SUM}" \
    --event-csv "${EVAL_RK_EVT}"

echo "[compare] --- plot + JSON summary ---"
PY_RUN=(python3)
if command -v micromamba >/dev/null 2>&1 && micromamba env list | grep -qE '^\s*anaroot-env\b'; then
    PY_RUN=(micromamba run -n anaroot-env python)
fi
"${PY_RUN[@]}" "${SCRIPT_DIR}/make_figures.py" \
    --nn-events "${EVAL_NN_EVT}" \
    --rk-events "${EVAL_RK_EVT}" \
    --out-dir "${OUT_DIR}"

echo "[compare] done. outputs in ${OUT_DIR}"
