#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "Usage: $0 <sim.root> <geometry.mac> [output_dir] [max_events]"
    exit 1
fi

SIM_ROOT="$1"
GEOM_MACRO="$2"
OUT_DIR="${3:-data/nn_target_momentum/run}"
MAX_EVENTS="${4:-0}"
TARGET_NORMALIZATION="${TARGET_NORMALIZATION:-none}"
LOSS_WEIGHTING="${LOSS_WEIGHTING:-none}"
LOSS_WEIGHTS="${LOSS_WEIGHTS:-}"
LR_SCHEDULER="${LR_SCHEDULER:-none}"
LR_SCHEDULER_PATIENCE="${LR_SCHEDULER_PATIENCE:-5}"
LR_SCHEDULER_FACTOR="${LR_SCHEDULER_FACTOR:-0.5}"
MIN_LR="${MIN_LR:-1e-5}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

mkdir -p "${OUT_DIR}"

DATASET_ROOT="${OUT_DIR}/dataset_train.root"
DATASET_CSV="${OUT_DIR}/dataset_train.csv"
MODEL_DIR="${OUT_DIR}/model"
PRED_CSV="${OUT_DIR}/predictions.csv"
METRICS_JSON="${OUT_DIR}/metrics.json"

pushd "${REPO_DIR}" >/dev/null

echo "[run_pipeline] Step 1/4: Build dataset"
root -l -q "scripts/reconstruction/nn_target_momentum/build_dataset.C+(\"${SIM_ROOT}\",\"${GEOM_MACRO}\",${MAX_EVENTS},0.5,0.5,\"${DATASET_ROOT}\",\"${DATASET_CSV}\")"

echo "[run_pipeline] Step 2/4: Train model"
TRAIN_ARGS=(
    --input-csv "${DATASET_CSV}"
    --output-dir "${MODEL_DIR}"
    --target-normalization "${TARGET_NORMALIZATION}"
    --loss-weighting "${LOSS_WEIGHTING}"
    --lr-scheduler "${LR_SCHEDULER}"
    --lr-scheduler-patience "${LR_SCHEDULER_PATIENCE}"
    --lr-scheduler-factor "${LR_SCHEDULER_FACTOR}"
    --min-lr "${MIN_LR}"
)
if [[ -n "${LOSS_WEIGHTS}" ]]; then
    TRAIN_ARGS+=(--loss-weights "${LOSS_WEIGHTS}")
fi
python3 "scripts/reconstruction/nn_target_momentum/train_mlp.py" "${TRAIN_ARGS[@]}"

echo "[run_pipeline] Step 3/4: Inference"
python3 "scripts/reconstruction/nn_target_momentum/infer_mlp.py" \
    --input-csv "${DATASET_CSV}" \
    --model-path "${MODEL_DIR}/model.pt" \
    --meta-path "${MODEL_DIR}/model_meta.json" \
    --output-csv "${PRED_CSV}"

echo "[run_pipeline] Step 4/4: Evaluate"
python3 "scripts/reconstruction/nn_target_momentum/evaluate_mlp.py" \
    --pred-csv "${PRED_CSV}" \
    --output-json "${METRICS_JSON}"

popd >/dev/null

echo "[run_pipeline] Done"
echo "[run_pipeline] Output directory: ${OUT_DIR}"
