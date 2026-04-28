#!/bin/bash
# [EN] Process a single (condition, gamma, helicity) cell: subsample, reco, error analysis.
# [CN] 处理单个 (condition, gamma, helicity) 单元：抽样、重建、误差分析。
#
# Args: <condition> <gamma> <helicity> <seed>
# Env: SMSIM_DIR OUT_ROOT TARGET N_PER_CELL PROFILE_PER_Q MCMC_PER_Q
#      MCMC_N_SAMPLES MCMC_BURN_IN RK_FIT_MODE

set -euo pipefail

if [[ $# -ne 4 ]]; then
    echo "usage: $0 <condition> <gamma> <helicity> <seed>" >&2
    exit 1
fi

cond=$1
gamma=$2
hel=$3
seed=$4

SMSIM_DIR=${SMSIM_DIR:-/home/tian/workspace/dpol/smsimulator5.5}
BUILD_DIR=${BUILD_DIR:-${SMSIM_DIR}/build}
OUT_ROOT=${OUT_ROOT:-${BUILD_DIR}/test_output/ypol_pilot}
TARGET=${TARGET:-d+Sn124E190}
N_PER_CELL=${N_PER_CELL:-50}
PROFILE_PER_Q=${PROFILE_PER_Q:-2}
MCMC_PER_Q=${MCMC_PER_Q:-2}
MCMC_N_SAMPLES=${MCMC_N_SAMPLES:-160}
MCMC_BURN_IN=${MCMC_BURN_IN:-80}
RK_FIT_MODE=${RK_FIT_MODE:-fixed-target-pdc-only}

GEOM_MAC=${SMSIM_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac
FIELD_MAP=${SMSIM_DIR}/configs/simulation/geometry/filed_map/180703-1,15T-3000.table

tag="${cond}_${gamma}${hel}"
INPUT="${SMSIM_DIR}/data/simulation/g4output/ypol_new_20260413_${cond}/${TARGET}/${TARGET}${gamma}${hel}-RP360/geminiout0000.root"
SAMPLED="${OUT_ROOT}/sampled/${tag}.root"
RECO="${OUT_ROOT}/reco/${tag}_reco.root"
ANA_DIR="${OUT_ROOT}/error_analysis/${tag}"
LOG="${OUT_ROOT}/logs/${tag}.log"

mkdir -p "${OUT_ROOT}/sampled" "${OUT_ROOT}/reco" "${ANA_DIR}" "${OUT_ROOT}/logs"

if [[ ! -f "${INPUT}" ]]; then
    echo "[${tag}] missing input: ${INPUT}" | tee -a "${LOG}" >&2
    exit 0
fi

START=$SECONDS

# 1. Subsample (skip if cached)
if [[ ! -f "${SAMPLED}" ]]; then
    echo "[${tag}] subsample n=${N_PER_CELL} seed=${seed}" >> "${LOG}"
    python3 "${SMSIM_DIR}/scripts/analysis/ypol_pilot/subsample_geminiout.py" \
        --input "${INPUT}" \
        --output "${SAMPLED}" \
        --n-events "${N_PER_CELL}" \
        --seed "${seed}" >> "${LOG}" 2>&1
fi

# 2. Reco (skip if cached)
if [[ ! -f "${RECO}" ]]; then
    echo "[${tag}] reco" >> "${LOG}"
    "${BUILD_DIR}/bin/reconstruct_target_momentum" \
        --input-file "${SAMPLED}" \
        --output-file "${RECO}" \
        --geometry-macro "${GEOM_MAC}" \
        --magnetic-field-map "${FIELD_MAP}" \
        --backend rk \
        --rk-fit-mode "${RK_FIT_MODE}" \
        --rk-write-errors on \
        --rk-write-laplace on >> "${LOG}" 2>&1
fi

# 3. Error analysis (skip if cached)
if [[ ! -f "${ANA_DIR}/track_summary.csv" ]]; then
    echo "[${tag}] error_analysis profile/q=${PROFILE_PER_Q} mcmc/q=${MCMC_PER_Q}" >> "${LOG}"
    "${BUILD_DIR}/bin/analyze_pdc_rk_error" \
        --input-file "${RECO}" \
        --output-dir "${ANA_DIR}" \
        --geometry-macro "${GEOM_MAC}" \
        --magnetic-field-map "${FIELD_MAP}" \
        --rk-fit-mode "${RK_FIT_MODE}" \
        --profile-per-quartile "${PROFILE_PER_Q}" \
        --mcmc-per-quartile "${MCMC_PER_Q}" \
        --mcmc-n-samples "${MCMC_N_SAMPLES}" \
        --mcmc-burn-in "${MCMC_BURN_IN}" >> "${LOG}" 2>&1
fi

echo "[${tag}] done in $((SECONDS - START))s" | tee -a "${LOG}"
