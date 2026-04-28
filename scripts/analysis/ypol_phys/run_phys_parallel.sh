#!/bin/bash
# Parallel orchestrator for ypol elastic phys pilot (8 cells).
set -euo pipefail

SMSIM_DIR=${SMSIM_DIR:-/home/tian/workspace/dpol/smsimulator5.5}
J=${J:-6}

# Targets / gammas / helicities can be overridden via env, e.g.
#   GAMMAS="g050 g060 g070 g080" bash run_phys_parallel.sh
read -r -a TARGETS <<< "${TARGETS:-Sn112E190 Sn124E190}"
read -r -a GAMMAS  <<< "${GAMMAS:-g050 g080}"
read -r -a HELICITIES <<< "${HELICITIES:-ynp ypn}"

cd "${SMSIM_DIR}"

TASKS=()
for tgt in "${TARGETS[@]}"; do
    for g in "${GAMMAS[@]}"; do
        for h in "${HELICITIES[@]}"; do
            TASKS+=("${tgt} ${g} ${h}")
        done
    done
done

echo "[phys] launching ${#TASKS[@]} cells, ${J} parallel jobs"
START=$SECONDS

printf "%s\n" "${TASKS[@]}" | xargs -n 3 -P "${J}" \
    bash "${SMSIM_DIR}/scripts/analysis/ypol_phys/run_one_phys_cell.sh"

echo "[phys] all ${#TASKS[@]} cells done in $((SECONDS - START))s"
