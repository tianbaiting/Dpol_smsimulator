#!/usr/bin/env bash
# [EN] Push 20260413ypol Sn112/Sn124 dbreak.dat (only) from local to spana03,
# preserving the source tree so GenInputRoot's symlink farm in
# `prepare` stage can point into it.
# [CN] 把本地 20260413ypol/Sn112,Sn124 的 dbreak.dat 推到 spana03,保留原结构,
# 只同步 dbreak.dat (不要 geminiout.dat / reactiontype.dat)。
#
# Run on local. After this, ssh into spana03 and invoke
# `python3 scripts/batch/run_dbreak_elastic_pipeline.py prepare`.

set -euo pipefail

LOCAL_DIR="${LOCAL_DIR:-/home/tian/workspace/dpol/smsimulator5.5}"
REMOTE_HOST="${REMOTE_HOST:-spana03}"
REMOTE_DIR="${REMOTE_DIR:-/home/tbt/workspace/Dpol_smsimulator}"
SOURCE_REL="data/qmdrawdata/qmdrawdata/20260413ypol"

src="${LOCAL_DIR}/${SOURCE_REL}/"
dst="${REMOTE_HOST}:${REMOTE_DIR}/${SOURCE_REL}/"

DRY_RUN=""
[[ "${1:-}" == "--dry-run" ]] && DRY_RUN="--dry-run"

echo "[INFO] rsync ${src}  →  ${dst}"
rsync -avz --info=progress2 \
    --include='*/' --include='dbreak.dat' --exclude='*' \
    --prune-empty-dirs \
    ${DRY_RUN} \
    "${src}" "${dst}"

echo "[OK] sync done. Next: ssh ${REMOTE_HOST} and run the pipeline driver."
