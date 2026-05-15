#!/usr/bin/env bash
# tests/integration/test_sim_deuteron_with_nebula_plus.sh
# [EN] Runs sim_deuteron with the NEBULA-Plus smoke macro and asserts that
#      the output tree has the NEBULA-Plus branches and parameter objects.
# [CN] 用 NEBULA-Plus 烟雾宏运行 sim_deuteron，验证输出 ROOT 文件包含
#      NEBULA-Plus 分支和参数对象。
set -euo pipefail

# Resolve project root from this script's location
SMSIMDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
export SMSIMDIR

SIM_BIN="${SMSIMDIR}/build/bin/sim_deuteron"
SMOKE_MAC="${SMSIMDIR}/configs/simulation/macros/test_nebula_plus.mac"
LOG_FILE="/tmp/sim_nebula_plus_smoke_$$.log"
FAIL=0

echo "=== NEBULA-Plus smoke integration test ==="
echo "  SMSIMDIR:  ${SMSIMDIR}"
echo "  sim_bin:   ${SIM_BIN}"
echo "  macro:     ${SMOKE_MAC}"

# ── Step 0: prerequisite checks ─────────────────────────────────────────────
if [ ! -x "${SIM_BIN}" ]; then
    echo "FAIL: sim_deuteron binary not found or not executable: ${SIM_BIN}"
    exit 1
fi
if [ ! -f "${SMOKE_MAC}" ]; then
    echo "FAIL: smoke macro not found: ${SMOKE_MAC}"
    exit 1
fi

# ── Step 1: clean stale output ───────────────────────────────────────────────
# [EN] Remove any leftover ROOT files from a previous run so we can be sure
#      what we find below was produced by this invocation.
rm -f "${SMSIMDIR}/data/simulation/output_tree/nebula_plus_smoke"*.root

# ── Step 2: run the simulation ──────────────────────────────────────────────
echo ""
echo "--- Running sim_deuteron (1 event, NEBULA + NEBULA-Plus geometry) ---"
if ! "${SIM_BIN}" "${SMOKE_MAC}" > "${LOG_FILE}" 2>&1; then
    echo "FAIL: sim_deuteron exited with non-zero status"
    echo "--- last 30 lines of sim log ---"
    tail -30 "${LOG_FILE}"
    exit 1
fi

# ── Step 3: locate output ROOT file ─────────────────────────────────────────
ROOT_FILE=$(ls "${SMSIMDIR}/data/simulation/output_tree/nebula_plus_smoke"*.root 2>/dev/null | head -1)
if [ -z "${ROOT_FILE}" ]; then
    echo "FAIL: no ROOT output produced under data/simulation/output_tree/nebula_plus_smoke*.root"
    echo "--- last 30 lines of sim log ---"
    tail -30 "${LOG_FILE}"
    exit 1
fi
echo "  Output file: ${ROOT_FILE}"

# ── Step 4: introspect the ROOT file ────────────────────────────────────────
# [EN] Use root -b to enumerate TTree branches and top-level object keys.
#      We rely on micromamba running the anaroot-env root, which already has
#      the SM libraries loaded via rootlogon.C when SMSIMDIR is set.

ROOT_INSPECT_OUTPUT=$(root -b -l -q \
    "${ROOT_FILE}" \
    -e "
        // Print branches in the main tree
        auto* t = (TTree*)gFile->Get(\"tree\");
        if (t) {
            for (auto b : *t->GetListOfBranches())
                std::cout << \"BRANCH:\" << b->GetName() << std::endl;
        }
        // Print top-level keys
        for (auto k : *gFile->GetListOfKeys())
            std::cout << \"KEY:\" << ((TKey*)k)->GetClassName()
                      << \":\" << ((TKey*)k)->GetName() << std::endl;
    " 2>/dev/null)

echo ""
echo "--- ROOT introspection results ---"
echo "${ROOT_INSPECT_OUTPUT}" | grep -E "^BRANCH:|^KEY:" | head -40 || true

# ── Step 5: assert expected items are present ────────────────────────────────

check_branch() {
    local name="$1"
    if echo "${ROOT_INSPECT_OUTPUT}" | grep -q "^BRANCH:${name}$"; then
        echo "  OK  branch: ${name}"
    else
        echo "  FAIL branch missing: ${name}"
        FAIL=1
    fi
}

check_key_class() {
    local classname="$1"
    if echo "${ROOT_INSPECT_OUTPUT}" | grep -q "^KEY:${classname}:"; then
        echo "  OK  top-level key of class: ${classname}"
    else
        echo "  FAIL top-level key missing for class: ${classname}"
        FAIL=1
    fi
}

echo ""
echo "--- Assertions ---"
# TTree branches
check_branch "NEBULAPla"
check_branch "NEBULAPlusPla"
# Top-level parameter objects
check_key_class "TNEBULASimParameter"
check_key_class "TNEBULAPlusSimParameter"

# ── Final verdict ────────────────────────────────────────────────────────────
echo ""
if [ "${FAIL}" -ne 0 ]; then
    echo "FAIL: one or more assertions failed (see above)"
    echo "--- last 20 lines of sim log ---"
    tail -20 "${LOG_FILE}"
    rm -f "${LOG_FILE}"
    exit 1
fi

echo "PASS: NEBULA + NEBULA-Plus branches and parameter objects present in output"
rm -f "${LOG_FILE}"
exit 0
