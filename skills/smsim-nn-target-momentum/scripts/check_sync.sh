#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKILL_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd "${SKILL_DIR}/../.." && pwd)"
BASE_REF="${1:-HEAD~1}"

required_paths=(
    "scripts/reconstruction/nn_target_momentum/build_dataset.C"
    "scripts/reconstruction/nn_target_momentum/train_mlp.py"
    "scripts/reconstruction/nn_target_momentum/infer_mlp.py"
    "scripts/reconstruction/nn_target_momentum/evaluate_mlp.py"
    "scripts/reconstruction/nn_target_momentum/export_model_for_cpp.py"
    "scripts/reconstruction/nn_target_momentum/run_formal_training.sh"
    "scripts/reconstruction/nn_target_momentum/run_domain_matched_retrain.sh"
    "scripts/analysis/run_sn124_nn_reco_pipeline.sh"
    "libs/analysis_pdc_reco/include/PDCNNMomentumReconstructor.hh"
    "libs/analysis_pdc_reco/src/PDCNNMomentumReconstructor.cc"
    "libs/analysis_pdc_reco/src/PDCMomentumReconstructor.cc"
    "libs/analysis_pdc_reco/src/PDCMomentumReconstructorNN.cc"
    "apps/tools/reconstruct_sn_nn.cc"
    "apps/tools/evaluate_reconstruct_sn_nn.cc"
    "tests/analysis/test_PDCMomentumReconstructor.cc"
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
        scripts/reconstruction/nn_target_momentum/*|scripts/analysis/run_sn124_nn_reco_pipeline.sh|apps/tools/reconstruct_sn_nn.cc|apps/tools/evaluate_reconstruct_sn_nn.cc|libs/analysis_pdc_reco/*|tests/analysis/test_PDCMomentumReconstructor.cc)
            echo "[SYNC] NN target-momentum pipeline changed: ${f}"
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

echo "[OK] smsim-nn-target-momentum sync check passed."
