"""Per-event analysis of NEBULA hits to identify cascade (non-primary-neutron) contribution.

For each (px_bin, py_bin=0) of interest, scan g4output shards and split NEBULA Neut hits into:
  - primary-neutron hits: FragSimData entry in a Neut module with
    fParticleName=='neutron' and fParentID==0
  - cascade hits: same module but fParticleName != 'neutron' OR fParentID > 0

Outputs a parquet with columns:
  px_bin, n_events, n_evt_with_primary_neut_hit, n_evt_with_cascade_only,
  n_neut_hits_primary, n_neut_hits_cascade, edep_primary_total, edep_cascade_total,
  cascade_particle_top5 (dict serialized as string)

Also produces a layer-ratio table (L1 vs L2 hit counts at edge vs center) and a
fDetPos.x histogram per bin.
"""
from __future__ import annotations
import argparse, math, glob, json
from pathlib import Path
from collections import Counter

import numpy as np
import pandas as pd
import uproot


def lab_to_target(px, pz, theta_rad):
    ct, st = math.cos(theta_rad), math.sin(theta_rad)
    return ct * px - st * pz


def neut_module_name(s: str) -> bool:
    # FragSimData fDetectorName is something like "NeutronDetector" for Neut bars,
    # "VetoDetector" for Veto bars (verified by reading geometry CSV mapping).
    return s == "NeutronDetector"


def veto_module_name(s: str) -> bool:
    return s == "VetoDetector"


def analyze_shard(root_path: Path, target_px_bins: list[int], theta_rad: float) -> dict:
    """Return per-px_bin aggregates."""
    f = uproot.open(root_path)
    t = f["tree"]
    n_evt = t.num_entries

    # Truth momentum
    mom = t["beam/beam.fMomentum"].array(library="np")
    truth_px = np.array([m[0].member("fP").member("fX") if len(m) > 0 else np.nan for m in mom])
    truth_py = np.array([m[0].member("fP").member("fY") if len(m) > 0 else np.nan for m in mom])
    truth_pz = np.array([m[0].member("fP").member("fZ") if len(m) > 0 else np.nan for m in mom])

    px_tgt = math.cos(theta_rad) * truth_px - math.sin(theta_rad) * truth_pz
    py_tgt = truth_py
    px_bin = np.round(px_tgt).astype(int)
    py_bin = np.round(py_tgt).astype(int)

    target_set = set(target_px_bins)
    out: dict[int, dict] = {b: {
        "n_events": 0,
        "n_evt_with_primary_hit": 0,
        "n_evt_with_any_neut_hit": 0,
        "n_neut_hits_primary": 0,
        "n_neut_hits_cascade": 0,
        "edep_primary": 0.0,
        "edep_cascade": 0.0,
        "cascade_particle_counter": Counter(),
        "primary_parent_counter": Counter(),
        "layer_primary": Counter(),
        "layer_cascade": Counter(),
        "detpos_x_primary": [],
        "detpos_x_cascade": [],
    } for b in target_px_bins}

    # Pre-mask events by py_bin == 0 and px_bin in target
    event_mask = (py_bin == 0) & np.isin(px_bin, list(target_set))
    event_idxs = np.where(event_mask)[0]
    for b in target_px_bins:
        out[b]["n_events"] = int(((py_bin == 0) & (px_bin == b)).sum())

    if len(event_idxs) == 0:
        return out

    # Load full arrays for the shard (simpler than windowed reads which crash on
    # empty-string baskets); ~137k events per shard, FragSim is sparse here.
    arrs = t.arrays(
        [
            "FragSimData/FragSimData.fParticleName",
            "FragSimData/FragSimData.fParentID",
            "FragSimData/FragSimData.fDetectorName",
            "FragSimData/FragSimData.fModuleID",
            "FragSimData/FragSimData.fEnergyDeposit",
            "NEBULAPla/NEBULAPla.id",
            "NEBULAPla/NEBULAPla.fLayer",
            "NEBULAPla/NEBULAPla.fSubLayer",
            "NEBULAPla/NEBULAPla.fDetPos[3]",
            "NEBULAPla/NEBULAPla.fQAveCal",
        ],
        library="np",
    )
    pn = arrs["FragSimData/FragSimData.fParticleName"]
    ppid = arrs["FragSimData/FragSimData.fParentID"]
    pdn = arrs["FragSimData/FragSimData.fDetectorName"]
    pmid = arrs["FragSimData/FragSimData.fModuleID"]
    ped = arrs["FragSimData/FragSimData.fEnergyDeposit"]
    plaid = arrs["NEBULAPla/NEBULAPla.id"]
    player = arrs["NEBULAPla/NEBULAPla.fLayer"]
    psub = arrs["NEBULAPla/NEBULAPla.fSubLayer"]
    pdet = arrs["NEBULAPla/NEBULAPla.fDetPos[3]"]
    pq = arrs["NEBULAPla/NEBULAPla.fQAveCal"]

    if True:
        for global_i in event_idxs:
            local_i = int(global_i)
            b = int(px_bin[global_i])
            rec = out[b]

            # ---- Pass 1: per-FragSim-hit classification (primary vs cascade) ----
            # Build a per-(detector, moduleID) classification: which bar got primary
            # neutron edep vs cascade edep.
            bar_primary_edep: dict[tuple, float] = {}
            bar_cascade_edep: dict[tuple, float] = {}
            cascade_parts: Counter = Counter()
            primary_parts: Counter = Counter()

            names_e = pn[local_i]
            parents_e = ppid[local_i]
            dets_e = pdn[local_i]
            mids_e = pmid[local_i]
            edeps_e = ped[local_i]
            for k in range(len(names_e)):
                d = str(dets_e[k])
                if d != "NeutronDetector":
                    continue
                mid = int(mids_e[k])
                key = (d, mid)
                particle = str(names_e[k])
                parent = int(parents_e[k])
                edep = float(edeps_e[k])
                is_primary_neutron = (particle == "neutron" and parent == 0)
                if is_primary_neutron:
                    bar_primary_edep[key] = bar_primary_edep.get(key, 0.0) + edep
                    primary_parts[particle] += 1
                else:
                    bar_cascade_edep[key] = bar_cascade_edep.get(key, 0.0) + edep
                    cascade_parts[particle] += 1

            any_primary_hit = any(v > 0.0 for v in bar_primary_edep.values())
            any_neut_hit = bool(bar_primary_edep) or bool(bar_cascade_edep)

            if any_primary_hit:
                rec["n_evt_with_primary_hit"] += 1
            if any_neut_hit:
                rec["n_evt_with_any_neut_hit"] += 1

            # ---- Pass 2: walk NEBULAPla entries to gather (Layer, SubLayer, fDetPos.x) ----
            # Tag each NEBULA bar entry with "primary" if its moduleID has primary edep,
            # otherwise "cascade".
            for j in range(len(plaid[local_i])):
                bid = int(plaid[local_i][j])
                if bid < 1 or bid > 120:
                    continue  # skip Veto
                # ModuleID-to-NEBULAPla-id mapping: NEBULA bar IDs 1..120 should match
                # FragSimData fModuleID values used for Neut modules. We'll check both.
                key_a = ("NeutronDetector", bid)
                edep_prim = bar_primary_edep.get(key_a, 0.0)
                edep_casc = bar_cascade_edep.get(key_a, 0.0)
                xb = float(pdet[local_i][j][0])
                L = int(player[local_i][j])
                S = int(psub[local_i][j])
                if edep_prim > edep_casc:
                    rec["n_neut_hits_primary"] += 1
                    rec["edep_primary"] += edep_prim
                    rec["layer_primary"][(L, S)] += 1
                    rec["detpos_x_primary"].append(xb)
                else:
                    rec["n_neut_hits_cascade"] += 1
                    rec["edep_cascade"] += edep_casc
                    rec["layer_cascade"][(L, S)] += 1
                    rec["detpos_x_cascade"].append(xb)

            for p, c in cascade_parts.items():
                rec["cascade_particle_counter"][p] += c
            for p, c in primary_parts.items():
                rec["primary_parent_counter"][p] += c

    f.close()
    return out


def merge(a: dict, b: dict) -> dict:
    """Merge two per-bin dicts (sum scalar fields, union counters, extend lists)."""
    if not a:
        return b
    out = {}
    for k in a:
        ra, rb = a[k], b[k]
        merged = {}
        for f in ("n_events", "n_evt_with_primary_hit", "n_evt_with_any_neut_hit",
                  "n_neut_hits_primary", "n_neut_hits_cascade"):
            merged[f] = ra[f] + rb[f]
        for f in ("edep_primary", "edep_cascade"):
            merged[f] = ra[f] + rb[f]
        for f in ("cascade_particle_counter", "primary_parent_counter",
                  "layer_primary", "layer_cascade"):
            c = Counter(ra[f])
            c.update(rb[f])
            merged[f] = c
        for f in ("detpos_x_primary", "detpos_x_cascade"):
            merged[f] = ra[f] + rb[f]
        out[k] = merged
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--g4output-glob", required=True)
    ap.add_argument("--px-bins", nargs="+", type=int,
                    default=[0, 60, 100, 120, 140, 160, 180, 200])
    ap.add_argument("--target-angle-deg", type=float, default=3.0)
    ap.add_argument("--out", required=True)
    ap.add_argument("--max-shards", type=int, default=None)
    args = ap.parse_args()

    files = sorted(glob.glob(args.g4output_glob))
    if args.max_shards:
        files = files[: args.max_shards]
    print(f"[cascade] scanning {len(files)} shards, target px_bins={args.px_bins}")

    agg = {}
    for k, f in enumerate(files):
        print(f"  [{k+1}/{len(files)}] {f}", flush=True)
        agg = merge(agg, analyze_shard(Path(f), args.px_bins, math.radians(args.target_angle_deg)))

    # Build a tidy DataFrame
    rows = []
    for b in sorted(agg):
        r = agg[b]
        rows.append({
            "px_bin": b,
            "n_events": r["n_events"],
            "n_evt_with_any_neut": r["n_evt_with_any_neut_hit"],
            "n_evt_with_primary_neut": r["n_evt_with_primary_hit"],
            "n_neut_hits_primary": r["n_neut_hits_primary"],
            "n_neut_hits_cascade": r["n_neut_hits_cascade"],
            "edep_primary_MeV": r["edep_primary"],
            "edep_cascade_MeV": r["edep_cascade"],
            "frac_evt_any_neut": (r["n_evt_with_any_neut_hit"] / r["n_events"]) if r["n_events"] else 0.0,
            "frac_evt_primary_neut": (r["n_evt_with_primary_hit"] / r["n_events"]) if r["n_events"] else 0.0,
            "L1S1_primary": r["layer_primary"].get((1, 1), 0),
            "L1S2_primary": r["layer_primary"].get((1, 2), 0),
            "L2S1_primary": r["layer_primary"].get((2, 1), 0),
            "L2S2_primary": r["layer_primary"].get((2, 2), 0),
            "L1S1_cascade": r["layer_cascade"].get((1, 1), 0),
            "L1S2_cascade": r["layer_cascade"].get((1, 2), 0),
            "L2S1_cascade": r["layer_cascade"].get((2, 1), 0),
            "L2S2_cascade": r["layer_cascade"].get((2, 2), 0),
            "cascade_top5": json.dumps(r["cascade_particle_counter"].most_common(5)),
            "detpos_x_primary_mean": float(np.mean(r["detpos_x_primary"])) if r["detpos_x_primary"] else float("nan"),
            "detpos_x_cascade_mean": float(np.mean(r["detpos_x_cascade"])) if r["detpos_x_cascade"] else float("nan"),
            "detpos_x_primary_max": float(np.max(r["detpos_x_primary"])) if r["detpos_x_primary"] else float("nan"),
            "detpos_x_cascade_max": float(np.max(r["detpos_x_cascade"])) if r["detpos_x_cascade"] else float("nan"),
        })
    df = pd.DataFrame(rows)
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    df.to_parquet(args.out, index=False)
    print(df.to_string(index=False))
    print(f"\n[cascade] wrote {args.out}")


if __name__ == "__main__":
    main()
