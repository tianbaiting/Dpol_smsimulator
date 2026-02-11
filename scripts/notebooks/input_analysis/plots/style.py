from __future__ import annotations

import matplotlib.pyplot as plt


def apply_mpl_style() -> None:
    plt.rcParams.update({
        "font.family": "sans-serif",
        "font.sans-serif": ["Arial", "Helvetica", "DejaVu Sans"],
        "axes.labelsize": 12,
        "xtick.labelsize": 10,
        "ytick.labelsize": 10,
        "legend.fontsize": 10,
        "axes.titlesize": 13,
        "axes.linewidth": 1.0,
        "lines.linewidth": 1.4,
        "xtick.major.size": 4,
        "xtick.major.width": 1.0,
        "ytick.major.size": 4,
        "ytick.major.width": 1.0,
        "figure.dpi": 150,
    })
