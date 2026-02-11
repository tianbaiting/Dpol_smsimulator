from __future__ import annotations

import math
import random
from typing import List, Tuple

from .schema import Event
from .features import Derived

Point3D = Tuple[float, float, float]


class ReservoirSampler:
    def __init__(self, max_points: int, rng: random.Random) -> None:
        self.max_points = max_points
        self.rng = rng
        self.count = 0
        self.data: List[Point3D] = []

    def add(self, point: Point3D) -> None:
        self.count += 1
        if self.max_points <= 0:
            return
        if len(self.data) < self.max_points:
            self.data.append(point)
            return
        idx = self.rng.randint(0, self.count - 1)
        if idx < self.max_points:
            self.data[idx] = point


class Accumulator:
    def __init__(self, max_points_3d: int, rng: random.Random) -> None:
        self.count = 0
        self.proton_3d = ReservoirSampler(max_points_3d, rng)
        self.neutron_3d = ReservoirSampler(max_points_3d, rng)
        self.delta_3d = ReservoirSampler(max_points_3d, rng)
        self.pxp_raw: List[float] = []
        self.pxn_raw: List[float] = []
        self.phi_p_raw: List[float] = []
        self.phi_n_raw: List[float] = []
        self.pxp_rot: List[float] = []
        self.pxn_rot: List[float] = []
        self.pxn_minus_pxp: List[float] = []

    def add(self, event: Event, d: Derived) -> None:
        self.count += 1
        self.proton_3d.add((event.pxp, event.pyp, event.pzp))
        self.neutron_3d.add((event.pxn, event.pyn, event.pzn))
        self.delta_3d.add((event.pxp - event.pxn, event.pyp - event.pyn, event.pzp - event.pzn))
        self.pxp_raw.append(event.pxp)
        self.pxn_raw.append(event.pxn)
        self.phi_p_raw.append(math.atan2(event.pyp, event.pxp))
        self.phi_n_raw.append(math.atan2(event.pyn, event.pxn))
        self.pxp_rot.append(d.rot_pxp)
        self.pxn_rot.append(d.rot_pxn)
        self.pxn_minus_pxp.append(d.rot_pxn - d.rot_pxp)
