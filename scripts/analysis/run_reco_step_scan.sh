#!/usr/bin/env bash
# [EN] Compatibility wrapper for the legacy step-scan study. / [CN] 旧版步长扫描研究的兼容包装脚本。
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "[run_reco_step_scan] compatibility wrapper: forwarding to scripts/analysis/legacy_target_reco/run_reco_step_scan.sh" >&2
exec bash "${SCRIPT_DIR}/legacy_target_reco/run_reco_step_scan.sh" "$@"
