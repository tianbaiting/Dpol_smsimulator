#!/usr/bin/env bash
set -euo pipefail

BASE_REF="${1:-HEAD~1}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

skills=(
  "smsim-geant4-io"
  "smsim-pdc-track-reco"
  "smsim-pdc-momentum-reco"
  "smsim-nn-target-momentum"
  "smsim-pdc-target-reco-overview"
  "smsim-rk-geometry-filter"
)

rc=0
for s in "${skills[@]}"; do
  checker="${ROOT_DIR}/${s}/scripts/check_sync.sh"
  if [[ ! -x "${checker}" ]]; then
    echo "[MISSING] ${checker}"
    rc=2
    continue
  fi
  echo "=== ${s} ==="
  if ! "${checker}" "${BASE_REF}"; then
    rc=1
  fi
  echo

done

exit ${rc}
