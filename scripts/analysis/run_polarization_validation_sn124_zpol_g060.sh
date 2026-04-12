#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SETUP_SH="${REPO_DIR}/setup.sh"
if [[ -f "${SETUP_SH}" ]]; then
    # shellcheck disable=SC1090
    source "${SETUP_SH}"
fi

RESULT_DIR="${RESULT_DIR:-${REPO_DIR}/results/rate_target_design_190MeVu/polarization_validation}"
RAW_BASE="${RAW_BASE:-${REPO_DIR}/data/qmdrawdata/qmdrawdata/z_pol/b_discrete}"
STAGING_SOURCE_BASE="${STAGING_SOURCE_BASE:-${RESULT_DIR}/staging_qmd_source}"
G4INPUT_BASE="${G4INPUT_BASE:-${REPO_DIR}/data/simulation/g4input_validation/polarization_validation_sn124_zpol_g060}"
G4OUTPUT_BASE="${G4OUTPUT_BASE:-${REPO_DIR}/data/simulation/g4output/polarization_validation_sn124_zpol_g060}"
ARCHIVE_BASE="${ARCHIVE_BASE:-${REPO_DIR}/data/simulation/g4output/eval_20260227_235751/z_pol/b_discrete}"
GEN_BIN="${GEN_BIN:-${REPO_DIR}/build/bin/GenInputRoot_qmdrawdata}"
SIM_BIN="${SIM_BIN:-${REPO_DIR}/build/bin/sim_deuteron}"
GEOM_MACRO="${GEOM_MACRO:-${REPO_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac}"
VALIDATION_SCRIPT="${VALIDATION_SCRIPT:-${REPO_DIR}/scripts/analysis/validate_polarization_tex_from_data.py}"
CALC_SCRIPT="${CALC_SCRIPT:-${REPO_DIR}/scripts/analysis/calc_rate_target_design_190MeVu.py}"
TARGET_FILTER="${TARGET_FILTER:-d+Sn124E190g060}"
DATASET_ID="${DATASET_ID:-sn124_zpol_g060_bweighted}"
GENERATOR_LOG="${RESULT_DIR}/logs/generate_g4input.log"
MANIFEST_PATH="${RESULT_DIR}/run_manifest.json"

mkdir -p "${RESULT_DIR}" "${RESULT_DIR}/logs" "${RESULT_DIR}/_logs" "${G4OUTPUT_BASE}/_macros" "${G4OUTPUT_BASE}/_logs"
mkdir -p "${STAGING_SOURCE_BASE}/z_pol" "${G4INPUT_BASE}" "${G4OUTPUT_BASE}"
rm -rf "${STAGING_SOURCE_BASE}/z_pol/b_discrete"
mkdir -p "${STAGING_SOURCE_BASE}/z_pol/b_discrete"
for source_dir in "${RAW_BASE}/${TARGET_FILTER}znp" "${RAW_BASE}/${TARGET_FILTER}zpn"; do
    staged_dir="${STAGING_SOURCE_BASE}/z_pol/b_discrete/$(basename "${source_dir}")"
    mkdir -p "${staged_dir}"
    find "${source_dir}" -maxdepth 1 -type f -name "dbreakb*.dat" -exec cp -f {} "${staged_dir}/" \;
done

"${GEN_BIN}" \
    --mode zpol \
    --source elastic \
    --input-base "${STAGING_SOURCE_BASE}" \
    --output-base "${G4INPUT_BASE}" \
    --target-filter "${TARGET_FILTER}" \
    --cut-unphysical on \
    --rotate-zpol on \
    2>&1 | tee "${GENERATOR_LOG}"

while IFS= read -r -d '' input_file; do
    rel="${input_file#${G4INPUT_BASE}/}"
    rel_dir="$(dirname "${rel}")"
    run_name="$(basename "${input_file}" .root)"
    out_dir="${G4OUTPUT_BASE}/${rel_dir}"
    macro_name="${rel_dir//\//_}_${run_name}.mac"
    log_name="${rel_dir//\//_}_${run_name}.log"
    macro_file="${G4OUTPUT_BASE}/_macros/${macro_name}"
    log_file="${G4OUTPUT_BASE}/_logs/${log_name}"
    mkdir -p "${out_dir}"

    cat > "${macro_file}" <<EOF
/control/getEnv SMSIMDIR
/control/execute ${GEOM_MACRO}
/action/file/OverWrite y
/action/file/RunName ${run_name}
/action/file/SaveDirectory ${out_dir}/
/tracking/storeTrajectory 0
/action/gun/Type Tree
/action/gun/tree/InputFileName ${input_file}
/action/gun/tree/TreeName tree
/action/gun/tree/beamOn 0
exit
EOF

    "${SIM_BIN}" "${macro_file}" > "${log_file}" 2>&1
done < <(find "${G4INPUT_BASE}/z_pol/b_discrete" -type f -name "*.root" -print0 | sort -z)

python3 "${VALIDATION_SCRIPT}" \
    --dataset-id "${DATASET_ID}" \
    --raw-base "${RAW_BASE}" \
    --g4input-base "${G4INPUT_BASE}/z_pol/b_discrete" \
    --g4output-base "${G4OUTPUT_BASE}/z_pol/b_discrete" \
    --archive-base "${ARCHIVE_BASE}" \
    --generator-log "${GENERATOR_LOG}" \
    --result-dir "${RESULT_DIR}" \
    --geometry-macro "${GEOM_MACRO}"

python3 "${CALC_SCRIPT}"

python3 - <<PY
import json
from datetime import datetime, timezone
from pathlib import Path

payload = {
    "generated_at_utc": datetime.now(timezone.utc).isoformat(),
    "dataset_id": "${DATASET_ID}",
    "repo_dir": str(Path("${REPO_DIR}").resolve()),
    "raw_base": str(Path("${RAW_BASE}").resolve()),
    "staging_source_base": str(Path("${STAGING_SOURCE_BASE}").resolve()),
    "g4input_base": str(Path("${G4INPUT_BASE}").resolve()),
    "g4output_base": str(Path("${G4OUTPUT_BASE}").resolve()),
    "archive_base": str(Path("${ARCHIVE_BASE}").resolve()),
    "generator_log": str(Path("${GENERATOR_LOG}").resolve()),
    "geometry_macro": str(Path("${GEOM_MACRO}").resolve()),
    "generator_bin": str(Path("${GEN_BIN}").resolve()),
    "sim_bin": str(Path("${SIM_BIN}").resolve()),
    "validation_script": str(Path("${VALIDATION_SCRIPT}").resolve()),
    "calc_script": str(Path("${CALC_SCRIPT}").resolve()),
}
Path("${MANIFEST_PATH}").write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\\n", encoding="utf-8")
PY
