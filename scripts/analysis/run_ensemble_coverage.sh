#!/bin/bash
# [EN] Run the PDC target-momentum ensemble coverage pipeline end-to-end.
# [CN] 端到端运行 PDC 靶点动量 ensemble 覆盖率流水线。

set -euo pipefail

SMSIM_DIR=${SMSIM_DIR:-/home/tian/workspace/dpol/smsimulator5.5}
BUILD_DIR=${BUILD_DIR:-${SMSIM_DIR}/build}
ENSEMBLE_SIZE=${ENSEMBLE_SIZE:-500}
RK_FIT_MODE=${RK_FIT_MODE:-fixed-target-pdc-only}
OUT_ROOT=${OUT_ROOT:-${BUILD_DIR}/test_output/ensemble_coverage}
TAG=${TAG:-rk_${RK_FIT_MODE//-/_}}
SEED_A=${SEED_A:-20260326}
SEED_B=${SEED_B:-20260327}

# [EN] Normalize OUT_ROOT/BUILD_DIR to absolute paths (test binary cds into output-dir so relative paths break log writes). / [CN] OUT_ROOT/BUILD_DIR 必须为绝对路径，否则测试进程 cd 后 log 路径失效。
if [[ "${OUT_ROOT}" != /* ]]; then
    OUT_ROOT="$(pwd)/${OUT_ROOT}"
fi
if [[ "${BUILD_DIR}" != /* ]]; then
    BUILD_DIR="$(pwd)/${BUILD_DIR}"
fi

SIM_OUT=${OUT_ROOT}/${TAG}
ANA_OUT=${OUT_ROOT}/${TAG}_error_analysis

mkdir -p "${SIM_OUT}" "${ANA_OUT}"

echo "[ensemble] SMSIM_DIR=${SMSIM_DIR}"
echo "[ensemble] ENSEMBLE_SIZE=${ENSEMBLE_SIZE}"
echo "[ensemble] RK_FIT_MODE=${RK_FIT_MODE}"
echo "[ensemble] SIM_OUT=${SIM_OUT}"
echo "[ensemble] ANA_OUT=${ANA_OUT}"

# [EN] Stage 1: run Geant4 + reconstruction over (px,py) grid with N replicas. / [CN] 第一阶段：跑 Geant4 + 重建。
"${BUILD_DIR}/bin/test_pdc_target_momentum_e2e" \
    --backend rk \
    --smsimdir "${SMSIM_DIR}" \
    --output-dir "${SIM_OUT}" \
    --sim-bin "${BUILD_DIR}/bin/sim_deuteron" \
    --reco-bin "${BUILD_DIR}/bin/reconstruct_target_momentum" \
    --eval-bin "${BUILD_DIR}/bin/evaluate_target_momentum_reco" \
    --geometry-macro "${SMSIM_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac" \
    --field-map "${SMSIM_DIR}/configs/simulation/geometry/filed_map/180703-1,15T-3000.table" \
    --nn-model-json "${SMSIM_DIR}/data/nn_target_momentum/formal_B115T3deg_qmdwindow/20260227_223007/model/model_cpp.json" \
    --threshold-px 500 --threshold-py 500 --threshold-pz 500 \
    --seed-a "${SEED_A}" --seed-b "${SEED_B}" \
    --rk-fit-mode "${RK_FIT_MODE}" \
    --require-gate-pass 0 --require-min-matched 1 \
    --ensemble-size "${ENSEMBLE_SIZE}"

RECO_ROOT=${SIM_OUT}/reco/pdc_truth_grid_rk_${RK_FIT_MODE//-/_}_reco.root
if [[ ! -f "${RECO_ROOT}" ]]; then
    echo "[ensemble] reco ROOT not found: ${RECO_ROOT}" >&2
    exit 1
fi

# [EN] Stage 2: error analysis over all ensemble events. / [CN] 第二阶段：对全部 ensemble 事件做误差分析。
"${BUILD_DIR}/bin/analyze_pdc_rk_error" \
    --geometry-macro "${SMSIM_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac" \
    --magnetic-field-map "${SMSIM_DIR}/configs/simulation/geometry/filed_map/180703-1,15T-3000.table" \
    --input-file "${RECO_ROOT}" \
    --output-dir "${ANA_OUT}" \
    --profile-per-quartile "${PROFILE_PER_QUARTILE:-32}" \
    --mcmc-per-quartile "${MCMC_PER_QUARTILE:-32}"

echo "[ensemble] done. outputs:"
echo "  reco ROOT: ${RECO_ROOT}"
echo "  validation_summary: ${ANA_OUT}/validation_summary.csv"
echo "  track_summary: ${ANA_OUT}/track_summary.csv"
echo "  bayesian_samples: ${ANA_OUT}/bayesian_samples.csv"
echo "  profile_samples: ${ANA_OUT}/profile_samples.csv"
