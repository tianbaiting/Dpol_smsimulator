#!/usr/bin/env bash
# [EN] Batch driver for NEBULA-Plus geometry simulation.
# [CN] NEBULA-Plus 几何的批量跑批脚本。
#
# Processes y-pol (ynp+ypn) and z-pol (znp+zpn) for Sn112/Sn124,
# g050-g080, using 3deg1.15Tnebulaplus.mac geometry.
#
# Internally calls run_g4input_batch_parallel.sh for each input
# directory, with xargs -P N parallelism per batch.
#
# Usage:
#   SMSIMDIR=... JOBS=8 bash scripts/batch/batch_run_nebulaplus.sh [--dry-run]
#
# Environment:
#   SMSIMDIR   (required) — project root
#   JOBS       (optional, default 8) — parallel workers per batch

set -euo pipefail

SMSIMDIR="${SMSIMDIR:?SMSIMDIR required}"
JOBS="${JOBS:-8}"
DRY_RUN=false
[[ "${1:-}" == "--dry-run" ]] && DRY_RUN=true

GEOM_MAC="${SMSIMDIR}/configs/simulation/geometry/3deg1.15Tnebulaplus.mac"
SIM_EXEC="${SMSIMDIR}/build/bin/sim_deuteron"
MACRO_BASE="${SMSIMDIR}/build/macros_nebulaplus"
DRIVER="${SMSIMDIR}/scripts/simulation/run_g4input_batch_parallel.sh"

INPUT_YPOL="${SMSIMDIR}/data/simulation/g4input/20260413ypol"
INPUT_ZPOL="${SMSIMDIR}/data/simulation/g4input/z_pol/b_discrete"
OUTPUT_YPOL="${SMSIMDIR}/data/simulation/g4output/y_pol_nebulaplus/phi_random"
OUTPUT_ZPOL="${SMSIMDIR}/data/simulation/g4output/z_pol_nebulaplus/b_discrete"

TARGETS=("d+Sn112E190" "d+Sn124E190")
GAMMAS=("g050" "g060" "g070" "g080")

if [[ ! -x "$SIM_EXEC" ]]; then
  echo "[ERROR] sim_deuteron not executable: $SIM_EXEC" >&2; exit 1
fi
if [[ ! -f "$GEOM_MAC" ]]; then
  echo "[ERROR] geometry macro not found: $GEOM_MAC" >&2; exit 1
fi
if [[ ! -f "$DRIVER" ]]; then
  echo "[ERROR] driver script not found: $DRIVER" >&2; exit 1
fi

echo "[INIT] SMSIMDIR=${SMSIMDIR}"
echo "[INIT] GEOM=$(basename "$GEOM_MAC")"
echo "[INIT] SIM_EXEC=${SIM_EXEC}"
echo "[INIT] JOBS=${JOBS}"
echo "[INIT] DRY_RUN=${DRY_RUN}"
echo ""

total_planned=0

run_batch() {
  local tag="$1" input_dir="$2" output_base="$3"
  local macro_dir="${MACRO_BASE}/${tag}"
  mkdir -p "$macro_dir" "$output_base"

  if $DRY_RUN; then
    local n
    n=$(find -L "$input_dir" -type f -name "*.root" 2>/dev/null | wc -l)
    echo "[DRY-RUN ${tag}] ${n} files  INPUT=${input_dir}  OUTPUT=${output_base}"
    total_planned=$((total_planned + n))
    return
  fi

  echo "[BATCH ${tag}] INPUT=${input_dir} OUTPUT=${output_base}"
  SMSIMDIR="$SMSIMDIR" \
    SIM_EXEC="$SIM_EXEC" \
    GEOM_MAC="$GEOM_MAC" \
    INPUT_ROOT="$input_dir" \
    OUTPUT_BASE="$output_base" \
    MACRO_DIR="$macro_dir" \
    JOBS="$JOBS" \
    TAG="$tag" \
    bash "$DRIVER"

  echo ""
}

# --- y-pol: each {target}/{config}-RP360/ is a separate batch ---
for target in "${TARGETS[@]}"; do
  for gamma in "${GAMMAS[@]}"; do
    for pol in ynp ypn; do
      config="${target}${gamma}${pol}"
      input_dir="${INPUT_YPOL}/${target}/${config}-RP360"
      if [[ -d "$input_dir" ]]; then
        run_batch "ypol_${config}" "$input_dir" "${OUTPUT_YPOL}/${config}"
      else
        echo "[SKIP] ypol ${config} — directory not found: ${input_dir}"
      fi
    done
  done
done

# --- z-pol: each {config}/ is a separate batch ---
for target in "${TARGETS[@]}"; do
  for gamma in "${GAMMAS[@]}"; do
    for pol in znp zpn; do
      config="${target}${gamma}${pol}"
      input_dir="${INPUT_ZPOL}/${config}"
      if [[ -d "$input_dir" ]]; then
        run_batch "zpol_${config}" "$input_dir" "${OUTPUT_ZPOL}/${config}"
      else
        echo "[SKIP] zpol ${config} — directory not found: ${input_dir}"
      fi
    done
  done
done

echo "[DONE] total batches submitted"
$DRY_RUN && echo "[DRY-RUN] total planned files: ${total_planned}"
