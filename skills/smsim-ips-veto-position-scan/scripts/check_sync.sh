#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

required_files=(
  "skills/smsim-ips-veto-position-scan/SKILL.md"
  "skills/smsim-ips-veto-position-scan/references/workflow.md"
  "skills/smsim-ips-veto-position-scan/references/path-map.md"
  "apps/tools/scan_ips_position.cc"
  "apps/tools/prepare_ips_wrl_examples.cc"
  "scripts/analysis/run_ips_position_scan_3deg115T_smallb_all_noforward.sh"
  "scripts/analysis/export_ips_wrl_examples_3deg115T_offset0.sh"
  "configs/simulation/macros/export_ips_geometry_example.mac"
  "docs/mechanic/veto_impactPrameterSelector/ips_scan_3deg_1p15T_no_forward.zh.md"
)

for rel in "${required_files[@]}"; do
  if [[ ! -e "${ROOT_DIR}/${rel}" ]]; then
    echo "missing: ${rel}" >&2
    exit 1
  fi
done

rg -q '/samurai/geometry/Target/SetTarget false' "${ROOT_DIR}/apps/tools/scan_ips_position.cc"
rg -q '/samurai/geometry/Target/SetTarget false' "${ROOT_DIR}/scripts/analysis/export_ips_wrl_examples_3deg115T_offset0.sh"
rg -q 'smallb_all_noForward_beamOn300' "${ROOT_DIR}/skills/smsim-ips-veto-position-scan/references/workflow.md"

echo 'smsim-ips-veto-position-scan sync OK'
