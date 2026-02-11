from __future__ import annotations

from dataclasses import dataclass
import math

from .schema import Event


@dataclass(frozen=True)
class Derived:
    phi: float
    sum_xy2: float
    sum_px: float
    sum_py: float
    sum_pz: float
    diff_pz: float
    rot_pxp: float
    rot_pyp: float
    rot_pzp: float
    rot_pxn: float
    rot_pyn: float
    rot_pzn: float


def compute_derived(event: Event) -> Derived:
    sum_px = event.pxp + event.pxn
    sum_py = event.pyp + event.pyn
    sum_pz = event.pzp + event.pzn
    diff_pz = event.pzp - event.pzn
    sum_xy2 = sum_px * sum_px + sum_py * sum_py
    phi = math.atan2(sum_py, sum_px)

    angle = -phi
    cos_a = math.cos(angle)
    sin_a = math.sin(angle)

    rot_pxp = cos_a * event.pxp - sin_a * event.pyp
    rot_pyp = sin_a * event.pxp + cos_a * event.pyp
    rot_pzp = event.pzp

    rot_pxn = cos_a * event.pxn - sin_a * event.pyn
    rot_pyn = sin_a * event.pxn + cos_a * event.pyn
    rot_pzn = event.pzn

    return Derived(
        phi=phi,
        sum_xy2=sum_xy2,
        sum_px=sum_px,
        sum_py=sum_py,
        sum_pz=sum_pz,
        diff_pz=diff_pz,
        rot_pxp=rot_pxp,
        rot_pyp=rot_pyp,
        rot_pzp=rot_pzp,
        rot_pxn=rot_pxn,
        rot_pyn=rot_pyn,
        rot_pzn=rot_pzn,
    )
