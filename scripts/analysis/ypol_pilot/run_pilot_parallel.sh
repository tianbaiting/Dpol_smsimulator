#!/bin/bash
# [EN] Parallel orchestration of all 32 (condition, gamma, helicity) cells.
# [CN] 32 个 (条件, gamma, 螺旋度) 单元的并行编排。

set -euo pipefail

SMSIM_DIR=${SMSIM_DIR:-/home/tian/workspace/dpol/smsimulator5.5}
J=${J:-6}                                # parallel jobs (8 PDC analyzer instances ~4 GB RAM)
SEED_BASE=${SEED_BASE:-20260427}

CONDITIONS=(allair mixed)
GAMMAS=(g050 g055 g060 g065 g070 g075 g080 g085)
HELICITIES=(ynp ypn)

cd "${SMSIM_DIR}"

# Build the cell list (32 cells: cond × gamma × helicity).
TASKS=()
idx=0
for cond in "${CONDITIONS[@]}"; do
    for gamma in "${GAMMAS[@]}"; do
        for hel in "${HELICITIES[@]}"; do
            idx=$((idx + 1))
            seed=$((SEED_BASE + idx))
            TASKS+=("${cond} ${gamma} ${hel} ${seed}")
        done
    done
done

echo "[pilot] launching ${#TASKS[@]} cells, ${J} parallel jobs"
START=$SECONDS

# Use xargs -P for parallelism.
printf "%s\n" "${TASKS[@]}" | xargs -n 4 -P "${J}" \
    bash "${SMSIM_DIR}/scripts/analysis/ypol_pilot/run_one_cell.sh"

echo "[pilot] all 32 cells done in $((SECONDS - START))s"
