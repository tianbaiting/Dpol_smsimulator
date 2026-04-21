#!/bin/bash
# Verify _mixed.mac = _target3deg.mac + exactly one appended BeamLineVacuum line.
set -euo pipefail
cd "$(dirname "$0")/../../../.."    # repo root

BASELINE=configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg.mac
MIXED=configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_target3deg_mixed.mac

if [[ ! -f "$MIXED" ]]; then
    echo "FAIL: $MIXED 不存在" >&2
    exit 1
fi

# Mixed should have exactly one additional line: BeamLineVacuum true
NDIFF=$(diff "$BASELINE" "$MIXED" | grep -c '^[<>]' || true)
if [[ "$NDIFF" != "1" ]]; then
    echo "FAIL: 期望一条 > 行 (插入 BeamLineVacuum)，实得 $NDIFF" >&2
    diff "$BASELINE" "$MIXED" >&2 || true
    exit 1
fi

if ! grep -q '^/samurai/geometry/BeamLineVacuum true$' "$MIXED"; then
    echo "FAIL: $MIXED 里没有 'BeamLineVacuum true'" >&2
    exit 1
fi

if ! grep -q '^/samurai/geometry/FillAir true$' "$MIXED"; then
    echo "FAIL: $MIXED 里 FillAir 应保持 true（空气世界 + 真空束流管）" >&2
    exit 1
fi

# Update must still be the last /samurai/ command
LAST_CMD=$(grep -E '^/samurai/' "$MIXED" | tail -1)
if [[ "$LAST_CMD" != "/samurai/geometry/Update" ]]; then
    echo "FAIL: Update 应为最后一条 /samurai 命令，实得 '$LAST_CMD'" >&2
    exit 1
fi

echo "PASS: mixed macro diff 检查通过"
