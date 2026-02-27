#!/usr/bin/env python3

import argparse
import copy
import csv
import json
import os
from dataclasses import dataclass
from typing import Dict, List, Tuple

import numpy as np

try:
    import torch
    import torch.nn as nn
    from torch.utils.data import DataLoader, TensorDataset
except Exception as exc:  # pragma: no cover
    raise SystemExit(f"[train_mlp] PyTorch is required but not available: {exc}")

FEATURE_NAMES = [
    "pdc1_x",
    "pdc1_y",
    "pdc1_z",
    "pdc2_x",
    "pdc2_y",
    "pdc2_z",
]
TARGET_NAMES = ["px_truth", "py_truth", "pz_truth"]


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


@dataclass
class SplitData:
    x_train: np.ndarray
    y_train: np.ndarray
    x_val: np.ndarray
    y_val: np.ndarray
    x_test: np.ndarray
    y_test: np.ndarray


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Train MLP for target momentum reconstruction from PDC two-point features.")
    parser.add_argument("--input-csv", help="Single dataset CSV (used with internal random split)")
    parser.add_argument("--train-csv", help="Training dataset CSV")
    parser.add_argument("--val-csv", help="Validation dataset CSV")
    parser.add_argument("--test-csv", help="Test dataset CSV")
    parser.add_argument("--output-dir", default="data/nn_target_momentum/model", help="Directory to store model and metadata")
    parser.add_argument("--seed", type=int, default=20260227)
    parser.add_argument("--epochs", type=int, default=120)
    parser.add_argument("--batch-size", type=int, default=1024)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--weight-decay", type=float, default=1e-5)
    parser.add_argument("--patience", type=int, default=12)
    parser.add_argument("--hidden-dims", default="128,128,64", help="Comma-separated hidden layer sizes")
    return parser.parse_args()


def read_csv(path: str) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    if not os.path.exists(path):
        raise FileNotFoundError(f"input csv not found: {path}")

    data = np.genfromtxt(path, delimiter=",", names=True)
    if data.size == 0:
        raise RuntimeError("input csv has no rows")
    if data.ndim == 0:
        data = np.array([data], dtype=data.dtype)

    missing = [name for name in ["event_id", *FEATURE_NAMES, *TARGET_NAMES] if name not in data.dtype.names]
    if missing:
        raise RuntimeError(f"input csv missing columns: {missing}")

    event_ids = np.asarray(data["event_id"], dtype=np.int64)
    x = np.column_stack([np.asarray(data[name], dtype=np.float64) for name in FEATURE_NAMES])
    y = np.column_stack([np.asarray(data[name], dtype=np.float64) for name in TARGET_NAMES])

    finite_mask = np.isfinite(x).all(axis=1) & np.isfinite(y).all(axis=1)
    x = x[finite_mask]
    y = y[finite_mask]
    event_ids = event_ids[finite_mask]

    if x.shape[0] < 10:
        raise RuntimeError(f"not enough valid samples after filtering: {x.shape[0]}")

    return event_ids, x, y


def split_dataset(x: np.ndarray, y: np.ndarray, seed: int) -> SplitData:
    rng = np.random.default_rng(seed)
    indices = rng.permutation(x.shape[0])
    n_train = int(x.shape[0] * 0.70)
    n_val = int(x.shape[0] * 0.15)
    n_test = x.shape[0] - n_train - n_val
    if min(n_train, n_val, n_test) <= 0:
        raise RuntimeError(
            f"dataset split invalid (train={n_train}, val={n_val}, test={n_test}); provide more samples"
        )

    idx_train = indices[:n_train]
    idx_val = indices[n_train : n_train + n_val]
    idx_test = indices[n_train + n_val :]

    return SplitData(
        x_train=x[idx_train],
        y_train=y[idx_train],
        x_val=x[idx_val],
        y_val=y[idx_val],
        x_test=x[idx_test],
        y_test=y[idx_test],
    )


def normalize_features(split: SplitData) -> Tuple[SplitData, np.ndarray, np.ndarray]:
    x_mean = split.x_train.mean(axis=0)
    x_std = split.x_train.std(axis=0)
    x_std = np.where(x_std < 1e-8, 1.0, x_std)

    def norm(v: np.ndarray) -> np.ndarray:
        return (v - x_mean) / x_std

    normalized = SplitData(
        x_train=norm(split.x_train),
        y_train=split.y_train,
        x_val=norm(split.x_val),
        y_val=split.y_val,
        x_test=norm(split.x_test),
        y_test=split.y_test,
    )
    return normalized, x_mean, x_std


def compute_metrics(pred: np.ndarray, truth: np.ndarray) -> Dict[str, Dict[str, float]]:
    err = pred - truth
    metrics: Dict[str, Dict[str, float]] = {"components": {}, "momentum_norm": {}}

    for i, key in enumerate(["px", "py", "pz"]):
        abs_err = np.abs(err[:, i])
        metrics["components"][key] = {
            "mae": float(abs_err.mean()),
            "rmse": float(np.sqrt(np.mean(err[:, i] ** 2))),
            "bias": float(err[:, i].mean()),
            "p95_abs": float(np.percentile(abs_err, 95)),
        }

    pred_norm = np.linalg.norm(pred, axis=1)
    truth_norm = np.linalg.norm(truth, axis=1)
    d_norm = pred_norm - truth_norm
    abs_d_norm = np.abs(d_norm)
    metrics["momentum_norm"] = {
        "mae": float(abs_d_norm.mean()),
        "rmse": float(np.sqrt(np.mean(d_norm ** 2))),
        "bias": float(d_norm.mean()),
        "p95_abs": float(np.percentile(abs_d_norm, 95)),
    }
    return metrics


def build_split_from_args(args: argparse.Namespace) -> Tuple[SplitData, Dict[str, str], str]:
    external_args = [args.train_csv, args.val_csv, args.test_csv]
    has_external = any(v is not None for v in external_args)
    all_external = all(v is not None for v in external_args)

    if has_external and not all_external:
        raise RuntimeError("when using external split, provide --train-csv, --val-csv, and --test-csv together")

    if all_external:
        _, x_train, y_train = read_csv(args.train_csv)
        _, x_val, y_val = read_csv(args.val_csv)
        _, x_test, y_test = read_csv(args.test_csv)
        split = SplitData(
            x_train=x_train,
            y_train=y_train,
            x_val=x_val,
            y_val=y_val,
            x_test=x_test,
            y_test=y_test,
        )
        sources = {
            "train_csv": args.train_csv,
            "val_csv": args.val_csv,
            "test_csv": args.test_csv,
        }
        return split, sources, "external_files"

    if not args.input_csv:
        raise RuntimeError("provide --input-csv for internal split, or provide full external split (--train-csv/--val-csv/--test-csv)")

    _, x, y = read_csv(args.input_csv)
    split = split_dataset(x, y, args.seed)
    sources = {"input_csv": args.input_csv}
    return split, sources, "internal_random"


def main() -> None:
    args = parse_args()
    hidden_dims = [int(x) for x in args.hidden_dims.split(",") if x.strip()]
    if not hidden_dims:
        raise RuntimeError("hidden_dims cannot be empty")

    os.makedirs(args.output_dir, exist_ok=True)

    np.random.seed(args.seed)
    torch.manual_seed(args.seed)

    split_raw, split_sources, split_mode = build_split_from_args(args)
    split_norm, x_mean, x_std = normalize_features(split_raw)

    x_train_t = torch.tensor(split_norm.x_train, dtype=torch.float32)
    y_train_t = torch.tensor(split_norm.y_train, dtype=torch.float32)
    x_val_t = torch.tensor(split_norm.x_val, dtype=torch.float32)
    y_val_t = torch.tensor(split_norm.y_val, dtype=torch.float32)
    x_test_t = torch.tensor(split_norm.x_test, dtype=torch.float32)
    y_test_t = torch.tensor(split_norm.y_test, dtype=torch.float32)

    train_loader = DataLoader(
        TensorDataset(x_train_t, y_train_t),
        batch_size=args.batch_size,
        shuffle=True,
        drop_last=False,
    )

    model = MLP(input_dim=6, hidden_dims=hidden_dims, output_dim=3)
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr, weight_decay=args.weight_decay)
    criterion = nn.MSELoss()

    best_state = None
    best_val = float("inf")
    epochs_no_improve = 0
    history: List[Tuple[int, float, float]] = []

    for epoch in range(1, args.epochs + 1):
        model.train()
        train_losses: List[float] = []
        for xb, yb in train_loader:
            optimizer.zero_grad(set_to_none=True)
            pred = model(xb)
            loss = criterion(pred, yb)
            loss.backward()
            optimizer.step()
            train_losses.append(float(loss.item()))

        model.eval()
        with torch.no_grad():
            val_pred = model(x_val_t)
            val_loss = float(criterion(val_pred, y_val_t).item())
        train_loss = float(np.mean(train_losses)) if train_losses else float("inf")
        history.append((epoch, train_loss, val_loss))

        if val_loss < best_val:
            best_val = val_loss
            best_state = copy.deepcopy(model.state_dict())
            epochs_no_improve = 0
        else:
            epochs_no_improve += 1

        if epochs_no_improve >= args.patience:
            break

    if best_state is None:
        raise RuntimeError("training failed to produce a valid model")

    model.load_state_dict(best_state)
    model.eval()
    with torch.no_grad():
        pred_val = model(x_val_t).cpu().numpy()
        pred_test = model(x_test_t).cpu().numpy()

    val_metrics = compute_metrics(pred_val, split_norm.y_val)
    test_metrics = compute_metrics(pred_test, split_norm.y_test)

    model_path = os.path.join(args.output_dir, "model.pt")
    meta_path = os.path.join(args.output_dir, "model_meta.json")
    hist_path = os.path.join(args.output_dir, "training_history.csv")

    torch.save(
        {
            "state_dict": model.state_dict(),
            "input_dim": 6,
            "output_dim": 3,
            "hidden_dims": hidden_dims,
        },
        model_path,
    )

    meta = {
        "feature_names": FEATURE_NAMES,
        "target_names": TARGET_NAMES,
        "x_mean": x_mean.tolist(),
        "x_std": x_std.tolist(),
        "seed": args.seed,
        "split_mode": split_mode,
        "split_sources": split_sources,
        "epochs_requested": args.epochs,
        "best_val_loss": best_val,
        "split_counts": {
            "train": int(split_norm.x_train.shape[0]),
            "val": int(split_norm.x_val.shape[0]),
            "test": int(split_norm.x_test.shape[0]),
        },
        "hyper_parameters": {
            "hidden_dims": hidden_dims,
            "batch_size": args.batch_size,
            "lr": args.lr,
            "weight_decay": args.weight_decay,
            "patience": args.patience,
        },
        "val_metrics": val_metrics,
        "test_metrics": test_metrics,
    }

    with open(meta_path, "w", encoding="utf-8") as fout:
        json.dump(meta, fout, indent=2)

    with open(hist_path, "w", encoding="utf-8", newline="") as fout:
        writer = csv.writer(fout)
        writer.writerow(["epoch", "train_loss", "val_loss"])
        writer.writerows(history)

    print("[train_mlp] Training complete")
    print(f"[train_mlp] model: {model_path}")
    print(f"[train_mlp] meta:  {meta_path}")
    print(f"[train_mlp] split_mode: {split_mode}")
    print(f"[train_mlp] best_val_loss: {best_val:.8f}")
    print("[train_mlp] val momentum-norm RMSE: " f"{val_metrics['momentum_norm']['rmse']:.4f} MeV/c")
    print("[train_mlp] test momentum-norm RMSE: " f"{test_metrics['momentum_norm']['rmse']:.4f} MeV/c")


if __name__ == "__main__":
    main()
