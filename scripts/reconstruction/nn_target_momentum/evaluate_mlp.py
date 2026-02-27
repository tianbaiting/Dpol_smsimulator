#!/usr/bin/env python3

import argparse
import json
import os
from typing import Dict

import numpy as np

COMPONENTS = ["px", "py", "pz"]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Evaluate NN target-momentum reconstruction metrics.")
    parser.add_argument("--pred-csv", required=True, help="Prediction CSV from infer_mlp.py")
    parser.add_argument("--output-json", required=True, help="Where to write metrics JSON")
    return parser.parse_args()


def compute_component_metrics(pred: np.ndarray, truth: np.ndarray) -> Dict[str, float]:
    err = pred - truth
    abs_err = np.abs(err)
    return {
        "mae": float(abs_err.mean()),
        "rmse": float(np.sqrt(np.mean(err**2))),
        "bias": float(err.mean()),
        "p50_abs": float(np.percentile(abs_err, 50)),
        "p90_abs": float(np.percentile(abs_err, 90)),
        "p95_abs": float(np.percentile(abs_err, 95)),
    }


def main() -> None:
    args = parse_args()

    if not os.path.exists(args.pred_csv):
        raise FileNotFoundError(f"prediction csv not found: {args.pred_csv}")

    data = np.genfromtxt(args.pred_csv, delimiter=",", names=True)
    if data.size == 0:
        raise RuntimeError("prediction csv has no rows")
    if data.ndim == 0:
        data = np.array([data], dtype=data.dtype)

    required = [
        "reco_px",
        "reco_py",
        "reco_pz",
        "truth_px",
        "truth_py",
        "truth_pz",
        "reco_p",
        "truth_p",
    ]
    missing = [name for name in required if name not in data.dtype.names]
    if missing:
        raise RuntimeError(f"prediction csv missing columns: {missing}")

    pred = np.column_stack(
        [
            np.asarray(data["reco_px"], dtype=np.float64),
            np.asarray(data["reco_py"], dtype=np.float64),
            np.asarray(data["reco_pz"], dtype=np.float64),
        ]
    )
    truth = np.column_stack(
        [
            np.asarray(data["truth_px"], dtype=np.float64),
            np.asarray(data["truth_py"], dtype=np.float64),
            np.asarray(data["truth_pz"], dtype=np.float64),
        ]
    )

    finite_mask = np.isfinite(pred).all(axis=1) & np.isfinite(truth).all(axis=1)
    finite_mask &= np.isfinite(np.asarray(data["reco_p"], dtype=np.float64))
    finite_mask &= np.isfinite(np.asarray(data["truth_p"], dtype=np.float64))
    pred = pred[finite_mask]
    truth = truth[finite_mask]

    if pred.shape[0] == 0:
        raise RuntimeError("no finite rows available for evaluation")

    metrics: Dict[str, object] = {
        "sample_count": int(pred.shape[0]),
        "components": {},
        "momentum_norm": {},
    }

    for i, name in enumerate(COMPONENTS):
        metrics["components"][name] = compute_component_metrics(pred[:, i], truth[:, i])

    pred_norm = np.linalg.norm(pred, axis=1)
    truth_norm = np.linalg.norm(truth, axis=1)
    metrics["momentum_norm"] = compute_component_metrics(pred_norm, truth_norm)

    out_dir = os.path.dirname(args.output_json)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)

    with open(args.output_json, "w", encoding="utf-8") as fout:
        json.dump(metrics, fout, indent=2)

    print("[evaluate_mlp] Evaluation complete")
    print(f"[evaluate_mlp] samples: {metrics['sample_count']}")
    print(f"[evaluate_mlp] |p| RMSE: {metrics['momentum_norm']['rmse']:.4f} MeV/c")
    print(f"[evaluate_mlp] output:  {args.output_json}")


if __name__ == "__main__":
    main()
