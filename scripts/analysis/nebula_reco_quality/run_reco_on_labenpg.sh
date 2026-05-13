#!/bin/bash
# Drive the NEBULA-only reco over the (px,py) scan g4output on labenpg-hk.
#
# Usage:
#   bash run_reco_on_labenpg.sh [N_PARALLEL]
#
# Pre-requisites: full scan from run_scan_on_labenpg.sh completed (all
# pxpy_scan_shard*.root present on the remote g4output dir).
#
# Shard naming convention:
#   g4output/pxpy_scan_shard{jj}{rrrr}.root  <-> inputs/grid_shard{jj}.root
#   where jj = 2-digit shard index, rrrr = 4-digit sim_deuteron run counter
#   e.g. pxpy_scan_shard000000.root          <-> grid_shard00.root
set -euo pipefail

REMOTE_BASE=${REMOTE_BASE:-/data/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513}
N_PARALLEL=${1:-24}
# Use SCAN_HOST (not HOST) to avoid collision with the HOST env var set by conda
# build environments (conda sets HOST=x86_64-conda-linux-gnu).
SCAN_HOST=${SCAN_HOST:-labenpg-hk}

# Send the remote script via heredoc stdin.  REMOTE_BASE and N_PARALLEL are
# passed as positional args so the outer shell expands them cleanly; all other
# $ are protected by the single-quote heredoc delimiter.
ssh "${SCAN_HOST}" bash -s -- "${REMOTE_BASE}" "${N_PARALLEL}" <<'REMOTE_SCRIPT'
set -eo pipefail
REMOTE_BASE="$1"
N_PARALLEL="$2"

export MAMBA_EXE=/data/tian/software/micromamba/bin/micromamba
export MAMBA_ROOT_PREFIX=/data/tian/conda
set +u
eval "$($MAMBA_EXE shell hook --shell bash --root-prefix $MAMBA_ROOT_PREFIX)"
micromamba activate anaroot-env
source /data/tian/workspace/dpol/smsimulator5.5/setup.sh >/dev/null
set -u

MACRO=/data/tian/workspace/dpol/smsimulator5.5/scripts/analysis/nebula_reco_quality/run_nebula_reco.C

cd "${REMOTE_BASE}"
mkdir -p summary logs
echo "[run_reco] launching N=${N_PARALLEL} parallel reco jobs at $(date -Iseconds)"

# Pre-compile the macro once via ACLiC before spawning parallel workers.
# This avoids concurrent ACLiC cache races and bakes in the correct include
# paths from ROOT_INCLUDE_PATH (set by setup.sh on this host).
echo "[run_reco] pre-compiling ${MACRO} via ACLiC..."
root -l -b -q "${MACRO}+" 2>&1 | tail -3
echo "[run_reco] ACLiC compile done"

# For each g4output shard, derive the matching input shard index and emit a
# summary root.
#
# Naming convention produced by sim_deuteron:
#   macros use RunName "pxpy_scan_shard{jj}" (2-digit shard index)
#   sim_deuteron appends a 4-digit run counter → pxpy_scan_shard{jj}{rrrr}.root
#   e.g. pxpy_scan_shard000000.root  (jj=00, rrrr=0000) -> grid_shard00.root
#        pxpy_scan_shard010000.root  (jj=01, rrrr=0000) -> grid_shard01.root
#
# We extract the first 2 digits of the 6-digit suffix as the shard index.
# Use the compiled .so via the '+' suffix on each worker invocation.
ls g4output/pxpy_scan_shard*.root | xargs -n1 -P"${N_PARALLEL}" -I{} bash -c '
  g4out="$0"
  name=$(basename "$g4out" .root)
  # The 6-digit suffix encodes {shard_idx:02d}{run_id:04d}.
  # Extract first 2 digits as shard index (leading zeros preserved for grid_shard).
  six_digits=$(echo "$name" | sed -E "s/^pxpy_scan_shard([0-9]{6}).*$/\1/")
  shard_idx="${six_digits:0:2}"
  input="inputs/grid_shard${shard_idx}.root"
  outsum="summary/${name}.root"
  if [ ! -f "$input" ]; then
      echo "[${name}] SKIP: input not found: $input"
      exit 0
  fi
  echo "[${name}] start $(date -Iseconds)"
  root -l -b -q "/data/tian/workspace/dpol/smsimulator5.5/scripts/analysis/nebula_reco_quality/run_nebula_reco.C+(\"$g4out\",\"$input\",\"$outsum\")" \
      > /data/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513/logs/reco_${name}.log 2>&1
  rc=$?
  echo "[${name}] done rc=${rc} $(date -Iseconds)"
' {}

echo "[run_reco] all shards reconstructed at $(date -Iseconds)"
ls -1 summary/*.root 2>/dev/null | wc -l | xargs echo "summary files:"
REMOTE_SCRIPT
