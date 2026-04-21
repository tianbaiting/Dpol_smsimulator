# MS 消融实验 v1 · Air 关闭 — 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 通过开关 `fFillAir`（G4_AIR → G4_Galactic），对 3 个真值动量点重跑 ensemble，量化靶点→PDC 约 1 m 空气对 PDC 命中 σ_y（基线 8–15 mm）的贡献。

**Architecture:** 复用既有 `test_pdc_target_momentum_e2e` 流水线（已支持 `--geometry-macro`），只新建一个 `FillAir false` 宏；在 `scripts/analysis/ms_ablation/` 下加 3 个轻量脚本（bash 驱动 + Python 指标计算 + Python 对比）；产物分别落到 `build/test_output/ms_ablation_air/{baseline,no_air}/`，memo 写入 `docs/reports/reconstruction/ms_ablation/`。

**Tech Stack:** bash (驱动), Python 3 + ROOT + pandas + numpy + matplotlib (指标 + 画图), Geant4 sim 宏文件, 既有 CMake 构建产物 (`build/bin/test_pdc_target_momentum_e2e`)。

**Spec:** `docs/reports/reconstruction/ms_ablation/specs/2026-04-21-ms-ablation-air-design.md`

**Python 环境:** `micromamba run -n anaroot-env python3 ...`（和 `extract_pdc_hits_from_reco.py` 同）。

---

## Task 1: 创建 air-off 几何宏

**Files:**
- Create: `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_noair.mac`
- Reference: `configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227.mac`

- [ ] **Step 1: 写 smoke 断言脚本**（验证宏文件存在且只差一行）

Create `scripts/analysis/ms_ablation/tests/test_macro_diff.sh`:
```bash
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
if ! diff "$BASELINE" "$NOAIR" | grep -q 'FillAir'; then
    echo "FAIL: diff 不是 FillAir 行" >&2
    exit 1
fi

# And it must be true→false
if ! grep -q '/samurai/geometry/FillAir false' "$NOAIR"; then
    echo "FAIL: $NOAIR 里没有 'FillAir false'" >&2
    exit 1
fi
echo "PASS: macro diff 检查通过"
```

```bash
mkdir -p scripts/analysis/ms_ablation/tests
chmod +x scripts/analysis/ms_ablation/tests/test_macro_diff.sh
```

- [ ] **Step 2: 运行测试确认失败**

Run: `bash scripts/analysis/ms_ablation/tests/test_macro_diff.sh`
Expected: `FAIL: .../geometry_B115T_pdcOptimized_20260227_noair.mac 不存在`

- [ ] **Step 3: 生成 noair 宏**

```bash
cp configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227.mac \
   configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_noair.mac
sed -i 's|/samurai/geometry/FillAir true|/samurai/geometry/FillAir false|' \
    configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_noair.mac
```

- [ ] **Step 4: 运行测试确认通过**

Run: `bash scripts/analysis/ms_ablation/tests/test_macro_diff.sh`
Expected: `PASS: macro diff 检查通过`

- [ ] **Step 5: 提交**

```bash
git add configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_noair.mac \
        scripts/analysis/ms_ablation/tests/test_macro_diff.sh
git commit -m "feat(ms_ablation): add air-off geometry macro for MS ablation v1"
```

---

## Task 2: compute_metrics.py — 读 reco.root 算 M1/M2

**Files:**
- Create: `scripts/analysis/ms_ablation/compute_metrics.py`
- Create: `scripts/analysis/ms_ablation/tests/test_compute_metrics.py`
- Reference: `scripts/analysis/extract_pdc_hits_from_reco.py`（复用 `extract_pdc_hits` 函数的 TTree 解析逻辑）

**纯函数接口**（便于单元测试，与 I/O 分离）：

```python
def compute_dispersion_summary(
    hits: pd.DataFrame,  # 列: truth_px, truth_py, pdc1_x, pdc1_y, pdc2_x, pdc2_y, pdc1_z, pdc2_z
    truth_points: list[tuple[float, float]],  # [(px, py), ...]
) -> pd.DataFrame:
    # 输出每个 truth point 一行, 列:
    #   truth_px, truth_py, N,
    #   sigma_x_pdc1_mm, sigma_y_pdc1_mm, sigma_x_pdc2_mm, sigma_y_pdc2_mm,
    #   sigma_theta_x_mrad, sigma_theta_y_mrad
    ...
```

`sigma` 一律用鲁棒半宽 `(p84 - p16) / 2`；`theta_x = (pdc2_x - pdc1_x) / (pdc2_z - pdc1_z)`, 乘 1000 转 mrad。

- [ ] **Step 1: 写失败的单元测试**

Create `scripts/analysis/ms_ablation/tests/test_compute_metrics.py`:
```python
import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from compute_metrics import compute_dispersion_summary


def _make_hits(truth_px, truth_py, xs1, ys1, xs2, ys2, z1=4000.0, z2=5000.0):
    return pd.DataFrame({
        "truth_px": [truth_px] * len(xs1),
        "truth_py": [truth_py] * len(xs1),
        "pdc1_x": xs1,
        "pdc1_y": ys1,
        "pdc2_x": xs2,
        "pdc2_y": ys2,
        "pdc1_z": [z1] * len(xs1),
        "pdc2_z": [z2] * len(xs1),
    })


def test_robust_half_width_matches_percentile_definition():
    # 100 evenly spaced samples [0, 99]; p84=83.16, p16=15.84 → half-width ≈ 33.66
    xs = np.arange(100, dtype=float)
    hits = _make_hits(0, 0, xs, np.zeros(100), xs, np.zeros(100))
    out = compute_dispersion_summary(hits, [(0, 0)])
    assert len(out) == 1
    expected = (np.percentile(xs, 84) - np.percentile(xs, 16)) / 2
    assert out.iloc[0]["sigma_x_pdc1_mm"] == pytest.approx(expected, rel=1e-6)


def test_zero_dispersion():
    hits = _make_hits(0, 0, np.full(10, 5.0), np.full(10, -3.0),
                            np.full(10, 5.0), np.full(10, -3.0))
    out = compute_dispersion_summary(hits, [(0, 0)])
    row = out.iloc[0]
    assert row["sigma_x_pdc1_mm"] == pytest.approx(0.0)
    assert row["sigma_y_pdc1_mm"] == pytest.approx(0.0)
    assert row["sigma_theta_x_mrad"] == pytest.approx(0.0)
    assert row["sigma_theta_y_mrad"] == pytest.approx(0.0)


def test_theta_computation():
    # pdc1 at (0, 0, 4000), pdc2 vary in x from 500 to 600 (Δx = 100 spread),
    # Δz = 1000 → theta_x spread = 100/1000 = 0.1 rad = 100 mrad; robust half-width
    # over uniform [500, 600]: (p84 - p16)/2 / 1000 * 1000 mrad
    xs1 = np.zeros(100)
    ys1 = np.zeros(100)
    xs2 = np.linspace(500, 600, 100)
    ys2 = np.zeros(100)
    hits = _make_hits(0, 0, xs1, ys1, xs2, ys2)
    out = compute_dispersion_summary(hits, [(0, 0)])
    expected_mrad = (np.percentile(xs2, 84) - np.percentile(xs2, 16)) / 2 / 1000.0 * 1000.0
    assert out.iloc[0]["sigma_theta_x_mrad"] == pytest.approx(expected_mrad, rel=1e-6)


def test_groups_by_truth_point():
    df1 = _make_hits(0, 0, np.arange(50), np.zeros(50), np.arange(50), np.zeros(50))
    df2 = _make_hits(100, 0, np.zeros(50), np.zeros(50), np.zeros(50), np.zeros(50))
    hits = pd.concat([df1, df2], ignore_index=True)
    out = compute_dispersion_summary(hits, [(0, 0), (100, 0)])
    assert len(out) == 2
    assert set(out["truth_px"]) == {0, 100}
    assert out.loc[out["truth_px"] == 100, "sigma_x_pdc1_mm"].iloc[0] == pytest.approx(0.0)


def test_missing_truth_point_yields_nan_row():
    hits = _make_hits(0, 0, np.arange(10), np.zeros(10), np.arange(10), np.zeros(10))
    out = compute_dispersion_summary(hits, [(0, 0), (100, 0)])
    assert len(out) == 2
    missing = out[out["truth_px"] == 100].iloc[0]
    assert missing["N"] == 0
    assert pd.isna(missing["sigma_x_pdc1_mm"])


def test_truth_float_tolerance():
    # Truth stored as float may have small rounding; matching must tolerate it.
    hits = _make_hits(0.0 + 1e-9, 0.0, np.arange(10), np.zeros(10),
                      np.arange(10), np.zeros(10))
    out = compute_dispersion_summary(hits, [(0, 0)])
    assert out.iloc[0]["N"] == 10
```

- [ ] **Step 2: 运行测试确认失败**

Run:
```bash
cd /home/tian/workspace/dpol/smsimulator5.5
micromamba run -n anaroot-env python3 -m pytest scripts/analysis/ms_ablation/tests/test_compute_metrics.py -v
```
Expected: `ModuleNotFoundError: No module named 'compute_metrics'` 或类似失败

- [ ] **Step 3: 写 compute_metrics.py 最小实现**

Create `scripts/analysis/ms_ablation/compute_metrics.py`:
```python
#!/usr/bin/env python3
"""Compute PDC hit and angle dispersion summary for MS ablation study.

Reads a reco ROOT file plus the truth point list, groups events by
(truth_px, truth_py), and writes a robust-half-width summary CSV.

Usage:
  micromamba run -n anaroot-env python3 \\
    scripts/analysis/ms_ablation/compute_metrics.py \\
    --reco-root <path> \\
    --out-dir <path>
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np
import pandas as pd


TRUTH_POINTS = [(0.0, 0.0), (100.0, 0.0), (0.0, 20.0)]
TRUTH_TOL = 1e-3  # MeV/c


def _robust_half_width(values: np.ndarray) -> float:
    if len(values) < 2:
        return float("nan")
    p84, p16 = np.percentile(values, [84, 16])
    return float((p84 - p16) / 2.0)


def compute_dispersion_summary(
    hits: pd.DataFrame,
    truth_points: list[tuple[float, float]],
) -> pd.DataFrame:
    rows = []
    for tpx, tpy in truth_points:
        mask = (np.abs(hits["truth_px"] - tpx) < TRUTH_TOL) & \
               (np.abs(hits["truth_py"] - tpy) < TRUTH_TOL)
        sub = hits[mask]
        n = len(sub)
        row = {"truth_px": tpx, "truth_py": tpy, "N": n}
        if n == 0:
            for col in ("sigma_x_pdc1_mm", "sigma_y_pdc1_mm",
                        "sigma_x_pdc2_mm", "sigma_y_pdc2_mm",
                        "sigma_theta_x_mrad", "sigma_theta_y_mrad"):
                row[col] = float("nan")
            rows.append(row)
            continue

        row["sigma_x_pdc1_mm"] = _robust_half_width(sub["pdc1_x"].to_numpy())
        row["sigma_y_pdc1_mm"] = _robust_half_width(sub["pdc1_y"].to_numpy())
        row["sigma_x_pdc2_mm"] = _robust_half_width(sub["pdc2_x"].to_numpy())
        row["sigma_y_pdc2_mm"] = _robust_half_width(sub["pdc2_y"].to_numpy())

        dz = sub["pdc2_z"].to_numpy() - sub["pdc1_z"].to_numpy()
        theta_x = (sub["pdc2_x"].to_numpy() - sub["pdc1_x"].to_numpy()) / dz
        theta_y = (sub["pdc2_y"].to_numpy() - sub["pdc1_y"].to_numpy()) / dz
        row["sigma_theta_x_mrad"] = _robust_half_width(theta_x) * 1000.0
        row["sigma_theta_y_mrad"] = _robust_half_width(theta_y) * 1000.0

        rows.append(row)
    return pd.DataFrame(rows)


def _load_pdc_hits_from_reco(reco_root: Path) -> pd.DataFrame:
    """Read reco.root and return DataFrame with truth_* + pdc1_*/pdc2_* columns.

    Logic mirrors scripts/analysis/extract_pdc_hits_from_reco.py:extract_pdc_hits
    but additionally pulls truth_proton_p4 per event so we can group by truth.
    """
    import ROOT

    for candidate in ("build/lib/libpdcanalysis.so", "build/lib/libanalysis.so"):
        if Path(candidate).exists():
            ROOT.gSystem.Load(candidate)
            break

    f = ROOT.TFile.Open(str(reco_root), "READ")
    if not f or f.IsZombie():
        raise RuntimeError(f"cannot open {reco_root}")
    tree = f.Get("recoTree")
    if not tree:
        raise RuntimeError("recoTree not found in reco.root")

    # Schema check
    required = {"recoEvent", "truth_proton_p4"}
    branches = {b.GetName() for b in tree.GetListOfBranches()}
    missing = required - branches
    if missing:
        raise RuntimeError(
            f"reco.root missing required branches: {missing}. Found: {sorted(branches)}"
        )

    rows = []
    for entry in range(tree.GetEntries()):
        tree.GetEntry(entry)
        ev = tree.recoEvent
        if ev is None:
            continue
        truth_p4 = tree.truth_proton_p4
        truth_px = float(truth_p4.X())
        truth_py = float(truth_p4.Y())
        for trk_idx, trk in enumerate(ev.tracks):
            if trk.pdgCode == 2112:
                continue
            rows.append({
                "event_index": entry,
                "track_index": trk_idx,
                "truth_px": truth_px,
                "truth_py": truth_py,
                "pdc1_x": trk.start.X(), "pdc1_y": trk.start.Y(), "pdc1_z": trk.start.Z(),
                "pdc2_x": trk.end.X(),   "pdc2_y": trk.end.Y(),   "pdc2_z": trk.end.Z(),
            })
    return pd.DataFrame(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reco-root", type=Path, required=True,
                        help="Path to reco.root produced by test_pdc_target_momentum_e2e")
    parser.add_argument("--out-dir", type=Path, required=True,
                        help="Output directory for CSVs")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[3]
    import os
    os.chdir(repo_root)

    args.out_dir.mkdir(parents=True, exist_ok=True)
    print(f"[compute] reading {args.reco_root}", flush=True)
    hits = _load_pdc_hits_from_reco(args.reco_root)
    print(f"[compute]   {len(hits)} charged tracks", flush=True)

    hits_path = args.out_dir / "pdc_hits.csv"
    hits.to_csv(hits_path, index=False)
    print(f"[compute] wrote {hits_path}", flush=True)

    summary = compute_dispersion_summary(hits, TRUTH_POINTS)
    summary_path = args.out_dir / "dispersion_summary.csv"
    summary.to_csv(summary_path, index=False)
    print(f"[compute] wrote {summary_path}", flush=True)
    print(summary.to_string(index=False))


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: 运行测试确认通过**

Run:
```bash
micromamba run -n anaroot-env python3 -m pytest scripts/analysis/ms_ablation/tests/test_compute_metrics.py -v
```
Expected: 全部 6 个测试 PASS

- [ ] **Step 5: 提交**

```bash
git add scripts/analysis/ms_ablation/compute_metrics.py \
        scripts/analysis/ms_ablation/tests/test_compute_metrics.py
git commit -m "feat(ms_ablation): add compute_metrics.py for PDC dispersion M1+M2"
```

---

## Task 3: compare_ablation.py — 对比两条件生成 compare.csv + overlay PNG

**Files:**
- Create: `scripts/analysis/ms_ablation/compare_ablation.py`
- Create: `scripts/analysis/ms_ablation/tests/test_compare_ablation.py`

**纯函数接口**:

```python
def build_comparison(
    baseline: pd.DataFrame,  # dispersion_summary.csv schema
    noair: pd.DataFrame,
) -> pd.DataFrame:
    # 输出列: truth_px, truth_py, condition (baseline/no_air),
    #   + 所有 sigma_* 列
    # + 对每个 sigma_*, 额外算 delta_<col> = baseline - no_air 作为 baseline 行的附加列
    ...
```

画图：对每个 truth point 生成 1 张 PNG，包含 4 个 subplot（PDC1 x-y 散点 baseline 蓝、no_air 橘；PDC2 同；每 subplot 覆盖轴线和 σ 文字）。输入是合并的 pdc_hits（baseline + no_air 带 condition 列）。

- [ ] **Step 1: 写失败的单元测试**

Create `scripts/analysis/ms_ablation/tests/test_compare_ablation.py`:
```python
import sys
from pathlib import Path

import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from compare_ablation import build_comparison


def _sample_summary(sigma_y_pdc2):
    return pd.DataFrame([{
        "truth_px": 0.0, "truth_py": 0.0, "N": 50,
        "sigma_x_pdc1_mm": 2.8, "sigma_y_pdc1_mm": 11.2,
        "sigma_x_pdc2_mm": 3.5, "sigma_y_pdc2_mm": sigma_y_pdc2,
        "sigma_theta_x_mrad": 3.5, "sigma_theta_y_mrad": 8.0,
    }])


def test_comparison_has_both_conditions():
    baseline = _sample_summary(15.2)
    noair    = _sample_summary(12.0)
    out = build_comparison(baseline, noair)
    assert len(out) == 2
    assert set(out["condition"]) == {"baseline", "no_air"}


def test_comparison_delta_column_present():
    baseline = _sample_summary(15.2)
    noair    = _sample_summary(12.0)
    out = build_comparison(baseline, noair)
    baseline_row = out[out["condition"] == "baseline"].iloc[0]
    assert baseline_row["delta_sigma_y_pdc2_mm"] == pytest.approx(15.2 - 12.0)


def test_comparison_multiple_truth_points():
    b1 = _sample_summary(15.2).assign(truth_px=0.0,   truth_py=0.0)
    b2 = _sample_summary(12.3).assign(truth_px=100.0, truth_py=0.0)
    baseline = pd.concat([b1, b2], ignore_index=True)

    n1 = _sample_summary(11.0).assign(truth_px=0.0,   truth_py=0.0)
    n2 = _sample_summary(9.0).assign(truth_px=100.0,  truth_py=0.0)
    noair = pd.concat([n1, n2], ignore_index=True)

    out = build_comparison(baseline, noair)
    assert len(out) == 4  # 2 truth × 2 conditions
    d1 = out[(out["truth_px"] == 0.0) & (out["condition"] == "baseline")].iloc[0]
    d2 = out[(out["truth_px"] == 100.0) & (out["condition"] == "baseline")].iloc[0]
    assert d1["delta_sigma_y_pdc2_mm"] == pytest.approx(15.2 - 11.0)
    assert d2["delta_sigma_y_pdc2_mm"] == pytest.approx(12.3 - 9.0)


def test_noair_row_has_no_delta():
    baseline = _sample_summary(15.2)
    noair    = _sample_summary(12.0)
    out = build_comparison(baseline, noair)
    noair_row = out[out["condition"] == "no_air"].iloc[0]
    assert pd.isna(noair_row["delta_sigma_y_pdc2_mm"])
```

- [ ] **Step 2: 运行测试确认失败**

Run:
```bash
micromamba run -n anaroot-env python3 -m pytest scripts/analysis/ms_ablation/tests/test_compare_ablation.py -v
```
Expected: `ModuleNotFoundError: No module named 'compare_ablation'`

- [ ] **Step 3: 写 compare_ablation.py 实现**

Create `scripts/analysis/ms_ablation/compare_ablation.py`:
```python
#!/usr/bin/env python3
"""Compare baseline vs no_air dispersion summaries and produce:
  - compare.csv  (stacked rows with delta_ columns on baseline side)
  - figures/dispersion_overlay_<px>_<py>.png  (scatter overlay per truth point)
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import pandas as pd


SIGMA_COLS = [
    "sigma_x_pdc1_mm", "sigma_y_pdc1_mm",
    "sigma_x_pdc2_mm", "sigma_y_pdc2_mm",
    "sigma_theta_x_mrad", "sigma_theta_y_mrad",
]


def build_comparison(
    baseline: pd.DataFrame,
    noair: pd.DataFrame,
) -> pd.DataFrame:
    b = baseline.copy()
    n = noair.copy()
    b["condition"] = "baseline"
    n["condition"] = "no_air"

    # Compute deltas on the baseline rows
    key = ["truth_px", "truth_py"]
    n_lookup = n.set_index(key)
    for col in SIGMA_COLS:
        delta_col = f"delta_{col}"
        b[delta_col] = b.apply(
            lambda r: r[col] - n_lookup.loc[(r["truth_px"], r["truth_py"]), col]
            if (r["truth_px"], r["truth_py"]) in n_lookup.index else float("nan"),
            axis=1,
        )
        n[delta_col] = float("nan")

    return pd.concat([b, n], ignore_index=True, sort=False)


def _render_overlays(baseline_hits: pd.DataFrame, noair_hits: pd.DataFrame,
                     figures_dir: Path, truth_points: list[tuple[float, float]]):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figures_dir.mkdir(parents=True, exist_ok=True)
    for tpx, tpy in truth_points:
        fig, axes = plt.subplots(1, 2, figsize=(12, 5))
        for ax, which in zip(axes, ("pdc1", "pdc2")):
            for df, color, label in [(baseline_hits, "tab:blue", "baseline"),
                                     (noair_hits,    "tab:orange", "no_air")]:
                sub = df[(df["truth_px"] == tpx) & (df["truth_py"] == tpy)]
                ax.scatter(sub[f"{which}_x"], sub[f"{which}_y"],
                           c=color, s=18, alpha=0.6, label=f"{label} (N={len(sub)})")
            ax.set_xlabel(f"{which.upper()} x (mm)")
            ax.set_ylabel(f"{which.upper()} y (mm)")
            ax.set_title(f"{which.upper()} hits @ truth=({tpx:.0f},{tpy:.0f})")
            ax.legend(loc="best", fontsize=9)
            ax.grid(alpha=0.3)
        fig.tight_layout()
        out_path = figures_dir / f"dispersion_overlay_tp{int(tpx)}_{int(tpy)}.png"
        fig.savefig(out_path, dpi=120)
        plt.close(fig)
        print(f"[compare] wrote {out_path}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-dir", type=Path, required=True)
    parser.add_argument("--noair-dir", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)

    baseline_summary = pd.read_csv(args.baseline_dir / "dispersion_summary.csv")
    noair_summary    = pd.read_csv(args.noair_dir    / "dispersion_summary.csv")

    compare = build_comparison(baseline_summary, noair_summary)
    compare_path = args.out_dir / "compare.csv"
    compare.to_csv(compare_path, index=False)
    print(f"[compare] wrote {compare_path}", flush=True)
    print(compare.to_string(index=False))

    # Generate overlay PNGs if hit-level CSVs exist
    baseline_hits_path = args.baseline_dir / "pdc_hits.csv"
    noair_hits_path    = args.noair_dir    / "pdc_hits.csv"
    if baseline_hits_path.exists() and noair_hits_path.exists():
        baseline_hits = pd.read_csv(baseline_hits_path)
        noair_hits    = pd.read_csv(noair_hits_path)
        truth_points = list(zip(baseline_summary["truth_px"], baseline_summary["truth_py"]))
        _render_overlays(baseline_hits, noair_hits, args.out_dir / "figures", truth_points)


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: 运行测试确认通过**

Run:
```bash
micromamba run -n anaroot-env python3 -m pytest scripts/analysis/ms_ablation/tests/test_compare_ablation.py -v
```
Expected: 全部 4 个测试 PASS

- [ ] **Step 5: 提交**

```bash
git add scripts/analysis/ms_ablation/compare_ablation.py \
        scripts/analysis/ms_ablation/tests/test_compare_ablation.py
git commit -m "feat(ms_ablation): add compare_ablation.py for baseline vs no_air diff + overlay"
```

---

## Task 4: run_ablation.sh — 驱动脚本

**Files:**
- Create: `scripts/analysis/ms_ablation/run_ablation.sh`
- Reference: `scripts/analysis/run_ensemble_coverage.sh`（test_pdc_target_momentum_e2e CLI 参考）

**关键决策**：
- `ENSEMBLE_SIZE` 可通过环境变量覆盖，默认 50；smoke 测试时用 2 即可。
- 两条件用相同的 `--seed-a`, `--seed-b` 组合。
- 不跑 `analyze_pdc_rk_error`（那是 reco 分析，MS 归因用不上）。
- 入口打印 `git rev-parse HEAD` 和 build 时间作对照证据。

- [ ] **Step 1: 写 run_ablation.sh**

Create `scripts/analysis/ms_ablation/run_ablation.sh`:
```bash
#!/bin/bash
# Run the Air-off MS ablation experiment (v1).
# Two conditions (baseline / no_air) over 3 truth points × ENSEMBLE_SIZE seeds.
set -euo pipefail

SMSIM_DIR=${SMSIM_DIR:-/home/tian/workspace/dpol/smsimulator5.5}
BUILD_DIR=${BUILD_DIR:-${SMSIM_DIR}/build}
ENSEMBLE_SIZE=${ENSEMBLE_SIZE:-50}
SEED_A=${SEED_A:-20260421}
SEED_B=${SEED_B:-20260422}
OUT_ROOT=${OUT_ROOT:-${BUILD_DIR}/test_output/ms_ablation_air}
RK_FIT_MODE=fixed-target-pdc-only

MAC_BASELINE=${SMSIM_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227.mac
MAC_NOAIR=${SMSIM_DIR}/configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T_pdcOptimized_20260227_noair.mac
FIELD_MAP=${SMSIM_DIR}/configs/simulation/geometry/filed_map/180703-1,15T-3000.table
# [EN] nn-model-json is required by test_pdc_target_momentum_e2e argparse even when --backend=rk.
# Any valid NN model JSON works; we reuse the one from run_ensemble_coverage.sh baseline.
NN_MODEL_JSON=${SMSIM_DIR}/data/nn_target_momentum/formal_B115T3deg_qmdwindow/20260227_223007/model/model_cpp.json

# Sanity
for f in "$MAC_BASELINE" "$MAC_NOAIR" "$FIELD_MAP" "$NN_MODEL_JSON" \
         "$BUILD_DIR/bin/test_pdc_target_momentum_e2e" \
         "$BUILD_DIR/bin/sim_deuteron" \
         "$BUILD_DIR/bin/reconstruct_target_momentum" \
         "$BUILD_DIR/bin/evaluate_target_momentum_reco"; do
    [[ -e "$f" ]] || { echo "MISSING: $f" >&2; exit 1; }
done

echo "=== MS ablation Air v1 ==="
echo "git HEAD:      $(cd "$SMSIM_DIR" && git rev-parse HEAD)"
echo "ENSEMBLE_SIZE: $ENSEMBLE_SIZE"
echo "SEEDS:         A=$SEED_A B=$SEED_B"
echo "OUT_ROOT:      $OUT_ROOT"
echo

run_condition() {
    local label=$1
    local mac=$2
    local outdir=${OUT_ROOT}/${label}
    local simdir=${outdir}/sim

    mkdir -p "$simdir"
    echo "--- [$label] simulating with $(basename "$mac") ---"

    "$BUILD_DIR/bin/test_pdc_target_momentum_e2e" \
        --backend rk \
        --smsimdir "$SMSIM_DIR" \
        --output-dir "$simdir" \
        --sim-bin "$BUILD_DIR/bin/sim_deuteron" \
        --reco-bin "$BUILD_DIR/bin/reconstruct_target_momentum" \
        --eval-bin "$BUILD_DIR/bin/evaluate_target_momentum_reco" \
        --geometry-macro "$mac" \
        --field-map "$FIELD_MAP" \
        --nn-model-json "$NN_MODEL_JSON" \
        --threshold-px 500 --threshold-py 500 --threshold-pz 500 \
        --seed-a "$SEED_A" --seed-b "$SEED_B" \
        --rk-fit-mode "$RK_FIT_MODE" \
        --require-gate-pass 0 --require-min-matched 1 \
        --ensemble-size "$ENSEMBLE_SIZE"

    local reco_root=${simdir}/reco/pdc_truth_grid_rk_${RK_FIT_MODE//-/_}_reco.root
    [[ -f "$reco_root" ]] || { echo "MISSING reco root: $reco_root" >&2; exit 1; }

    echo "--- [$label] computing M1+M2 metrics ---"
    micromamba run -n anaroot-env python3 \
        "$SMSIM_DIR/scripts/analysis/ms_ablation/compute_metrics.py" \
        --reco-root "$reco_root" \
        --out-dir "$outdir"
}

run_condition "baseline" "$MAC_BASELINE"
run_condition "no_air"   "$MAC_NOAIR"

echo
echo "=== Comparing baseline vs no_air ==="
micromamba run -n anaroot-env python3 \
    "$SMSIM_DIR/scripts/analysis/ms_ablation/compare_ablation.py" \
    --baseline-dir "$OUT_ROOT/baseline" \
    --noair-dir    "$OUT_ROOT/no_air" \
    --out-dir      "$OUT_ROOT"

echo
echo "=== Done. Outputs under: $OUT_ROOT ==="
ls -l "$OUT_ROOT"
```

```bash
chmod +x scripts/analysis/ms_ablation/run_ablation.sh
```

- [ ] **Step 2: bash -n 语法校验**

Run: `bash -n scripts/analysis/ms_ablation/run_ablation.sh`
Expected: 无输出、exit 0

- [ ] **Step 3: 提交**

```bash
git add scripts/analysis/ms_ablation/run_ablation.sh
git commit -m "feat(ms_ablation): add run_ablation.sh orchestrator"
```

---

## Task 5: 端到端 smoke 测试（ENSEMBLE_SIZE=2）

**目标**：确认 bash 驱动、sim 流水线、compute_metrics、compare_ablation 串起来不炸，能产出合法 CSV 和 PNG。

- [ ] **Step 1: 确保 build 存在**

Run:
```bash
ls build/bin/test_pdc_target_momentum_e2e build/bin/sim_deuteron \
   build/bin/reconstruct_target_momentum build/bin/evaluate_target_momentum_reco
```
Expected: 四个 binary 都存在。若缺失，先跑 `./build.sh`。

- [ ] **Step 2: 运行 smoke 实验（ENSEMBLE_SIZE=2）**

Run:
```bash
cd /home/tian/workspace/dpol/smsimulator5.5
ENSEMBLE_SIZE=2 OUT_ROOT=build/test_output/ms_ablation_air_smoke \
    bash scripts/analysis/ms_ablation/run_ablation.sh 2>&1 | tail -60
```
Expected: 脚本运行完成，结尾显示 `=== Done. ===` 和目录列表。

- [ ] **Step 3: 验证产物结构**

Run:
```bash
find build/test_output/ms_ablation_air_smoke -type f \
    \( -name '*.csv' -o -name '*.png' \) | sort
```
Expected（至少包含）：
```
build/test_output/ms_ablation_air_smoke/baseline/dispersion_summary.csv
build/test_output/ms_ablation_air_smoke/baseline/pdc_hits.csv
build/test_output/ms_ablation_air_smoke/no_air/dispersion_summary.csv
build/test_output/ms_ablation_air_smoke/no_air/pdc_hits.csv
build/test_output/ms_ablation_air_smoke/compare.csv
build/test_output/ms_ablation_air_smoke/figures/dispersion_overlay_tp0_0.png
build/test_output/ms_ablation_air_smoke/figures/dispersion_overlay_tp100_0.png
build/test_output/ms_ablation_air_smoke/figures/dispersion_overlay_tp0_20.png
```

- [ ] **Step 4: 验证 compare.csv 结构合理**

Run:
```bash
cat build/test_output/ms_ablation_air_smoke/compare.csv
```
Expected: 6 行数据（3 truth × 2 condition），包含列 `delta_sigma_y_pdc2_mm` 等。N=2 情况下 rob half-width 可能是 NaN（允许）但 delta 列在 baseline 侧应存在（哪怕 NaN）。

- [ ] **Step 5: 清理 smoke 产物（不提交）**

Run: `rm -rf build/test_output/ms_ablation_air_smoke`

（smoke 目录不提交，产物本身是 gitignored。如果脚本有修复，按 Task 1-4 对应 task 回去改并提交。）

- [ ] **Step 6: 若 smoke 暴露 bug, 回溯修复后重跑 smoke。**

如果出现失败，典型 bug + 修复：
- `ModuleNotFoundError`: 检查 `sys.path.insert` 在测试中的路径层数；
- `Branch truth_proton_p4 missing`: 读 reco.root 的 schema，按报告 §8 更新分支名；
- 画图 backend 报错: 确认 `matplotlib.use("Agg")` 在 import pyplot 之前；
- `test_pdc_target_momentum_e2e` 参数名冲突: 对照 `run_ensemble_coverage.sh` 逐一比对（可能是 binary 版本升级过）。

---

## Task 6: 正式运行（ENSEMBLE_SIZE=50）

- [ ] **Step 1: 运行正式实验**

Run:
```bash
cd /home/tian/workspace/dpol/smsimulator5.5
bash scripts/analysis/ms_ablation/run_ablation.sh 2>&1 | tee /tmp/ms_ablation_air_run.log
```
Expected: 成功完成。耗时估计 5–15 分钟（取决于 sim 速度）。

- [ ] **Step 2: 校验 N=50 per truth point**

Run:
```bash
cat build/test_output/ms_ablation_air/baseline/dispersion_summary.csv
cat build/test_output/ms_ablation_air/no_air/dispersion_summary.csv
```
Expected: 每个文件 3 行，`N` 列均为 50（若少于 45 说明 event 丢失率异常，需在 memo 里记录）。

- [ ] **Step 3: 校验 compare.csv**

Run: `cat build/test_output/ms_ablation_air/compare.csv`
Expected: 6 行，baseline 侧 `delta_*` 列填充，no_air 侧 `delta_*` 列为空。

- [ ] **Step 4: 快照到 docs**

Run:
```bash
mkdir -p docs/reports/reconstruction/ms_ablation/{data,figures}
cp build/test_output/ms_ablation_air/compare.csv \
   docs/reports/reconstruction/ms_ablation/data/compare.csv
cp build/test_output/ms_ablation_air/figures/*.png \
   docs/reports/reconstruction/ms_ablation/figures/
cp build/test_output/ms_ablation_air/baseline/dispersion_summary.csv \
   docs/reports/reconstruction/ms_ablation/data/baseline_dispersion_summary.csv
cp build/test_output/ms_ablation_air/no_air/dispersion_summary.csv \
   docs/reports/reconstruction/ms_ablation/data/no_air_dispersion_summary.csv
```

- [ ] **Step 5: 暂存产物、下一 task 写 memo**

（暂不提交，等 memo 写完一起提交。）

---

## Task 7: 主 memo — ms_ablation_air_20260421_zh.md

**Files:**
- Create: `docs/reports/reconstruction/ms_ablation/ms_ablation_air_20260421_zh.md`

memo 必须按 spec §8.2 回答三个问题。由于具体数值要等 Task 6 跑完才知道，下面给出模板——**engineer 在执行此 task 时，先读 `data/compare.csv` 填真实数字再保存**。

- [ ] **Step 1: 读真实数字**

Run:
```bash
cat docs/reports/reconstruction/ms_ablation/data/compare.csv
cat docs/reports/reconstruction/ms_ablation/data/baseline_dispersion_summary.csv
cat docs/reports/reconstruction/ms_ablation/data/no_air_dispersion_summary.csv
```

记下：
- 3 个真值点的 σ_y(PDC2, baseline), σ_y(PDC2, no_air), Δσ_y
- σ_θy(baseline), σ_θy(no_air)（单位 mrad）
- baseline 与报告表 15 对应 3 个真值点的偏差百分比

- [ ] **Step 2: 手算 Highland 预测**

Highland 公式: θ₀ = 13.6 MeV/c × z / (β·c·p) × √(x/X0) × [1 + 0.038·ln(x·z²/(X0·β²))]

对 637 MeV/c proton（β ≈ 0.56, p = 637 MeV/c），x/X0 取以下三个估计：
- 1 m 空气（X0_air ≈ 30390 cm, x/X0 ≈ 0.0033）→ θ₀ ≈ ? mrad
- Exit window（厚度/X0 需查 `ExitWindowC2Construction.cc`）→ θ₀ ≈ ? mrad
- PDC gas 19 cm 厚 Ar/iC4H10 → θ₀ ≈ ? mrad

换算成 PDC2 位置 σ_y：σ_y ≈ θ₀ × drift_length（drift 取 ~1 m 从 target 到 PDC2 的有效臂长，或从具体几何算）。

- [ ] **Step 3: 写 memo**

Create `docs/reports/reconstruction/ms_ablation/ms_ablation_air_20260421_zh.md`:
```markdown
# MS 消融实验 v1 · Air 关闭 — 报告

- **日期**: 2026-04-21
- **作者**: TBT
- **Spec**: [specs/2026-04-21-ms-ablation-air-design.md](specs/2026-04-21-ms-ablation-air-design.md)
- **上游**: [RK reco status 20260416](../rk_reconstruction_status_20260416_zh.pdf) §7.5

## 1. 实验设置

- 几何: `geometry_B115T_pdcOptimized_20260227.mac` (baseline, FillAir true) / `..._noair.mac` (FillAir false)
- 真值点: (px, py) ∈ {(0,0), (+100,0), (0,+20)} MeV/c, pz=627, proton, Sn 靶关闭
- Ensemble: N=50 每点, seeds A=20260421/B=20260422
- 指标: M1 = PDC 命中鲁棒半宽; M2 = PDC1→PDC2 角度鲁棒半宽
- git HEAD: <填入 run 时的 HEAD>

## 2. 结果：PDC 命中弥散（M1）

| 真值 (px, py) | σ_y(PDC2, baseline) | σ_y(PDC2, no_air) | Δσ_y | Δσ_y / σ_y(base) |
|---|---|---|---|---|
| (0, 0)    | <填> mm | <填> mm | <填> mm | <填>% |
| (+100, 0) | <填>    | <填>    | <填>    | <填>% |
| (0, +20)  | <填>    | <填>    | <填>    | <填>% |

（σ_x 次要，列在 `data/compare.csv`；σ_y 才是 MS 主导方向）

### 2.1 与 20260416 报告 baseline 对照

本实验 baseline（N=50, 2026-04-21 重跑）与 20260416 报告表 15 三个对应 truth point 的偏差：
- (0,0): <填>% on σ_y(PDC2)
- (+100,0): <填>%
- (0,+20): <填>%

<判断：若偏差 > 15%, 在此段落解释为什么；否则一句话说"基线可重现"即可。>

## 3. 结果：角度弥散（M2）与 Highland 预测比较

| 真值 | σ_θy(baseline, mrad) | σ_θy(no_air, mrad) | Δσ_θy (mrad) |
|---|---|---|---|
| (0, 0)    | <填> | <填> | <填> |
| (+100, 0) | <填> | <填> | <填> |
| (0, +20)  | <填> | <填> | <填> |

### Highland 手算预测

对 637 MeV/c proton:
- 1 m 空气 (X/X0 ≈ 0.0033): θ₀ ≈ <填> mrad
- PDC 气体 19 cm Ar+iC4H10 (X/X0 ≈ <填>): θ₀ ≈ <填> mrad
- Exit window C2 (<查材料和厚度>, X/X0 ≈ <填>): θ₀ ≈ <填> mrad

实测 Δσ_θy（= 空气单独的贡献）与"1 m 空气 θ₀"预测比较: <观察偏差并评论>。

## 4. 图

三张散点对比图（蓝 baseline / 橘 no_air），分别对应三个真值点。位置：`figures/`。

- ![TP1 (0,0)](figures/dispersion_overlay_tp0_0.png)
- ![TP2 (+100,0)](figures/dispersion_overlay_tp100_0.png)
- ![TP3 (0,+20)](figures/dispersion_overlay_tp0_20.png)

## 5. 下一步判决

<按 spec §8.2 第三条填写。三种情形之一，并给出一句话的 rationale。>

- [ ] 空气主导（Δσ_y > 0.5 × σ_y(baseline)）→ 直接进入 RK process-noise 修复
- [ ] 空气贡献小（Δσ_y < 0.2 × σ_y(baseline)）→ 扩展到消融阶段 B（关 exit window）
- [ ] 中间情形 → 继续阶段 B 验证

## 6. 附件

- `data/compare.csv` - 完整对比表
- `data/baseline_dispersion_summary.csv`
- `data/no_air_dispersion_summary.csv`
- `figures/*.png` - 散点 overlay
- 原始 reco ROOT: `build/test_output/ms_ablation_air/{baseline,no_air}/sim/reco/` (未版本控制)
```

把所有 `<填>` 占位替换为真实数字后保存。

- [ ] **Step 4: 验证 memo 完整性**

Run:
```bash
grep -c '<填>' docs/reports/reconstruction/ms_ablation/ms_ablation_air_20260421_zh.md
```
Expected: `0`（占位符全部填完）

- [ ] **Step 5: 提交实验产物 + memo**

```bash
git add docs/reports/reconstruction/ms_ablation/ms_ablation_air_20260421_zh.md \
        docs/reports/reconstruction/ms_ablation/data/ \
        docs/reports/reconstruction/ms_ablation/figures/
git commit -m "docs(ms_ablation): add air-off experiment memo + data + figures"
```

---

## Task 8: README.md 文件夹索引

**Files:**
- Create: `docs/reports/reconstruction/ms_ablation/README.md`

- [ ] **Step 1: 写 README**

Create `docs/reports/reconstruction/ms_ablation/README.md`:
```markdown
# MS 消融实验文档

本目录收录 PDC 命中位置弥散的多重散射（MS）归因实验。

## 导航

- [specs/](specs/) - 设计文档（spec）
  - [2026-04-21-ms-ablation-air-design.md](specs/2026-04-21-ms-ablation-air-design.md) - v1: Air 关闭
- [plans/](plans/) - 实现计划
  - [2026-04-21-ms-ablation-air-plan.md](plans/2026-04-21-ms-ablation-air-plan.md) - v1: Air 关闭
- [ms_ablation_air_20260421_zh.md](ms_ablation_air_20260421_zh.md) - v1 主 memo
- [data/](data/) - 快照 CSV（`compare.csv`, 两条件的 `dispersion_summary.csv`）
- [figures/](figures/) - 散点 overlay PNG

## 实验版本

| 版本 | 日期 | 消融内容 | 状态 | memo |
|---|---|---|---|---|
| v1 | 2026-04-21 | Air (`FillAir true → false`) | 完成 | [ms_ablation_air_20260421_zh.md](ms_ablation_air_20260421_zh.md) |
| v2 | planned | Exit window → vacuum | 未实现 | 依赖 v1 结果 |
| v3 | planned | PDC gas → 低密度 | 未实现 | 依赖 v2 结果 |

## 上游参考

- [../rk_reconstruction_status_20260416_zh.pdf](../rk_reconstruction_status_20260416_zh.pdf) - RK 重建状态报告（§7.5 原始观测）

## 复现方式

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
bash scripts/analysis/ms_ablation/run_ablation.sh
# Outputs: build/test_output/ms_ablation_air/
```
```

- [ ] **Step 2: 提交**

```bash
git add docs/reports/reconstruction/ms_ablation/README.md \
        docs/reports/reconstruction/ms_ablation/plans/2026-04-21-ms-ablation-air-plan.md
git commit -m "docs(ms_ablation): add README index and implementation plan"
```

---

## 依赖关系图

```
Task 1 (macro) ──┐
                 ├──→ Task 4 (run_ablation.sh) ──→ Task 5 (smoke) ──→ Task 6 (full run) ──→ Task 7 (memo) ──→ Task 8 (README)
Task 2 (metrics)─┤
Task 3 (compare)─┘
```

- **Task 1, 2, 3 互不依赖** → 可并行（subagent 并发跑）。
- **Task 4** 依赖 Task 2, 3（脚本调它们）。Task 1 其实也不强依赖（smoke 只要宏存在），但严谨上要先有。
- **Task 5, 6, 7, 8 严格串行**（前一步产物是下一步输入）。

## 成功判据

所有 task 完成后，以下命令应全部成功：

```bash
# 单元测试
micromamba run -n anaroot-env python3 -m pytest scripts/analysis/ms_ablation/tests/ -v

# 产物存在
ls docs/reports/reconstruction/ms_ablation/{README.md,ms_ablation_air_20260421_zh.md}
ls docs/reports/reconstruction/ms_ablation/data/compare.csv
ls docs/reports/reconstruction/ms_ablation/figures/dispersion_overlay_tp*.png

# memo 无占位
! grep -r '<填>' docs/reports/reconstruction/ms_ablation/
```
