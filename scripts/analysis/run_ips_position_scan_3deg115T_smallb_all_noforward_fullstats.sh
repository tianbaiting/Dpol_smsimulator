#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BASE_DIR="${BASE_DIR:-${REPO_DIR}/data/simulation/ips_scan/sn124_3deg_1.15T}"
SOURCE_INPUT_DIR="${SOURCE_INPUT_DIR:-${BASE_DIR}/input/representative}"
BALANCED_INPUT_DIR="${BALANCED_INPUT_DIR:-${BASE_DIR}/input/balanced}"
RESULT_DIR="${RESULT_DIR:-${BASE_DIR}/results/smallb_all_noForward_fullstats}"
MANIFEST_PATH="${MANIFEST_PATH:-${RESULT_DIR}/scan_manifest.txt}"

SCAN_BIN="${SCAN_BIN:-${REPO_DIR}/build/bin/scan_ips_position}"
SIM_BIN="${SIM_BIN:-${REPO_DIR}/build/bin/sim_deuteron}"
GEOM_MACRO="${GEOM_MACRO:-${REPO_DIR}/configs/simulation/geometry/3deg_1.15T.mac}"
HADD_BIN="${HADD_BIN:-$(command -v hadd)}"

mkdir -p "${BALANCED_INPUT_DIR}" "${RESULT_DIR}"

for b in 01 02 03 04 05 06 07 08 09 10; do
    "${HADD_BIN}" -f "${BALANCED_INPUT_DIR}/elastic_b${b}.root" \
        "${SOURCE_INPUT_DIR}/d+Sn124-ypolE190g050ypn/dbreakb${b}.root" \
        "${SOURCE_INPUT_DIR}/d+Sn124-ypolE190g050ynp/dbreakb${b}.root" >/dev/null
done

for b in 01 02 03 04 05 06 07; do
    "${HADD_BIN}" -f "${BALANCED_INPUT_DIR}/allevent_b${b}.root" \
        "${SOURCE_INPUT_DIR}/d+Sn124-ypolE190g050ypn/geminioutb${b}.root" \
        "${SOURCE_INPUT_DIR}/d+Sn124-ypolE190g050ynp/geminioutb${b}.root" >/dev/null
done

printf "run_started=%s\n" "$(date '+%Y-%m-%d %H:%M:%S %Z')" > "${MANIFEST_PATH}"
printf "geometry_macro=%s\n" "${GEOM_MACRO}" >> "${MANIFEST_PATH}"
printf "source_input_dir=%s\n" "${SOURCE_INPUT_DIR}" >> "${MANIFEST_PATH}"
printf "balanced_input_dir=%s\n" "${BALANCED_INPUT_DIR}" >> "${MANIFEST_PATH}"
printf "result_dir=%s\n" "${RESULT_DIR}" >> "${MANIFEST_PATH}"
printf "progress_log=%s\n" "${RESULT_DIR}/scan_progress.log" >> "${MANIFEST_PATH}"
printf "selection_mode=%s\n" "all_events" >> "${MANIFEST_PATH}"
printf "small_b_definition=%s\n" "bimp<=7 from all-event inputs" >> "${MANIFEST_PATH}"
printf "elastic_background=%s\n" "cleaned dbreak inputs without forward requirement" >> "${MANIFEST_PATH}"
printf "coarse_grid=%s\n" "-200:200:40 mm" >> "${MANIFEST_PATH}"
printf "refine_grid=%s\n" "top2 +-20 mm step 5 mm" >> "${MANIFEST_PATH}"
printf "beam_on=%s\n" "0 per merged root" >> "${MANIFEST_PATH}"
printf "run_type=%s\n" "full_statistics" >> "${MANIFEST_PATH}"
printf "comparison_baseline=%s\n" "${BASE_DIR}/results/smallb_all_noForward_beamOn300" >> "${MANIFEST_PATH}"
printf "summary_shape=%s\n" "all_candidates" >> "${MANIFEST_PATH}"

# [EN] Reuse the official merged small-b dataset definition, but set beamOn=0 so every merged ROOT is processed completely. / [CN] 复用当前官方 small-b 合并输入定义，但把 beamOn 设为 0，使每个合并后的 ROOT 都完整跑完。
"${SCAN_BIN}" \
    --sim-bin "${SIM_BIN}" \
    --geometry-macro "${GEOM_MACRO}" \
    --output-dir "${RESULT_DIR}" \
    --beam-on 0 \
    --coarse-min-mm -200 \
    --coarse-max-mm 200 \
    --coarse-step-mm 40 \
    --refine-half-window-mm 20 \
    --refine-step-mm 5 \
    --topk 2 \
    --small-b-max 7 \
    --require-forward false \
    --scan-elastic "${BALANCED_INPUT_DIR}/elastic_b01.root" \
    --scan-elastic "${BALANCED_INPUT_DIR}/elastic_b02.root" \
    --scan-elastic "${BALANCED_INPUT_DIR}/elastic_b03.root" \
    --scan-elastic "${BALANCED_INPUT_DIR}/elastic_b04.root" \
    --scan-elastic "${BALANCED_INPUT_DIR}/elastic_b05.root" \
    --scan-elastic "${BALANCED_INPUT_DIR}/elastic_b06.root" \
    --scan-elastic "${BALANCED_INPUT_DIR}/elastic_b07.root" \
    --scan-elastic "${BALANCED_INPUT_DIR}/elastic_b08.root" \
    --scan-elastic "${BALANCED_INPUT_DIR}/elastic_b09.root" \
    --scan-elastic "${BALANCED_INPUT_DIR}/elastic_b10.root" \
    --scan-allevent "${BALANCED_INPUT_DIR}/allevent_b01.root" \
    --scan-allevent "${BALANCED_INPUT_DIR}/allevent_b02.root" \
    --scan-allevent "${BALANCED_INPUT_DIR}/allevent_b03.root" \
    --scan-allevent "${BALANCED_INPUT_DIR}/allevent_b04.root" \
    --scan-allevent "${BALANCED_INPUT_DIR}/allevent_b05.root" \
    --scan-allevent "${BALANCED_INPUT_DIR}/allevent_b06.root" \
    --scan-allevent "${BALANCED_INPUT_DIR}/allevent_b07.root"

printf "run_finished=%s\n" "$(date '+%Y-%m-%d %H:%M:%S %Z')" >> "${MANIFEST_PATH}"
