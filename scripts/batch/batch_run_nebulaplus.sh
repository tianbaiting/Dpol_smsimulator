#!/usr/bin/env bash
# [EN] NEBULA-Plus geometry batch: flat global parallel, nohup-safe.
# [CN] NEBULA-Plus 几何批量跑批：全局扁平并行，nohup 安全。
#
# Processes y-pol (ynp+ypn) and z-pol (znp+zpn) for Sn112/Sn124,
# g050-g080, using 3deg1.15Tnebulaplus.mac geometry.
#
# All 176 input files are collected into one flat joblist and
# dispatched via xargs -P N for true global parallelism.
#
# Usage (on spana03):
#   nohup bash scripts/batch/batch_run_nebulaplus.sh \
#     > logs/nebulaplus_batch.log 2>&1 &
#   echo $! > logs/nebulaplus_batch.pid
#
# Monitor:
#   tail -f logs/nebulaplus_batch.log
#   grep -c "\[OK" logs/nebulaplus_batch.log
#
# Environment:
#   SMSIMDIR   (required) — project root
#   JOBS       (optional, default 8) — parallel workers

set -euo pipefail

SMSIMDIR="${SMSIMDIR:?SMSIMDIR required}"
JOBS="${JOBS:-8}"
GEOM_MAC="${SMSIMDIR}/configs/simulation/geometry/3deg1.15Tnebulaplus.mac"
SIM_EXEC="${SMSIMDIR}/build/bin/sim_deuteron"
MACRO_DIR="${SMSIMDIR}/build/macros_nebulaplus"
JOBLIST="${MACRO_DIR}/_joblist.txt"

INPUT_YPOL="${SMSIMDIR}/data/simulation/g4input/20260413ypol"
INPUT_ZPOL="${SMSIMDIR}/data/simulation/g4input/z_pol/b_discrete"
OUTPUT_YPOL="${SMSIMDIR}/data/simulation/g4output/y_pol_nebulaplus/phi_random"
OUTPUT_ZPOL="${SMSIMDIR}/data/simulation/g4output/z_pol_nebulaplus/b_discrete"

TARGETS=("d+Sn112E190" "d+Sn124E190")
GAMMAS=("g050" "g060" "g070" "g080")

[[ -x "$SIM_EXEC" ]] || { echo "[ERROR] not executable: $SIM_EXEC" >&2; exit 1; }
[[ -f "$GEOM_MAC" ]] || { echo "[ERROR] not found: $GEOM_MAC" >&2; exit 1; }

mkdir -p "$MACRO_DIR"
: > "$JOBLIST"

planned=0
skipped=0

add_job() {
  local input_file="$1" output_dir="$2" run_name="$3" config="$4"
  mkdir -p "$output_dir"

  local existing_size=0 cand sz
  for cand in "${output_dir}/${run_name}".root "${output_dir}/${run_name}"[0-9]*.root; do
    [[ -s "$cand" ]] || continue
    sz=$(stat -c %s "$cand" 2>/dev/null || echo 0)
    [[ "$sz" -gt "$existing_size" ]] && existing_size=$sz
  done
  if [[ "$existing_size" -gt 100000 ]]; then
    skipped=$((skipped+1))
    return
  fi

  local macro_file="${MACRO_DIR}/run_${config}_${run_name}.mac"
  cat > "$macro_file" <<EOF
/action/file/OverWrite y
/action/file/SaveDirectory ${output_dir}/
/action/file/RunName ${run_name}
/control/execute ${GEOM_MAC}
/tracking/storeTrajectory 0
/action/gun/Type Tree
/action/gun/tree/InputFileName ${input_file}
/action/gun/tree/TreeName tree
/action/gun/tree/beamOn 0
exit
EOF
  echo "${macro_file}|${output_dir}/${run_name}.log" >> "$JOBLIST"
  planned=$((planned+1))
}

# --- y-pol: 2 targets × 4 gammas × 2 pols = 16 jobs ---
for target in "${TARGETS[@]}"; do
  for gamma in "${GAMMAS[@]}"; do
    for pol in ynp ypn; do
      config="${target}${gamma}${pol}"
      input="${INPUT_YPOL}/${target}/${config}-RP360/dbreak.root"
      output="${OUTPUT_YPOL}/${config}"
      [[ -f "$input" ]] && add_job "$input" "$output" "dbreak" "$config"
    done
  done
done

# --- z-pol: 2 targets × 4 gammas × 2 pols × 10 files = 160 jobs ---
for target in "${TARGETS[@]}"; do
  for gamma in "${GAMMAS[@]}"; do
    for pol in znp zpn; do
      config="${target}${gamma}${pol}"
      input_dir="${INPUT_ZPOL}/${config}"
      output="${OUTPUT_ZPOL}/${config}"
      [[ -d "$input_dir" ]] || continue
      for f in $(find -L "$input_dir" -type f -name "dbreakb*.root" | sort); do
        run_name="$(basename "$f" .root)"
        add_job "$f" "$output" "$run_name" "$config"
      done
    done
  done
done

echo "[INIT] GEOM=$(basename "$GEOM_MAC") JOBS=${JOBS} planned=${planned} skipped=${skipped}"
echo "[INIT] SIM_EXEC=${SIM_EXEC}"

[[ "$planned" -eq 0 ]] && { echo "[DONE] nothing to run"; exit 0; }

run_one() {
  local entry="$1"
  local macro="${entry%%|*}"
  local logf="${entry##*|}"
  local start end rc
  start=$(date +%s)
  if "$SIM_EXEC" "$macro" > "$logf" 2>&1; then
    end=$(date +%s)
    echo "[OK $((end-start))s] $(basename "$(dirname "$logf")")/$(basename "$logf" .log)"
    rm -f "$macro"
  else
    rc=$?
    end=$(date +%s)
    echo "[FAIL rc=$rc t=$((end-start))s] macro=$macro log=$logf"
  fi
}
export -f run_one
export SIM_EXEC

xargs -a "$JOBLIST" -I {} -P "$JOBS" bash -c 'run_one "$@"' _ {}

echo "[DONE] planned=${planned}"
