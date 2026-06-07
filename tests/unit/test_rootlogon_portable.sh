#!/usr/bin/env bash
set -euo pipefail

repo_root="${1:?repo root is required}"
rootlogon="${repo_root}/rootlogon.C"
setup_sh="${repo_root}/setup.sh"

require_text() {
    local file="$1"
    local text="$2"
    if ! grep -Fq "$text" "$file"; then
        echo "missing expected text in ${file}: ${text}" >&2
        exit 1
    fi
}

reject_text() {
    local file="$1"
    local text="$2"
    if grep -Fq "$text" "$file"; then
        echo "found non-portable text in ${file}: ${text}" >&2
        exit 1
    fi
}

require_text "$rootlogon" 'gSystem->Getenv("SMSIMDIR")'
require_text "$rootlogon" 'gSystem->AddDynamicPath'
require_text "$rootlogon" 'TString::Format("%s/build/lib", smsim_dir)'

require_text "$setup_sh" 'gSystem->Getenv("SMSIMDIR")'
require_text "$setup_sh" 'gSystem->AddDynamicPath'
require_text "$setup_sh" 'TString::Format("%s/build/lib", smsim_dir)'

reject_text "$rootlogon" '/home/tian/workspace'
reject_text "$rootlogon" '/home/tbt/workspace'
reject_text "$rootlogon" '/data/tian/workspace'
reject_text "$setup_sh" 'sed -i "s|\$SMSIMDIR|$SMSIMDIR|g"'
