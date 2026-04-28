#!/bin/bash
# [EN] Orchestrate the ypol pilot: subsample, reco, error analysis across
#      (condition, gamma, helicity) cells; output per-cell CSVs.
# [CN] ypol pilot 端到端编排：抽样、重建、误差分析；按 (condition, gamma, helicity) 单元产出 CSV。

set -euo pipefail

SMSIM_DIR=${SMSIM_DIR:-/home/tian/workspace/dpol/smsimulator5.5}
BUILD_DIR=${BUILD_DIR:-${SMSIM_DIR}/build}
OUT_ROOT=${OUT_ROOT:-${BUILD_DIR}/test_output/ypol_pilot}
TARGET=${TARGET:-d+Sn124E190}
N_PER_CELL=${N_PER_CELL:-125}        # Geant4 events per (gamma, helicity)
PROFILE_PER_Q=${PROFILE_PER_Q:-4}    # Profile-LH events per chi^2 quartile (×4 = 16/cell)
MCMC_PER_Q=${MCMC_PER_Q:-2}          # MCMC events per chi^2 quartile (×4 = 8/cell)
MCMC_N_SAMPLES=${MCMC_N_SAMPLES:-160}
MCMC_BURN_IN=${MCMC_BURN_IN:-80}
SEED_BASE=${SEED_BASE:-20260427}
RK_FIT_MODE=${RK_FIT_MODE:-fixed-target-pdc-only}

GEOM_MAC=${SMSIM_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac
FIELD_MAP=${SMSIM_DIR}/configs/simulation/geometry/filed_map/180703-1,15T-3000.table

mkdir -p "${OUT_ROOT}/sampled" "${OUT_ROOT}/reco" "${OUT_ROOT}/error_analysis"

CONDITIONS=(allair mixed)
# 8 gammas × 2 helicities = 16 cells per condition.
GAMMAS=(g050 g055 g060 g065 g070 g075 g080 g085)
HELICITIES=(ynp ypn)

# Cell counter for unique seeds.
cell_idx=0

for cond in "${CONDITIONS[@]}"; do
    INPUT_BASE="${SMSIM_DIR}/data/simulation/g4output/ypol_new_20260413_${cond}/${TARGET}"
    if [[ ! -d "${INPUT_BASE}" ]]; then
        echo "[pilot] missing input dir: ${INPUT_BASE}" >&2
        exit 1
    fi

    for gamma in "${GAMMAS[@]}"; do
        for hel in "${HELICITIES[@]}"; do
            cell_idx=$((cell_idx + 1))
            seed=$((SEED_BASE + cell_idx))
            tag="${cond}_${gamma}${hel}"
            INPUT="${INPUT_BASE}/${TARGET}${gamma}${hel}-RP360/geminiout0000.root"
            SAMPLED="${OUT_ROOT}/sampled/${tag}.root"
            RECO="${OUT_ROOT}/reco/${tag}_reco.root"
            ANA_DIR="${OUT_ROOT}/error_analysis/${tag}"

            if [[ ! -f "${INPUT}" ]]; then
                echo "[pilot] missing input file (skip cell): ${INPUT}" >&2
                continue
            fi

            mkdir -p "${ANA_DIR}"

            # 1. Subsample.
            if [[ ! -f "${SAMPLED}" ]]; then
                echo "[pilot] [${cell_idx}/32] subsample ${tag} (n=${N_PER_CELL}, seed=${seed})"
                python3 "${SMSIM_DIR}/scripts/analysis/ypol_pilot/subsample_geminiout.py" \
                    --input "${INPUT}" \
                    --output "${SAMPLED}" \
                    --n-events "${N_PER_CELL}" \
                    --seed "${seed}"
            else
                echo "[pilot] [${cell_idx}/32] subsample ${tag} cached"
            fi

            # 2. Reco.
            if [[ ! -f "${RECO}" ]]; then
                echo "[pilot] [${cell_idx}/32] reco ${tag}"
                "${BUILD_DIR}/bin/reconstruct_target_momentum" \
                    --input-file "${SAMPLED}" \
                    --output-file "${RECO}" \
                    --geometry-macro "${GEOM_MAC}" \
                    --magnetic-field-map "${FIELD_MAP}" \
                    --backend rk \
                    --rk-fit-mode "${RK_FIT_MODE}" \
                    --rk-write-errors on \
                    --rk-write-laplace on >/dev/null
            else
                echo "[pilot] [${cell_idx}/32] reco ${tag} cached"
            fi

            # 3. Error analysis.
            if [[ ! -f "${ANA_DIR}/track_summary.csv" ]]; then
                echo "[pilot] [${cell_idx}/32] error_analysis ${tag}"
                "${BUILD_DIR}/bin/analyze_pdc_rk_error" \
                    --input-file "${RECO}" \
                    --output-dir "${ANA_DIR}" \
                    --geometry-macro "${GEOM_MAC}" \
                    --magnetic-field-map "${FIELD_MAP}" \
                    --rk-fit-mode "${RK_FIT_MODE}" \
                    --profile-per-quartile "${PROFILE_PER_Q}" \
                    --mcmc-per-quartile "${MCMC_PER_Q}" \
                    --mcmc-n-samples "${MCMC_N_SAMPLES}" \
                    --mcmc-burn-in "${MCMC_BURN_IN}" >/dev/null
            else
                echo "[pilot] [${cell_idx}/32] error_analysis ${tag} cached"
            fi
        done
    done
done

echo "[pilot] all 32 cells processed."
echo "[pilot] outputs under: ${OUT_ROOT}/error_analysis/<cond>_<gamma><hel>/track_summary.csv"
