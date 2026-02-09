#!/usr/bin/env bash
set -euo pipefail

SMSIMDIR="${SMSIMDIR:-$(pwd)}"

OUT_DIR="${SMSIMDIR}/results/step_size_scan/B115T_3deg"
INPUT_DIR="${OUT_DIR}/inputs"
MACRO_DIR="${OUT_DIR}/macros"

mkdir -p "${OUT_DIR}" "${INPUT_DIR}" "${MACRO_DIR}"

INPUT_DAT="${INPUT_DIR}/pn_4tracks.dat"
INPUT_ROOT="${SMSIMDIR}/d_work/rootfiles/inputs/pn_4tracks.root"
SIM_MACRO="${MACRO_DIR}/run_step_scan_B115T_3deg.mac"

# [EN] Prepare 4 proton tracks with fixed pz and symmetric px. / [CN] 准备4条质子轨迹，固定pz并设置对称px。
cat > "${INPUT_DAT}" <<'DATA'
# no pxp pyp pzp pxn pyn pzn
1  100 0 627  0 0 0
2 -100 0 627  0 0 0
3   50 0 627  0 0 0
4  -50 0 627  0 0 0
DATA

# [EN] Generate ROOT input using the standard generator macro. / [CN] 使用标准生成器宏生成ROOT输入。
root -l -q "${SMSIMDIR}/scripts/event_generator/dpol_eventgen/GenInputRoot_np_atime.cc(\"${INPUT_DAT}\", TVector3(0,0,0))"

# [EN] Keep a local copy for provenance. / [CN] 保留本地副本便于追溯。
cp -f "${INPUT_ROOT}" "${INPUT_DIR}/pn_4tracks.root"

# [EN] Build a batch macro to avoid interactive visualization. / [CN] 构建批处理宏以避免交互可视化。
cat > "${SIM_MACRO}" <<EOF_MAC
/samurai/geometry/Target/Position -1.2488 0.0009 -106.9458 cm
/samurai/geometry/Target/Angle 3.0 deg
/control/execute ${SMSIMDIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T.mac

/action/file/OverWrite y
/action/file/RunName step_scan
/action/file/SaveDirectory ${OUT_DIR}

/tracking/storeTrajectory 0

/action/gun/Type Tree
/action/gun/tree/InputFileName ${INPUT_ROOT}
/action/gun/tree/TreeName tree
/action/gun/tree/beamOn 4

/run/beamOn 4
EOF_MAC

# [EN] Run Geant4 simulation to produce PDC hit data. / [CN] 运行Geant4模拟以生成PDC击中数据。
"${SMSIMDIR}/bin/sim_deuteron" "${SIM_MACRO}"

SIM_ROOT=$(ls -1 "${OUT_DIR}"/step_scan*.root | head -n 1)
if [[ -z "${SIM_ROOT}" ]]; then
  echo "No sim output root file found in ${OUT_DIR}" >&2
  exit 1
fi

FIELD_TABLE="${SMSIMDIR}/configs/simulation/geometry/filed_map/180703-1,15T-3000.table"
GEOM_MACRO="${SIM_MACRO}"
OUT_ROOT="${OUT_DIR}/step_size_compare.root"
OUT_CSV="${OUT_DIR}/step_size_compare.csv"

# [EN] Compare ParticleTrajectory results across step sizes. / [CN] 比较不同步长下的ParticleTrajectory结果。
root -l -q "${SMSIMDIR}/scripts/analysis/compare_step_sizes.C+(\"${SIM_ROOT}\", \"${FIELD_TABLE}\", 30.0, \"${GEOM_MACRO}\", \"${OUT_ROOT}\", \"1,2,5,10,20\", \"${OUT_CSV}\", -1)"

echo "Done. Results in ${OUT_DIR}"
