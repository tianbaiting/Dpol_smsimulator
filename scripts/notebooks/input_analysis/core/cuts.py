from __future__ import annotations

import math
from typing import Callable, Dict

from .schema import Event
from .features import Derived

CutFn = Callable[[Event, Derived], bool]


def ypol_cut_loose(event: Event, d: Derived) -> bool:
    return abs(event.pyp - event.pyn) < 150 and d.sum_xy2 > 2500


def ypol_cut_mid(event: Event, d: Derived) -> bool:
    return ypol_cut_loose(event, d) and (d.rot_pxp + d.rot_pxn) < 200


def ypol_cut_tight(event: Event, d: Derived) -> bool:
    return ypol_cut_mid(event, d) and (math.pi - abs(d.phi)) < 0.2


def zpol_cut_loose(event: Event, d: Derived) -> bool:
    return (event.pzp + event.pzn) > 1150 and abs(event.pzp - event.pzn) < 150


def zpol_cut_mid(event: Event, d: Derived) -> bool:
    return zpol_cut_loose(event, d) and (event.pxp + event.pxn) < 200 and d.sum_xy2 > 2500


def zpol_cut_tight(event: Event, d: Derived) -> bool:
    return zpol_cut_mid(event, d) and (math.pi - abs(d.phi)) < 0.5


CUTS: Dict[str, Dict[str, CutFn]] = {
    "ypol": {
        "loose": ypol_cut_loose,
        "mid": ypol_cut_mid,
        "tight": ypol_cut_tight,
    },
    "zpol": {
        "loose": zpol_cut_loose,
        "mid": zpol_cut_mid,
        "tight": zpol_cut_tight,
    },
}
