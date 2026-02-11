from __future__ import annotations

from pathlib import Path
from typing import Iterable, List, Tuple

import plotly.graph_objects as go
from plotly.subplots import make_subplots

Point3D = Tuple[float, float, float]


def _split_points(points: Iterable[Point3D]) -> Tuple[List[float], List[float], List[float]]:
    pts = list(points)
    if not pts:
        return [], [], []
    xs, ys, zs = zip(*pts)
    return list(xs), list(ys), list(zs)


def plot_3d_proton_neutron(
    proton_points: Iterable[Point3D],
    neutron_points: Iterable[Point3D],
    title: str,
    output_html: Path,
) -> None:
    px, py, pz = _split_points(proton_points)
    nx, ny, nz = _split_points(neutron_points)

    fig = make_subplots(
        rows=1,
        cols=2,
        subplot_titles=["Proton", "Neutron"],
        specs=[[{"type": "scatter3d"}, {"type": "scatter3d"}]],
        horizontal_spacing=0.05,
    )

    fig.add_trace(
        go.Scatter3d(
            x=px,
            y=py,
            z=pz,
            mode="markers",
            marker=dict(size=2, opacity=0.6, color="#1f77b4"),
            name="Proton",
        ),
        row=1,
        col=1,
    )

    fig.add_trace(
        go.Scatter3d(
            x=nx,
            y=ny,
            z=nz,
            mode="markers",
            marker=dict(size=2, opacity=0.6, color="#d62728"),
            name="Neutron",
        ),
        row=1,
        col=2,
    )

    fig.update_layout(
        title=title,
        showlegend=False,
        width=1100,
        height=520,
    )

    fig.update_scenes(xaxis_title="Px (MeV/c)", yaxis_title="Py (MeV/c)", zaxis_title="Pz (MeV/c)")
    output_html.parent.mkdir(parents=True, exist_ok=True)
    fig.write_html(str(output_html))


def plot_3d_delta(
    delta_points: Iterable[Point3D],
    title: str,
    output_html: Path,
) -> None:
    dx, dy, dz = _split_points(delta_points)

    fig = go.Figure(
        data=[
            go.Scatter3d(
                x=dx,
                y=dy,
                z=dz,
                mode="markers",
                marker=dict(size=2, opacity=0.6, color="#2ca02c"),
                name="Delta",
            )
        ]
    )

    fig.update_layout(
        title=title,
        width=700,
        height=600,
        scene=dict(
            xaxis_title="ΔPx (MeV/c)",
            yaxis_title="ΔPy (MeV/c)",
            zaxis_title="ΔPz (MeV/c)",
        ),
    )

    output_html.parent.mkdir(parents=True, exist_ok=True)
    fig.write_html(str(output_html))
