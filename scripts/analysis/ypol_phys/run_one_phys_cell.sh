#!/bin/bash
# Per-cell driver for ypol elastic phys pilot.
# Args: <target> <gamma> <helicity>
# Env: SMSIM_DIR OUT_ROOT N_PER_CELL SEED
set -euo pipefail

if [[ $# -ne 3 ]]; then
    echo "usage: $0 <target> <gamma> <helicity>" >&2
    exit 1
fi
target=$1   # Sn112E190 / Sn124E190
gamma=$2    # g050 / g080
hel=$3      # ynp / ypn

SMSIM_DIR=${SMSIM_DIR:-/home/tian/workspace/dpol/smsimulator5.5}
BUILD_DIR=${BUILD_DIR:-${SMSIM_DIR}/build}
OUT_ROOT=${OUT_ROOT:-${BUILD_DIR}/test_output/ypol_phys_20260428}
N_PER_CELL=${N_PER_CELL:-5000}
SEED=${SEED:-20260428}
RK_FIT_MODE=${RK_FIT_MODE:-fixed-target-pdc-only}

GEOM_MAC=${SMSIM_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac
FIELD_MAP=${SMSIM_DIR}/configs/simulation/geometry/filed_map/180703-1,15T-3000.table

tag="${target}_${gamma}${hel}"
INPUT="${SMSIM_DIR}/data/simulation/g4output/ypol_new_20260413_elastic_allair/d+${target}/d+${target}${gamma}${hel}-RP360/dbreak0000.root"
SAMPLED="${OUT_ROOT}/sampled/${tag}.root"
RECO="${OUT_ROOT}/reco/${tag}_reco.root"
CSV="${OUT_ROOT}/csv/${tag}.csv"
LOG="${OUT_ROOT}/logs/${tag}.log"

mkdir -p "$(dirname "${SAMPLED}")" "$(dirname "${RECO}")" "$(dirname "${CSV}")" "$(dirname "${LOG}")"

if [[ ! -f "${INPUT}" ]]; then
    echo "[${tag}] missing input: ${INPUT}" | tee -a "${LOG}" >&2
    exit 0
fi

START=$SECONDS

# Compute deterministic seed offset from cell name (so reruns are reproducible).
seed_offset=$((${SEED} + $(echo -n "${tag}" | cksum | cut -d' ' -f1) % 1000))

# 1. Subsample
if [[ ! -f "${SAMPLED}" ]]; then
    echo "[${tag}] subsample N=${N_PER_CELL} seed=${seed_offset}" >> "${LOG}"
    python3 "${SMSIM_DIR}/scripts/analysis/ypol_phys/subsample_dbreak.py" \
        --input "${INPUT}" \
        --output "${SAMPLED}" \
        --n-events "${N_PER_CELL}" \
        --seed "${seed_offset}" >> "${LOG}" 2>&1
fi

# 2. Reco (proton + neutron)
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

# 3. Extract observables to CSV
if [[ ! -f "${CSV}" ]]; then
    echo "[${tag}] extract" >> "${LOG}"
    cd "${SMSIM_DIR}"
    root -b -l -q -e ".L ${SMSIM_DIR}/scripts/analysis/ypol_phys/extract_phys_observables.C" -e "extract_phys_observables(\"${RECO}\", \"${CSV}\", \"${tag}\")" >> "${LOG}" 2>&1
fi

echo "[${tag}] done in $((SECONDS - START))s" | tee -a "${LOG}"
