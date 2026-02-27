#!/usr/bin/env python3

import argparse
import csv
import json
import os
from typing import Dict, List, Tuple

import numpy as np

try:
    import torch
    import torch.nn as nn
except Exception as exc:  # pragma: no cover
    raise SystemExit(f"[infer_mlp] PyTorch is required but not available: {exc}")

FEATURE_NAMES = [
    "pdc1_x",
    "pdc1_y",
    "pdc1_z",
    "pdc2_x",
    "pdc2_y",
    "pdc2_z",
]
TRUTH_NAMES = ["px_truth", "py_truth", "pz_truth"]


class MLP(nn.Module):
    def __init__(self, input_dim: int, hidden_dims: List[int], output_dim: int) -> None:
        super().__init__()
        layers: List[nn.Module] = []
        prev = input_dim
        for h in hidden_dims:
            layers.append(nn.Linear(prev, h))
            layers.append(nn.ReLU())
            prev = h
        layers.append(nn.Linear(prev, output_dim))
        self.net = nn.Sequential(*layers)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.net(x)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run MLP inference for target momentum reconstruction.")
    parser.add_argument("--input-csv", required=True, help="Dataset CSV from build_dataset.C")
    parser.add_argument("--model-path", required=True, help="model.pt from train_mlp.py")
    parser.add_argument("--meta-path", required=True, help="model_meta.json from train_mlp.py")
    parser.add_argument("--output-csv", required=True, help="Prediction output CSV")
    return parser.parse_args()


def read_csv(path: str) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    if not os.path.exists(path):
        raise FileNotFoundError(f"input csv not found: {path}")

    data = np.genfromtxt(path, delimiter=",", names=True)
    if data.size == 0:
        raise RuntimeError("input csv has no rows")
    if data.ndim == 0:
        data = np.array([data], dtype=data.dtype)

    required = ["event_id", *FEATURE_NAMES]
    missing = [name for name in required if name not in data.dtype.names]
    if missing:
        raise RuntimeError(f"input csv missing columns: {missing}")

    event_ids = np.asarray(data["event_id"], dtype=np.int64)
    x = np.column_stack([np.asarray(data[name], dtype=np.float64) for name in FEATURE_NAMES])

    truth_available = all(name in data.dtype.names for name in TRUTH_NAMES)
    if truth_available:
        y = np.column_stack([np.asarray(data[name], dtype=np.float64) for name in TRUTH_NAMES])
    else:
        y = np.full((x.shape[0], 3), np.nan, dtype=np.float64)

    finite_mask = np.isfinite(x).all(axis=1)
    event_ids = event_ids[finite_mask]
    x = x[finite_mask]
    y = y[finite_mask]

    return event_ids, x, y


def main() -> None:
    args = parse_args()

    with open(args.meta_path, "r", encoding="utf-8") as fin:
        meta: Dict[str, object] = json.load(fin)

    x_mean = np.asarray(meta["x_mean"], dtype=np.float64)
    x_std = np.asarray(meta["x_std"], dtype=np.float64)
    x_std = np.where(x_std < 1e-8, 1.0, x_std)

    checkpoint = torch.load(args.model_path, map_location="cpu")
    hidden_dims = [int(v) for v in checkpoint["hidden_dims"]]
    model = MLP(
        input_dim=int(checkpoint["input_dim"]),
        hidden_dims=hidden_dims,
        output_dim=int(checkpoint["output_dim"]),
    )
    model.load_state_dict(checkpoint["state_dict"])
    model.eval()

    event_ids, x, y = read_csv(args.input_csv)
    x_norm = (x - x_mean) / x_std

    with torch.no_grad():
        pred = model(torch.tensor(x_norm, dtype=torch.float32)).cpu().numpy().astype(np.float64)

    reco_p = np.linalg.norm(pred, axis=1)
    truth_p = np.linalg.norm(y, axis=1)

    out_dir = os.path.dirname(args.output_csv)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    with open(args.output_csv, "w", encoding="utf-8", newline="") as fout:
        writer = csv.writer(fout)
        writer.writerow(
            [
                "event_id",
                *FEATURE_NAMES,
                "reco_px",
                "reco_py",
                "reco_pz",
                "reco_p",
                "truth_px",
                "truth_py",
                "truth_pz",
                "truth_p",
            ]
        )
        for i in range(event_ids.shape[0]):
            writer.writerow(
                [
                    int(event_ids[i]),
                    *x[i, :].tolist(),
                    float(pred[i, 0]),
                    float(pred[i, 1]),
                    float(pred[i, 2]),
                    float(reco_p[i]),
                    float(y[i, 0]),
                    float(y[i, 1]),
                    float(y[i, 2]),
                    float(truth_p[i]),
                ]
            )

    print("[infer_mlp] Inference complete")
    print(f"[infer_mlp] samples: {event_ids.shape[0]}")
    print(f"[infer_mlp] output:  {args.output_csv}")


if __name__ == "__main__":
    main()
