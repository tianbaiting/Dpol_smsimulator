#!/usr/bin/env python3
"""Stratified random subsample of an elastic dbreak0000.root.

Schema is identical to geminiout0000.root (18 branches), so the subsample
helper only differs in default filename pattern.

Usage:
    subsample_dbreak.py --input <src.root> --output <dst.root> \
                        --n-events 5000 --seed 20260428
"""
from __future__ import annotations

import argparse
import os
import random
import sys

import ROOT


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--output", required=True)
    ap.add_argument("--n-events", type=int, required=True)
    ap.add_argument("--seed", type=int, default=20260428)
    ap.add_argument(
        "--no-filter",
        action="store_true",
        help="Skip OK_PDC1 && OK_PDC2 filtering.",
    )
    args = ap.parse_args()

    if not os.path.isfile(args.input):
        print(f"[subsample] input not found: {args.input}", file=sys.stderr)
        sys.exit(1)

    src = ROOT.TFile.Open(args.input, "READ")
    if not src or src.IsZombie():
        print(f"[subsample] failed to open {args.input}", file=sys.stderr)
        sys.exit(1)

    src_tree = src.Get("tree")
    if not src_tree:
        print("[subsample] no 'tree' in input", file=sys.stderr)
        sys.exit(1)

    n_total = src_tree.GetEntries()
    print(f"[subsample] input entries: {n_total}")
    rng = random.Random(args.seed)

    if args.no_filter:
        candidates = list(range(n_total))
    else:
        n_sel = src_tree.Draw("Entry$", "OK_PDC1==1 && OK_PDC2==1", "goff")
        v1 = src_tree.GetV1()
        candidates = [int(v1[i]) for i in range(n_sel)]
        print(
            f"[subsample] candidates after filter (OK_PDC1 && OK_PDC2): {len(candidates)}"
        )

    if len(candidates) == 0:
        print("[subsample] no candidates pass filter — aborting", file=sys.stderr)
        sys.exit(2)

    n_pick = min(args.n_events, len(candidates))
    if n_pick < args.n_events:
        print(
            f"[subsample] requested {args.n_events} but only {len(candidates)} "
            f"candidates; picking {n_pick}",
            file=sys.stderr,
        )

    picked = sorted(rng.sample(candidates, n_pick))

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    dst = ROOT.TFile.Open(args.output, "RECREATE")
    if not dst or dst.IsZombie():
        print(f"[subsample] failed to create {args.output}", file=sys.stderr)
        sys.exit(1)

    src_tree.SetBranchStatus("*", 1)
    dst_tree = src_tree.CloneTree(0)
    for idx in picked:
        src_tree.GetEntry(idx)
        dst_tree.Fill()
    dst_tree.Write("", ROOT.TObject.kOverwrite)

    for key_name in ("FragParameter", "NEBULAParameter", "RunParameter"):
        obj = src.Get(key_name)
        if obj:
            dst.cd()
            obj.Write(key_name)

    dst.Close()
    src.Close()
    print(f"[subsample] wrote {n_pick} events to {args.output}")


if __name__ == "__main__":
    main()
