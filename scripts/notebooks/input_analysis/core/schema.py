from __future__ import annotations

from dataclasses import dataclass
from typing import Optional


@dataclass(frozen=True)
class Event:
    dataset: str
    target: str
    gamma: str
    pol: str
    event_no: int
    pxp: float
    pyp: float
    pzp: float
    pxn: float
    pyn: float
    pzn: float
    b: Optional[float] = None
    rpphi_deg: Optional[float] = None
