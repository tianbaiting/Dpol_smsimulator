#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BASE_DIR="${BASE_DIR:-${REPO_DIR}/data/simulation/ips_scan/sn124_3deg_1.15T}"
INPUT_DIR="${INPUT_DIR:-${BASE_DIR}/input/representative}"
RESULT_DIR="${RESULT_DIR:-${BASE_DIR}/results/representative_beamOn300}"
GEOMETRY_DIR="${GEOMETRY_DIR:-${BASE_DIR}/geometry}"
GEOMETRY_WRL="${GEOMETRY_WRL:-${GEOMETRY_DIR}/ips_geometry_3deg_B115T_offset0.wrl}"
MANIFEST_PATH="${MANIFEST_PATH:-${BASE_DIR}/run_manifest.txt}"

GEN_BIN="${GEN_BIN:-${REPO_DIR}/build/bin/GenInputRoot_qmdrawdata}"
SCAN_BIN="${SCAN_BIN:-${REPO_DIR}/build/bin/scan_ips_position}"
SIM_BIN="${SIM_BIN:-${REPO_DIR}/build/bin/sim_deuteron}"
INPUT_BASE="${INPUT_BASE:-${REPO_DIR}/data/qmdrawdata/qmdrawdata/allevent}"
GEOM_MACRO="${GEOM_MACRO:-${REPO_DIR}/configs/simulation/geometry/3deg_1.15T.mac}"
EXPORT_SCRIPT="${EXPORT_SCRIPT:-${REPO_DIR}/scripts/analysis/export_ips_geometry_wrl.sh}"

mkdir -p "${INPUT_DIR}" "${RESULT_DIR}" "${GEOMETRY_DIR}"

printf "run_started=%s\n" "$(date '+%Y-%m-%d %H:%M:%S %Z')" > "${MANIFEST_PATH}"
printf "base_dir=%s\n" "${BASE_DIR}" >> "${MANIFEST_PATH}"
printf "geometry_macro=%s\n" "${GEOM_MACRO}" >> "${MANIFEST_PATH}"
printf "input_base=%s\n" "${INPUT_BASE}" >> "${MANIFEST_PATH}"
printf "input_dir=%s\n" "${INPUT_DIR}" >> "${MANIFEST_PATH}"
printf "result_dir=%s\n" "${RESULT_DIR}" >> "${MANIFEST_PATH}"
printf "progress_log=%s\n" "${RESULT_DIR}/scan_progress.log" >> "${MANIFEST_PATH}"
printf "geometry_wrl=%s\n" "${GEOMETRY_WRL}" >> "${MANIFEST_PATH}"
printf "scan_mode=representative_g050_ypol\n" >> "${MANIFEST_PATH}"
printf "scan_extra_args=%s\n" "--beam-on 300 --coarse-min-mm -200 --coarse-max-mm 200 --coarse-step-mm 20 --refine-half-window-mm 20 --refine-step-mm 5 --topk 3" >> "${MANIFEST_PATH}"
printf "summary_shape=%s\n" "all_candidates" >> "${MANIFEST_PATH}"

# [EN] Export one geometry snapshot next to the scan outputs so the IPS placement can be inspected with the same field and beam-angle setup. / [CN] 在扫描结果旁边导出一份几何快照，便于用同一套磁场和束流角配置检查IPS位置。
"${EXPORT_SCRIPT}" "${GEOMETRY_WRL}" >> "${MANIFEST_PATH}"

"${GEN_BIN}" \
    --mode ypol \
    --source both \
    --input-base "${INPUT_BASE}" \
    --output-base "${INPUT_DIR}" \
    --target-filter d+Sn124-ypolE190g050

"${SCAN_BIN}" \
    --sim-bin "${SIM_BIN}" \
    --geometry-macro "${GEOM_MACRO}" \
    --output-dir "${RESULT_DIR}" \
    --beam-on 300 \
    --coarse-min-mm -200 \
    --coarse-max-mm 200 \
    --coarse-step-mm 20 \
    --refine-half-window-mm 20 \
    --refine-step-mm 5 \
    --topk 3 \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ypn/dbreakb01.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ypn/dbreakb02.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ypn/dbreakb03.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ypn/dbreakb04.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ypn/dbreakb05.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ypn/dbreakb06.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ypn/dbreakb07.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ypn/dbreakb08.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ypn/dbreakb09.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ypn/dbreakb10.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ynp/dbreakb01.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ynp/dbreakb02.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ynp/dbreakb03.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ynp/dbreakb04.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ynp/dbreakb05.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ynp/dbreakb06.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ynp/dbreakb07.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ynp/dbreakb08.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ynp/dbreakb09.root" \
    --scan-elastic "${INPUT_DIR}/d+Sn124-ypolE190g050ynp/dbreakb10.root" \
    --scan-allevent "${INPUT_DIR}/d+Sn124-ypolE190g050ypn/geminioutb01.root" \
    --scan-allevent "${INPUT_DIR}/d+Sn124-ypolE190g050ypn/geminioutb02.root" \
    --scan-allevent "${INPUT_DIR}/d+Sn124-ypolE190g050ypn/geminioutb03.root" \
    --scan-allevent "${INPUT_DIR}/d+Sn124-ypolE190g050ypn/geminioutb04.root" \
    --scan-allevent "${INPUT_DIR}/d+Sn124-ypolE190g050ypn/geminioutb05.root" \
    --scan-allevent "${INPUT_DIR}/d+Sn124-ypolE190g050ypn/geminioutb06.root" \
    --scan-allevent "${INPUT_DIR}/d+Sn124-ypolE190g050ypn/geminioutb07.root" \
    --scan-allevent "${INPUT_DIR}/d+Sn124-ypolE190g050ynp/geminioutb01.root" \
    --scan-allevent "${INPUT_DIR}/d+Sn124-ypolE190g050ynp/geminioutb02.root" \
    --scan-allevent "${INPUT_DIR}/d+Sn124-ypolE190g050ynp/geminioutb03.root" \
    --scan-allevent "${INPUT_DIR}/d+Sn124-ypolE190g050ynp/geminioutb04.root" \
    --scan-allevent "${INPUT_DIR}/d+Sn124-ypolE190g050ynp/geminioutb05.root" \
    --scan-allevent "${INPUT_DIR}/d+Sn124-ypolE190g050ynp/geminioutb06.root" \
    --scan-allevent "${INPUT_DIR}/d+Sn124-ypolE190g050ynp/geminioutb07.root"

printf "run_finished=%s\n" "$(date '+%Y-%m-%d %H:%M:%S %Z')" >> "${MANIFEST_PATH}"
