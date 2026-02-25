#!/usr/bin/env bash
# [EN] Usage: bash scripts/analysis/run_reco_step_scan.sh / [CN] 用法: bash scripts/analysis/run_reco_step_scan.sh
set -uo pipefail

SMSIMDIR="${SMSIMDIR:-$(pwd)}"
OUT_DIR="${SMSIMDIR}/results/reco_step_scan/B115T_3deg"
DAT_DIR="${OUT_DIR}/g4input_dat"
MACRO_DIR="${OUT_DIR}/macros"
LOG_DIR="${OUT_DIR}/logs"
RUN_NAME="reco_step_scan_B115T_3deg"
STEP_SIZES="${STEP_SIZES:-0.5,1,1.5,2,3,5,7.5,10}"

INPUT_DAT="${DAT_DIR}/protons_7momenta.dat"
INPUT_ROOT_NAME="protons_7momenta.root"
GEN_INPUT_ROOT="${SMSIMDIR}/data/simulation/g4input/$(basename "${DAT_DIR}")/${INPUT_ROOT_NAME}"
LOCAL_INPUT_ROOT="${OUT_DIR}/inputs/${INPUT_ROOT_NAME}"

SIM_MACRO="${MACRO_DIR}/${RUN_NAME}.mac"
FIELD_TABLE="${SMSIMDIR}/configs/simulation/geometry/filed_map/180703-1,15T-3000.table"
COMPARE_ROOT="${OUT_DIR}/reco_step_scan.root"
COMPARE_CSV="${OUT_DIR}/reco_step_scan.csv"

mkdir -p "${OUT_DIR}" "${DAT_DIR}" "${MACRO_DIR}" "${LOG_DIR}" "${OUT_DIR}/inputs"

cat > "${INPUT_DAT}" <<'DATA'
# no pxp pyp pzp pxn pyn pzn
# proton momenta scan, neutron momentum set to zero
1    0    0   627    0 0 0
2  150    0   627    0 0 0
3 -150    0   627    0 0 0
4  100    0   627    0 0 0
5 -100    0   627    0 0 0
6   50    0   627    0 0 0
7  -50    0   627    0 0 0
DATA

echo "[run_reco_step_scan] Generating Geant4 input ROOT from ${INPUT_DAT}"
root -l -q "${SMSIMDIR}/scripts/event_generator/dpol_eventgen/GenInputRoot_np_atime.cc(\"${INPUT_DAT}\", TVector3(0,0,0))" \
    > "${LOG_DIR}/gen_input.log" 2>&1 || true

if [[ ! -f "${GEN_INPUT_ROOT}" ]]; then
    echo "[run_reco_step_scan] ERROR: generated input ROOT not found: ${GEN_INPUT_ROOT}" >&2
    echo "[run_reco_step_scan] Check ${LOG_DIR}/gen_input.log" >&2
    exit 1
fi
cp -f "${GEN_INPUT_ROOT}" "${LOCAL_INPUT_ROOT}"

cat > "${SIM_MACRO}" <<EOF_MAC
/samurai/geometry/Target/Position -1.2488 0.0009 -106.9458 cm
/samurai/geometry/Target/Angle 3.0 deg

/control/execute ${SMSIMDIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T.mac

/action/file/OverWrite y
/action/file/RunName ${RUN_NAME}
/action/file/SaveDirectory ${OUT_DIR}

/tracking/storeTrajectory 0

/action/gun/Type Tree
/action/gun/tree/InputFileName ${GEN_INPUT_ROOT}
/action/gun/tree/TreeName tree
/action/gun/tree/beamOn 7
EOF_MAC

rm -f "${OUT_DIR}/${RUN_NAME}"*.root

echo "[run_reco_step_scan] Running sim_deuteron with ${SIM_MACRO}"
"${SMSIMDIR}/bin/sim_deuteron" "${SIM_MACRO}" > "${LOG_DIR}/sim_deuteron.log" 2>&1

SIM_ROOT=$(ls -1t "${OUT_DIR}/${RUN_NAME}"*.root 2>/dev/null | head -n 1)
if [[ -z "${SIM_ROOT}" ]]; then
    echo "[run_reco_step_scan] ERROR: simulation output ROOT not found in ${OUT_DIR}" >&2
    echo "[run_reco_step_scan] Check ${LOG_DIR}/sim_deuteron.log" >&2
    exit 1
fi

echo "[run_reco_step_scan] Comparing TMinuit vs ThreePoint across step sizes: ${STEP_SIZES}"
root -l -q "${SMSIMDIR}/scripts/analysis/compare_reco_step_sizes.C+(\"${SIM_ROOT}\", \"${SIM_MACRO}\", \"${FIELD_TABLE}\", 30.0, \"${STEP_SIZES}\", \"${COMPARE_ROOT}\", \"${COMPARE_CSV}\", 627.0, 5.0, 1000, 80, 0.05, 0.5, 5.0, -1)" \
    > "${LOG_DIR}/compare_reco_step_sizes.log" 2>&1 || true

if [[ ! -f "${COMPARE_CSV}" ]]; then
    echo "[run_reco_step_scan] ERROR: comparison CSV not generated: ${COMPARE_CSV}" >&2
    echo "[run_reco_step_scan] Check ${LOG_DIR}/compare_reco_step_sizes.log" >&2
    exit 1
fi

echo "[run_reco_step_scan] Result CSV: ${COMPARE_CSV}"
python3 - "${COMPARE_CSV}" <<'PY'
import csv
import math
import sys

path = sys.argv[1]
rows = []
with open(path, "r", newline="") as f:
    reader = csv.DictReader(f)
    for row in reader:
        row["step_size"] = float(row["step_size"])
        row["mean_d_pdc"] = float(row["mean_d_pdc"])
        row["mean_d_target"] = float(row["mean_d_target"])
        row["mean_runtime_ms"] = float(row.get("mean_runtime_ms", 0.0))
        row["success_rate"] = float(row["success_rate"])
        row["count"] = int(row["count"])
        rows.append(row)

for method in ("minuit", "threepoint"):
    cand = [r for r in rows if r["method"] == method and r["count"] > 0]
    if not cand:
        print(f"[summary] {method}: no valid rows")
        continue
    best = min(cand, key=lambda r: (r["mean_d_pdc"], r["mean_d_target"], r["mean_runtime_ms"], -r["success_rate"]))
    print(
        "[summary] {method}: best_step={step:.3f} mm, mean_d_pdc={dpdc:.4f} mm, "
        "mean_d_target={dtgt:.4f} mm, runtime={rt:.1f} ms, success={succ:.1%}, count={count}".format(
            method=method,
            step=best["step_size"],
            dpdc=best["mean_d_pdc"],
            dtgt=best["mean_d_target"],
            rt=best["mean_runtime_ms"],
            succ=best["success_rate"],
            count=best["count"],
        )
    )
PY

echo "[run_reco_step_scan] Done"
