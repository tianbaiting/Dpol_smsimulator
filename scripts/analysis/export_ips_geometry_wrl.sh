#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SIM_BIN="${SIM_BIN:-${REPO_DIR}/build/bin/sim_deuteron}"
MACRO_PATH="${MACRO_PATH:-${REPO_DIR}/configs/simulation/macros/export_ips_geometry_example.mac}"
OUTPUT_PATH="${1:-${REPO_DIR}/docs/mechanic/veto_impactPrameterSelector/examples/ips_geometry_3deg_B115T_offset0.wrl}"

if [[ ! -x "${SIM_BIN}" ]]; then
    echo "missing sim_deuteron binary: ${SIM_BIN}" >&2
    exit 1
fi

if [[ ! -f "${MACRO_PATH}" ]]; then
    echo "missing export macro: ${MACRO_PATH}" >&2
    exit 1
fi

mkdir -p "$(dirname "${OUTPUT_PATH}")"

TMP_WORKDIR="$(mktemp -d)"
cleanup() {
    rm -rf "${TMP_WORKDIR}"
}
trap cleanup EXIT

export SMSIMDIR="${REPO_DIR}"
export G4VRMLFILE_FILE_NAME="${OUTPUT_PATH}"

cd "${TMP_WORKDIR}"

# [EN] Run the export in an isolated temporary directory so Geant4 fallback g4_*.wrl files do not pollute the repository / [CN] 在独立临时目录中导出，避免 Geant4 回退生成的 g4_*.wrl 污染仓库
"${SIM_BIN}" "${MACRO_PATH}"

if [[ ! -f "${OUTPUT_PATH}" ]]; then
    echo "expected WRL output missing: ${OUTPUT_PATH}" >&2
    exit 1
fi

echo "${OUTPUT_PATH}"
