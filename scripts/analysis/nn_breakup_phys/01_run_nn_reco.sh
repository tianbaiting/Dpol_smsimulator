#!/bin/bash
# Run NN reconstruction on every ypol+zpol breakup cell.
# Idempotent: skips an output ROOT that already exists.
set -euo pipefail

SMSIM_DIR=${SMSIM_DIR:-/home/tian/workspace/dpol/smsimulator5.5}
G4_BASE=${G4_BASE:-${SMSIM_DIR}/data/simulation/g4output}
OUT_BASE=${OUT_BASE:-${SMSIM_DIR}/data/reconstruction/breakup_nn_20260503}
LOG_DIR=${LOG_DIR:-${OUT_BASE}/logs}
NN_MODEL=${NN_MODEL:-${SMSIM_DIR}/data/nn_target_momentum/formal_B115T3deg/20260420_184345/model/model_cpp.json}
GEOM=${GEOM:-${SMSIM_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac}
RECO_BIN=${RECO_BIN:-${SMSIM_DIR}/build/bin/reconstruct_target_momentum}
J=${J:-6}

mkdir -p "${OUT_BASE}/y_pol" "${OUT_BASE}/z_pol" "${LOG_DIR}"

run_one() {
    local input=$1
    local out=$2
    local tag=$3
    if [[ -f "${out}" ]]; then
        echo "[skip] ${tag}"
        return 0
    fi
    local log="${LOG_DIR}/${tag}.log"
    "${RECO_BIN}" \
        --input-file "${input}" \
        --output-file "${out}" \
        --geometry-macro "${GEOM}" \
        --backend nn \
        --nn-model-json "${NN_MODEL}" > "${log}" 2>&1
    echo "[done] ${tag}"
}
export -f run_one
export RECO_BIN GEOM NN_MODEL LOG_DIR

WORK_LIST=$(mktemp)
trap "rm -f ${WORK_LIST}" EXIT

# ypol: one input per cell
while IFS= read -r f; do
    cell=$(basename "$(dirname "${f}")")
    out="${OUT_BASE}/y_pol/${cell}_reco_nn.root"
    echo "${f}|${out}|ypol_${cell}" >> "${WORK_LIST}"
done < <(find "${G4_BASE}/y_pol/phi_random" -name 'dbreak0000.root' 2>/dev/null | sort)

# zpol: per-b-segment input
while IFS= read -r f; do
    cell=$(basename "$(dirname "${f}")")
    seg=$(basename "${f}" .root | sed 's/^dbreak//; s/0000$//')   # b01..b10
    out="${OUT_BASE}/z_pol/${cell}_${seg}_reco_nn.root"
    echo "${f}|${out}|zpol_${cell}_${seg}" >> "${WORK_LIST}"
done < <(find "${G4_BASE}/z_pol/b_discrete" -name 'dbreakb*.root' 2>/dev/null | sort)

n_total=$(wc -l < "${WORK_LIST}")
echo "[run_nn_reco] ${n_total} cells/segments queued, ${J} parallel jobs"
START=$SECONDS
xargs -P "${J}" -I{} -d '\n' bash -c 'IFS="|" read -r in out tag <<<"$1"; run_one "${in}" "${out}" "${tag}"' _ {} < "${WORK_LIST}"
echo "[run_nn_reco] done in $((SECONDS - START))s"

ypol_n=$(ls "${OUT_BASE}/y_pol"/*.root 2>/dev/null | wc -l)
zpol_n=$(ls "${OUT_BASE}/z_pol"/*.root 2>/dev/null | wc -l)
echo "[run_nn_reco] ypol reco files: ${ypol_n} (expect 16)"
echo "[run_nn_reco] zpol reco files: ${zpol_n} (expect 160)"
