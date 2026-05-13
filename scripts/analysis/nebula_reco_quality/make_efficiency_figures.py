"""Part B figures: 2D heatmaps + 1D slices for ε_geom, ε_det, ε_reco, conditionals."""
from __future__ import annotations
import argparse
from pathlib import Path
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def heatmap(ax, df, col, title):
    pivot = df.pivot(index="py", columns="px", values=col).sort_index()
    im = ax.imshow(pivot.values, origin="lower", aspect="auto",
                   extent=[pivot.columns.min(), pivot.columns.max(),
                           pivot.index.min(), pivot.index.max()],
                   vmin=0, vmax=1, cmap="viridis")
    ax.set_xlabel("px [MeV/c]"); ax.set_ylabel("py [MeV/c]"); ax.set_title(title)
    return im


def slice_py0(df, cols, labels, out: Path, title: str):
    sub = df[df.py == 0].sort_values("px")
    fig, ax = plt.subplots(figsize=(7, 4.5))
    for c, l in zip(cols, labels):
        ax.plot(sub.px, sub[c], marker="o", label=l)
    ax.set_xlabel("px [MeV/c]  (py=0)"); ax.set_ylabel("efficiency")
    ax.set_ylim(-0.02, 1.05); ax.grid(alpha=0.3); ax.legend()
    ax.set_title(title)
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--efficiency", required=True)
    ap.add_argument("--out-dir", required=True)
    args = ap.parse_args()

    df = pd.read_parquet(args.efficiency)
    # Drop rows where eps_geom is NaN (edge bins outside the scan grid)
    df = df.dropna(subset=["eps_geom"])
    out = Path(args.out_dir); out.mkdir(parents=True, exist_ok=True)

    # 2x2 heatmap
    fig, axes = plt.subplots(2, 2, figsize=(11, 8))
    im = None
    for ax, col, title in zip(axes.flat,
                              ["eps_geom", "eps_det", "eps_reco", "eps_reco_given_det"],
                              [r"$\varepsilon_\mathrm{geom}$",
                               r"$\varepsilon_\mathrm{det}$",
                               r"$\varepsilon_\mathrm{reco}$",
                               r"$\varepsilon_\mathrm{reco}\,|\,\mathrm{det}$"]):
        im = heatmap(ax, df, col, title)
    fig.colorbar(im, ax=axes.ravel().tolist(), shrink=0.7)
    fig.savefig(out / "fig_B1_efficiency_2D.pdf", dpi=120)
    plt.close(fig)

    slice_py0(df, ["eps_geom", "eps_det", "eps_reco"],
              [r"$\varepsilon_\mathrm{geom}$",
               r"$\varepsilon_\mathrm{det}$",
               r"$\varepsilon_\mathrm{reco}$"],
              out / "fig_B2_efficiency_slice_py0.pdf",
              title="Efficiency vs px (py=0 slice)")

    slice_py0(df, ["eps_det_given_geom", "eps_reco_given_det"],
              [r"$\varepsilon_\mathrm{det}\,|\,\mathrm{geom}$",
               r"$\varepsilon_\mathrm{reco}\,|\,\mathrm{det}$"],
              out / "fig_B3_efficiency_conditional_py0.pdf",
              title="Conditional efficiency vs px (py=0 slice)")

    print(f"[make_efficiency_figures] 3 figures -> {out}")


if __name__ == "__main__":
    main()
