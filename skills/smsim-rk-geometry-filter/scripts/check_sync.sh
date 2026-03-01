#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKILL_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd "${SKILL_DIR}/../.." && pwd)"
BASE_REF="${1:-HEAD~1}"

required_paths=(
    "libs/analysis/include/ParticleTrajectory.hh"
    "libs/analysis/src/ParticleTrajectory.cc"
    "libs/geo_accepentce/include/DetectorAcceptanceCalculator.hh"
    "libs/geo_accepentce/src/DetectorAcceptanceCalculator.cc"
    "libs/qmd_geo_filter/include/QMDGeoFilter.hh"
    "libs/qmd_geo_filter/src/QMDGeoFilter.cc"
    "libs/qmd_geo_filter/src/RunQMDGeoFilter.cc"
)

missing=0
for p in "${required_paths[@]}"; do
    if [[ ! -e "${REPO_ROOT}/${p}" ]]; then
        echo "[MISSING] ${p}"
        missing=1
    fi
done

changed_files=()
if git -C "${REPO_ROOT}" rev-parse --verify "${BASE_REF}" >/dev/null 2>&1; then
    while IFS= read -r line; do
        [[ -n "${line}" ]] && changed_files+=("${line}")
    done < <(git -C "${REPO_ROOT}" diff --name-only "${BASE_REF}"...HEAD)
else
    while IFS= read -r line; do
        [[ -n "${line}" ]] && changed_files+=("${line}")
    done < <(git -C "${REPO_ROOT}" diff --name-only)
fi

for f in "${changed_files[@]}"; do
    case "${f}" in
        libs/analysis/src/ParticleTrajectory.cc|libs/analysis/include/ParticleTrajectory.hh|libs/geo_accepentce/*|libs/qmd_geo_filter/*)
            echo "[SYNC] RK geometry filter changed: ${f}"
            echo "       Update references/workflow.md if behavior/contracts changed."
            break
            ;;
    esac
done

python3 /home/tian/.codex/skills/.system/skill-creator/scripts/quick_validate.py "${SKILL_DIR}"

if [[ ${missing} -ne 0 ]]; then
    echo "[ACTION] Structure changed. Update references/path-map.md and scripts/check_sync.sh."
    exit 2
fi

echo "[OK] smsim-rk-geometry-filter sync check passed."
