#!/usr/bin/env bash
set -euo pipefail

DRY_RUN=0
if [[ "${1:-}" == "--dry-run" ]]; then
    DRY_RUN=1
    shift
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

INPUT_G4OUTPUT_DIR="${INPUT_G4OUTPUT_DIR:-${REPO_DIR}/data/simulation/g4output/eval_20260227_235751}"
BASELINE_RECO_DIR="${BASELINE_RECO_DIR:-${REPO_DIR}/data/reconstruction/eval_20260227_235751}"
GEOMETRY_MACRO="${GEOMETRY_MACRO:-${REPO_DIR}/build/bin/configs/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227.mac}"

SEED="${SEED:-20260301}"
SIGMA_U="${SIGMA_U:-0.5}"
SIGMA_V="${SIGMA_V:-0.5}"
EPOCHS="${EPOCHS:-120}"
BATCH_SIZE="${BATCH_SIZE:-1024}"
MAX_FILES_PER_DOMAIN="${MAX_FILES_PER_DOMAIN:-0}"
TARGET_NORMALIZATION="${TARGET_NORMALIZATION:-none}"
LOSS_WEIGHTING="${LOSS_WEIGHTING:-none}"
LOSS_WEIGHTS="${LOSS_WEIGHTS:-}"
LR_SCHEDULER="${LR_SCHEDULER:-none}"
LR_SCHEDULER_PATIENCE="${LR_SCHEDULER_PATIENCE:-5}"
LR_SCHEDULER_FACTOR="${LR_SCHEDULER_FACTOR:-0.5}"
MIN_LR="${MIN_LR:-1e-5}"

ROOT_BIN="${ROOT_BIN:-root}"
RECO_BIN="${RECO_BIN:-${REPO_DIR}/build/bin/reconstruct_sn_nn}"
EVAL_BIN="${EVAL_BIN:-${REPO_DIR}/build/bin/evaluate_reconstruct_sn_nn}"
TRAIN_PY="${REPO_DIR}/scripts/reconstruction/nn_target_momentum/train_mlp.py"
EXPORT_PY="${REPO_DIR}/scripts/reconstruction/nn_target_momentum/export_model_for_cpp.py"
BUILD_DATA_MACRO="${REPO_DIR}/scripts/reconstruction/nn_target_momentum/build_dataset.C"

if [[ -n "${PYTHON_EXE:-}" ]]; then
    PY_RUN=("${PYTHON_EXE}")
elif [[ -x "/home/tian/micromamba/envs/anaroot-env/bin/python" ]]; then
    PY_RUN=("/home/tian/micromamba/envs/anaroot-env/bin/python")
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
    "${ROOT_BIN}" -l -b -q "${call}"
    rc=$?
    set -e

    if [[ "${rc}" -eq 0 ]]; then
        return 0
    fi

    local missing=0
    for path in "${expected_outputs[@]}"; do
        if [[ ! -s "${path}" ]]; then
            echo "[run_domain_matched_retrain] expected output missing after ROOT exit ${rc}: ${path}" >&2
            missing=1
        fi
    done

    if [[ "${missing}" -eq 0 ]]; then
        echo "[run_domain_matched_retrain] warning: ROOT exited with ${rc} but outputs exist; continuing" >&2
        return 0
    fi

    return "${rc}"
}

require_file() {
    local path="$1"
    local label="$2"
    if [[ ! -e "${path}" ]]; then
        echo "[run_domain_matched_retrain] missing ${label}: ${path}" >&2
        exit 1
    fi
}

list_split_files() {
    local domain_label="$1"
    local domain_dir="$2"
    local seed="$3"
    local max_files="$4"
    local out_json="$5"

    "${PY_RUN[@]}" - <<PY
import json
import pathlib
import random

domain_label = ${domain_label@Q}
domain_dir = pathlib.Path(${domain_dir@Q})
seed = int(${seed@Q})
max_files = int(${max_files@Q})
out_json = pathlib.Path(${out_json@Q})

files = sorted(str(p) for p in domain_dir.rglob("*.root"))
if max_files > 0 and len(files) > max_files:
    files = files[:max_files]

rng = random.Random(seed)
rng.shuffle(files)

n = len(files)
n_train = int(n * 0.70)
n_val = int(n * 0.15)
n_test = n - n_train - n_val

if n > 0:
    if n_train == 0:
        n_train = 1
    if n_val == 0 and n >= 3:
        n_val = 1
    n_test = n - n_train - n_val
    if n_test <= 0 and n >= 2:
        n_test = 1
        if n_train > 1:
            n_train -= 1
        elif n_val > 1:
            n_val -= 1

train = files[:n_train]
val = files[n_train:n_train+n_val]
test = files[n_train+n_val:n_train+n_val+n_test]

payload = {
    "domain": domain_label,
    "all_count": n,
    "train": train,
    "val": val,
    "test": test,
}
out_json.parent.mkdir(parents=True, exist_ok=True)
out_json.write_text(json.dumps(payload, indent=2), encoding="utf-8")
print(json.dumps(payload, indent=2))
PY
}

build_split_dataset() {
    local split_name="$1"
    local files_txt="$2"
    local out_split_dir="$3"
    local out_csv="$4"

    run_cmd mkdir -p "${out_split_dir}"
    if [[ "${DRY_RUN}" -eq 0 ]]; then
        : > "${out_csv}"
    fi

    local first=1
    local index=0
    while IFS= read -r sim_root; do
        [[ -n "${sim_root}" ]] || continue
        local tmp_root="${out_split_dir}/${split_name}_$(printf '%04d' "${index}").root"
        local tmp_csv="${out_split_dir}/${split_name}_$(printf '%04d' "${index}").csv"
        local call="${BUILD_DATA_MACRO}+(\"${sim_root}\",\"${GEOMETRY_MACRO}\",0,${SIGMA_U},${SIGMA_V},\"${tmp_root}\",\"${tmp_csv}\")"
        run_root_macro_expect_outputs "${call}" "${tmp_root}" "${tmp_csv}"

        if [[ "${DRY_RUN}" -eq 0 ]]; then
            if [[ -s "${tmp_csv}" ]]; then
                if [[ "${first}" -eq 1 ]]; then
                    cat "${tmp_csv}" >> "${out_csv}"
                    first=0
                else
                    tail -n +2 "${tmp_csv}" >> "${out_csv}"
                fi
            fi
        fi
        index=$((index + 1))
    done < "${files_txt}"
}

require_file "${INPUT_G4OUTPUT_DIR}" "input g4output dir"
require_file "${GEOMETRY_MACRO}" "geometry macro"
require_file "${ROOT_BIN}" "root executable"
require_file "${RECO_BIN}" "reconstruct_sn_nn"
require_file "${EVAL_BIN}" "evaluate_reconstruct_sn_nn"
require_file "${TRAIN_PY}" "train_mlp.py"
require_file "${EXPORT_PY}" "export_model_for_cpp.py"
require_file "${BUILD_DATA_MACRO}" "build_dataset.C"

STAMP="$(date +%Y%m%d_%H%M%S)"
OUT_BASE="${REPO_DIR}/data/nn_target_momentum/domain_matched_retrain/${STAMP}"
SPLIT_DIR="${OUT_BASE}/split_files"
DATASET_DIR="${OUT_BASE}/dataset_parts"
MODEL_DIR="${OUT_BASE}/model"
REPORT_DIR="${OUT_BASE}/reports"
RECO_DIR="${REPO_DIR}/data/reconstruction/domain_matched_retrain_${STAMP}"

TRAIN_LIST="${SPLIT_DIR}/train_files.txt"
VAL_LIST="${SPLIT_DIR}/val_files.txt"
TEST_LIST="${SPLIT_DIR}/test_files.txt"
TRAIN_CSV="${OUT_BASE}/dataset_train.csv"
VAL_CSV="${OUT_BASE}/dataset_val.csv"
TEST_CSV="${OUT_BASE}/dataset_test.csv"
MODEL_JSON="${MODEL_DIR}/model_cpp.json"

run_cmd mkdir -p "${SPLIT_DIR}" "${DATASET_DIR}" "${MODEL_DIR}" "${REPORT_DIR}" "${RECO_DIR}"

YPOL_JSON="${SPLIT_DIR}/ypol_split.json"
ZPOL_JSON="${SPLIT_DIR}/zpol_split.json"

list_split_files "ypol_pb208" "${INPUT_G4OUTPUT_DIR}/y_pol/phi_random" "${SEED}" "${MAX_FILES_PER_DOMAIN}" "${YPOL_JSON}"
list_split_files "zpol_sn124" "${INPUT_G4OUTPUT_DIR}/z_pol/b_discrete" "$((SEED + 1))" "${MAX_FILES_PER_DOMAIN}" "${ZPOL_JSON}"

if [[ "${DRY_RUN}" -eq 0 ]]; then
    "${PY_RUN[@]}" - <<PY
import json
from pathlib import Path

ypol = json.loads(Path(${YPOL_JSON@Q}).read_text(encoding="utf-8"))
zpol = json.loads(Path(${ZPOL_JSON@Q}).read_text(encoding="utf-8"))

def save(path, rows):
    Path(path).write_text("\\n".join(rows) + ("\\n" if rows else ""), encoding="utf-8")

train = ypol["train"] + zpol["train"]
val = ypol["val"] + zpol["val"]
test = ypol["test"] + zpol["test"]
save(${TRAIN_LIST@Q}, train)
save(${VAL_LIST@Q}, val)
save(${TEST_LIST@Q}, test)

manifest = {
    "seed": int(${SEED@Q}),
    "max_files_per_domain": int(${MAX_FILES_PER_DOMAIN@Q}),
    "ypol": ypol,
    "zpol": zpol,
    "combined_counts": {
        "train_files": len(train),
        "val_files": len(val),
        "test_files": len(test),
    },
}
Path(${SPLIT_DIR@Q}, "split_manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
print(json.dumps(manifest["combined_counts"], indent=2))
PY
fi

if [[ "${DRY_RUN}" -eq 0 ]]; then
    for f in "${TRAIN_LIST}" "${VAL_LIST}" "${TEST_LIST}"; do
        if [[ ! -s "${f}" ]]; then
            echo "[run_domain_matched_retrain] split file list is empty: ${f}" >&2
            exit 1
        fi
    done
fi

build_split_dataset "train" "${TRAIN_LIST}" "${DATASET_DIR}/train" "${TRAIN_CSV}"
build_split_dataset "val" "${VAL_LIST}" "${DATASET_DIR}/val" "${VAL_CSV}"
build_split_dataset "test" "${TEST_LIST}" "${DATASET_DIR}/test" "${TEST_CSV}"

TRAIN_ARGS=(
    --train-csv "${TRAIN_CSV}"
    --val-csv "${VAL_CSV}"
    --test-csv "${TEST_CSV}"
    --output-dir "${MODEL_DIR}"
    --epochs "${EPOCHS}"
    --batch-size "${BATCH_SIZE}"
    --seed "${SEED}"
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

run_cmd "${PY_RUN[@]}" "${EXPORT_PY}" \
    --model-path "${MODEL_DIR}/model.pt" \
    --meta-path "${MODEL_DIR}/model_meta.json" \
    --output-json "${MODEL_JSON}"

if [[ "${DRY_RUN}" -eq 0 ]]; then
    find "${INPUT_G4OUTPUT_DIR}" -type f -name '*.root' | sort > "${OUT_BASE}/all_sim_files.txt"
fi

if [[ "${DRY_RUN}" -eq 0 ]]; then
    while IFS= read -r sim_root; do
        [[ -n "${sim_root}" ]] || continue
        rel="${sim_root#${INPUT_G4OUTPUT_DIR}/}"
        reco_out="${RECO_DIR}/${rel%.root}_reco.root"
        mkdir -p "$(dirname "${reco_out}")"
        run_cmd "${RECO_BIN}" \
            --input-file "${sim_root}" \
            --output-file "${reco_out}" \
            --geometry-macro "${GEOMETRY_MACRO}" \
            --nn-model-json "${MODEL_JSON}"
    done < "${OUT_BASE}/all_sim_files.txt"
fi

run_cmd "${EVAL_BIN}" --input-dir "${BASELINE_RECO_DIR}" | tee "${REPORT_DIR}/baseline_eval.txt"
run_cmd "${EVAL_BIN}" --input-dir "${RECO_DIR}" | tee "${REPORT_DIR}/retrained_eval.txt"
run_cmd "${EVAL_BIN}" --input-dir "${RECO_DIR}/y_pol" | tee "${REPORT_DIR}/retrained_eval_ypol.txt"
run_cmd "${EVAL_BIN}" --input-dir "${RECO_DIR}/z_pol" | tee "${REPORT_DIR}/retrained_eval_zpol.txt"

echo "[run_domain_matched_retrain] done"
echo "[run_domain_matched_retrain] output base: ${OUT_BASE}"
echo "[run_domain_matched_retrain] retrained model: ${MODEL_DIR}"
echo "[run_domain_matched_retrain] retrained reco: ${RECO_DIR}"
echo "[run_domain_matched_retrain] reports: ${REPORT_DIR}"
