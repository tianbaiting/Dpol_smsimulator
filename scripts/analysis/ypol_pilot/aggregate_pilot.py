#!/usr/bin/env python3
"""Aggregate ypol pilot results: per-condition coverage, per-event consistency,
cross-condition (allair vs mixed) ablation.

Reads:
  build/test_output/ypol_pilot/error_analysis/<cond>_<gamma><hel>/{track_summary,profile_samples,bayesian_samples}.csv

Writes:
  build/test_output/ypol_pilot/aggregate/
    merged_track_summary.csv         (all 1600 Fisher/Laplace tracks tagged with cond/gamma/hel)
    merged_profile_samples.csv
    merged_bayesian_samples.csv
    coverage_per_condition.csv       (Fisher/Laplace/Profile/MCMC × cond × component × 68/95)
    coverage_overall.csv             (combined across conditions)
    method_consistency.csv           (per-event method-vs-method width ratios)
    cross_condition_pairs.csv        (paired allair vs mixed comparison)
    pilot_summary.txt                (human-readable headline numbers)
"""

from __future__ import annotations

import argparse
import csv
import math
import os
import statistics
import sys
from collections import defaultdict
from pathlib import Path


def clopper_pearson(k: int, n: int, alpha: float = 0.05) -> tuple[float, float]:
    """Clopper-Pearson exact binomial CI."""
    if n == 0:
        return (0.0, 1.0)
    from math import lgamma, log, exp

    def beta_inv(p, a, b):
        # bisection on regularized incomplete beta
        from scipy.special import betaincinv  # type: ignore

        return betaincinv(a, b, p)

    try:
        lo = 0.0 if k == 0 else beta_inv(alpha / 2, k, n - k + 1)
        hi = 1.0 if k == n else beta_inv(1 - alpha / 2, k + 1, n - k)
    except Exception:
        # Fallback: normal approximation (Wald) if scipy unavailable.
        p = k / n
        z = 1.96
        half = z * math.sqrt(p * (1 - p) / n) if n else 0.5
        lo = max(0.0, p - half)
        hi = min(1.0, p + half)
    return (lo, hi)


def parse_track_summary(path: str, cond: str, gamma: str, hel: str) -> list[dict]:
    """Load and tag rows from a per-cell track_summary.csv."""
    rows = []
    with open(path) as f:
        for r in csv.DictReader(f):
            r["condition"] = cond
            r["gamma"] = gamma
            r["helicity"] = hel
            rows.append(r)
    return rows


def covered(truth: float, lo: float, hi: float) -> bool:
    return (lo <= truth <= hi) if not (math.isnan(truth) or math.isnan(lo) or math.isnan(hi)) else False


def safe_float(s: str) -> float:
    try:
        return float(s)
    except (ValueError, TypeError):
        return float("nan")


def compute_fisher_intervals(row: dict) -> dict[str, tuple[float, float, float, float]]:
    """For each component, return (lo68, hi68, lo95, hi95) using Fisher σ around reco center."""
    out = {}
    for comp in ("px", "py", "pz", "p"):
        center = safe_float(row.get(f"reco_{comp}", "nan"))
        sigma = safe_float(row.get(f"fisher_{comp}_sigma", "nan"))
        out[comp] = (
            center - sigma,
            center + sigma,
            center - 1.96 * sigma,
            center + 1.96 * sigma,
        )
    return out


def compute_laplace_intervals(row: dict) -> dict[str, tuple[float, float, float, float]]:
    out = {}
    for comp in ("px", "py", "pz", "p"):
        lo68 = safe_float(row.get(f"laplace_{comp}_lower68", "nan"))
        hi68 = safe_float(row.get(f"laplace_{comp}_upper68", "nan"))
        lo95 = safe_float(row.get(f"laplace_{comp}_lower95", "nan"))
        hi95 = safe_float(row.get(f"laplace_{comp}_upper95", "nan"))
        # If 95 columns missing (some tools only emit 68%), fall back to ±1.96σ scaled.
        if math.isnan(lo95):
            half = (hi68 - lo68) / 2 if (not math.isnan(hi68) and not math.isnan(lo68)) else float("nan")
            center = (lo68 + hi68) / 2 if (not math.isnan(hi68) and not math.isnan(lo68)) else float("nan")
            lo95 = center - 1.96 * half
            hi95 = center + 1.96 * half
        out[comp] = (lo68, hi68, lo95, hi95)
    return out


def compute_method_coverage(
    rows: list[dict],
    method_intervals: callable,  # type: ignore
    conditions: list[str] | None = None,
):
    """Return {component: (n, k68, k95)} aggregated over rows (filtered by condition if given)."""
    counts = {comp: [0, 0, 0] for comp in ("px", "py", "pz", "p")}
    for r in rows:
        if conditions is not None and r["condition"] not in conditions:
            continue
        if r["truth_available"] != "1":
            continue
        intervals = method_intervals(r)
        tx = safe_float(r["truth_px"])
        ty = safe_float(r["truth_py"])
        tz = safe_float(r["truth_pz"])
        tp = math.sqrt(tx * tx + ty * ty + tz * tz)
        truth_vals = {"px": tx, "py": ty, "pz": tz, "p": tp}
        for comp in ("px", "py", "pz", "p"):
            truth = truth_vals[comp]
            lo68, hi68, lo95, hi95 = intervals[comp]
            counts[comp][0] += 1
            if covered(truth, lo68, hi68):
                counts[comp][1] += 1
            if covered(truth, lo95, hi95):
                counts[comp][2] += 1
    return counts


def parse_profile_samples(path: str) -> list[dict]:
    if not os.path.isfile(path):
        return []
    with open(path) as f:
        return list(csv.DictReader(f))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--input-root",
        default="/home/tian/workspace/dpol/smsimulator5.5/build/test_output/ypol_pilot",
    )
    ap.add_argument(
        "--output-root",
        default="/home/tian/workspace/dpol/smsimulator5.5/build/test_output/ypol_pilot/aggregate",
    )
    args = ap.parse_args()

    in_root = Path(args.input_root)
    out_root = Path(args.output_root)
    out_root.mkdir(parents=True, exist_ok=True)

    # 1. Walk error_analysis/<cond>_<gamma><hel>/.
    ana_root = in_root / "error_analysis"
    cells = sorted(p for p in ana_root.iterdir() if p.is_dir())
    print(f"[aggregate] {len(cells)} cells found under {ana_root}")

    all_tracks: list[dict] = []
    all_profile: list[dict] = []
    all_mcmc: list[dict] = []

    for cell in cells:
        # Parse cell name: <cond>_<gamma><hel>, where cond ∈ {allair, mixed},
        # gamma ∈ {g050..g085} (4 chars), hel ∈ {ynp, ypn} (3 chars).
        name = cell.name
        if name.startswith("allair_"):
            cond, rest = "allair", name[len("allair_") :]
        elif name.startswith("mixed_"):
            cond, rest = "mixed", name[len("mixed_") :]
        else:
            print(f"[aggregate] unknown cell prefix: {name}")
            continue
        # rest = <gamma><hel>: gamma is 4 chars, hel is 3 chars.
        gamma, hel = rest[:4], rest[4:]

        ts_path = cell / "track_summary.csv"
        if not ts_path.exists():
            print(f"[aggregate] missing track_summary in {cell}")
            continue
        cell_rows = parse_track_summary(str(ts_path), cond, gamma, hel)
        all_tracks.extend(cell_rows)

        ps_path = cell / "profile_samples.csv"
        for r in parse_profile_samples(str(ps_path)):
            r["condition"] = cond
            r["gamma"] = gamma
            r["helicity"] = hel
            all_profile.append(r)

        ms_path = cell / "bayesian_samples.csv"
        for r in parse_profile_samples(str(ms_path)):
            r["condition"] = cond
            r["gamma"] = gamma
            r["helicity"] = hel
            all_mcmc.append(r)

    print(
        f"[aggregate] tracks={len(all_tracks)} profile={len(all_profile)} mcmc={len(all_mcmc)}"
    )

    # 2. Write merged CSVs.
    if all_tracks:
        with open(out_root / "merged_track_summary.csv", "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(all_tracks[0].keys()))
            w.writeheader()
            w.writerows(all_tracks)
    if all_profile:
        with open(out_root / "merged_profile_samples.csv", "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(all_profile[0].keys()))
            w.writeheader()
            w.writerows(all_profile)
    if all_mcmc:
        with open(out_root / "merged_bayesian_samples.csv", "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(all_mcmc[0].keys()))
            w.writeheader()
            w.writerows(all_mcmc)

    # 3. Per-condition + overall Fisher/Laplace coverage from all_tracks.
    coverage_rows = []
    for cond_set, label in [(["allair"], "allair"), (["mixed"], "mixed"), (None, "overall")]:
        for method, getter in [
            ("Fisher", compute_fisher_intervals),
            ("Laplace", compute_laplace_intervals),
        ]:
            counts = compute_method_coverage(all_tracks, getter, cond_set)
            for comp, (n, k68, k95) in counts.items():
                cp68 = clopper_pearson(k68, n)
                cp95 = clopper_pearson(k95, n)
                coverage_rows.append(
                    {
                        "method": method,
                        "condition": label,
                        "component": comp,
                        "n": n,
                        "k68": k68,
                        "cover68": k68 / n if n else float("nan"),
                        "cp68_lo": cp68[0],
                        "cp68_hi": cp68[1],
                        "k95": k95,
                        "cover95": k95 / n if n else float("nan"),
                        "cp95_lo": cp95[0],
                        "cp95_hi": cp95[1],
                    }
                )

    # Build a lookup index from track_summary by (cond, gamma, hel, event_index, track_index)
    # so we can attach truth values when joining with profile_samples / bayesian_samples.
    track_index_lut = {}
    for r in all_tracks:
        key = (
            r["condition"],
            r["gamma"],
            r["helicity"],
            r["event_index"],
            r["track_index"],
        )
        track_index_lut[key] = r

    def _truth_from_lut(r):
        key = (
            r["condition"],
            r["gamma"],
            r["helicity"],
            r["event_index"],
            r["track_index"],
        )
        t = track_index_lut.get(key)
        if t is None or t["truth_available"] != "1":
            return None
        return t

    # 4. Per-condition Profile coverage. profile_samples lacks truth — join with track_summary.
    # schema check: profile_samples has profile_{px,py,pz,p}_lower68/upper68 + 95.
    if all_profile:
        for cond_set, label in [(["allair"], "allair"), (["mixed"], "mixed"), (None, "overall")]:
            counts = {comp: [0, 0, 0] for comp in ("px", "py", "pz", "p")}
            for r in all_profile:
                if cond_set is not None and r["condition"] not in cond_set:
                    continue
                t = _truth_from_lut(r)
                if t is None:
                    continue
                tx = safe_float(t["truth_px"])
                ty = safe_float(t["truth_py"])
                tz = safe_float(t["truth_pz"])
                tp = math.sqrt(tx * tx + ty * ty + tz * tz)
                truth_vals = {"px": tx, "py": ty, "pz": tz, "p": tp}
                for comp in ("px", "py", "pz", "p"):
                    truth = truth_vals[comp]
                    lo68 = safe_float(r.get(f"profile_{comp}_lower68", "nan"))
                    hi68 = safe_float(r.get(f"profile_{comp}_upper68", "nan"))
                    lo95 = safe_float(r.get(f"profile_{comp}_lower95", "nan"))
                    hi95 = safe_float(r.get(f"profile_{comp}_upper95", "nan"))
                    if math.isnan(lo68) or math.isnan(hi68):
                        continue
                    counts[comp][0] += 1
                    if covered(truth, lo68, hi68):
                        counts[comp][1] += 1
                    if (
                        not math.isnan(lo95)
                        and not math.isnan(hi95)
                        and covered(truth, lo95, hi95)
                    ):
                        counts[comp][2] += 1
            for comp, (n, k68, k95) in counts.items():
                if n == 0:
                    continue
                cp68 = clopper_pearson(k68, n)
                cp95 = clopper_pearson(k95, n)
                coverage_rows.append(
                    {
                        "method": "Profile",
                        "condition": label,
                        "component": comp,
                        "n": n,
                        "k68": k68,
                        "cover68": k68 / n,
                        "cp68_lo": cp68[0],
                        "cp68_hi": cp68[1],
                        "k95": k95,
                        "cover95": k95 / n,
                        "cp95_lo": cp95[0],
                        "cp95_hi": cp95[1],
                    }
                )

    # 5. MCMC coverage. Note: bayesian_samples.csv only carries |p| intervals
    # (mcmc_p_lower68/upper68 + 95), not per-component. Restrict to |p|.
    if all_mcmc:
        for cond_set, label in [(["allair"], "allair"), (["mixed"], "mixed"), (None, "overall")]:
            counts = {"p": [0, 0, 0]}
            for r in all_mcmc:
                if cond_set is not None and r["condition"] not in cond_set:
                    continue
                t = _truth_from_lut(r)
                if t is None:
                    continue
                tx = safe_float(t["truth_px"])
                ty = safe_float(t["truth_py"])
                tz = safe_float(t["truth_pz"])
                truth = math.sqrt(tx * tx + ty * ty + tz * tz)
                lo68 = safe_float(r.get("mcmc_p_lower68", "nan"))
                hi68 = safe_float(r.get("mcmc_p_upper68", "nan"))
                lo95 = safe_float(r.get("mcmc_p_lower95", "nan"))
                hi95 = safe_float(r.get("mcmc_p_upper95", "nan"))
                if math.isnan(lo68) or math.isnan(hi68):
                    continue
                counts["p"][0] += 1
                if covered(truth, lo68, hi68):
                    counts["p"][1] += 1
                if (
                    not math.isnan(lo95)
                    and not math.isnan(hi95)
                    and covered(truth, lo95, hi95)
                ):
                    counts["p"][2] += 1
            for comp, (n, k68, k95) in counts.items():
                if n == 0:
                    continue
                cp68 = clopper_pearson(k68, n)
                cp95 = clopper_pearson(k95, n)
                coverage_rows.append(
                    {
                        "method": "MCMC",
                        "condition": label,
                        "component": comp,
                        "n": n,
                        "k68": k68,
                        "cover68": k68 / n,
                        "cp68_lo": cp68[0],
                        "cp68_hi": cp68[1],
                        "k95": k95,
                        "cover95": k95 / n,
                        "cp95_lo": cp95[0],
                        "cp95_hi": cp95[1],
                    }
                )

    # Write coverage table.
    with open(out_root / "coverage_per_condition.csv", "w", newline="") as f:
        if coverage_rows:
            w = csv.DictWriter(f, fieldnames=list(coverage_rows[0].keys()))
            w.writeheader()
            w.writerows(coverage_rows)

    # 6. Per-event method consistency (Fisher vs Laplace widths from track_summary).
    # Per-event Fisher 68% half-width = fisher_pi_sigma; Laplace = (upper-lower)/2.
    # We already know Fisher == Laplace structurally; this just tabulates the per-event
    # max disagreement.
    consistency_rows = []
    for r in all_tracks:
        if r["truth_available"] != "1":
            continue
        for comp in ("px", "py", "pz", "p"):
            f_w = safe_float(r.get(f"fisher_{comp}_sigma", "nan"))
            l_lo = safe_float(r.get(f"laplace_{comp}_lower68", "nan"))
            l_hi = safe_float(r.get(f"laplace_{comp}_upper68", "nan"))
            l_w = (l_hi - l_lo) / 2 if not (math.isnan(l_hi) or math.isnan(l_lo)) else float("nan")
            if math.isnan(f_w) or math.isnan(l_w):
                continue
            ratio = l_w / f_w if f_w > 0 else float("nan")
            consistency_rows.append(
                {
                    "condition": r["condition"],
                    "gamma": r["gamma"],
                    "helicity": r["helicity"],
                    "event_index": r["event_index"],
                    "track_index": r["track_index"],
                    "component": comp,
                    "fisher_w": f_w,
                    "laplace_w": l_w,
                    "ratio_l_over_f": ratio,
                }
            )
    if consistency_rows:
        with open(out_root / "method_consistency.csv", "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(consistency_rows[0].keys()))
            w.writeheader()
            w.writerows(consistency_rows)

    # 7. Cross-condition pairs: same event_index in allair vs mixed (same gamma+helicity).
    # Group by (gamma, helicity, event_index, track_index), require both conditions.
    by_key = defaultdict(dict)
    for r in all_tracks:
        if r["truth_available"] != "1":
            continue
        key = (r["gamma"], r["helicity"], r["event_index"], r["track_index"])
        by_key[key][r["condition"]] = r

    pair_rows = []
    for key, conds in by_key.items():
        if "allair" in conds and "mixed" in conds:
            a = conds["allair"]
            m = conds["mixed"]
            row = {
                "gamma": key[0],
                "helicity": key[1],
                "event_index": key[2],
                "track_index": key[3],
            }
            for comp in ("px", "py", "pz", "p"):
                truth = (
                    safe_float(a.get(f"truth_{comp}", "nan"))
                    if comp != "p"
                    else math.sqrt(
                        safe_float(a.get("truth_px", "nan")) ** 2
                        + safe_float(a.get("truth_py", "nan")) ** 2
                        + safe_float(a.get("truth_pz", "nan")) ** 2
                    )
                )
                row[f"truth_{comp}"] = truth
                row[f"reco_allair_{comp}"] = safe_float(
                    a.get(f"reco_{comp}", "nan")
                )
                row[f"reco_mixed_{comp}"] = safe_float(
                    m.get(f"reco_{comp}", "nan")
                )
                row[f"sigma_allair_{comp}"] = safe_float(
                    a.get(f"fisher_{comp}_sigma", "nan")
                )
                row[f"sigma_mixed_{comp}"] = safe_float(
                    m.get(f"fisher_{comp}_sigma", "nan")
                )
            pair_rows.append(row)

    if pair_rows:
        with open(out_root / "cross_condition_pairs.csv", "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(pair_rows[0].keys()))
            w.writeheader()
            w.writerows(pair_rows)

    # 8. Headline summary.
    summary_lines = []
    summary_lines.append("# YPOL pilot — headline summary")
    summary_lines.append(f"tracks: {len(all_tracks)}")
    summary_lines.append(f"profile samples: {len(all_profile)}")
    summary_lines.append(f"mcmc samples: {len(all_mcmc)}")
    summary_lines.append(f"cross-condition pairs: {len(pair_rows)}")
    summary_lines.append("")
    summary_lines.append("## Coverage table (cover68 [CP 95% CI])")
    summary_lines.append(
        f"{'method':10s} {'cond':8s} {'comp':5s} {'N':>6s} {'cover68':>8s} {'CP_lo':>7s} {'CP_hi':>7s}"
    )
    for row in coverage_rows:
        summary_lines.append(
            f"{row['method']:10s} {row['condition']:8s} {row['component']:5s} "
            f"{row['n']:6d} {row['cover68']:8.3f} {row['cp68_lo']:7.3f} {row['cp68_hi']:7.3f}"
        )
    out_text = "\n".join(summary_lines) + "\n"
    with open(out_root / "pilot_summary.txt", "w") as f:
        f.write(out_text)
    print()
    print(out_text)


if __name__ == "__main__":
    main()
