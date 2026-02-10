#!/usr/bin/env bash
set -euo pipefail

# [EN] Batch-run sim_deuteron for g4input ypol/zpol trees and mirror outputs into g4output / [CN] 批量运行 sim_deuteron 处理 g4input 的 ypol/zpol 树并在 g4output 中保持目录结构

# [EN] Defaults (can be overridden by env or CLI) / [CN] 默认值（可通过环境变量或命令行覆盖）
SMSIMDIR="${SMSIMDIR:-/home/tian/workspace/dpol/smsimulator5.5}"
SIM_EXEC="${SIM_EXEC:-${SMSIMDIR}/build/bin/sim_deuteron}"
GEOM_MAC="${GEOM_MAC:-${SMSIMDIR}/configs/simulation/geometry/3deg_1.15T.mac}"
INPUT_BASE="${INPUT_BASE:-${SMSIMDIR}/data/simulation/g4input}"
OUTPUT_BASE="${OUTPUT_BASE:-${SMSIMDIR}/data/simulation/g4output}"
MACRO_DIR="${MACRO_DIR:-${SMSIMDIR}/scripts/simulation/_macros}"
MODE="${MODE:-both}"
ONLY_DIRS="${ONLY_DIRS:-}"
DRY_RUN=0

usage() {
    echo "Usage: $0 [--mode ypol|zpol|both] [--input-base PATH] [--output-base PATH] [--only dir1,dir2] [--dry-run]"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --mode) MODE="$2"; shift 2 ;;
        --input-base) INPUT_BASE="$2"; shift 2 ;;
        --output-base) OUTPUT_BASE="$2"; shift 2 ;;
        --only) ONLY_DIRS="$2"; shift 2 ;;
        --dry-run) DRY_RUN=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1"; usage; exit 1 ;;
    esac
done

if [[ ! -x "$SIM_EXEC" ]]; then
    echo "[ERROR] sim_deuteron not found or not executable: $SIM_EXEC"
    exit 1
fi
if [[ ! -f "$GEOM_MAC" ]]; then
    echo "[ERROR] Geometry macro not found: $GEOM_MAC"
    exit 1
fi
if [[ ! -d "$INPUT_BASE" ]]; then
    echo "[ERROR] Input base not found: $INPUT_BASE"
    exit 1
fi

shopt -s nullglob

declare -a ROOTS=()
declare -A SEEN=()
add_root() {
    local dir="$1"
    if [[ -d "$dir" && -z "${SEEN[$dir]:-}" ]]; then
        ROOTS+=("$dir")
        SEEN["$dir"]=1
    fi
}

if [[ "$MODE" == "ypol" || "$MODE" == "both" ]]; then
    for d in "${INPUT_BASE}"/ypol* "${INPUT_BASE}"/y_pol; do
        add_root "$d"
    done
fi
if [[ "$MODE" == "zpol" || "$MODE" == "both" ]]; then
    for d in "${INPUT_BASE}"/zpol* "${INPUT_BASE}"/z_pol; do
        add_root "$d"
    done
fi

if [[ ${#ROOTS[@]} -eq 0 ]]; then
    echo "[ERROR] No ypol/zpol directories found under: $INPUT_BASE"
    exit 1
fi

mkdir -p "$MACRO_DIR"
mkdir -p "$OUTPUT_BASE"

total=0
success=0
SKIP_COUNT=0

IFS=',' read -r -a ONLY_ARR <<< "${ONLY_DIRS}"

for root in "${ROOTS[@]}"; do
    while IFS= read -r -d '' input_file; do
        total=$((total + 1))
        rel="${input_file#${INPUT_BASE}/}"
        rel_dir="$(dirname "$rel")"
        if [[ -n "${ONLY_DIRS}" ]]; then
            match=0
            for p in "${ONLY_ARR[@]}"; do
                [[ -z "$p" ]] && continue
                case "$rel_dir" in
                    "$p"|"$p"/*) match=1 ;;
                esac
                if [[ $match -eq 1 ]]; then
                    break
                fi
            done
            if [[ $match -eq 0 ]]; then
                SKIP_COUNT=$((SKIP_COUNT + 1))
                continue
            fi
        fi
        output_dir="${OUTPUT_BASE}/${rel_dir}"
        run_name="$(basename "$input_file" .root)"

        mkdir -p "$output_dir"
        macro_file="${MACRO_DIR}/run_${run_name}_$$.mac"
        log_file="${output_dir}/${run_name}.log"

        cat > "$macro_file" <<EOF
# [EN] Auto-generated macro for ${input_file} / [CN] 自动生成宏
/action/file/OverWrite y
/action/file/SaveDirectory ${output_dir}/
/action/file/RunName ${run_name}
/control/execute ${GEOM_MAC}
/tracking/storeTrajectory 0
/action/gun/Type Tree
/action/gun/tree/InputFileName ${input_file}
/action/gun/tree/TreeName tree
/action/gun/tree/beamOn 0
exit
EOF

        if [[ $DRY_RUN -eq 1 ]]; then
            echo "[DRY] ${SIM_EXEC} ${macro_file}"
            continue
        fi

        if "${SIM_EXEC}" "${macro_file}" > "${log_file}" 2>&1; then
            success=$((success + 1))
            rm -f "$macro_file"
            echo "[OK] ${input_file} -> ${output_dir} (${run_name})"
        else
            echo "[FAIL] ${input_file} (log: ${log_file}, macro: ${macro_file})"
        fi
    done < <(find "$root" -type f -name "*.root" -print0 | sort -z)
done

echo "[DONE] total=${total} success=${success} fail=$((total - success)) skipped=${SKIP_COUNT}"
