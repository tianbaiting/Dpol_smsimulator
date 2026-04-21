#!/bin/bash
set -euo pipefail
BASELINE=configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227.mac
NOAIR=configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_noair.mac

if [[ ! -f "$NOAIR" ]]; then
    echo "FAIL: $NOAIR 不存在" >&2
    exit 1
fi

# Expect exactly one differing line
NDIFF=$(diff "$BASELINE" "$NOAIR" | grep -c '^[<>]' || true)
if [[ "$NDIFF" != "2" ]]; then
    echo "FAIL: 期望两条 diff 行 (一 < 一 >)，实得 $NDIFF" >&2
    diff "$BASELINE" "$NOAIR" >&2 || true
    exit 1
fi

# The differing line must be FillAir
if ! { diff "$BASELINE" "$NOAIR" || true; } | grep -q 'FillAir'; then
    echo "FAIL: diff 不是 FillAir 行" >&2
    exit 1
fi

# And it must be true→false
if ! grep -q '/samurai/geometry/FillAir false' "$NOAIR"; then
    echo "FAIL: $NOAIR 里没有 'FillAir false'" >&2
    exit 1
fi
echo "PASS: macro diff 检查通过"
