#!/usr/bin/env bash
# [EN] Usage: bash scripts/analysis/legacy_target_reco/run_synthetic_reco_study.sh / [CN] 用法: bash scripts/analysis/legacy_target_reco/run_synthetic_reco_study.sh
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SMSIMDIR="${SMSIMDIR:-$(pwd)}"
EVENTS="${EVENTS:-20000}"
ANALYZE_EVENTS="${ANALYZE_EVENTS:-3000}"
PROTON_RECO_EVENTS="${PROTON_RECO_EVENTS:-600}"
SEED="${SEED:-20260224}"

OUT_DIR="${SMSIMDIR}/results/synthetic_reco/B115T_3deg"
LOG_DIR="${OUT_DIR}/logs"
MACRO_DIR="${OUT_DIR}/macros"
INPUT_DIR="${SMSIMDIR}/data/simulation/g4input/synthetic_scan"

DAT_FILE="${INPUT_DIR}/synthetic_px_scan.dat"
INPUT_ROOT="${INPUT_DIR}/synthetic_px_scan.root"
SIM_MACRO="${MACRO_DIR}/run_synthetic_px_scan_B115T_3deg.mac"
RUN_NAME="synthetic_px_scan_B115T_3deg"

FIELD_TABLE="${SMSIMDIR}/configs/simulation/geometry/filed_map/180703-1,15T-3000.table"
ANALYSIS_ROOT="${OUT_DIR}/synthetic_reco_analysis.root"
ANALYSIS_CSV="${OUT_DIR}/synthetic_reco_summary.csv"

mkdir -p "${OUT_DIR}" "${LOG_DIR}" "${MACRO_DIR}" "${INPUT_DIR}"

echo "[run_synthetic_reco_study] Generating synthetic .dat (${EVENTS} events)"
python3 "${SMSIMDIR}/scripts/event_generator/dpol_eventgen/generate_synthetic_px_scan.py" \
    --output "${DAT_FILE}" \
    --events "${EVENTS}" \
    --xmin -150 \
    --xmax 150 \
    --pz 627 \
    --seed "${SEED}" > "${LOG_DIR}/generate_dat.log" 2>&1

echo "[run_synthetic_reco_study] Converting .dat -> Geant4 input ROOT"
root -l -q "${SMSIMDIR}/scripts/event_generator/dpol_eventgen/GenInputRoot_np_atime.cc(\"${DAT_FILE}\", TVector3(0,0,0))" \
    > "${LOG_DIR}/gen_input_root.log" 2>&1 || true

if [[ ! -f "${INPUT_ROOT}" ]]; then
    echo "[run_synthetic_reco_study] ERROR: input ROOT not found: ${INPUT_ROOT}" >&2
    echo "[run_synthetic_reco_study] Check ${LOG_DIR}/gen_input_root.log" >&2
    exit 1
fi

cat > "${SIM_MACRO}" <<EOF_MAC
/samurai/geometry/Target/Position -1.2488 0.0009 -106.9458 cm
/samurai/geometry/Target/Angle 3.0 deg
/control/execute ${SMSIMDIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T.mac

/action/file/OverWrite y
/action/file/RunName ${RUN_NAME}
/action/file/SaveDirectory ${OUT_DIR}

/tracking/storeTrajectory 0

/action/gun/Type Tree
/action/gun/tree/InputFileName ${INPUT_ROOT}
/action/gun/tree/TreeName tree
/action/gun/tree/beamOn ${EVENTS}
EOF_MAC

rm -f "${OUT_DIR}/${RUN_NAME}"*.root

echo "[run_synthetic_reco_study] Running Geant4 simulation"
"${SMSIMDIR}/bin/sim_deuteron" "${SIM_MACRO}" > "${LOG_DIR}/sim_deuteron.log" 2>&1

SIM_ROOT=$(ls -1t "${OUT_DIR}/${RUN_NAME}"*.root 2>/dev/null | head -n 1)
if [[ -z "${SIM_ROOT}" ]]; then
    echo "[run_synthetic_reco_study] ERROR: Geant4 output ROOT not found" >&2
    echo "[run_synthetic_reco_study] Check ${LOG_DIR}/sim_deuteron.log" >&2
    exit 1
fi

echo "[run_synthetic_reco_study] Running reconstruction analysis (focus on ΔPx)"
root -l -q "${SCRIPT_DIR}/analyze_synthetic_px_reco.C+(\"${SIM_ROOT}\", \"${SIM_MACRO}\", \"${FIELD_TABLE}\", 30.0, \"${ANALYSIS_ROOT}\", \"${ANALYSIS_CSV}\", ${ANALYZE_EVENTS}, ${PROTON_RECO_EVENTS}, 10.0, 627.0)" \
    > "${LOG_DIR}/analyze_synthetic_reco.log" 2>&1 || true

if [[ ! -f "${ANALYSIS_CSV}" || ! -s "${ANALYSIS_CSV}" ]]; then
    # [EN] Fallback: ROOT may crash on shutdown in this environment, so rebuild CSV from metrics tree if needed. / [CN] 兜底：本环境ROOT可能在退出时崩溃，因此必要时从metrics树重建CSV。
    root -l -q "${SCRIPT_DIR}/extract_synthetic_reco_summary.C+(\"${ANALYSIS_ROOT}\",\"${ANALYSIS_CSV}\")" \
        > "${LOG_DIR}/extract_summary_from_root.log" 2>&1 || true
fi

if [[ ! -f "${ANALYSIS_CSV}" || ! -s "${ANALYSIS_CSV}" ]]; then
    echo "[run_synthetic_reco_study] ERROR: analysis CSV not generated" >&2
    echo "[run_synthetic_reco_study] Check ${LOG_DIR}/analyze_synthetic_reco.log" >&2
    exit 1
fi

echo "[run_synthetic_reco_study] Summary CSV:"
cat "${ANALYSIS_CSV}"
echo "[run_synthetic_reco_study] Plots: ${OUT_DIR}/plots"
echo "[run_synthetic_reco_study] Done"
