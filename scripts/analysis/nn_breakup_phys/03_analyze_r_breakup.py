#!/usr/bin/env python3
"""Apply ypol/zpol input_ana cuts to NN-reco breakup CSVs and compute
Delta p_x distributions plus R = N(>0)/N(<0) per cut x reco-variant.

Reads per-cell CSVs produced by 02_extract_observables.C.
Cuts come from scripts/notebooks/input_analysis/core/cuts.py (re-implemented
inline against CSV columns to avoid the Event/Derived class dependency).

Reco variants (matching ypol_phys_20260428_zh.tex):
  A truth      : truth_p for both proton and neutron (always available)
  B reco mixed : NN proton; if NEBULA-hit then reco neutron else truth neutron
  C full reco  : NN proton + reco neutron, NEBULA-hit subset only
"""
from __future__ import annotations
import argparse, json, math
from pathlib import Path
import numpy as np
import pandas as pd

# --- cut definitions (mirror scripts/notebooks/input_analysis/core/cuts.py) ---

def ypol_cuts(row):
    sum_px = row.truth_pxp + row.truth_pxn
    sum_py = row.truth_pyp + row.truth_pyn
    sum_xy2 = sum_px * sum_px + sum_py * sum_py
    loose = (abs(row.truth_pyp - row.truth_pyn) < 150) and (sum_xy2 > 2500)
    if not loose: return False, False, False
    phi = math.atan2(sum_py, sum_px)
    cos, sin = math.cos(-phi), math.sin(-phi)
    rot_pxp = cos * row.truth_pxp - sin * row.truth_pyp
    rot_pxn = cos * row.truth_pxn - sin * row.truth_pyn
    mid = (rot_pxp + rot_pxn) < 200
    if not mid: return True, False, False
    tight = (math.pi - abs(phi)) < 0.2
    return True, True, tight

def zpol_cuts(row):
    loose = ((row.truth_pzp + row.truth_pzn) > 1150) and (abs(row.truth_pzp - row.truth_pzn) < 150)
    if not loose: return False, False, False
    sum_px = row.truth_pxp + row.truth_pxn
    sum_py = row.truth_pyp + row.truth_pyn
    sum_xy2 = sum_px * sum_px + sum_py * sum_py
    mid = ((row.truth_pxp + row.truth_pxn) < 200) and (sum_xy2 > 2500)
    if not mid: return True, False, False
    phi = math.atan2(sum_py, sum_px)
    tight = (math.pi - abs(phi)) < 0.5
    return True, True, tight

CUT_FN = {"ypol": ypol_cuts, "zpol": zpol_cuts}

# --- reaction-plane rotation ---

def add_rotated_columns(df: pd.DataFrame) -> pd.DataFrame:
    sum_px = df.truth_pxp + df.truth_pxn
    sum_py = df.truth_pyp + df.truth_pyn
    phi = np.arctan2(sum_py, sum_px)
    cos, sin = np.cos(-phi), np.sin(-phi)
    df = df.copy()
    df["phi_event"] = phi
    for prefix in ("truth", "nn", "reco"):
        pxn_col = f"{prefix}_pxn" if prefix != "nn" else "reco_pxn"  # NN does not reco neutron
        pyn_col = f"{prefix}_pyn" if prefix != "nn" else "reco_pyn"
        # proton (NN provides nn_p*, RK would provide rk_p* in a future report)
        if prefix == "truth":
            df["truth_rot_pxp"] = cos * df.truth_pxp - sin * df.truth_pyp
            df["truth_rot_pxn"] = cos * df.truth_pxn - sin * df.truth_pyn
        elif prefix == "nn":
            df["nn_rot_pxp"] = cos * df.nn_pxp - sin * df.nn_pyp
        elif prefix == "reco":
            df["reco_rot_pxn"] = cos * df.reco_pxn - sin * df.reco_pyn
    return df

# --- variant Δp_x and R ---

def variant_dpx(df: pd.DataFrame) -> dict[str, np.ndarray]:
    """Return Δp_x_rot per variant. Caller supplies a df already filtered to a cut."""
    truth_dpx = (df.truth_rot_pxp - df.truth_rot_pxn).values

    nebula_hit = df.n_reco_neutrons > 0
    # B reco mixed: nn proton − (reco neutron if NEBULA hit else truth neutron)
    pxn_for_mixed = np.where(nebula_hit, df.reco_rot_pxn, df.truth_rot_pxn)
    mixed_dpx = (df.nn_rot_pxp.values - pxn_for_mixed)

    full_mask = nebula_hit
    full_dpx = (df.nn_rot_pxp[full_mask] - df.reco_rot_pxn[full_mask]).values
    return {"truth": truth_dpx, "mixed": mixed_dpx, "full": full_dpx}

def R_with_boot(values: np.ndarray, n_boot: int = 1000, rng_seed: int = 20260503) -> tuple[float, float, float, int]:
    n_pos = int((values > 0).sum())
    n_neg = int((values < 0).sum())
    if n_neg == 0:
        return float("inf"), float("nan"), float("nan"), n_pos + n_neg
    R_central = n_pos / n_neg
    rng = np.random.default_rng(rng_seed)
    n = len(values)
    if n < 50:
        return R_central, float("nan"), float("nan"), n_pos + n_neg
    boots = []
    for _ in range(n_boot):
        idx = rng.integers(0, n, n)
        sample = values[idx]
        np_ = (sample > 0).sum(); nn_ = (sample < 0).sum()
        boots.append(np_ / nn_ if nn_ > 0 else float("nan"))
    boots = np.array([b for b in boots if np.isfinite(b)])
    lo, hi = np.percentile(boots, [16, 84])
    return R_central, float(lo), float(hi), n_pos + n_neg

# --- driver ---

def parse_cell(tag: str) -> tuple[str, str, str]:
    # e.g. "d+Sn124E190g050ynp" -> target="Sn124E190" gamma="g050" hel="ynp"
    rest = tag[2:] if tag.startswith("d+") else tag
    # split off helicity (last 3 chars: ynp/ypn/znp/zpn)
    hel = rest[-3:]
    body = rest[:-3]
    # gamma = "gNNN" at end of body
    g_idx = body.rfind("g")
    gamma = body[g_idx:]
    target = body[:g_idx]
    return target, gamma, hel

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv-dir", required=True, help="Per-cell CSV directory (one pol)")
    ap.add_argument("--pol", choices=["ypol", "zpol"], required=True)
    ap.add_argument("--out-dir", required=True)
    args = ap.parse_args()

    cut_fn = CUT_FN[args.pol]
    out = Path(args.out_dir); out.mkdir(parents=True, exist_ok=True)

    csv_paths = sorted(Path(args.csv_dir).glob("*.csv"))
    print(f"[analyze] pol={args.pol} reading {len(csv_paths)} CSVs from {args.csv_dir}")
    dfs = [pd.read_csv(p) for p in csv_paths]
    df = pd.concat(dfs, ignore_index=True)
    print(f"[analyze] total rows = {len(df)}")

    df = df[(df.truth_has_proton == 1) & (df.truth_has_neutron == 1)].copy()
    print(f"[analyze] truth_has_proton & truth_has_neutron = {len(df)}")
    nn_ok = (df.nn_status == 0) & (df[["nn_pxp", "nn_pyp", "nn_pzp"]].abs().sum(axis=1) > 0)
    df = df[nn_ok].copy()
    print(f"[analyze] nn_status==0 = {len(df)}")

    # cut booleans (truth-based)
    cut_results = df.apply(cut_fn, axis=1, result_type="expand")
    cut_results.columns = ["loose", "mid", "tight"]
    df = pd.concat([df.reset_index(drop=True), cut_results.reset_index(drop=True)], axis=1)
    df = add_rotated_columns(df)

    parsed = df["tag"].apply(parse_cell).tolist()
    df["target"] = [p[0] for p in parsed]
    df["gamma"] = [p[1] for p in parsed]
    df["hel"]   = [p[2] for p in parsed]

    # passrate table per (target, gamma) [pool helicities]
    rows = []
    for (tgt, gam), sub in df.groupby(["target", "gamma"]):
        rows.append({
            "target": tgt, "gamma": gam,
            "N_raw": len(sub),
            "N_loose": int(sub.loose.sum()),
            "N_mid":   int(sub.mid.sum()),
            "N_tight": int(sub.tight.sum()),
            "N_NEBULA_in_tight": int(((sub.tight) & (sub.n_reco_neutrons > 0)).sum()),
        })
    pd.DataFrame(rows).to_csv(out / "cell_passrates.csv", index=False)

    # R table per (target, gamma) x cut x variant
    r_rows = []
    for (tgt, gam), sub in df.groupby(["target", "gamma"]):
        for cut in ("loose", "mid", "tight"):
            sub_c = sub[sub[cut]]
            if len(sub_c) == 0: continue
            dpx = variant_dpx(sub_c)
            for variant in ("truth", "mixed", "full"):
                R, lo, hi, n_used = R_with_boot(dpx[variant])
                r_rows.append({
                    "target": tgt, "gamma": gam, "cut": cut, "variant": variant,
                    "N": n_used, "R": R, "R_lo": lo, "R_hi": hi,
                })
    pd.DataFrame(r_rows).to_csv(out / "r_table.csv", index=False)

    # global summary
    summary = {
        "pol": args.pol,
        "n_cells": int(df.tag.nunique()),
        "n_total_after_filter": int(len(df)),
        "n_loose": int(df.loose.sum()),
        "n_mid":   int(df.mid.sum()),
        "n_tight": int(df.tight.sum()),
        "n_NEBULA_in_tight": int((df.tight & (df.n_reco_neutrons > 0)).sum()),
    }
    (out / "summary.json").write_text(json.dumps(summary, indent=2))
    print(json.dumps(summary, indent=2))

    # save the joined df for the figure step (parquet preferred, CSV fallback)
    try:
        df.to_parquet(out / "joined.parquet", index=False)
        print(f"[analyze] wrote joined.parquet")
    except ImportError:
        df.to_csv(out / "joined.csv.gz", index=False, compression="gzip")
        print(f"[analyze] pyarrow/fastparquet missing; wrote joined.csv.gz instead")
    print(f"[analyze] wrote -> {out}")

if __name__ == "__main__":
    main()
