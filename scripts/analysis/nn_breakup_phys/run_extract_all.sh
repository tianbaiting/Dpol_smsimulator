#!/bin/bash
# Extract per-event CSVs for every cell from the NN reco ROOTs.
# zpol b-segments are appended into a single per-cell CSV.
set -euo pipefail

SMSIM_DIR=${SMSIM_DIR:-/home/tian/workspace/dpol/smsimulator5.5}
RECO_BASE=${RECO_BASE:-${SMSIM_DIR}/data/reconstruction/breakup_nn_20260503}
CSV_BASE=${CSV_BASE:-${SMSIM_DIR}/build/test_output/nn_breakup_phys_20260503/csv}
LOG_DIR=${LOG_DIR:-${SMSIM_DIR}/build/test_output/nn_breakup_phys_20260503/logs_extract}
MACRO=${MACRO:-${SMSIM_DIR}/scripts/analysis/nn_breakup_phys/02_extract_observables.C}
J=${J:-4}

mkdir -p "${CSV_BASE}/ypol" "${CSV_BASE}/zpol" "${LOG_DIR}"

extract_ypol() {
    local input=$1
    local cell=$(basename "${input}" _reco_nn.root)
    local csv="${CSV_BASE}/ypol/${cell}.csv"
    local log="${LOG_DIR}/ypol_${cell}.log"
    if [[ -f "${csv}" ]]; then echo "[skip] ypol ${cell}"; return 0; fi
    cd "${SMSIM_DIR}"
    root -b -l -q -e ".L ${MACRO}" \
        -e "extract_observables(\"${input}\",\"${csv}\",\"${cell}\",\"\")" \
        > "${log}" 2>&1
    echo "[done] ypol ${cell}  rows=$(wc -l < ${csv})"
}

extract_zpol() {
    local cell=$1
    local csv="${CSV_BASE}/zpol/${cell}.csv"
    local log="${LOG_DIR}/zpol_${cell}.log"
    if [[ -f "${csv}" ]]; then echo "[skip] zpol ${cell}"; return 0; fi
    cd "${SMSIM_DIR}"
    : > "${csv}"   # start fresh; macro appends across segments
    for input in "${RECO_BASE}/z_pol/${cell}"_b*_reco_nn.root; do
        [[ -f "${input}" ]] || continue
        local seg=$(basename "${input}" _reco_nn.root | sed "s/^${cell}_//")
        root -b -l -q -e ".L ${MACRO}" \
            -e "extract_observables(\"${input}\",\"${csv}\",\"${cell}\",\"${seg}\")" \
            >> "${log}" 2>&1
    done
    echo "[done] zpol ${cell}  rows=$(wc -l < ${csv})"
}
export -f extract_ypol extract_zpol
export SMSIM_DIR RECO_BASE CSV_BASE LOG_DIR MACRO

# ypol: one job per file
ls "${RECO_BASE}/y_pol"/*_reco_nn.root 2>/dev/null \
    | xargs -P "${J}" -I{} bash -c 'extract_ypol "$1"' _ {}

# zpol: one job per cell (cell name is the prefix shared by 10 b-segments)
ls "${RECO_BASE}/z_pol"/*_b*_reco_nn.root 2>/dev/null \
    | sed 's|.*/||; s/_b[0-9][0-9]_reco_nn\.root$//' \
    | sort -u \
    | xargs -P "${J}" -I{} bash -c 'extract_zpol "$1"' _ {}

echo "--- summary ---"
echo "ypol CSVs: $(ls ${CSV_BASE}/ypol | wc -l) (expect 16)"
echo "zpol CSVs: $(ls ${CSV_BASE}/zpol | wc -l) (expect 16)"
