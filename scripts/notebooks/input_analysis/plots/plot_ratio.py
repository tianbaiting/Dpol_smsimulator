from __future__ import annotations

from pathlib import Path
from typing import Dict, Iterable, List, Tuple

import math
import matplotlib.pyplot as plt

from .style import apply_mpl_style


RatioMap = Dict[str, List[Tuple[float, float]]]


def plot_ratio_vs_gamma(
    ratio_by_cut: RatioMap,
    title: str,
    output_png: Path,
) -> None:
    apply_mpl_style()
    fig, ax = plt.subplots(figsize=(6.8, 5.2))

    styles = ["-", "--", "-.", ":"]
    markers = ["o", "s", "^", "D"]
    for idx, (cut, pairs) in enumerate(ratio_by_cut.items()):
        xs: List[float] = []
        ys: List[float] = []
        for x, y in pairs:
            if y is None or math.isnan(y) or math.isinf(y):
                continue
            xs.append(x)
            ys.append(y)
        if not xs:
            continue
        ax.plot(
            xs,
            ys,
            linestyle=styles[idx % len(styles)],
            marker=markers[idx % len(markers)],
            label=cut,
        )

    ax.set_title(title)
    ax.set_xlabel("gamma")
    ax.set_ylabel("ratio = N(pxp-pxn>0) / N(pxp-pxn<0)")
    ax.grid(False)
    ax.legend(frameon=False)
    fig.tight_layout()
    output_png.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_png, bbox_inches="tight")
    plt.close(fig)
