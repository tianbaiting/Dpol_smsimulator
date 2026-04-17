#!/bin/bash
# [EN] Integration test: run analyze_pdc_rk_error on E2E reco files and validate output.
# [CN] 集成测试：在E2E重建文件上运行误差分析工具并验证输出。
#
# This test is designed to be run AFTER the E2E reconstruction tests have completed
# and produced reco ROOT files in build/test_output/pdc_target_momentum_scan/.
#
# Usage: test_pdc_error_analysis_e2e.sh <analyze_bin> <geometry_macro> <field_map> <reco_root> <output_dir>
#
# Exit codes:
#   0 = all checks passed
#   1 = tool execution failed
#   2 = output file missing
#   3 = CSV validation failed

set -euo pipefail

ANALYZE_BIN="${1:?Usage: $0 <analyze_bin> <geometry_macro> <field_map> <reco_root> <output_dir>}"
GEOMETRY_MACRO="${2:?}"
FIELD_MAP="${3:?}"
RECO_ROOT="${4:?}"
OUTPUT_DIR="${5:?}"

echo "=== PDC Error Analysis E2E Test ==="
echo "  analyze_bin:    ${ANALYZE_BIN}"
echo "  geometry_macro: ${GEOMETRY_MACRO}"
echo "  field_map:      ${FIELD_MAP}"
echo "  reco_root:      ${RECO_ROOT}"
echo "  output_dir:     ${OUTPUT_DIR}"

# ── Step 1: Check prerequisites ─────────────────────────────────────
if [ ! -x "${ANALYZE_BIN}" ]; then
    echo "FAIL: analyze binary not found or not executable: ${ANALYZE_BIN}"
    exit 1
fi
if [ ! -f "${RECO_ROOT}" ]; then
    echo "FAIL: reco ROOT file not found: ${RECO_ROOT}"
    exit 1
fi

mkdir -p "${OUTPUT_DIR}"

# ── Step 2: Run error analysis tool ─────────────────────────────────
echo ""
echo "--- Running analyze_pdc_rk_error ---"
"${ANALYZE_BIN}" \
    --input-file "${RECO_ROOT}" \
    --output-dir "${OUTPUT_DIR}" \
    --geometry-macro "${GEOMETRY_MACRO}" \
    --magnetic-field-map "${FIELD_MAP}" \
    --magnet-rotation-deg 30 \
    --rk-fit-mode fixed-target-pdc-only \
    --max-events-per-file 10 \
    --profile-points 17 \
    --profile-per-quartile 24 \
    --mcmc-per-quartile 8 \
    --mcmc-n-samples 160 \
    --mcmc-burn-in 80 \
    --mcmc-thin 2

if [ $? -ne 0 ]; then
    echo "FAIL: analyze_pdc_rk_error exited with error"
    exit 1
fi

# ── Step 3: Check output files exist ────────────────────────────────
EXPECTED_FILES=(
    "track_summary.csv"
    "summary.csv"
    "validation_summary.csv"
    "profile_samples.csv"
    "bayesian_samples.csv"
)

echo ""
echo "--- Checking output files ---"
MISSING=0
for f in "${EXPECTED_FILES[@]}"; do
    if [ ! -f "${OUTPUT_DIR}/${f}" ]; then
        echo "FAIL: missing output file: ${f}"
        MISSING=1
    else
        LINES=$(wc -l < "${OUTPUT_DIR}/${f}")
        echo "  OK: ${f} (${LINES} lines)"
    fi
done
if [ ${MISSING} -ne 0 ]; then
    exit 2
fi

# ── Step 4: Validate CSV content ────────────────────────────────────
echo ""
echo "--- Validating CSV content ---"
ERRORS=0

# Helper: check a numeric field is positive and finite
check_positive() {
    local file="$1" field="$2" label="$3"
    local val
    val=$(awk -F, -v f="${field}" 'NR==1{for(i=1;i<=NF;i++)if($i==f)col=i} NR>1&&col{print $col}' "${file}" | head -1)
    if [ -z "${val}" ]; then
        echo "FAIL: ${label}: field '${field}' not found in ${file}"
        ERRORS=$((ERRORS+1))
        return
    fi
    # Check finite and positive (awk handles nan/inf)
    local ok
    ok=$(echo "${val}" | awk '{if($1+0>0 && $1+0<1e15)print 1; else print 0}')
    if [ "${ok}" != "1" ]; then
        echo "FAIL: ${label}: ${field}=${val} (expected positive finite)"
        ERRORS=$((ERRORS+1))
    else
        echo "  OK: ${label}: ${field}=${val}"
    fi
}

# 4a: track_summary.csv — Fisher sigmas should be positive
TRACK_CSV="${OUTPUT_DIR}/track_summary.csv"
N_TRACKS=$(awk 'END{print NR-1}' "${TRACK_CSV}")
echo "  Tracks analyzed: ${N_TRACKS}"
if [ "${N_TRACKS}" -lt 1 ]; then
    echo "FAIL: no tracks in track_summary.csv"
    ERRORS=$((ERRORS+1))
fi

check_positive "${TRACK_CSV}" "fisher_px_sigma" "track_summary Fisher px"
check_positive "${TRACK_CSV}" "fisher_py_sigma" "track_summary Fisher py"
check_positive "${TRACK_CSV}" "fisher_pz_sigma" "track_summary Fisher pz"
check_positive "${TRACK_CSV}" "fisher_p_sigma" "track_summary Fisher |p|"

# 4b: summary.csv — metric,value format (two columns: metric name, value)
SUMMARY_CSV="${OUTPUT_DIR}/summary.csv"
check_metric() {
    local file="$1" metric="$2" label="$3"
    local val
    val=$(awk -F, -v m="${metric}" '$1==m{print $2}' "${file}")
    if [ -z "${val}" ]; then
        echo "FAIL: ${label}: metric '${metric}' not found in ${file}"
        ERRORS=$((ERRORS+1))
        return
    fi
    local ok
    ok=$(echo "${val}" | awk '{if($1+0>0 && $1+0<1e15)print 1; else print 0}')
    if [ "${ok}" != "1" ]; then
        echo "FAIL: ${label}: ${metric}=${val} (expected positive finite)"
        ERRORS=$((ERRORS+1))
    else
        echo "  OK: ${label}: ${metric}=${val}"
    fi
}
for metric in median_fisher_p_width68 profile_sample_count mcmc_sample_count; do
    check_metric "${SUMMARY_CSV}" "${metric}" "summary ${metric}"
done

# 4c: validation_summary.csv — coverage should be numeric
VAL_CSV="${OUTPUT_DIR}/validation_summary.csv"
N_METHODS=$(awk 'END{print NR-1}' "${VAL_CSV}")
if [ "${N_METHODS}" -lt 2 ]; then
    echo "FAIL: validation_summary has fewer than 2 methods"
    ERRORS=$((ERRORS+1))
else
    echo "  OK: validation_summary has ${N_METHODS} methods"
fi

# 4d: profile_samples.csv — intervals should have lower < upper
PROFILE_CSV="${OUTPUT_DIR}/profile_samples.csv"
N_PROFILE=$(awk 'END{print NR-1}' "${PROFILE_CSV}")
if [ "${N_PROFILE}" -lt 1 ]; then
    echo "FAIL: no entries in profile_samples.csv"
    ERRORS=$((ERRORS+1))
else
    # Check that profile_p_lower68 < profile_p_upper68 for first row
    LOWER=$(awk -F, 'NR==1{for(i=1;i<=NF;i++)if($i=="profile_p_lower68")col=i} NR==2&&col{print $col}' "${PROFILE_CSV}")
    UPPER=$(awk -F, 'NR==1{for(i=1;i<=NF;i++)if($i=="profile_p_upper68")col=i} NR==2&&col{print $col}' "${PROFILE_CSV}")
    INTERVAL_OK=$(echo "${LOWER} ${UPPER}" | awk '{if($1+0<$2+0)print 1; else print 0}')
    if [ "${INTERVAL_OK}" != "1" ]; then
        echo "FAIL: profile_p interval inverted: lower=${LOWER} upper=${UPPER}"
        ERRORS=$((ERRORS+1))
    else
        echo "  OK: profile |p| 68% interval: [${LOWER}, ${UPPER}]"
    fi
fi

# 4e: bayesian_samples.csv — acceptance rate in (0,1)
BAYES_CSV="${OUTPUT_DIR}/bayesian_samples.csv"
ACC=$(awk -F, 'NR==1{for(i=1;i<=NF;i++)if($i=="acceptance_rate")col=i} NR==2&&col{print $col}' "${BAYES_CSV}")
ACC_OK=$(echo "${ACC}" | awk '{if($1+0>0 && $1+0<1)print 1; else print 0}')
if [ "${ACC_OK}" != "1" ]; then
    echo "FAIL: MCMC acceptance_rate=${ACC} (expected 0<x<1)"
    ERRORS=$((ERRORS+1))
else
    echo "  OK: MCMC acceptance_rate=${ACC}"
fi

# ── Step 5: Print summary table from actual data ────────────────────
echo ""
echo "=== Error Analysis Summary (from actual test output) ==="
echo ""
echo "--- Per-track Fisher information sigmas (MeV/c) ---"
awk -F, 'NR==1{for(i=1;i<=NF;i++){h[i]=$i}} NR>1{
    printf "  event=%s track=%s: σ_px=%.2f σ_py=%.2f σ_pz=%.2f σ_|p|=%.2f  chi2_red=%.3f\n",
        $2,$3,$16,$17,$18,$19,$13
}' "${TRACK_CSV}"

echo ""
echo "--- Per-track truth comparison (MeV/c) ---"
awk -F, 'NR==1{for(i=1;i<=NF;i++){h[i]=$i}} NR>1{
    dpx=$8-$5; dpy=$9-$6; dpz=$10-$7;
    printf "  event=%s: truth=(%.1f,%.1f,%.1f) reco=(%.1f,%.1f,%.1f) Δ=(%.1f,%.1f,%.1f)\n",
        $2,$5,$6,$7,$8,$9,$10,dpx,dpy,dpz
}' "${TRACK_CSV}"

echo ""
echo "--- Aggregated metrics ---"
awk -F, 'NR>1{printf "  %s = %s\n", $1, $2}' "${SUMMARY_CSV}"

echo ""
echo "--- Coverage validation ---"
awk -F, 'NR==1{printf "  %-15s %5s %7s %7s\n", $1,$2,$3,$4} NR>1{printf "  %-15s %5s %7s %7s\n", $1,$2,$3,$4}' "${VAL_CSV}"

# ── Final verdict ───────────────────────────────────────────────────
echo ""
if [ ${ERRORS} -ne 0 ]; then
    echo "FAIL: ${ERRORS} validation error(s)"
    exit 3
fi
echo "PASS: all error analysis checks passed"
exit 0
