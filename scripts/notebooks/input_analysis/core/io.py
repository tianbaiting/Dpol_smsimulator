from __future__ import annotations

from pathlib import Path
from typing import Iterable, Iterator, List, Optional

from .schema import Event
from .paths import ypol_phi_random_dir, zpol_b_discrete_dir


def _read_lines(filepath: Path) -> Iterator[List[str]]:
    with filepath.open("r", encoding="utf-8") as f:
        _ = f.readline()
        _ = f.readline()
        for line in f:
            parts = line.split()
            if not parts:
                continue
            yield parts


def iter_ypol_phi_random(
    root: Path,
    target: str,
    energy: str,
    gamma: str,
    pols: Iterable[str],
) -> Iterator[Event]:
    for pol in pols:
        folder = ypol_phi_random_dir(root, target, energy, gamma, pol)
        if not folder.is_dir():
            print(f"[ypol] missing folder: {folder}")
            continue
        filepath = folder / "dbreak.dat"
        if not filepath.is_file():
            print(f"[ypol] missing: {filepath}")
            continue
        for parts in _read_lines(filepath):
            if len(parts) < 9:
                continue
            event_no = int(parts[0])
            pxp, pyp, pzp = map(float, parts[1:4])
            pxn, pyn, pzn = map(float, parts[4:7])
            b = float(parts[7])
            rpphi_deg = float(parts[8])
            yield Event(
                dataset="ypol",
                target=target,
                gamma=gamma,
                pol=pol,
                event_no=event_no,
                pxp=pxp,
                pyp=pyp,
                pzp=pzp,
                pxn=pxn,
                pyn=pyn,
                pzn=pzn,
                b=b,
                rpphi_deg=rpphi_deg,
            )


def iter_zpol_b_discrete(
    root: Path,
    target: str,
    energy: str,
    gamma: str,
    pols: Iterable[str],
    b_min: int,
    b_max: int,
    bmax_for_event_filter: int,
) -> Iterator[Event]:
    for pol in pols:
        folder = zpol_b_discrete_dir(root, target, energy, gamma, pol)
        if not folder.is_dir():
            print(f"[zpol] missing folder: {folder}")
            continue
        for b in range(b_min, b_max + 1):
            filepath = folder / f"dbreakb{b:02d}.dat"
            if not filepath.is_file():
                print(f"[zpol] missing: {filepath}")
                continue
            for parts in _read_lines(filepath):
                if len(parts) < 7:
                    continue
                event_no = int(parts[0])
                if event_no >= 10000.0 * b / bmax_for_event_filter:
                    continue
                pxp, pyp, pzp = map(float, parts[1:4])
                pxn, pyn, pzn = map(float, parts[4:7])
                yield Event(
                    dataset="zpol",
                    target=target,
                    gamma=gamma,
                    pol=pol,
                    event_no=event_no,
                    pxp=pxp,
                    pyp=pyp,
                    pzp=pzp,
                    pxn=pxn,
                    pyn=pyn,
                    pzn=pzn,
                    b=float(b),
                    rpphi_deg=None,
                )
