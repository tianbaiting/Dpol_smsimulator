#!/usr/bin/env bash
set -euo pipefail

DRY_RUN=0
if [[ "${1:-}" == "--dry-run" ]]; then
    DRY_RUN=1
    shift
fi

GEOM_MACRO_INPUT="${1:-build/bin/configs/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227.mac}"
OUT_BASE_INPUT="${2:-data/nn_target_momentum/formal_B115T3deg}"

TRAIN_EVENTS="${TRAIN_EVENTS:-200000}"
VAL_EVENTS="${VAL_EVENTS:-30000}"
TEST_EVENTS="${TEST_EVENTS:-30000}"

TRAIN_SEED="${TRAIN_SEED:-20260301}"
VAL_SEED="${VAL_SEED:-20260302}"
TEST_SEED="${TEST_SEED:-20260303}"
TARGET_NORMALIZATION="${TARGET_NORMALIZATION:-none}"
LOSS_WEIGHTING="${LOSS_WEIGHTING:-none}"
LOSS_WEIGHTS="${LOSS_WEIGHTS:-}"
LR_SCHEDULER="${LR_SCHEDULER:-none}"
LR_SCHEDULER_PATIENCE="${LR_SCHEDULER_PATIENCE:-5}"
LR_SCHEDULER_FACTOR="${LR_SCHEDULER_FACTOR:-0.5}"
MIN_LR="${MIN_LR:-1e-5}"

R_MAX_MEVC="${R_MAX_MEVC:-150}"
PZ_MIN_MEVC="${PZ_MIN_MEVC:-500}"
PZ_MAX_MEVC="${PZ_MAX_MEVC:-700}"

TARGET_X_MM="${TARGET_X_MM:--12.4883}"
TARGET_Y_MM="${TARGET_Y_MM:-0.0089}"
TARGET_Z_MM="${TARGET_Z_MM:--1069.4585}"
TARGET_X_CM="${TARGET_X_CM:--1.2488}"
TARGET_Y_CM="${TARGET_Y_CM:-0.0009}"
TARGET_Z_CM="${TARGET_Z_CM:--106.9458}"
TARGET_ANGLE_DEG="${TARGET_ANGLE_DEG:-3.0}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

SIM_BIN="${SIM_BIN:-${REPO_DIR}/build/bin/sim_deuteron}"
ROOT_BIN="${ROOT_BIN:-root}"

TEMPLATE_MACRO="${REPO_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/run_nntrain_B115T_3deg_template.mac"
GEN_TREE_MACRO="${REPO_DIR}/scripts/reconstruction/nn_target_momentum/generate_tree_input_disk_pz.C"
BUILD_DATA_MACRO="${REPO_DIR}/scripts/reconstruction/nn_target_momentum/build_dataset.C"
TRAIN_PY="${REPO_DIR}/scripts/reconstruction/nn_target_momentum/train_mlp.py"
INFER_PY="${REPO_DIR}/scripts/reconstruction/nn_target_momentum/infer_mlp.py"
EVAL_PY="${REPO_DIR}/scripts/reconstruction/nn_target_momentum/evaluate_mlp.py"

resolve_path() {
    local input_path="$1"
    if [[ -f "$input_path" ]]; then
        cd "$(dirname "$input_path")" >/dev/null
        pwd
        cd - >/dev/null
        return 0
    fi
    if [[ -f "${REPO_DIR}/${input_path}" ]]; then
        cd "$(dirname "${REPO_DIR}/${input_path}")" >/dev/null
        pwd
        cd - >/dev/null
        return 0
    fi
    return 1
}

abs_file() {
    local input_path="$1"
    if [[ -f "$input_path" ]]; then
        echo "$(cd "$(dirname "$input_path")" && pwd)/$(basename "$input_path")"
        return
    fi
    if [[ -f "${REPO_DIR}/${input_path}" ]]; then
        echo "$(cd "$(dirname "${REPO_DIR}/${input_path}")" && pwd)/$(basename "$input_path")"
        return
    fi
    echo ""
}

if ! command -v "${ROOT_BIN}" >/dev/null 2>&1; then
    echo "[run_formal_training] root command not found: ${ROOT_BIN}" >&2
    exit 1
fi

if [[ ! -x "${SIM_BIN}" ]]; then
    echo "[run_formal_training] sim binary not found or not executable: ${SIM_BIN}" >&2
    exit 1
fi

if [[ ! -f "${TEMPLATE_MACRO}" ]]; then
    echo "[run_formal_training] template macro missing: ${TEMPLATE_MACRO}" >&2
    exit 1
fi

for f in "${GEN_TREE_MACRO}" "${BUILD_DATA_MACRO}" "${TRAIN_PY}" "${INFER_PY}" "${EVAL_PY}"; do
    if [[ ! -f "$f" ]]; then
        echo "[run_formal_training] required file missing: $f" >&2
        exit 1
    fi
done

GEOM_ABS="$(abs_file "${GEOM_MACRO_INPUT}")"
if [[ -z "${GEOM_ABS}" ]]; then
    echo "[run_formal_training] geometry macro not found: ${GEOM_MACRO_INPUT}" >&2
    exit 1
fi

if [[ "${OUT_BASE_INPUT}" = /* ]]; then
    OUT_BASE="${OUT_BASE_INPUT}"
else
    OUT_BASE="${REPO_DIR}/${OUT_BASE_INPUT}"
fi

if [[ -n "${PYTHON_EXE:-}" ]]; then
    PY_RUN=("${PYTHON_EXE}")
elif command -v micromamba >/dev/null 2>&1 && micromamba env list | grep -qE '^\s*anaroot-env\b'; then
    PY_RUN=(micromamba run -n anaroot-env python)
else
    PY_RUN=(python3)
fi

run_cmd() {
    echo "+ $*"
    if [[ "${DRY_RUN}" -eq 0 ]]; then
        "$@"
    fi
}

csv_rows() {
    local path="$1"
    if [[ ! -f "$path" ]]; then
        echo 0
        return
    fi
    local lines
    lines=$(wc -l < "$path")
    if [[ "$lines" -le 1 ]]; then
        echo 0
    else
        echo $((lines - 1))
    fi
}

render_run_macro() {
    local output_macro="$1"
    local run_name="$2"
    local input_tree="$3"
    local n_events="$4"

    sed \
        -e "s|__RUN_NAME__|${run_name}|g" \
        -e "s|__SAVE_DIR__|${SIM_ROOT_DIR}|g" \
        -e "s|__INPUT_TREE__|${input_tree}|g" \
        -e "s|__N_EVENTS__|${n_events}|g" \
        -e "s|__GEOMETRY_MACRO__|${GEOM_ABS}|g" \
        -e "s|__TARGET_POS_X_CM__|${TARGET_X_CM}|g" \
        -e "s|__TARGET_POS_Y_CM__|${TARGET_Y_CM}|g" \
        -e "s|__TARGET_POS_Z_CM__|${TARGET_Z_CM}|g" \
        -e "s|__TARGET_ANGLE_DEG__|${TARGET_ANGLE_DEG}|g" \
        "${TEMPLATE_MACRO}" > "${output_macro}"
}

run_root_macro() {
    local call="$1"
    run_cmd "${ROOT_BIN}" -l -b -q "$call"
}

run_root_macro_expect_outputs() {
    local call="$1"
    shift
    local expected_outputs=("$@")

    echo "+ ${ROOT_BIN} -l -b -q ${call}"
    if [[ "${DRY_RUN}" -eq 1 ]]; then
        return 0
    fi

    local rc=0
    set +e
    "${ROOT_BIN}" -l -b -q "$call"
    rc=$?
    set -e

    if [[ "${rc}" -eq 0 ]]; then
        return 0
    fi

    local missing=0
    for path in "${expected_outputs[@]}"; do
        if [[ ! -s "${path}" ]]; then
            echo "[run_formal_training] expected output missing after ROOT exit ${rc}: ${path}" >&2
            missing=1
        fi
    done

    if [[ "${missing}" -eq 0 ]]; then
        echo "[run_formal_training] warning: ROOT exited with ${rc} but expected outputs exist; continuing" >&2
        return 0
    fi

    return "${rc}"
}

find_sim_root() {
    local run_name="$1"
    local candidate="${SIM_ROOT_DIR}/${run_name}0000.root"
    if [[ -f "${candidate}" ]]; then
        echo "${candidate}"
        return
    fi
    local fallback
    fallback=$(ls -1 "${SIM_ROOT_DIR}/${run_name}"*.root 2>/dev/null | head -n 1 || true)
    echo "${fallback}"
}

STAMP="$(date +%Y%m%d_%H%M%S)"
OUT_DIR="${OUT_BASE}/${STAMP}"
TREE_DIR="${OUT_DIR}/tree_input"
MACRO_DIR="${OUT_DIR}/macros"
SIM_ROOT_DIR="${OUT_DIR}/sim_root"
DATASET_DIR="${OUT_DIR}/dataset"
MODEL_DIR="${OUT_DIR}/model"
PRED_DIR="${OUT_DIR}/predictions"
MANIFEST_PATH="${OUT_DIR}/manifest.json"

run_cmd mkdir -p "${TREE_DIR}" "${MACRO_DIR}" "${SIM_ROOT_DIR}" "${DATASET_DIR}" "${MODEL_DIR}" "${PRED_DIR}"

TREE_TRAIN="${TREE_DIR}/tree_input_train.root"
TREE_VAL="${TREE_DIR}/tree_input_val.root"
TREE_TEST="${TREE_DIR}/tree_input_test.root"

SIM_RUN_TRAIN="nntrain_B115T_3deg_train"
SIM_RUN_VAL="nntrain_B115T_3deg_val"
SIM_RUN_TEST="nntrain_B115T_3deg_test"

MACRO_TRAIN="${MACRO_DIR}/run_train.mac"
MACRO_VAL="${MACRO_DIR}/run_val.mac"
MACRO_TEST="${MACRO_DIR}/run_test.mac"

DATASET_TRAIN_ROOT="${DATASET_DIR}/dataset_train.root"
DATASET_VAL_ROOT="${DATASET_DIR}/dataset_val.root"
DATASET_TEST_ROOT="${DATASET_DIR}/dataset_test.root"

DATASET_TRAIN_CSV="${DATASET_DIR}/dataset_train.csv"
DATASET_VAL_CSV="${DATASET_DIR}/dataset_val.csv"
DATASET_TEST_CSV="${DATASET_DIR}/dataset_test.csv"

VAL_PRED_CSV="${PRED_DIR}/predictions_val.csv"
TEST_PRED_CSV="${PRED_DIR}/predictions_test.csv"
VAL_METRICS_JSON="${PRED_DIR}/metrics_val.json"
TEST_METRICS_JSON="${PRED_DIR}/metrics_test.json"

echo "[run_formal_training] output directory: ${OUT_DIR}"
echo "[run_formal_training] geometry: ${GEOM_ABS}"
echo "[run_formal_training] python runner: ${PY_RUN[*]}"

printf -v GEN_TRAIN_CALL '%s+(%d,%d,%.10g,%.10g,%.10g,%.10g,%.10g,%.10g,"%s")' \
    "${GEN_TREE_MACRO}" "${TRAIN_EVENTS}" "${TRAIN_SEED}" "${R_MAX_MEVC}" "${PZ_MIN_MEVC}" "${PZ_MAX_MEVC}" \
    "${TARGET_X_MM}" "${TARGET_Y_MM}" "${TARGET_Z_MM}" "${TREE_TRAIN}"
printf -v GEN_VAL_CALL '%s+(%d,%d,%.10g,%.10g,%.10g,%.10g,%.10g,%.10g,"%s")' \
    "${GEN_TREE_MACRO}" "${VAL_EVENTS}" "${VAL_SEED}" "${R_MAX_MEVC}" "${PZ_MIN_MEVC}" "${PZ_MAX_MEVC}" \
    "${TARGET_X_MM}" "${TARGET_Y_MM}" "${TARGET_Z_MM}" "${TREE_VAL}"
printf -v GEN_TEST_CALL '%s+(%d,%d,%.10g,%.10g,%.10g,%.10g,%.10g,%.10g,"%s")' \
    "${GEN_TREE_MACRO}" "${TEST_EVENTS}" "${TEST_SEED}" "${R_MAX_MEVC}" "${PZ_MIN_MEVC}" "${PZ_MAX_MEVC}" \
    "${TARGET_X_MM}" "${TARGET_Y_MM}" "${TARGET_Z_MM}" "${TREE_TEST}"

run_root_macro "${GEN_TRAIN_CALL}"
run_root_macro "${GEN_VAL_CALL}"
run_root_macro "${GEN_TEST_CALL}"

run_cmd render_run_macro "${MACRO_TRAIN}" "${SIM_RUN_TRAIN}" "${TREE_TRAIN}" "${TRAIN_EVENTS}"
run_cmd render_run_macro "${MACRO_VAL}" "${SIM_RUN_VAL}" "${TREE_VAL}" "${VAL_EVENTS}"
run_cmd render_run_macro "${MACRO_TEST}" "${SIM_RUN_TEST}" "${TREE_TEST}" "${TEST_EVENTS}"

run_cmd "${SIM_BIN}" "${MACRO_TRAIN}"
run_cmd "${SIM_BIN}" "${MACRO_VAL}"
run_cmd "${SIM_BIN}" "${MACRO_TEST}"

SIM_ROOT_TRAIN="${SIM_ROOT_DIR}/${SIM_RUN_TRAIN}0000.root"
SIM_ROOT_VAL="${SIM_ROOT_DIR}/${SIM_RUN_VAL}0000.root"
SIM_ROOT_TEST="${SIM_ROOT_DIR}/${SIM_RUN_TEST}0000.root"

if [[ "${DRY_RUN}" -eq 0 ]]; then
    SIM_ROOT_TRAIN="$(find_sim_root "${SIM_RUN_TRAIN}")"
    SIM_ROOT_VAL="$(find_sim_root "${SIM_RUN_VAL}")"
    SIM_ROOT_TEST="$(find_sim_root "${SIM_RUN_TEST}")"
fi

if [[ "${DRY_RUN}" -eq 0 ]]; then
    for sim_file in "${SIM_ROOT_TRAIN}" "${SIM_ROOT_VAL}" "${SIM_ROOT_TEST}"; do
        if [[ -z "${sim_file}" || ! -f "${sim_file}" ]]; then
            echo "[run_formal_training] simulation output ROOT not found" >&2
            exit 1
        fi
    done
fi

printf -v BUILD_TRAIN_CALL '%s+("%s","%s",0,0.5,0.5,"%s","%s")' \
    "${BUILD_DATA_MACRO}" "${SIM_ROOT_TRAIN}" "${MACRO_TRAIN}" "${DATASET_TRAIN_ROOT}" "${DATASET_TRAIN_CSV}"
printf -v BUILD_VAL_CALL '%s+("%s","%s",0,0.5,0.5,"%s","%s")' \
    "${BUILD_DATA_MACRO}" "${SIM_ROOT_VAL}" "${MACRO_VAL}" "${DATASET_VAL_ROOT}" "${DATASET_VAL_CSV}"
printf -v BUILD_TEST_CALL '%s+("%s","%s",0,0.5,0.5,"%s","%s")' \
    "${BUILD_DATA_MACRO}" "${SIM_ROOT_TEST}" "${MACRO_TEST}" "${DATASET_TEST_ROOT}" "${DATASET_TEST_CSV}"

run_root_macro_expect_outputs "${BUILD_TRAIN_CALL}" "${DATASET_TRAIN_ROOT}" "${DATASET_TRAIN_CSV}"
run_root_macro_expect_outputs "${BUILD_VAL_CALL}" "${DATASET_VAL_ROOT}" "${DATASET_VAL_CSV}"
run_root_macro_expect_outputs "${BUILD_TEST_CALL}" "${DATASET_TEST_ROOT}" "${DATASET_TEST_CSV}"

TRAIN_ARGS=(
    --train-csv "${DATASET_TRAIN_CSV}"
    --val-csv "${DATASET_VAL_CSV}"
    --test-csv "${DATASET_TEST_CSV}"
    --output-dir "${MODEL_DIR}"
    --seed 20260227
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

run_cmd "${PY_RUN[@]}" "${TRAIN_PY}" "${TRAIN_ARGS[@]}"

run_cmd "${PY_RUN[@]}" "${INFER_PY}" \
    --input-csv "${DATASET_VAL_CSV}" \
    --model-path "${MODEL_DIR}/model.pt" \
    --meta-path "${MODEL_DIR}/model_meta.json" \
    --output-csv "${VAL_PRED_CSV}"

run_cmd "${PY_RUN[@]}" "${EVAL_PY}" \
    --pred-csv "${VAL_PRED_CSV}" \
    --output-json "${VAL_METRICS_JSON}"

run_cmd "${PY_RUN[@]}" "${INFER_PY}" \
    --input-csv "${DATASET_TEST_CSV}" \
    --model-path "${MODEL_DIR}/model.pt" \
    --meta-path "${MODEL_DIR}/model_meta.json" \
    --output-csv "${TEST_PRED_CSV}"

run_cmd "${PY_RUN[@]}" "${EVAL_PY}" \
    --pred-csv "${TEST_PRED_CSV}" \
    --output-json "${TEST_METRICS_JSON}"

if [[ "${DRY_RUN}" -eq 0 ]]; then
    GEOM_SHA256=""
    if command -v sha256sum >/dev/null 2>&1; then
        GEOM_SHA256="$(sha256sum "${GEOM_ABS}" | awk '{print $1}')"
    fi

    TRAIN_ROWS="$(csv_rows "${DATASET_TRAIN_CSV}")"
    VAL_ROWS="$(csv_rows "${DATASET_VAL_CSV}")"
    TEST_ROWS="$(csv_rows "${DATASET_TEST_CSV}")"

    export MANIFEST_PATH GEOM_ABS GEOM_SHA256 TARGET_ANGLE_DEG
    export R_MAX_MEVC PZ_MIN_MEVC PZ_MAX_MEVC
    export TARGET_X_MM TARGET_Y_MM TARGET_Z_MM TARGET_X_CM TARGET_Y_CM TARGET_Z_CM
    export TRAIN_EVENTS VAL_EVENTS TEST_EVENTS TRAIN_SEED VAL_SEED TEST_SEED
    export TREE_TRAIN TREE_VAL TREE_TEST
    export SIM_ROOT_TRAIN SIM_ROOT_VAL SIM_ROOT_TEST
    export DATASET_TRAIN_CSV DATASET_VAL_CSV DATASET_TEST_CSV
    export TRAIN_ROWS VAL_ROWS TEST_ROWS
    export MODEL_DIR VAL_METRICS_JSON TEST_METRICS_JSON

    "${PY_RUN[@]}" - <<'PY'
import json
import os
from pathlib import Path

manifest_path = Path(os.environ["MANIFEST_PATH"])
manifest = {
    "campaign": "formal_B115T3deg_diskR150_pz500_700",
    "geometry": {
        "path": os.environ["GEOM_ABS"],
        "sha256": os.environ.get("GEOM_SHA256", ""),
    },
    "target": {
        "angle_deg": float(os.environ["TARGET_ANGLE_DEG"]),
        "position_mm": [
            float(os.environ["TARGET_X_MM"]),
            float(os.environ["TARGET_Y_MM"]),
            float(os.environ["TARGET_Z_MM"]),
        ],
        "position_cm_for_macro": [
            float(os.environ["TARGET_X_CM"]),
            float(os.environ["TARGET_Y_CM"]),
            float(os.environ["TARGET_Z_CM"]),
        ],
    },
    "momentum_sampling": {
        "px_py": {
            "shape": "uniform_disk",
            "r_max_mevc": float(os.environ["R_MAX_MEVC"]),
        },
        "pz": {
            "distribution": "uniform",
            "min_mevc": float(os.environ["PZ_MIN_MEVC"]),
            "max_mevc": float(os.environ["PZ_MAX_MEVC"]),
        },
    },
    "splits": {
        "train": {
            "requested_events": int(os.environ["TRAIN_EVENTS"]),
            "seed": int(os.environ["TRAIN_SEED"]),
            "tree_input_root": os.environ["TREE_TRAIN"],
            "sim_root": os.environ["SIM_ROOT_TRAIN"],
            "dataset_csv": os.environ["DATASET_TRAIN_CSV"],
            "dataset_rows": int(os.environ["TRAIN_ROWS"]),
        },
        "val": {
            "requested_events": int(os.environ["VAL_EVENTS"]),
            "seed": int(os.environ["VAL_SEED"]),
            "tree_input_root": os.environ["TREE_VAL"],
            "sim_root": os.environ["SIM_ROOT_VAL"],
            "dataset_csv": os.environ["DATASET_VAL_CSV"],
            "dataset_rows": int(os.environ["VAL_ROWS"]),
        },
        "test": {
            "requested_events": int(os.environ["TEST_EVENTS"]),
            "seed": int(os.environ["TEST_SEED"]),
            "tree_input_root": os.environ["TREE_TEST"],
            "sim_root": os.environ["SIM_ROOT_TEST"],
            "dataset_csv": os.environ["DATASET_TEST_CSV"],
            "dataset_rows": int(os.environ["TEST_ROWS"]),
        },
    },
    "artifacts": {
        "model_dir": os.environ["MODEL_DIR"],
        "model_meta_json": str(Path(os.environ["MODEL_DIR"]) / "model_meta.json"),
        "metrics_val_json": os.environ["VAL_METRICS_JSON"],
        "metrics_test_json": os.environ["TEST_METRICS_JSON"],
    },
}

manifest_path.parent.mkdir(parents=True, exist_ok=True)
with manifest_path.open("w", encoding="utf-8") as fout:
    json.dump(manifest, fout, indent=2)
PY
fi

echo "[run_formal_training] done"
echo "[run_formal_training] output directory: ${OUT_DIR}"
if [[ "${DRY_RUN}" -eq 1 ]]; then
    echo "[run_formal_training] dry-run mode: no command executed"
fi
