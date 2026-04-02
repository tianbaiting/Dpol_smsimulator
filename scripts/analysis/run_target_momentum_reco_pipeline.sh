#!/usr/bin/env bash
set -euo pipefail

DRY_RUN=0
if [[ "${1:-}" == "--dry-run" ]]; then
    DRY_RUN=1
    shift
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

MODE="${MODE:-both}"
RECO_BACKEND="${RECO_BACKEND:-nn}"
ZPOL_TARGET_FILTER="${ZPOL_TARGET_FILTER:-Sn124}"
YPOL_TARGET_FILTER="${YPOL_TARGET_FILTER:-Pb208}"
MAX_FILES="${MAX_FILES:-0}"
BEAM_ON="${BEAM_ON:-0}"
REUSE_G4INPUT="${REUSE_G4INPUT:-1}"
ALLOW_PARTIAL_FOR_TEST="${ALLOW_PARTIAL_FOR_TEST:-0}"

QMD_INPUT_BASE="${QMD_INPUT_BASE:-${REPO_DIR}/data/qmdrawdata/qmdrawdata}"
G4INPUT_BASE="${G4INPUT_BASE:-${REPO_DIR}/data/simulation/g4input}"
G4OUTPUT_BASE="${G4OUTPUT_BASE:-${REPO_DIR}/data/simulation/g4output/sn124_nn_B115T}"
RECO_OUTPUT_DIR="${RECO_OUTPUT_DIR:-${REPO_DIR}/data/reconstruction/sn124_nn_B115T}"

GEOMETRY_MACRO="${GEOMETRY_MACRO:-${REPO_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac}"

MODEL_PT="${MODEL_PT:-${REPO_DIR}/data/nn_target_momentum/formal_B115T3deg_qmdwindow/20260227_223007/model/model.pt}"
MODEL_META="${MODEL_META:-${REPO_DIR}/data/nn_target_momentum/formal_B115T3deg_qmdwindow/20260227_223007/model/model_meta.json}"
MODEL_JSON="${MODEL_JSON:-${REPO_DIR}/data/nn_target_momentum/formal_B115T3deg_qmdwindow/20260227_223007/model/model_cpp.json}"
MAGNETIC_FIELD_MAP="${MAGNETIC_FIELD_MAP:-${REPO_DIR}/configs/simulation/geometry/filed_map/180703-1,15T-3000.table}"
MAGNET_ROTATION_DEG="${MAGNET_ROTATION_DEG:-30}"
RK_FIT_MODE="${RK_FIT_MODE:-three-point-free}"

GEN_BIN="${GEN_BIN:-${REPO_DIR}/build/bin/GenInputRoot_qmdrawdata}"
SIM_BIN="${SIM_BIN:-${REPO_DIR}/build/bin/sim_deuteron}"
RECO_BIN="${RECO_BIN:-${REPO_DIR}/build/bin/reconstruct_target_momentum}"
EXPORT_PY="${REPO_DIR}/scripts/reconstruction/nn_target_momentum/export_model_for_cpp.py"
USE_NN_ASSETS=0

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

collect_files() {
    local dir="$1"
    if [[ ! -d "${dir}" ]]; then
        return 0
    fi
    find "${dir}" -type f -name "*.root" | sort
}

ensure_exists() {
    local path="$1"
    local label="$2"
    if [[ ! -e "${path}" ]]; then
        echo "[run_target_momentum_reco_pipeline] missing ${label}: ${path}" >&2
        exit 1
    fi
}

ensure_exists "${SIM_BIN}" "sim_deuteron binary"
ensure_exists "${RECO_BIN}" "reconstruct_target_momentum binary"
ensure_exists "${GEOMETRY_MACRO}" "geometry macro"
if [[ "${RECO_BACKEND}" != "nn" ]]; then
    ensure_exists "${MAGNETIC_FIELD_MAP}" "magnetic field map"
fi
if [[ "${REUSE_G4INPUT}" != "1" && ( "${MODE}" == "zpol" || "${MODE}" == "both" ) ]]; then
    ensure_exists "${GEN_BIN}" "GenInputRoot_qmdrawdata binary"
fi

if [[ "${RECO_BACKEND}" == "nn" ]]; then
    USE_NN_ASSETS=1
elif [[ "${RECO_BACKEND}" == "auto" && -e "${MODEL_PT}" && -e "${MODEL_META}" && -e "${EXPORT_PY}" ]]; then
    USE_NN_ASSETS=1
fi

if [[ "${USE_NN_ASSETS}" -eq 1 ]]; then
    ensure_exists "${MODEL_PT}" "model.pt"
    ensure_exists "${MODEL_META}" "model_meta.json"
    ensure_exists "${EXPORT_PY}" "export_model_for_cpp.py"
fi

if [[ "${ALLOW_PARTIAL_FOR_TEST}" != "1" ]]; then
    if [[ "${BEAM_ON}" != "0" ]]; then
        echo "[run_target_momentum_reco_pipeline] production run requires BEAM_ON=0 (all entries). Set ALLOW_PARTIAL_FOR_TEST=1 to override." >&2
        exit 1
    fi
    if [[ "${MAX_FILES}" != "0" ]]; then
        echo "[run_target_momentum_reco_pipeline] production run requires MAX_FILES=0 (all files). Set ALLOW_PARTIAL_FOR_TEST=1 to override." >&2
        exit 1
    fi
fi

run_cmd mkdir -p "${G4OUTPUT_BASE}" "${RECO_OUTPUT_DIR}"

if [[ "${USE_NN_ASSETS}" -eq 1 ]]; then
    echo "[run_target_momentum_reco_pipeline] exporting NN model for C++ inference"
    run_cmd "${PY_RUN[@]}" "${EXPORT_PY}" \
        --model-path "${MODEL_PT}" \
        --meta-path "${MODEL_META}" \
        --output-json "${MODEL_JSON}"
fi

if [[ "${REUSE_G4INPUT}" != "1" && ( "${MODE}" == "zpol" || "${MODE}" == "both" ) ]]; then
    echo "[run_target_momentum_reco_pipeline] converting zpol rawdata -> g4input (target=${ZPOL_TARGET_FILTER})"
    run_cmd "${GEN_BIN}" \
        --mode zpol \
        --input-base "${QMD_INPUT_BASE}" \
        --output-base "${G4INPUT_BASE}" \
        --target-filter "${ZPOL_TARGET_FILTER}" \
        --cut-unphysical on \
        --rotate-zpol on
fi

if [[ "${REUSE_G4INPUT}" != "1" && ( "${MODE}" == "ypol" || "${MODE}" == "both" ) ]]; then
    echo "[run_target_momentum_reco_pipeline] converting ypol rawdata -> g4input (source=phi_random, target=${YPOL_TARGET_FILTER})"
    run_cmd "${GEN_BIN}" \
        --mode ypol \
        --input-base "${QMD_INPUT_BASE}" \
        --output-base "${G4INPUT_BASE}" \
        --target-filter "${YPOL_TARGET_FILTER}" \
        --cut-unphysical on \
        --rotate-ypol off
fi

if [[ "${REUSE_G4INPUT}" == "1" ]]; then
    echo "[run_target_momentum_reco_pipeline] reusing existing g4input files"
fi

declare -a INPUT_FILES=()
if [[ "${MODE}" == "zpol" || "${MODE}" == "both" ]]; then
    while IFS= read -r path; do
        [[ "${path}" == *"${ZPOL_TARGET_FILTER}"* ]] || continue
        INPUT_FILES+=("${path}")
    done < <(collect_files "${G4INPUT_BASE}/z_pol/b_discrete")
fi

if [[ "${MODE}" == "ypol" || "${MODE}" == "both" ]]; then
    while IFS= read -r path; do
        [[ "${path}" == *"${YPOL_TARGET_FILTER}"* ]] || continue
        INPUT_FILES+=("${path}")
    done < <(collect_files "${G4INPUT_BASE}/y_pol/phi_random")
fi

if [[ "${#INPUT_FILES[@]}" -eq 0 ]]; then
    echo "[run_target_momentum_reco_pipeline] no g4input files selected" >&2
    exit 1
fi

IFS=$'\n' INPUT_FILES=($(printf '%s\n' "${INPUT_FILES[@]}" | sort))
unset IFS

if [[ "${MAX_FILES}" -gt 0 && "${#INPUT_FILES[@]}" -gt "${MAX_FILES}" ]]; then
    INPUT_FILES=("${INPUT_FILES[@]:0:${MAX_FILES}}")
fi

echo "[run_target_momentum_reco_pipeline] selected ${#INPUT_FILES[@]} g4input files"

MACRO_DIR="${G4OUTPUT_BASE}/_macros"
run_cmd mkdir -p "${MACRO_DIR}"

for input_file in "${INPUT_FILES[@]}"; do
    rel="${input_file#${G4INPUT_BASE}/}"
    run_name="$(basename "${input_file}" .root)"
    out_dir="${G4OUTPUT_BASE}/$(dirname "${rel}")"
    macro_name="$(echo "${rel}" | sed 's#/#_#g; s#\\.root$##')"
    macro_path="${MACRO_DIR}/${macro_name}.mac"

    run_cmd mkdir -p "${out_dir}"

    if [[ "${DRY_RUN}" -eq 0 ]]; then
        cat > "${macro_path}" <<EOF
/control/getEnv SMSIMDIR
/control/execute ${GEOMETRY_MACRO}
/action/file/OverWrite y
/action/file/RunName ${run_name}
/action/file/SaveDirectory ${out_dir}
/tracking/storeTrajectory 0
/action/gun/Type Tree
/action/gun/tree/InputFileName ${input_file}
/action/gun/tree/TreeName tree
/action/gun/tree/beamOn ${BEAM_ON}
EOF
    else
        echo "+ write macro ${macro_path}"
    fi

    run_cmd "${SIM_BIN}" "${macro_path}"

    sim_output="${out_dir}/${run_name}0000.root"
    if [[ "${DRY_RUN}" -eq 0 && ! -f "${sim_output}" ]]; then
        fallback="$(ls -1 "${out_dir}/${run_name}"*.root 2>/dev/null | head -n 1 || true)"
        if [[ -n "${fallback}" ]]; then
            sim_output="${fallback}"
        fi
    fi

    reco_output="${RECO_OUTPUT_DIR}/$(dirname "${rel}")/$(basename "${sim_output}" .root)_reco.root"
    run_cmd mkdir -p "$(dirname "${reco_output}")"

    RECO_ARGS=(
        --backend "${RECO_BACKEND}"
        --input-file "${sim_output}"
        --output-file "${reco_output}"
        --geometry-macro "${GEOMETRY_MACRO}"
    )
    if [[ "${USE_NN_ASSETS}" -eq 1 ]]; then
        RECO_ARGS+=(--nn-model-json "${MODEL_JSON}")
    fi
    if [[ "${RECO_BACKEND}" != "nn" ]]; then
        RECO_ARGS+=(--magnetic-field-map "${MAGNETIC_FIELD_MAP}" --magnet-rotation-deg "${MAGNET_ROTATION_DEG}")
    fi
    if [[ "${RECO_BACKEND}" == "rk" || "${RECO_BACKEND}" == "auto" ]]; then
        RECO_ARGS+=(--rk-fit-mode "${RK_FIT_MODE}")
    fi
    run_cmd "${RECO_BIN}" "${RECO_ARGS[@]}"
done

echo "[run_target_momentum_reco_pipeline] done"
echo "[run_target_momentum_reco_pipeline] g4output: ${G4OUTPUT_BASE}"
echo "[run_target_momentum_reco_pipeline] reconstruction: ${RECO_OUTPUT_DIR}"
