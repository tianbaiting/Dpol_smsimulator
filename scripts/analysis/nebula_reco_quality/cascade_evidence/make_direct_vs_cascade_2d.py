"""2D heatmaps of ε_direct and ε_cascade across (px_tgt, py_tgt).

Reads `direct_vs_cascade_grid.parquet` produced by classify_direct_vs_cascade.py.

Layout (3 panels, shared colormap [0,0.6]):
  - ε_geom       — geometric acceptance (truth ray intersects Neut AABB)
  - ε_direct     — truth neutron deposited Q at the predicted bar (±150 mm)
  - ε_cascade    — Neut hit, but no hit at predicted bar (upstream-cascade)

Plus a fourth panel:
  - cascade_fraction = ε_cascade / (ε_direct + ε_cascade)
    (only where there are hits; gives where cascade dominates the signal)
"""
from __future__ import annotations
import argparse
from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def pivot(df, val_col, px_grid, py_grid):
    M = np.full((len(py_grid), len(px_grid)), np.nan)
    for _, r in df.iterrows():
        ix = np.searchsorted(px_grid, r.px)
        iy = np.searchsorted(py_grid, r.py)
        if 0 <= ix < len(px_grid) and 0 <= iy < len(py_grid):
            if px_grid[ix] == r.px and py_grid[iy] == r.py:
                M[iy, ix] = r[val_col]
    return M


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--grid", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    df = pd.read_parquet(args.grid)
    px_grid = np.array(sorted(df.px.unique()))
    py_grid = np.array(sorted(df.py.unique()))

    eg = pivot(df, "eps_geom", px_grid, py_grid)
    ed = pivot(df, "eps_direct", px_grid, py_grid)
    ec = pivot(df, "eps_cascade", px_grid, py_grid)
    eb = ed + ec  # total Neut-hit event fraction
    with np.errstate(divide="ignore", invalid="ignore"):
        cfrac = np.where(eb > 0.01, ec / eb, np.nan)

    extent = [px_grid.min() - 10, px_grid.max() + 10,
              py_grid.min() - 5, py_grid.max() + 5]

    fig, axes = plt.subplots(2, 2, figsize=(11, 8.6))

    panels = [
        (axes[0, 0], eg,    "ε_geom  (truth ray hits Neut AABB)",   0, 1, "viridis"),
        (axes[0, 1], ed,    "ε_direct  (Neut hit at predicted bar)", 0, 0.6, "viridis"),
        (axes[1, 0], ec,    "ε_cascade  (hit but NOT at predicted bar)", 0, 0.2, "magma"),
        (axes[1, 1], cfrac, "cascade fraction = ε_cascade / (ε_direct+ε_cascade)", 0, 1, "magma"),
    ]
    for ax, M, title, vmin, vmax, cmap in panels:
        im = ax.imshow(M, origin="lower", extent=extent,
                       aspect="auto", vmin=vmin, vmax=vmax, cmap=cmap,
                       interpolation="nearest")
        ax.set_xlabel(r"$p_x^\mathrm{tgt}$  [MeV/c]")
        ax.set_ylabel(r"$p_y^\mathrm{tgt}$  [MeV/c]")
        ax.set_title(title, fontsize=10)
        plt.colorbar(im, ax=ax, fraction=0.046, pad=0.04)
        # Mark ε_geom > 0 region boundary as a contour on direct/cascade plots
        if M is ed or M is ec or M is cfrac:
            try:
                ax.contour(px_grid, py_grid, eg, levels=[0.5], colors="white", linewidths=0.8)
            except Exception:
                pass

    fig.suptitle("NEBULA acceptance split: direct neutron vs cascade secondary  "
                 "(target-frame, py pooled per 10 MeV/c)", fontsize=12)
    fig.tight_layout(rect=[0, 0, 1, 0.96])
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.out)
    plt.close(fig)
    print(f"[fig] wrote {args.out}")


if __name__ == "__main__":
    main()
