#!/usr/bin/env python3
"""
Step 1: Read 176 RK reco files (spana03 via SSHFS) + 176 NN reco files (local),
align by (cell, entry_index), write merged_events.root.

Both reco sets are produced from the same g4output, so entry i in RK recoTree
aligns 1:1 with entry i in NN recoTree for the same cell.

Schema notes (verified against actual files):
  - TLorentzVector: accessed via .fP.fX / .fP.fY / .fP.fZ / .fE
  - TVector3 (e.g. RecoNeutron.direction): accessed via .fX / .fY / .fZ
  - RecoEvent.neutrons: vector<RecoNeutron> with fields
      direction (TVector3, normalized), beta, energy
    Momentum = 939.565 * gamma * beta * direction where gamma = 1/sqrt(1-beta^2)
  - Provenance: written as TObjString (uproot 5.7+ has no TNamed write API)
"""
import argparse
import json
import sys
import subprocess
from pathlib import Path

import numpy as np
import uproot
import uproot.writing
import awkward as ak


NEUTRON_MASS_MEV = 939.565

TARGETS = ["d+Sn112E190", "d+Sn124E190"]
GAMMAS = ["g050", "g060", "g070", "g080"]
YPOL_HELS = ["ynp", "ypn"]
ZPOL_HELS = ["znp", "zpn"]


def ypol_cells():
    for t in TARGETS:
        for g in GAMMAS:
            for h in YPOL_HELS:
                yield f"{t}{g}{h}"


def zpol_cells():
    for t in TARGETS:
        for g in GAMMAS:
            for h in ZPOL_HELS:
                for b in range(1, 11):
                    yield f"{t}{g}{h}", f"b{b:02d}"


def rk_path(rk_base, pol, cell, b_index):
    """RK reco path on spana03 (accessed via SSHFS mount).
    Naming convention: <input_stem>_reco.root (per apps/run_reconstruction/main.cc::BuildOutputPath).
      ypol: input=dbreak0000.root  -> dbreak0000_reco.root
      zpol: input=dbreakb<NN>0000.root -> dbreakb<NN>0000_reco.root
    b_index is stored as 'b01'..'b10'; we extract the integer for the filename."""
    if pol == "ypol":
        return Path(rk_base) / "y_pol" / "phi_random" / cell / "dbreak0000_reco.root"
    # zpol: b_index like 'b01' -> extract '01' -> form 'dbreakb010000_reco.root'
    b_num = b_index[1:] if b_index.startswith("b") else b_index  # '01'..'10'
    return Path(rk_base) / "z_pol" / "b_discrete" / cell / f"dbreakb{b_num}0000_reco.root"


def nn_path(nn_base, pol, cell, b_index):
    if pol == "ypol":
        return Path(nn_base) / "y_pol" / f"{cell}_reco_nn.root"
    return Path(nn_base) / "z_pol" / f"{cell}_{b_index}_reco_nn.root"


def parse_cell_filter(cells_arg, zpol_cells_arg):
    selected = []
    if cells_arg:
        for c in cells_arg.split(","):
            c = c.strip()
            if c:
                selected.append(("ypol", c, None))
    if zpol_cells_arg:
        for pair in zpol_cells_arg.split(","):
            pair = pair.strip()
            if not pair:
                continue
            if ":" not in pair:
                raise ValueError(f"zpol cell must be cell:b_index, got {pair}")
            cell, b_index = pair.split(":", 1)
            selected.append(("zpol", cell.strip(), b_index.strip()))
    if not selected:
        for c in ypol_cells():
            selected.append(("ypol", c, None))
        for c, b in zpol_cells():
            selected.append(("zpol", c, b))
    return selected


def pre_flight(rk_base, nn_base, selected):
    print(f"[merge] pre-flight: {len(selected)} (cell, b_index) pairs to merge")
    ready = []
    for pol, cell, b_index in selected:
        rk = rk_path(rk_base, pol, cell, b_index)
        nn = nn_path(nn_base, pol, cell, b_index)
        if not rk.exists():
            print(f"[merge] WARN: RK reco missing: {rk}", file=sys.stderr)
            continue
        if not nn.exists():
            print(f"[merge] WARN: NN reco missing: {nn}", file=sys.stderr)
            continue
        ready.append((pol, cell, b_index, rk, nn))
    n_missing = len(selected) - len(ready)
    if n_missing > len(selected) * 0.05:
        print(f"[merge] {n_missing}/{len(selected)} files missing (>5%); aborting", file=sys.stderr)
        sys.exit(1)
    if n_missing > 0:
        print(f"[merge] {n_missing}/{len(selected)} files missing (<=5%); continuing with {len(ready)}")
    print(f"[merge] pre-flight OK: {len(ready)} (cell, b_index) pairs ready")
    return ready


# Branches we need from each file
RK_BRANCHES = [
    "truth_has_proton", "truth_proton_p4",
    "truth_has_neutron", "truth_neutron_p4",
    "reco_proton_px", "reco_proton_py", "reco_proton_pz",
    "reco_proton_status", "reco_proton_chi2_reduced", "reco_proton_p_sigma",
    "recoEvent/neutrons/neutrons.direction",
    "recoEvent/neutrons/neutrons.beta",
]
NN_BRANCHES = [
    "truth_has_proton", "truth_proton_p4",
    "truth_has_neutron", "truth_neutron_p4",
    "reco_proton_px", "reco_proton_py", "reco_proton_pz",
    "recoEvent/neutrons/neutrons.direction",
    "recoEvent/neutrons/neutrons.beta",
]


def safe_open(path):
    """Open a recoTree. Returns None if the file is missing, empty, or not properly closed
    (TFile::Recover case — uproot cannot read unclosed files; ROOT can recover them but
    we treat them as missing and skip)."""
    try:
        f = uproot.open(str(path))
        if not f.keys():
            return None  # unclosed/corrupted file (empty keys)
        return f["recoTree"]
    except Exception as e:
        raise RuntimeError(f"failed to open {path}: {e}") from e


def pick_best_proton_vectorized(px_v, py_v, pz_v, truth_p, has_proton_mask):
    """Vectorized: for each event, pick the reco proton candidate closest in |p| to truth_p.
    px_v/py_v/pz_v are awkward arrays (one vector<double> per event).
    truth_p is a numpy array (one scalar per event).
    Returns (best_px, best_py, best_pz, n_candidates) as numpy arrays.
    Events with no candidates get (nan, nan, nan, 0)."""
    n = len(px_v)
    best_px = np.full(n, np.nan)
    best_py = np.full(n, np.nan)
    best_pz = np.full(n, np.nan)
    n_cand = np.zeros(n, dtype=np.int32)

    # Number of candidates per event
    n_cand = ak.to_numpy(ak.num(px_v, axis=1))
    has_cand = n_cand > 0

    if not np.any(has_cand):
        return best_px, best_py, best_pz, n_cand

    # Compute |p| for every candidate in every event (flat)
    # px_v is awkward ListOffsetArray; arithmetic broadcasts element-wise
    p_reco_flat = np.sqrt(
        np.asarray(ak.flatten(px_v)) ** 2
        + np.asarray(ak.flatten(py_v)) ** 2
        + np.asarray(ak.flatten(pz_v)) ** 2
    )

    # Broadcast truth_p to per-candidate: each event's truth_p repeated for each candidate
    # ak.broadcast_to with the right axis
    truth_p_per_cand = np.repeat(truth_p, n_cand)

    # Per-candidate |p_reco - truth_p|
    diff_flat = np.abs(p_reco_flat - truth_p_per_cand)

    # For each event, find the index (within the event's candidate list) of min diff
    # Rebuild per-event diff array (awkward), then argmin with mask_identity for empty lists
    diff_per_event = ak.unflatten(diff_flat, n_cand)
    local_best_idx = ak.argmin(diff_per_event, axis=1, mask_identity=True)
    # local_best_idx is now a masked array (None for empty events)
    local_best_idx_np = ak.to_numpy(local_best_idx, allow_missing=True)
    # local_best_idx_np is a numpy masked array; fill with 0 (will be masked out later by has_cand)
    local_best_filled = np.ma.filled(local_best_idx_np, 0).astype(np.int64)

    # Flatten the per-event px/py/pz, pick the best index per event
    flat_px = np.asarray(ak.flatten(px_v))
    flat_py = np.asarray(ak.flatten(py_v))
    flat_pz = np.asarray(ak.flatten(pz_v))

    # Convert local-best-index to flat index: offset[i] + local_best_idx[i]
    offsets = np.concatenate([[0], np.cumsum(n_cand)])
    flat_best_idx = offsets[:-1] + local_best_filled

    # Only fill events that have candidates
    best_px[has_cand] = flat_px[flat_best_idx[has_cand]]
    best_py[has_cand] = flat_py[flat_best_idx[has_cand]]
    best_pz[has_cand] = flat_pz[flat_best_idx[has_cand]]

    return best_px, best_py, best_pz, n_cand


def neutron_momentum_vectorized(dir_v, beta_v, has_neutron_mask):
    """Vectorized: compute neutron momentum for first neutron per event.
    dir_v is awkward array of TVector3 (per event, variable count).
    beta_v is awkward array of double (per event, variable count).
    Returns (has_reco, px, py, pz) as numpy arrays.
    has_reco is True where there's at least 1 neutron with valid beta."""
    n = len(dir_v)
    has_reco = np.zeros(n, dtype=bool)
    px = np.full(n, np.nan)
    py = np.full(n, np.nan)
    pz = np.full(n, np.nan)

    n_neutrons = ak.to_numpy(ak.num(dir_v, axis=1))
    has_n = n_neutrons > 0
    if not np.any(has_n):
        return has_reco, px, py, pz

    # Take the first neutron per event (only where has_n)
    # ak.firsts returns None for empty lists → we mask
    first_dir = dir_v[has_n][:, 0]  # first neutron per event, only events with neutrons
    first_beta = beta_v[has_n][:, 0]

    beta_np = np.asarray(first_beta)
    dx = np.asarray(first_dir.fX)
    dy = np.asarray(first_dir.fY)
    dz = np.asarray(first_dir.fZ)

    valid_beta = (beta_np > 0) & (beta_np < 1)
    gamma = np.where(valid_beta, 1.0 / np.sqrt(np.maximum(1.0 - beta_np ** 2, 1e-30)), 0.0)
    p_mag = NEUTRON_MASS_MEV * gamma * beta_np  # MeV/c

    # Fill back into the full arrays
    has_n_indices = np.where(has_n)[0]
    valid_indices = has_n_indices[valid_beta]
    has_reco[valid_indices] = True
    px[valid_indices] = p_mag[valid_beta] * dx[valid_beta]
    py[valid_indices] = p_mag[valid_beta] * dy[valid_beta]
    pz[valid_indices] = p_mag[valid_beta] * dz[valid_beta]

    return has_reco, px, py, pz


def first_scalar_or_nan(vec_field):
    """For an awkward array of vector<double> per event, return numpy array of first element (or nan if empty)."""
    n = len(vec_field)
    out = np.full(n, np.nan)
    counts = ak.to_numpy(ak.num(vec_field, axis=1))
    has = counts > 0
    if not np.any(has):
        return out
    first_vals = vec_field[has][:, 0]
    out[has] = np.asarray(first_vals, dtype=float)
    return out


def merge_one_cell(pol, cell, b_index, rk_file, nn_file):
    """Merge one (cell, b_index) pair. Returns dict of per-event numpy arrays."""
    rk_tree = safe_open(rk_file)
    nn_tree = safe_open(nn_file)
    if rk_tree is None:
        print(f"[merge] WARN: {cell}/{b_index} RK file unreadable (unclosed/corrupted); skipping cell", file=sys.stderr)
        return None
    if nn_tree is None:
        print(f"[merge] WARN: {cell}/{b_index} NN file unreadable (unclosed/corrupted); skipping cell", file=sys.stderr)
        return None

    n_rk = rk_tree.num_entries
    n_nn = nn_tree.num_entries
    if n_rk != n_nn:
        raise RuntimeError(f"entry count mismatch for {cell}/{b_index}: RK={n_rk}, NN={n_nn}")
    if n_rk == 0:
        print(f"[merge] WARN: {cell}/{b_index} has 0 events", file=sys.stderr)
        return None

    rk_arr = rk_tree.arrays(RK_BRANCHES, library="ak")
    nn_arr = nn_tree.arrays(NN_BRANCHES, library="ak")

    # Pre-extract per-event fields as numpy-friendly arrays
    # TLorentzVector: .fP.fX/fY/fZ, .fE
    rk_thp = np.asarray(rk_arr["truth_has_proton"])
    nn_thp = np.asarray(nn_arr["truth_has_proton"])
    rk_thn = np.asarray(rk_arr["truth_has_neutron"])

    rk_tpx = np.asarray(rk_arr["truth_proton_p4"].fP.fX)
    rk_tpy = np.asarray(rk_arr["truth_proton_p4"].fP.fY)
    rk_tpz = np.asarray(rk_arr["truth_proton_p4"].fP.fZ)

    nn_tpx = np.asarray(nn_arr["truth_proton_p4"].fP.fX)
    nn_tpy = np.asarray(nn_arr["truth_proton_p4"].fP.fY)
    nn_tpz = np.asarray(nn_arr["truth_proton_p4"].fP.fZ)

    rk_tnx = np.asarray(rk_arr["truth_neutron_p4"].fP.fX)
    rk_tny = np.asarray(rk_arr["truth_neutron_p4"].fP.fY)
    rk_tnz = np.asarray(rk_arr["truth_neutron_p4"].fP.fZ)

    # Truth-identity check (RK file and NN file must agree on truth for same entry)
    truth_mismatch = (
        (np.abs(rk_tpx - nn_tpx) > 1e-6)
        | (np.abs(rk_tpy - nn_tpy) > 1e-6)
        | (np.abs(rk_tpz - nn_tpz) > 1e-6)
        | (rk_thp != nn_thp)
    )
    n_truth_mismatch = int(np.sum(truth_mismatch))
    if n_truth_mismatch > 0:
        bad_idx = np.where(truth_mismatch)[0][:5]
        raise RuntimeError(
            f"truth mismatch for {cell}/{b_index}: {n_truth_mismatch}/{n_rk} events differ between "
            f"RK and NN files (g4output drift). First bad indices: {bad_idx.tolist()}"
        )

    # Truth |p| for proton candidate selection (use 0.0 where no truth proton — pick_best handles it)
    truth_p = np.sqrt(rk_tpx ** 2 + rk_tpy ** 2 + rk_tpz ** 2)
    truth_p_safe = np.where(rk_thp, truth_p, 0.0)

    # --- Vectorized RK reco proton selection ---
    rk_px_v = rk_arr["reco_proton_px"]
    rk_py_v = rk_arr["reco_proton_py"]
    rk_pz_v = rk_arr["reco_proton_pz"]
    rk_px, rk_py, rk_pz, rk_ncand = pick_best_proton_vectorized(
        rk_px_v, rk_py_v, rk_pz_v, truth_p_safe, rk_thp
    )
    rk_has = rk_ncand > 0
    rk_status = first_scalar_or_nan(rk_arr["reco_proton_status"]).astype(np.int32, copy=False)
    rk_status = np.where(np.isfinite(rk_status.astype(float)), rk_status, -1)
    rk_chi2 = first_scalar_or_nan(rk_arr["reco_proton_chi2_reduced"])
    rk_psig = first_scalar_or_nan(rk_arr["reco_proton_p_sigma"])

    # --- Vectorized NN reco proton selection ---
    nn_px_v = nn_arr["reco_proton_px"]
    nn_py_v = nn_arr["reco_proton_py"]
    nn_pz_v = nn_arr["reco_proton_pz"]
    nn_px, nn_py, nn_pz, nn_ncand = pick_best_proton_vectorized(
        nn_px_v, nn_py_v, nn_pz_v, truth_p_safe, rk_thp
    )
    nn_has = nn_ncand > 0

    # --- Vectorized neutron reco (from NN file) ---
    nn_n_dir = nn_arr["recoEvent/neutrons/neutrons.direction"]
    nn_n_beta = nn_arr["recoEvent/neutrons/neutrons.beta"]
    n_has, n_px, n_py, n_pz = neutron_momentum_vectorized(nn_n_dir, nn_n_beta, rk_thn)

    # Stats for logging
    n_rk_reco = int(np.sum(rk_has))
    n_nn_reco = int(np.sum(nn_has))
    n_multi_candidate = int(np.sum(rk_ncand > 1) + np.sum(nn_ncand > 1))
    n_neutron_reco = int(np.sum(n_has))

    out = {
        "pol": np.full(n_rk, pol, dtype="<U4"),
        "cell": np.full(n_rk, cell, dtype="<U32"),
        "b_index": np.full(n_rk, b_index if b_index else "", dtype="<U4"),
        "event_index": np.arange(n_rk, dtype=np.int64),
        "truth_has_proton": rk_thp,
        "truth_px": np.where(rk_thp, rk_tpx, np.nan),
        "truth_py": np.where(rk_thp, rk_tpy, np.nan),
        "truth_pz": np.where(rk_thp, rk_tpz, np.nan),
        "truth_has_neutron": rk_thn,
        "truth_n_px": np.where(rk_thn, rk_tnx, np.nan),
        "truth_n_py": np.where(rk_thn, rk_tny, np.nan),
        "truth_n_pz": np.where(rk_thn, rk_tnz, np.nan),
        "rk_has_proton": rk_has,
        "rk_px": rk_px,
        "rk_py": rk_py,
        "rk_pz": rk_pz,
        "rk_status": rk_status,
        "rk_chi2_reduced": rk_chi2,
        "rk_p_sigma": rk_psig,
        "rk_n_candidates": rk_ncand,
        "nn_has_proton": nn_has,
        "nn_px": nn_px,
        "nn_py": nn_py,
        "nn_pz": nn_pz,
        "nn_n_candidates": nn_ncand,
        "n_has_reco": n_has,
        "n_px": n_px,
        "n_py": n_py,
        "n_pz": n_pz,
    }

    print(f"[merge] {cell}/{b_index or '-'}: n={n_rk} truth_p={int(rk_thp.sum())} "
          f"rk_reco={n_rk_reco} nn_reco={n_nn_reco} multi={n_multi_candidate} "
          f"n_reco={n_neutron_reco} truth_mismatch={n_truth_mismatch}")

    return out


def get_git_sha():
    try:
        repo = Path(__file__).resolve().parents[2]
        return subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=repo, stderr=subprocess.DEVNULL
        ).decode().strip()
    except Exception:
        return "unknown"


def write_merged_root(out_path, all_rows, provenance):
    """Write merged_events.root with TTree 'events' + TObjString 'Provenance'."""
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    if out_path.exists():
        out_path.unlink()

    # Concatenate per-cell arrays
    keys = list(all_rows[0].keys())
    merged = {k: np.concatenate([r[k] for r in all_rows]) for k in keys}

    with uproot.recreate(out_path) as f:
        # Build spec dict — string columns need conversion to list-of-str for awkward
        spec = {}
        for k, v in merged.items():
            if v.dtype.kind == "U":  # unicode string numpy array
                spec[k] = ak.Array(v.tolist())  # list of str → awkward string array
            else:
                spec[k] = ak.Array(v)
        f["events"] = spec
        # Provenance as TObjString
        f["Provenance"] = uproot.writing.to_TObjString(json.dumps(provenance, indent=2))

    return int(len(merged["event_index"]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rk-reco-base", required=True)
    ap.add_argument("--nn-reco-base", required=True)
    ap.add_argument("--output", required=True)
    ap.add_argument("--cells", default="", help="comma-separated ypol cells (default: all 16)")
    ap.add_argument("--zpol-cells", default="", help="comma-separated 'cell:b_index' zpol pairs (default: all 160)")
    args = ap.parse_args()

    selected = parse_cell_filter(args.cells, args.zpol_cells)
    ready = pre_flight(args.rk_reco_base, args.nn_reco_base, selected)

    if not ready:
        print("[merge] ERROR: no cells ready to merge", file=sys.stderr)
        sys.exit(1)

    all_rows = []
    n_skipped = 0
    for pol, cell, b_index, rk_p, nn_p in ready:
        try:
            rows = merge_one_cell(pol, cell, b_index, rk_p, nn_p)
            if rows is not None:
                all_rows.append(rows)
            else:
                n_skipped += 1
        except Exception as e:
            print(f"[merge] WARN: skipping {cell}/{b_index} due to error: {e}", file=sys.stderr)
            n_skipped += 1

    if not all_rows:
        print("[merge] ERROR: no cells merged successfully", file=sys.stderr)
        sys.exit(1)

    if n_skipped > 0:
        print(f"[merge] NOTE: {n_skipped} cell(s) skipped; continuing with {len(all_rows)}")

    # Build provenance
    total_events = sum(len(r["event_index"]) for r in all_rows)
    provenance = {
        "rk_reco_base": str(args.rk_reco_base),
        "nn_reco_base": str(args.nn_reco_base),
        "n_cells_merged": len(all_rows),
        "total_events": total_events,
        "git_sha": get_git_sha(),
        "g4output_source_size_bytes": 285252607,
        "note": "RK reco from spana03 (dbreak_elastic); NN reco local (breakup_nn_20260503). Both derived from same g4output.",
        "cell_list": [f"{r['pol'][0]}:{r['cell'][0]}:{r['b_index'][0]}" for r in all_rows],
    }

    n_written = write_merged_root(args.output, all_rows, provenance)
    print(f"\n[merge] wrote {args.output}")
    print(f"[merge] total events: {n_written} from {len(all_rows)} cells")
    print(f"[merge] provenance: {json.dumps(provenance, indent=2)}")


if __name__ == "__main__":
    main()
