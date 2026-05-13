"""Generate sharded tree-gun input ROOT files for the (px,py) x pz grid scan.

Each shard is an independent ROOT file; sim_deuteron jobs run in parallel,
each consuming one shard.  Events are round-robin distributed so every shard
covers the full (px,py,pz) grid uniformly.

Schema
------
The Tree gun in PrimaryGeneratorActionBasic::BeamTypeTree reads a TTree named
"tree" with a single branch "TBeamSimData" of type vector<TBeamSimData>*.
libsmdata.so (build/lib/) must be on LD_LIBRARY_PATH and ROOT_INCLUDE_PATH
must point to libs/smg4lib/src/data/include/.  Both are set by setup.sh.

Typical invocation (after source setup.sh):
    python scripts/analysis/nebula_reco_quality/gen_pxpy_grid_input.py \\
        --out-prefix /path/to/dir/neutron_pxpy \\
        --n-shards 32 \\
        --px-min -200 --px-max 200 --px-step 20 \\
        --py-min -100 --py-max 100 --py-step 10 \\
        --pz-list 550,600,627,650,700 \\
        --n-per-bin 2000
"""
from __future__ import annotations

import argparse
import math
import os
from pathlib import Path

import numpy as np

# ---------------------------------------------------------------------------
# ROOT / smdata bootstrap
# ---------------------------------------------------------------------------
import ROOT  # noqa: E402  (must come after numpy to avoid symbol clashes)

# [EN] Tell ROOT where to find TBeamSimData.hh before loading the library so
# PyROOT can resolve the class definition (rootmap declares the header by
# basename only). / [CN] 在加载库前告诉 ROOT 头文件路径，PyROOT 才能解析
# 类定义（rootmap 只用 basename）。
_SMSIM = os.environ.get("SMSIMDIR", str(Path(__file__).resolve().parents[3]))
for inc in (
    f"{_SMSIM}/libs/smg4lib/src/data/include",
    f"{_SMSIM}/libs/analysis/include",
):
    if Path(inc).is_dir():
        ROOT.gInterpreter.AddIncludePath(inc)

ROOT.gSystem.Load("libsmdata.so")  # TBeamSimData + TBeamSimDataArray
ROOT.gInterpreter.ProcessLine('#include "TBeamSimData.hh"')


def _make_neutron(px_mev: float, py_mev: float, pz_mev: float,
                  x_mm: float = 0.0, y_mm: float = 0.0, z_mm: float = 0.0,
                  primary_id: int = 0) -> "ROOT.TBeamSimData":
    """Return a TBeamSimData representing a single neutron."""
    Mn = 939.565330  # MeV
    E = math.sqrt(px_mev**2 + py_mev**2 + pz_mev**2 + Mn**2)
    mom = ROOT.TLorentzVector(px_mev, py_mev, pz_mev, E)
    pos = ROOT.TVector3(x_mm, y_mm, z_mm)
    particle = ROOT.TBeamSimData()
    particle.fParticleName = "neutron"
    particle.fMomentum = mom
    particle.fPosition = pos
    particle.fPrimaryParticleID = primary_id
    particle.fIsAccepted = True
    return particle


def main() -> None:
    ap = argparse.ArgumentParser(
        description="Generate sharded tree-gun inputs for the (px,py) x pz grid."
    )
    ap.add_argument("--out-prefix", required=True,
                    help="Output path prefix, e.g. /path/to/dir/neutron_pxpy "
                         "→ emits neutron_pxpy_shard00.root, ...")
    ap.add_argument("--n-shards", type=int, default=32,
                    help="Number of output shards (default: 32)")
    ap.add_argument("--px-min", type=float, default=-200.0,
                    help="px grid minimum [MeV/c] (default: -200)")
    ap.add_argument("--px-max", type=float, default=+200.0,
                    help="px grid maximum [MeV/c] (default: +200)")
    ap.add_argument("--px-step", type=float, default=20.0,
                    help="px grid step [MeV/c] (default: 20)")
    ap.add_argument("--py-min", type=float, default=-100.0,
                    help="py grid minimum [MeV/c] (default: -100)")
    ap.add_argument("--py-max", type=float, default=+100.0,
                    help="py grid maximum [MeV/c] (default: +100)")
    ap.add_argument("--py-step", type=float, default=10.0,
                    help="py grid step [MeV/c] (default: 10)")
    ap.add_argument("--pz-list", type=str, default="550,600,627,650,700",
                    help="Comma-separated list of pz values [MeV/c] "
                         "(default: 550,600,627,650,700)")
    ap.add_argument("--n-per-bin", type=int, default=2000,
                    help="Events per (px,py,pz) bin (default: 2000)")
    ap.add_argument("--smear-sigma", type=float, default=0.1,
                    help="Gaussian smear sigma [MeV/c] applied to each "
                         "momentum component (default: 0.1)")
    ap.add_argument("--x-mm", type=float, default=0.0,
                    help="Vertex x position [mm] (default: 0)")
    ap.add_argument("--y-mm", type=float, default=0.0,
                    help="Vertex y position [mm] (default: 0)")
    ap.add_argument("--z-mm", type=float, default=0.0,
                    help="Vertex z position [mm] (default: 0)")
    ap.add_argument("--seed", type=int, default=20260513,
                    help="NumPy RNG seed for reproducibility (default: 20260513)")
    args = ap.parse_args()

    # Build grids
    px_grid = np.arange(args.px_min,
                        args.px_max + 0.5 * args.px_step,
                        args.px_step)
    py_grid = np.arange(args.py_min,
                        args.py_max + 0.5 * args.py_step,
                        args.py_step)
    pz_list = [float(s) for s in args.pz_list.split(",")]

    n_bins = len(px_grid) * len(py_grid) * len(pz_list)
    n_total_expected = n_bins * args.n_per_bin

    print(f"[gen_pxpy_grid_input] grid: "
          f"px {len(px_grid)} pts x py {len(py_grid)} pts x pz {len(pz_list)} pts "
          f"= {n_bins} bins, {args.n_per_bin} events/bin → "
          f"{n_total_expected} total events across {args.n_shards} shards")

    # Create parent directory
    out_prefix = args.out_prefix
    Path(out_prefix).parent.mkdir(parents=True, exist_ok=True)

    # TBeamSimDataArray is a typedef for std::vector<TBeamSimData>.
    # PyROOT does not expose the typedef name, so we use the template directly.
    VecT = ROOT.std.vector("TBeamSimData")

    # Open output files and create trees
    files: list[ROOT.TFile] = []
    trees: list[ROOT.TTree] = []
    arrays: list = []

    for s in range(args.n_shards):
        path = f"{out_prefix}_shard{s:02d}.root"
        f = ROOT.TFile(path, "RECREATE")
        t = ROOT.TTree("tree", "neutron (px,py)xpz grid shard")
        arr = VecT()
        # Branch on the vector directly; ROOT serialises it as the registered
        # vector<TBeamSimData> class, which the Tree gun reads via
        # fBranchInput->SetAddress(&gBeamSimDataArray).
        t.Branch("TBeamSimData", arr)
        files.append(f)
        trees.append(t)
        arrays.append(arr)

    rng = np.random.default_rng(args.seed)
    n_total = 0

    for px_nom in px_grid:
        for py_nom in py_grid:
            for pz_nom in pz_list:
                # Tiny Gaussian smear so each event is distinct (avoids
                # the sim treating identical events as duplicates)
                dpx = rng.normal(0.0, args.smear_sigma, size=args.n_per_bin)
                dpy = rng.normal(0.0, args.smear_sigma, size=args.n_per_bin)
                dpz = rng.normal(0.0, args.smear_sigma, size=args.n_per_bin)

                for k in range(args.n_per_bin):
                    shard_idx = n_total % args.n_shards
                    arr = arrays[shard_idx]
                    arr.clear()

                    particle = _make_neutron(
                        float(px_nom) + dpx[k],
                        float(py_nom) + dpy[k],
                        float(pz_nom) + dpz[k],
                        x_mm=args.x_mm,
                        y_mm=args.y_mm,
                        z_mm=args.z_mm,
                    )
                    arr.push_back(particle)
                    trees[shard_idx].Fill()
                    n_total += 1

    # Write and close all files
    for s in range(args.n_shards):
        files[s].cd()
        trees[s].Write("", ROOT.TObject.kOverwrite)
        files[s].Close()

    print(f"[gen_pxpy_grid_input] wrote {n_total} events across "
          f"{args.n_shards} shards → {out_prefix}_shard*.root")


if __name__ == "__main__":
    main()
