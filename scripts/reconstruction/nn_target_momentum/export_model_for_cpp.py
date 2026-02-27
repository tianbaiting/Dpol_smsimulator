#!/usr/bin/env python3

import argparse
import json
import os
from typing import Dict, List, Tuple

import numpy as np

try:
    import torch
except Exception as exc:  # pragma: no cover
    raise SystemExit(f"[export_model_for_cpp] PyTorch is required but not available: {exc}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Export trained PyTorch MLP to plain JSON for C++ inference."
    )
    parser.add_argument("--model-path", required=True, help="model.pt produced by train_mlp.py")
    parser.add_argument("--meta-path", required=True, help="model_meta.json produced by train_mlp.py")
    parser.add_argument("--output-json", required=True, help="Output JSON path for C++ inference")
    return parser.parse_args()


def collect_linear_layers(state_dict: Dict[str, torch.Tensor]) -> List[Tuple[int, torch.Tensor, torch.Tensor]]:
    layer_ids = set()
    for key in state_dict.keys():
        if not key.startswith("net.") or not (key.endswith(".weight") or key.endswith(".bias")):
            continue
        parts = key.split(".")
        if len(parts) != 3:
            continue
        layer_ids.add(int(parts[1]))

    layers: List[Tuple[int, torch.Tensor, torch.Tensor]] = []
    for idx in sorted(layer_ids):
        w_key = f"net.{idx}.weight"
        b_key = f"net.{idx}.bias"
        if w_key not in state_dict or b_key not in state_dict:
            continue
        weight = state_dict[w_key].detach().cpu()
        bias = state_dict[b_key].detach().cpu()
        if weight.ndim != 2 or bias.ndim != 1:
            continue
        if weight.shape[0] != bias.shape[0]:
            continue
        layers.append((idx, weight, bias))
    if not layers:
        raise RuntimeError("no linear layers found in state_dict")
    return layers


def main() -> None:
    args = parse_args()

    if not os.path.exists(args.model_path):
        raise FileNotFoundError(f"model not found: {args.model_path}")
    if not os.path.exists(args.meta_path):
        raise FileNotFoundError(f"meta not found: {args.meta_path}")

    with open(args.meta_path, "r", encoding="utf-8") as fin:
        meta = json.load(fin)

    x_mean = np.asarray(meta["x_mean"], dtype=np.float64)
    x_std = np.asarray(meta["x_std"], dtype=np.float64)
    if x_mean.shape != (6,) or x_std.shape != (6,):
        raise RuntimeError(f"x_mean/x_std shape mismatch: {x_mean.shape}, {x_std.shape}")
    x_std = np.where(np.abs(x_std) < 1e-8, 1.0, x_std)

    checkpoint = torch.load(args.model_path, map_location="cpu")
    state_dict = checkpoint["state_dict"]
    layers_raw = collect_linear_layers(state_dict)

    layers = []
    for _, weight_t, bias_t in layers_raw:
        weight = weight_t.numpy().astype(np.float64, copy=False)
        bias = bias_t.numpy().astype(np.float64, copy=False)
        layers.append(
            {
                "in_dim": int(weight.shape[1]),
                "out_dim": int(weight.shape[0]),
                "weights": weight.reshape(-1).tolist(),
                "bias": bias.tolist(),
            }
        )

    payload = {
        "format": "smsimulator_pdc_mlp_v1",
        "feature_names": meta.get("feature_names", []),
        "target_names": meta.get("target_names", []),
        "x_mean": x_mean.tolist(),
        "x_std": x_std.tolist(),
        "layers": layers,
    }

    out_dir = os.path.dirname(args.output_json)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(args.output_json, "w", encoding="utf-8") as fout:
        json.dump(payload, fout, indent=2)

    print("[export_model_for_cpp] done")
    print(f"[export_model_for_cpp] output: {args.output_json}")


if __name__ == "__main__":
    main()
