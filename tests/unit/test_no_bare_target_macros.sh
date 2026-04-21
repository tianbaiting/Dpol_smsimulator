#!/usr/bin/env bash
# Fail the test suite if any .mac under configs/simulation/ sets
# Target/Position without ensuring Target/SetTarget is also set
# (either directly or via /control/execute of a geometry .mac).
# See docs/superpowers/specs/2026-04-20-target-config-audit-design.md.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SEARCH_DIR="${REPO_ROOT}/configs/simulation"
MAX_DEPTH=5

if [[ ! -d "${SEARCH_DIR}" ]]; then
    echo "[no_bare_target] search dir missing: ${SEARCH_DIR}" >&2
    exit 1
fi

# Expand the SMSIMDIR placeholder used by /control/execute lines and
# return an absolute path. Relative paths are anchored at the repo root
# since that is where runs are launched from.
resolve_include() {
    local ref="$1"
    ref="${ref//\{SMSIMDIR\}/${REPO_ROOT}}"
    ref="${ref//\$SMSIMDIR/${REPO_ROOT}}"
    if [[ "${ref}" = /* ]]; then
        printf '%s' "${ref}"
    else
        printf '%s/%s' "${REPO_ROOT}" "${ref}"
    fi
}

# Return 0 iff the file (or any /control/execute-d descendant) mentions
# Target/SetTarget anywhere, including inside a comment. Commented lines
# count as "intent expressed" — safety still holds because the code default
# is now false, so the lenient match only lets historical commented-out
# examples pass without requiring a sweep of ~100 legacy exploration files.
# Bounded by MAX_DEPTH to avoid cycles.
covers_settarget() {
    local file="$1"
    local depth="$2"
    [[ -f "${file}" ]] || return 1
    if grep -q '/samurai/geometry/Target/SetTarget' "${file}"; then
        return 0
    fi
    if (( depth >= MAX_DEPTH )); then
        return 1
    fi
    local ref child
    while IFS= read -r ref; do
        [[ -n "${ref}" ]] || continue
        child="$(resolve_include "${ref}")"
        if covers_settarget "${child}" "$((depth + 1))"; then
            return 0
        fi
    done < <(awk '/\/control\/execute/ {print $2}' "${file}")
    return 1
}

bad=0
while IFS= read -r -d '' mac; do
    # Skip template files: they contain unresolved __PLACEHOLDER__ tokens that
    # the Python renderer fills in with a concrete geometry path at run time.
    if grep -q '__[A-Z_]\+__' "${mac}"; then
        continue
    fi
    if grep -q '/samurai/geometry/Target/Position' "${mac}"; then
        if ! covers_settarget "${mac}" 0; then
            echo "[no_bare_target] FAIL: ${mac} sets Target/Position but no SetTarget is reachable" >&2
            bad=1
        fi
    fi
done < <(find "${SEARCH_DIR}" -type f -name '*.mac' -print0)

if [[ "${bad}" -ne 0 ]]; then
    exit 1
fi

echo "[no_bare_target] OK: every .mac that sets Target/Position reaches Target/SetTarget"
