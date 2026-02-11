from __future__ import annotations

from pathlib import Path
from typing import Iterable

import matplotlib.pyplot as plt

from .style import apply_mpl_style


def plot_pxn_pxp_corr(
    pxp_values: Iterable[float],
    pxn_values: Iterable[float],
    title: str,
    output_png: Path,
    bins: int = 80,
) -> None:
    apply_mpl_style()
    fig, ax = plt.subplots(figsize=(6.5, 5.5))
    h = ax.hist2d(pxp_values, pxn_values, bins=bins, cmap="viridis")
    fig.colorbar(h[3], ax=ax, label="Counts")
    ax.set_title(title)
    ax.set_xlabel("Pxp (MeV/c)")
    ax.set_ylabel("Pxn (MeV/c)")
    ax.grid(False)
    fig.tight_layout()
    output_png.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_png, bbox_inches="tight")
    plt.close(fig)


def plot_pxn_minus_pxp(
    pxn_minus_pxp: Iterable[float],
    title: str,
    output_png: Path,
    bins: int = 60,
) -> None:
    apply_mpl_style()
    fig, ax = plt.subplots(figsize=(6.5, 5))
    ax.hist(pxn_minus_pxp, bins=bins, histtype="step", color="#1f77b4")
    ax.axvline(0.0, color="#555555", linewidth=1.0, linestyle="--")
    ax.set_title(title)
    ax.set_xlabel("Pxn - Pxp (MeV/c)")
    ax.set_ylabel("Counts")
    ax.grid(False)
    fig.tight_layout()
    output_png.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_png, bbox_inches="tight")
    plt.close(fig)


def plot_pxp_pxn_corr_raw(
    pxp_values: Iterable[float],
    pxn_values: Iterable[float],
    title: str,
    output_png: Path,
    bins: int = 80,
) -> None:
    apply_mpl_style()
    fig, ax = plt.subplots(figsize=(6.5, 5.5))
    h = ax.hist2d(pxp_values, pxn_values, bins=bins, cmap="plasma")
    fig.colorbar(h[3], ax=ax, label="Counts")
    ax.set_title(title)
    ax.set_xlabel("Pxp (MeV/c)")
    ax.set_ylabel("Pxn (MeV/c)")
    ax.grid(False)
    fig.tight_layout()
    output_png.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_png, bbox_inches="tight")
    plt.close(fig)


def plot_phi_corr_raw(
    phi_p: Iterable[float],
    phi_n: Iterable[float],
    title: str,
    output_png: Path,
    bins: int = 80,
) -> None:
    apply_mpl_style()
    fig, ax = plt.subplots(figsize=(6.5, 5.5))
    h = ax.hist2d(phi_p, phi_n, bins=bins, cmap="magma")
    fig.colorbar(h[3], ax=ax, label="Counts")
    ax.set_title(title)
    ax.set_xlabel("phi_p = atan2(py_p, px_p) (rad)")
    ax.set_ylabel("phi_n = atan2(py_n, px_n) (rad)")
    ax.grid(False)
    fig.tight_layout()
    output_png.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_png, bbox_inches="tight")
    plt.close(fig)
