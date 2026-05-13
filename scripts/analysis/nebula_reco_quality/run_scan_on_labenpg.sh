#!/bin/bash
# Drive the (px,py) acceptance scan on labenpg-hk via parallel sim_deuteron jobs.
#
# Usage:
#   bash run_scan_on_labenpg.sh [N_PARALLEL]
#
# Pre-requisites (set up by earlier tasks):
#   - Inputs rsynced to ${REMOTE_BASE}/inputs/<prefix>_shard*.root
#   - Macros rsynced to ${REMOTE_BASE}/macros/pxpy_scan_shard*.mac
#   - g4output and logs dirs created on the remote
#
# Each shard mac is processed by one sim_deuteron invocation.  xargs -P
# parallelizes across shards.  Each shard's stdout/stderr goes to a per-shard
# log under ${REMOTE_BASE}/logs/.
set -euo pipefail

REMOTE_BASE=${REMOTE_BASE:-/data/tian/workspace/dpol/smsimulator5.5/build/test_output/nebula_reco_quality_20260513}
N_PARALLEL=${1:-24}
# Use SCAN_HOST (not HOST) to avoid collision with the HOST env var set by conda
# build environments (conda sets HOST=x86_64-conda-linux-gnu).
SCAN_HOST=${SCAN_HOST:-labenpg-hk}

# Send the remote script via heredoc stdin (avoids multi-level quote escaping).
# REMOTE_BASE and N_PARALLEL are passed as positional arguments so the outer
# shell expands them cleanly; all other $ are protected by the single-quote
# heredoc delimiter.
ssh "${SCAN_HOST}" bash -s -- "${REMOTE_BASE}" "${N_PARALLEL}" <<'REMOTE_SCRIPT'
set -eo pipefail
REMOTE_BASE="$1"
N_PARALLEL="$2"

export MAMBA_EXE=/data/tian/software/micromamba/bin/micromamba
export MAMBA_ROOT_PREFIX=/data/tian/conda
# conda activate scripts and setup.sh reference env vars that may be unbound
# in a non-login shell; relax -u around them.
set +u
eval "$($MAMBA_EXE shell hook --shell bash --root-prefix $MAMBA_ROOT_PREFIX)"
micromamba activate anaroot-env
source /data/tian/workspace/dpol/smsimulator5.5/setup.sh >/dev/null
set -u

SIM=/data/tian/workspace/dpol/smsimulator5.5/bin/sim_deuteron

cd "${REMOTE_BASE}"
mkdir -p g4output logs
echo "[run_scan] launching N=${N_PARALLEL} parallel sim_deuteron jobs at $(date -Iseconds)"

# -I replaces MACFILE with each mac path; -P runs N_PARALLEL at a time.
# The bash -c sub-shell uses single-quoted body with SIM and REMOTE_BASE
# injected via shell parameter expansion in the outer (heredoc) scope.
ls macros/pxpy_scan_shard*.mac \
| xargs -P"${N_PARALLEL}" -I'MACFILE' bash -c "
    mac='MACFILE'
    name=\"\$(basename \"\$mac\" .mac)\"
    echo \"[\${name}] start \$(date -Iseconds)\"
    ${SIM} \"\$mac\" > ${REMOTE_BASE}/logs/\${name}.log 2>&1
    rc=\$?
    echo \"[\${name}] done rc=\${rc} \$(date -Iseconds)\"
"

echo "[run_scan] all shards complete at $(date -Iseconds)"
ls -1 g4output/pxpy_scan_shard*.root 2>/dev/null | wc -l | xargs echo "output files:"
REMOTE_SCRIPT
