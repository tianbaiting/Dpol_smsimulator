from __future__ import annotations

from pathlib import Path
import os


def default_data_root() -> Path:
    env_root = os.getenv("SMSIMDIR")
    if env_root:
        candidate = Path(env_root) / "data" / "qmdrawdata" / "qmdrawdata"
        if candidate.exists():
            return candidate
    here = Path(__file__).resolve()
    repo_root = here.parents[3]
    return repo_root / "data" / "qmdrawdata" / "qmdrawdata"


def ypol_phi_random_dir(root: Path, target: str, energy: str, gamma: str, pol: str) -> Path:
    return root / "y_pol" / "phi_random" / f"d+{target}E{energy}g{gamma}{pol}"


def zpol_b_discrete_dir(root: Path, target: str, energy: str, gamma: str, pol: str) -> Path:
    return root / "z_pol" / "b_discrete" / f"d+{target}E{energy}g{gamma}{pol}"
