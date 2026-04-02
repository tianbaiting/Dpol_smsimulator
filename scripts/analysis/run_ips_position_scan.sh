#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

GEN_BIN="${GEN_BIN:-${REPO_DIR}/build/bin/GenInputRoot_qmdrawdata}"
SCAN_BIN="${SCAN_BIN:-${REPO_DIR}/build/bin/scan_ips_position}"
SIM_BIN="${SIM_BIN:-${REPO_DIR}/build/bin/sim_deuteron}"
GEOM_MACRO="${GEOM_MACRO:-${REPO_DIR}/configs/simulation/geometry/3deg_1.15T.mac}"
INPUT_BASE="${INPUT_BASE:-${REPO_DIR}/data/qmdrawdata/qmdrawdata/allevent}"
G4INPUT_DIR="${G4INPUT_DIR:-${REPO_DIR}/data/simulation/ips_scan_input}"
SCAN_OUT_DIR="${SCAN_OUT_DIR:-${REPO_DIR}/data/simulation/ips_scan_output}"
SCAN_EXTRA_ARGS="${SCAN_EXTRA_ARGS:-}"

ensure_exists() {
    local path="$1"
    local label="$2"
    if [[ ! -e "${path}" ]]; then
        echo "missing ${label}: ${path}" >&2
        exit 1
    fi
}

ensure_exists "${GEN_BIN}" "GenInputRoot_qmdrawdata"
ensure_exists "${SCAN_BIN}" "scan_ips_position"
ensure_exists "${SIM_BIN}" "sim_deuteron"
ensure_exists "${GEOM_MACRO}" "geometry macro"
ensure_exists "${INPUT_BASE}" "QMD input base"

mkdir -p "${G4INPUT_DIR}" "${SCAN_OUT_DIR}"

"${GEN_BIN}" \
    --mode ypol \
    --source both \
    --input-base "${INPUT_BASE}" \
    --output-base "${G4INPUT_DIR}" \
    --target-filter d+Sn124-ypolE190g050

"${GEN_BIN}" \
    --mode ypol \
    --source both \
    --input-base "${INPUT_BASE}" \
    --output-base "${G4INPUT_DIR}" \
    --target-filter d+Sn124-ypolE190g100

"${GEN_BIN}" \
    --mode zpol \
    --source both \
    --input-base "${INPUT_BASE}" \
    --output-base "${G4INPUT_DIR}" \
    --target-filter d+Sn124-zpolE190g050

"${GEN_BIN}" \
    --mode zpol \
    --source both \
    --input-base "${INPUT_BASE}" \
    --output-base "${G4INPUT_DIR}" \
    --target-filter d+Sn124-zpolE190g100

"${SCAN_BIN}" \
    --sim-bin "${SIM_BIN}" \
    --geometry-macro "${GEOM_MACRO}" \
    --output-dir "${SCAN_OUT_DIR}" \
    --scan-elastic "${G4INPUT_DIR}/d+Sn124-ypolE190g050ypn" \
    --scan-elastic "${G4INPUT_DIR}/d+Sn124-ypolE190g050ynp" \
    --scan-allevent "${G4INPUT_DIR}/d+Sn124-ypolE190g050ypn" \
    --scan-allevent "${G4INPUT_DIR}/d+Sn124-ypolE190g050ynp" \
    --validation-elastic "${G4INPUT_DIR}/d+Sn124-ypolE190g100ypn" \
    --validation-elastic "${G4INPUT_DIR}/d+Sn124-ypolE190g100ynp" \
    --validation-elastic "${G4INPUT_DIR}/d+Sn124-zpolE190g050zpn" \
    --validation-elastic "${G4INPUT_DIR}/d+Sn124-zpolE190g050znp" \
    --validation-elastic "${G4INPUT_DIR}/d+Sn124-zpolE190g100zpn" \
    --validation-elastic "${G4INPUT_DIR}/d+Sn124-zpolE190g100znp" \
    --validation-allevent "${G4INPUT_DIR}/d+Sn124-ypolE190g100ypn" \
    --validation-allevent "${G4INPUT_DIR}/d+Sn124-ypolE190g100ynp" \
    --validation-allevent "${G4INPUT_DIR}/d+Sn124-zpolE190g050zpn" \
    --validation-allevent "${G4INPUT_DIR}/d+Sn124-zpolE190g050znp" \
    --validation-allevent "${G4INPUT_DIR}/d+Sn124-zpolE190g100zpn" \
    --validation-allevent "${G4INPUT_DIR}/d+Sn124-zpolE190g100znp" \
    ${SCAN_EXTRA_ARGS}
