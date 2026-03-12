#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKILL_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd "${SKILL_DIR}/../.." && pwd)"
BASE_REF="${1:-HEAD~1}"

required_paths=(
    "configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac"
    "docs/reports/reconstruction/nn_retrain_qmdwindow_B115T3deg_notes.zh.md"
    "docs/note_log/task.md"
    "libs/analysis/include/PDCSimAna.hh"
    "libs/analysis/src/PDCSimAna.cc"
    "libs/analysis_pdc_reco/include/PDCMomentumReconstructor.hh"
    "libs/analysis_pdc_reco/src/PDCMomentumReconstructor.cc"
    "libs/analysis_pdc_reco/include/PDCNNMomentumReconstructor.hh"
    "libs/analysis_pdc_reco/src/PDCNNMomentumReconstructor.cc"
    "scripts/reconstruction/nn_target_momentum/build_dataset.C"
    "scripts/reconstruction/nn_target_momentum/train_mlp.py"
    "scripts/reconstruction/nn_target_momentum/export_model_for_cpp.py"
    "scripts/analysis/run_sn124_nn_reco_pipeline.sh"
    "apps/tools/reconstruct_sn_nn.cc"
    "apps/tools/evaluate_reconstruct_sn_nn.cc"
    "skills/smsim-pdc-track-reco/SKILL.md"
    "skills/smsim-pdc-momentum-reco/SKILL.md"
    "skills/smsim-nn-target-momentum/SKILL.md"
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
        configs/simulation/DbeamTest/detailMag1to1.2T/*|docs/reports/reconstruction/nn_retrain_qmdwindow_B115T3deg_notes.zh.md|docs/note_log/task.md|scripts/reconstruction/nn_target_momentum/*|scripts/analysis/run_sn124_nn_reco_pipeline.sh|apps/tools/reconstruct_sn_nn.cc|apps/tools/evaluate_reconstruct_sn_nn.cc|libs/analysis/src/PDCSimAna.cc|libs/analysis/include/PDCSimAna.hh|libs/analysis_pdc_reco/*|skills/smsim-pdc-track-reco/*|skills/smsim-pdc-momentum-reco/*|skills/smsim-nn-target-momentum/*)
            echo "[SYNC] PDC target-reconstruction overview anchors changed: ${f}"
            echo "       Update references/workflow.md, references/principles.md, or references/path-map.md if the project map changed."
            break
            ;;
    esac
done

python3 /home/tian/.codex/skills/.system/skill-creator/scripts/quick_validate.py "${SKILL_DIR}"

if [[ ${missing} -ne 0 ]]; then
    echo "[ACTION] Structure changed. Update references/path-map.md and scripts/check_sync.sh."
    exit 2
fi

echo "[OK] smsim-pdc-target-reco-overview sync check passed."
