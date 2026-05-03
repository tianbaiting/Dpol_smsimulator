#!/bin/bash
# Mirror spana03 ypol+zpol breakup g4output to local.
# Idempotent: rsync skips unchanged files.
set -euo pipefail

SMSIM_DIR=${SMSIM_DIR:-/home/tian/workspace/dpol/smsimulator5.5}
REMOTE=${REMOTE:-spana03}
REMOTE_BASE=${REMOTE_BASE:-/home/tbt/workspace/Dpol_smsimulator/data/simulation/g4output}
LOCAL_BASE=${LOCAL_BASE:-${SMSIM_DIR}/data/simulation/g4output}

mkdir -p "${LOCAL_BASE}/y_pol" "${LOCAL_BASE}/z_pol"

echo "[rsync] ypol  ${REMOTE}:${REMOTE_BASE}/y_pol/  ->  ${LOCAL_BASE}/y_pol/"
rsync -avh --info=progress2 \
    "${REMOTE}:${REMOTE_BASE}/y_pol/" \
    "${LOCAL_BASE}/y_pol/"

echo "[rsync] zpol  ${REMOTE}:${REMOTE_BASE}/z_pol/  ->  ${LOCAL_BASE}/z_pol/"
rsync -avh --info=progress2 \
    "${REMOTE}:${REMOTE_BASE}/z_pol/" \
    "${LOCAL_BASE}/z_pol/"

ypol_n=$(find "${LOCAL_BASE}/y_pol/phi_random" -name 'dbreak*.root' 2>/dev/null | wc -l)
zpol_n=$(find "${LOCAL_BASE}/z_pol/b_discrete" -name 'dbreakb*.root' 2>/dev/null | wc -l)
ypol_size=$(du -sh "${LOCAL_BASE}/y_pol" 2>/dev/null | awk '{print $1}')
zpol_size=$(du -sh "${LOCAL_BASE}/z_pol" 2>/dev/null | awk '{print $1}')

echo "[rsync] done"
echo "[rsync] ypol files: ${ypol_n}  size: ${ypol_size}  (expected 16 files / ~7.1 GB)"
echo "[rsync] zpol files: ${zpol_n}  size: ${zpol_size}  (expected 160 files / ~269 MB)"

if [[ "${ypol_n}" -ne 16 ]]; then echo "[rsync] WARN: ypol file count mismatch"; fi
if [[ "${zpol_n}" -ne 160 ]]; then echo "[rsync] WARN: zpol file count mismatch"; fi
