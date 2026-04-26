#!/usr/bin/env bash
# [EN] Parallel Geant4 driver for g4input trees.
# [EN] Similar spirit to run_g4input_batch.sh but:
#   (a) Takes INPUT_ROOT as the directory that directly holds the *.root tree files
#       (find descends recursively); no ypol*/zpol* glob, so symlinked dirs work.
#   (b) Pre-generates one macro per input and runs them through `xargs -P N`.
# [CN] g4input 树的并行 Geant4 跑批脚本
#   (a) INPUT_ROOT 直接指向含 *.root 的目录（递归），不做 ypol*/zpol* 通配，软链接目录也能跑。
#   (b) 每个输入文件生成一个 macro，再用 xargs -P N 并行调用 sim_deuteron。
#
# Usage:
#   INPUT_ROOT=...  OUTPUT_BASE=...  GEOM_MAC=...  MACRO_DIR=...  [JOBS=16]  [TAG=allair] \
#       bash scripts/simulation/run_g4input_batch_parallel.sh
#
# Skip-if-exists: any output *.root > 100 KB is treated as already done.
# Per-file log/macro survive for failed jobs; successful jobs delete the macro.

set -u

SMSIMDIR="${SMSIMDIR:-/home/tian/workspace/dpol/smsimulator5.5}"
SIM_EXEC="${SIM_EXEC:-${SMSIMDIR}/build/bin/sim_deuteron}"
GEOM_MAC="${GEOM_MAC:?GEOM_MAC required (absolute path to geometry macro)}"
INPUT_ROOT="${INPUT_ROOT:?INPUT_ROOT required (dir containing *.root input trees)}"
OUTPUT_BASE="${OUTPUT_BASE:?OUTPUT_BASE required}"
MACRO_DIR="${MACRO_DIR:?MACRO_DIR required (per-run macro staging dir)}"
JOBS="${JOBS:-16}"
TAG="${TAG:-g4sim}"
# [EN] Optional filename glob to pick a subset of input ROOTs (e.g. "dbreak*.root").
# [CN] 可选的文件名通配符，用于在同一目录里挑选子集（例如 "dbreak*.root" 只跑弹性）。
PATTERN="${PATTERN:-*.root}"

if [[ ! -x "$SIM_EXEC" ]]; then
  echo "[ERROR] sim_deuteron not executable: $SIM_EXEC" >&2
  exit 1
fi
if [[ ! -f "$GEOM_MAC" ]]; then
  echo "[ERROR] Geometry macro not found: $GEOM_MAC" >&2
  exit 1
fi
if [[ ! -d "$INPUT_ROOT" ]]; then
  echo "[ERROR] INPUT_ROOT not a directory: $INPUT_ROOT" >&2
  exit 1
fi

mkdir -p "$MACRO_DIR" "$OUTPUT_BASE"
JOBLIST="${MACRO_DIR}/_joblist.txt"
: > "$JOBLIST"

# [EN] Enumerate input ROOTs under INPUT_ROOT matching PATTERN; sort for deterministic order.
# [CN] 收集 INPUT_ROOT 下匹配 PATTERN 的 .root 输入并按字典序排序。
mapfile -t FILES < <(find "$INPUT_ROOT" -type f -name "$PATTERN" | sort)
total="${#FILES[@]}"
planned=0
skipped=0

for f in "${FILES[@]}"; do
  rel="${f#${INPUT_ROOT}/}"
  rel_dir="$(dirname "$rel")"
  run_name="$(basename "$f" .root)"
  output_dir="${OUTPUT_BASE}/${rel_dir}"
  out_root="${output_dir}/${run_name}.root"
  macro_file="${MACRO_DIR}/run_${rel_dir//\//_}_${run_name}.mac"
  mkdir -p "$output_dir"

  # [EN] Skip if a plausibly-complete output already exists (>100 KB).
  # [CN] 若输出已存在且 >100 KB，认为之前已完成，跳过。
  if [[ -s "$out_root" ]] && [[ $(stat -c %s "$out_root") -gt 100000 ]]; then
    skipped=$((skipped+1))
    continue
  fi

  # [EN] Per-file Geant4 macro: same shape as run_g4input_batch.sh, beamOn=0 means "use all events in tree".
  # [CN] 每个输入对应一个宏，与 run_g4input_batch.sh 一致，beamOn=0 表示用 tree 中全部事件。
  cat > "$macro_file" <<EOF
/action/file/OverWrite y
/action/file/SaveDirectory ${output_dir}/
/action/file/RunName ${run_name}
/control/execute ${GEOM_MAC}
/tracking/storeTrajectory 0
/action/gun/Type Tree
/action/gun/tree/InputFileName ${f}
/action/gun/tree/TreeName tree
/action/gun/tree/beamOn 0
exit
EOF
  echo "${macro_file}|${output_dir}/${run_name}.log" >> "$JOBLIST"
  planned=$((planned+1))
done

echo "[INIT ${TAG}] total=${total} skipped=${skipped} planned=${planned} jobs=${JOBS} pattern=${PATTERN}"
echo "[INIT ${TAG}] GEOM=$(basename "$GEOM_MAC")"
echo "[INIT ${TAG}] OUTPUT_BASE=${OUTPUT_BASE}"

# [EN] run_one is invoked by xargs once per joblist line.
# [CN] run_one 由 xargs 针对 joblist 的每一行调用一次。
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

# [EN] xargs -P N: up to N jobs in flight; one line per job; quoting-safe because
# entries have no spaces (joblist uses | as the delimiter inside a single field).
# [CN] xargs -P N 并行，每行一个 job。entries 不含空格，用 | 分隔 macro 和 log 路径。
xargs -a "$JOBLIST" -I {} -P "$JOBS" bash -c 'run_one "$@"' _ {}

echo "[DONE ${TAG}] planned=${planned}"
