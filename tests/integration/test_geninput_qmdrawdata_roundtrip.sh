#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
GEN_BIN="${REPO_DIR}/build/bin/GenInputRoot_qmdrawdata"

if [[ ! -x "$GEN_BIN" ]]; then
    echo "[ERROR] $GEN_BIN not built — should have been gated at CMake-configure time" >&2
    exit 1
fi

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

# [EN] Build a synthetic 9-column y_pol input with known rpphi values.
mkdir -p "$WORK_DIR/in/y_pol/phi_random/d+Sn124E190g050ynp"
cat > "$WORK_DIR/in/y_pol/phi_random/d+Sn124E190g050ynp/dbreak.dat" <<'EOF'
ynp d+124Sn   190.00MeV/u b= 0.000fm   3events
  No.  pxp(MeV/c)  pyp(MeV/c)  pzp(MeV/c)  pxn(MeV/c)  pyn(MeV/c)  pzn(MeV/c)           b       rpphi
       1    100.0    0.0    600.0    -100.0    0.0    600.0   5.0      0.0
       2    100.0    0.0    600.0    -100.0    0.0    600.0   5.0     90.0
       3    100.0    0.0    600.0    -100.0    0.0    600.0   5.0    180.0
EOF

# [EN] Build a synthetic 7-column z_pol input.
mkdir -p "$WORK_DIR/in/z_pol/b_discrete/d+Sn124E190g050znp"
cat > "$WORK_DIR/in/z_pol/b_discrete/d+Sn124E190g050znp/dbreakb05.dat" <<'EOF'
znp d+124Sn   190.00MeV/u b= 5.000fm  3events
  No.  pxp(MeV/c)  pyp(MeV/c)  pzp(MeV/c)  pxn(MeV/c)  pyn(MeV/c)  pzn(MeV/c)
       1    100.0    0.0    600.0    -100.0    0.0    600.0
       2    100.0    0.0    600.0    -100.0    0.0    600.0
       3    100.0    0.0    600.0    -100.0    0.0    600.0
EOF

mkdir -p "$WORK_DIR/out"

# [EN] y_pol: rotation OFF. b_phi = rpphi (mod 2π).
"$GEN_BIN" --mode ypol --source elastic \
    --input-base "$WORK_DIR/in" --output-base "$WORK_DIR/out" \
    --randomize-ypol off

# [EN] z_pol: rotation ON, fixed seed. b_phi ∈ [0, 2π).
"$GEN_BIN" --mode zpol --source elastic \
    --input-base "$WORK_DIR/in" --output-base "$WORK_DIR/out" \
    --randomize-zpol on --rotation-seed 42

# [EN] Clean any stale ACLiC artifacts from previous runs.
rm -f "$REPO_DIR/tests/integration/test_geninput_qmdrawdata_roundtrip_C"*.so \
      "$REPO_DIR/tests/integration/test_geninput_qmdrawdata_roundtrip_C"*.d \
      "$REPO_DIR/tests/integration/test_geninput_qmdrawdata_roundtrip_C"*.pcm \
      "$REPO_DIR/tests/integration/test_geninput_qmdrawdata_roundtrip_C_ACLiC"* 2>/dev/null || true

# [EN] Hand off to the ROOT macro for assertions. Add smg4lib/include to ACLiC's
# include path so QMDInputMetadata.hh resolves.
root -b -l -q \
    -e "gSystem->AddIncludePath(\" -I$REPO_DIR/libs/smg4lib/include \")" \
    -e ".L $REPO_DIR/tests/integration/test_geninput_qmdrawdata_roundtrip.C+" \
    -e "test_geninput_qmdrawdata_roundtrip(\"$WORK_DIR\")"
