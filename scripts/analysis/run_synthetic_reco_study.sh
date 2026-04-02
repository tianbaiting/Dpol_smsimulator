#!/usr/bin/env bash
# [EN] Compatibility wrapper for the legacy synthetic-reconstruction study. / [CN] 旧版合成数据重建研究的兼容包装脚本。
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "[run_synthetic_reco_study] compatibility wrapper: forwarding to scripts/analysis/legacy_target_reco/run_synthetic_reco_study.sh" >&2
exec bash "${SCRIPT_DIR}/legacy_target_reco/run_synthetic_reco_study.sh" "$@"
