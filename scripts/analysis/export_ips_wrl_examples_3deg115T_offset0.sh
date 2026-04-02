#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BASE_DIR="${BASE_DIR:-${REPO_DIR}/data/simulation/ips_scan/sn124_3deg_1.15T}"
OUTPUT_DIR="${OUTPUT_DIR:-${REPO_DIR}/data/simulation/ips_wrl/sn124_3deg_1.15T_offset0}"
SIM_BIN="${SIM_BIN:-${REPO_DIR}/build/bin/sim_deuteron}"
PREP_BIN="${PREP_BIN:-${REPO_DIR}/build/bin/prepare_ips_wrl_examples}"
GEOM_MACRO="${GEOM_MACRO:-${REPO_DIR}/configs/simulation/geometry/3deg_1.15T.mac}"
SIM_WORKDIR="${SIM_WORKDIR:-${REPO_DIR}/configs/simulation}"
INPUT_DIR="${OUTPUT_DIR}/inputs"
MACRO_DIR="${OUTPUT_DIR}/macros"
WRL_DIR="${OUTPUT_DIR}/wrl"
LOG_DIR="${OUTPUT_DIR}/logs"
SELECTION_DIR="${OUTPUT_DIR}/selection"
OFFSET0_STAGE_DIR="${SELECTION_DIR}/offset0_source"
G4OUTPUT_DIR="${LOG_DIR}/g4output"

mkdir -p "${INPUT_DIR}" "${MACRO_DIR}" "${WRL_DIR}" "${LOG_DIR}" "${SELECTION_DIR}" "${OFFSET0_STAGE_DIR}" "${G4OUTPUT_DIR}"

if [[ ! -x "${SIM_BIN}" ]]; then
    echo "missing sim_deuteron binary: ${SIM_BIN}" >&2
    exit 1
fi
if [[ ! -x "${PREP_BIN}" ]]; then
    echo "missing prepare_ips_wrl_examples binary: ${PREP_BIN}" >&2
    exit 1
fi
if [[ ! -f "${GEOM_MACRO}" ]]; then
    echo "missing geometry macro: ${GEOM_MACRO}" >&2
    exit 1
fi

export SMSIMDIR="${REPO_DIR}"

cleanup_vis_scratch() {
    rm -f "${SIM_WORKDIR}/detector_geometry.gdml"
    rm -f "${SIM_WORKDIR}"/g4_*.wrl 2>/dev/null || true
}

write_offset0_source_macro() {
    local bucket="$1"
    local input_root="$2"
    local macro_path="$3"
    local run_name="allevent_scan_allevent_b${bucket}_off_0"
    cat > "${macro_path}" <<EOF
/action/file/OverWrite y
/action/file/SaveDirectory ${OFFSET0_STAGE_DIR}/
/action/file/RunName ${run_name}
/tracking/storeTrajectory 0
/control/execute ${GEOM_MACRO}
/samurai/geometry/Target/SetTarget false
/samurai/geometry/IPS/SetIPS true
/samurai/geometry/IPS/AxisOffset 0 mm
/samurai/geometry/Update
/action/gun/Type Tree
/action/gun/tree/InputFileName ${input_root}
/action/gun/tree/TreeName tree
/action/gun/tree/beamOn 300
EOF
}

ensure_offset0_allevent_output() {
    local bucket="$1"
    local tag="b${bucket}"
    local staged_root="${OFFSET0_STAGE_DIR}/allevent_scan_allevent_${tag}_off_0_00000.root"
    local actual_root="${OFFSET0_STAGE_DIR}/allevent_scan_allevent_${tag}_off_00000.root"
    local input_root="${BASE_DIR}/input/balanced/allevent_${tag}.root"
    local macro_path="${SELECTION_DIR}/ensure_offset0_allevent_${tag}.mac"
    local log_path="${SELECTION_DIR}/ensure_offset0_allevent_${tag}.log"

    rm -f "${staged_root}" "${actual_root}"
    if [[ ! -f "${input_root}" ]]; then
        echo "missing allevent input for ${tag}: ${input_root}" >&2
        exit 1
    fi

    write_offset0_source_macro "${bucket}" "${input_root}" "${macro_path}"
    cleanup_vis_scratch
    (
        cd "${SIM_WORKDIR}"
        "${SIM_BIN}" "${macro_path}" > "${log_path}" 2>&1
    )
    cleanup_vis_scratch
    if [[ -f "${actual_root}" && ! -f "${staged_root}" ]]; then
        ln -sf "${actual_root}" "${staged_root}"
    fi
    if [[ ! -f "${staged_root}" ]]; then
        echo "failed to generate offset0 source output for ${tag}, see ${log_path}" >&2
        exit 1
    fi
}

write_geometry_macro() {
    local macro_path="$1"
    local run_name="$2"
    cat > "${macro_path}" <<EOF
/vis/open VRML2FILE
/tracking/storeTrajectory 0
/action/file/OverWrite y
/action/file/SaveDirectory ${G4OUTPUT_DIR}/
/action/file/RunName ${run_name}
/control/execute ${GEOM_MACRO}
/samurai/geometry/Target/SetTarget false
/samurai/geometry/IPS/SetIPS true
/samurai/geometry/IPS/AxisOffset 0 mm
/samurai/geometry/Update
/vis/drawVolume
/vis/viewer/set/upVector 1 0 0
/vis/viewer/set/viewpointThetaPhi 90. 90.
/vis/scene/add/axes 0 0 0 6000 mm
/vis/viewer/flush
exit
EOF
}

write_event_macro() {
    local macro_path="$1"
    local run_name="$2"
    local input_root="$3"
    cat > "${macro_path}" <<EOF
/vis/open VRML2FILE
/tracking/storeTrajectory 1
/action/file/OverWrite y
/action/file/SaveDirectory ${G4OUTPUT_DIR}/
/action/file/RunName ${run_name}
/control/execute ${GEOM_MACRO}
/samurai/geometry/Target/SetTarget false
/samurai/geometry/IPS/SetIPS true
/samurai/geometry/IPS/AxisOffset 0 mm
/samurai/geometry/Update
/vis/drawVolume
/vis/viewer/set/upVector 1 0 0
/vis/viewer/set/viewpointThetaPhi 90. 90.
/vis/scene/add/axes 0 0 0 6000 mm
/vis/scene/add/trajectories smooth
/vis/modeling/trajectories/create/drawByCharge
/vis/modeling/trajectories/drawByCharge-0/default/setDrawStepPts true
/vis/modeling/trajectories/drawByCharge-0/default/setStepPtsSize 3
/vis/scene/endOfEventAction accumulate
/vis/viewer/set/autoRefresh false
/action/gun/Type Tree
/action/gun/tree/InputFileName ${input_root}
/action/gun/tree/TreeName tree
/action/gun/tree/beamOn 1
/vis/viewer/set/autoRefresh true
/vis/viewer/refresh
/vis/viewer/update
/vis/viewer/flush
exit
EOF
}

run_wrl_export() {
    local macro_path="$1"
    local wrl_path="$2"
    local log_path="$3"

    rm -f "${wrl_path}"
    cleanup_vis_scratch
    (
        cd "${SIM_WORKDIR}"
        G4VRMLFILE_FILE_NAME="${wrl_path}" "${SIM_BIN}" "${macro_path}" > "${log_path}" 2>&1
    )
    if [[ ! -f "${wrl_path}" ]]; then
        local fallback=""
        fallback="$(ls -t "${SIM_WORKDIR}"/g4_*.wrl 2>/dev/null | head -1 || true)"
        if [[ -n "${fallback}" && -f "${fallback}" ]]; then
            mv "${fallback}" "${wrl_path}"
        fi
    fi
    cleanup_vis_scratch
    if [[ ! -f "${wrl_path}" ]]; then
        echo "expected WRL output missing: ${wrl_path} (see ${log_path})" >&2
        exit 1
    fi
}

for bucket in 01 04 07; do
    ensure_offset0_allevent_output "${bucket}"
done

"${PREP_BIN}" \
    --base-dir "${BASE_DIR}" \
    --output-dir "${OUTPUT_DIR}" \
    --offset0-output-dir "${OFFSET0_STAGE_DIR}"

write_geometry_macro "${MACRO_DIR}/offset0_geometry_only.mac" "offset0_geometry_only"
write_event_macro "${MACRO_DIR}/proton_px_scan_pz627_offset0.mac" "proton_px_scan_pz627_offset0" "${INPUT_DIR}/proton_px_scan_pz627_offset0.root"
write_event_macro "${MACRO_DIR}/neutron_px_scan_pz627_offset0.mac" "neutron_px_scan_pz627_offset0" "${INPUT_DIR}/neutron_px_scan_pz627_offset0.root"
write_event_macro "${MACRO_DIR}/elastic_b01_typical_offset0.mac" "elastic_b01_typical_offset0" "${INPUT_DIR}/elastic_b01_typical_offset0.root"
write_event_macro "${MACRO_DIR}/elastic_b05_typical_offset0.mac" "elastic_b05_typical_offset0" "${INPUT_DIR}/elastic_b05_typical_offset0.root"
write_event_macro "${MACRO_DIR}/elastic_b10_typical_offset0.mac" "elastic_b10_typical_offset0" "${INPUT_DIR}/elastic_b10_typical_offset0.root"
write_event_macro "${MACRO_DIR}/allevent_b01_typical_offset0.mac" "allevent_b01_typical_offset0" "${INPUT_DIR}/allevent_b01_typical_offset0.root"
write_event_macro "${MACRO_DIR}/allevent_b04_typical_offset0.mac" "allevent_b04_typical_offset0" "${INPUT_DIR}/allevent_b04_typical_offset0.root"
write_event_macro "${MACRO_DIR}/allevent_b07_typical_offset0.mac" "allevent_b07_typical_offset0" "${INPUT_DIR}/allevent_b07_typical_offset0.root"

# [EN] Export the geometry-only scene first so the reference volume can be compared against every trajectory view at the same offset. / [CN] 先导出纯几何场景，使后续所有轨迹图都能与同一offset下的参考几何对照。
run_wrl_export "${MACRO_DIR}/offset0_geometry_only.mac" "${WRL_DIR}/offset0_geometry_only.wrl" "${LOG_DIR}/offset0_geometry_only.log"
run_wrl_export "${MACRO_DIR}/proton_px_scan_pz627_offset0.mac" "${WRL_DIR}/proton_px_scan_pz627_offset0.wrl" "${LOG_DIR}/proton_px_scan_pz627_offset0.log"
run_wrl_export "${MACRO_DIR}/neutron_px_scan_pz627_offset0.mac" "${WRL_DIR}/neutron_px_scan_pz627_offset0.wrl" "${LOG_DIR}/neutron_px_scan_pz627_offset0.log"
run_wrl_export "${MACRO_DIR}/elastic_b01_typical_offset0.mac" "${WRL_DIR}/elastic_b01_typical_offset0.wrl" "${LOG_DIR}/elastic_b01_typical_offset0.log"
run_wrl_export "${MACRO_DIR}/elastic_b05_typical_offset0.mac" "${WRL_DIR}/elastic_b05_typical_offset0.wrl" "${LOG_DIR}/elastic_b05_typical_offset0.log"
run_wrl_export "${MACRO_DIR}/elastic_b10_typical_offset0.mac" "${WRL_DIR}/elastic_b10_typical_offset0.wrl" "${LOG_DIR}/elastic_b10_typical_offset0.log"
run_wrl_export "${MACRO_DIR}/allevent_b01_typical_offset0.mac" "${WRL_DIR}/allevent_b01_typical_offset0.wrl" "${LOG_DIR}/allevent_b01_typical_offset0.log"
run_wrl_export "${MACRO_DIR}/allevent_b04_typical_offset0.mac" "${WRL_DIR}/allevent_b04_typical_offset0.wrl" "${LOG_DIR}/allevent_b04_typical_offset0.log"
run_wrl_export "${MACRO_DIR}/allevent_b07_typical_offset0.mac" "${WRL_DIR}/allevent_b07_typical_offset0.wrl" "${LOG_DIR}/allevent_b07_typical_offset0.log"

printf "output_dir=%s\n" "${OUTPUT_DIR}" > "${SELECTION_DIR}/export_manifest.txt"
printf "base_dir=%s\n" "${BASE_DIR}" >> "${SELECTION_DIR}/export_manifest.txt"
printf "geometry_macro=%s\n" "${GEOM_MACRO}" >> "${SELECTION_DIR}/export_manifest.txt"
printf "sim_bin=%s\n" "${SIM_BIN}" >> "${SELECTION_DIR}/export_manifest.txt"
printf "selection_manifest=%s\n" "${SELECTION_DIR}/selection_manifest.csv" >> "${SELECTION_DIR}/export_manifest.txt"
printf "target_material=%s\n" "disabled_reference_only" >> "${SELECTION_DIR}/export_manifest.txt"
printf "wrl_count=%s\n" "9" >> "${SELECTION_DIR}/export_manifest.txt"

echo "${WRL_DIR}"
