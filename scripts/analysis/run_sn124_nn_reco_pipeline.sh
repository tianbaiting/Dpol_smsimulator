#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ -z "${RECO_BACKEND:-}" ]]; then
    export RECO_BACKEND=nn
fi

exec bash "${SCRIPT_DIR}/run_target_momentum_reco_pipeline.sh" "$@"
