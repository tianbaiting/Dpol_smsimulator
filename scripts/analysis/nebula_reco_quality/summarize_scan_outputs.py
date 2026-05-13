"""Concatenate per-shard summary roots into one parquet for local analysis.

Each input root has a tree 'summary' with truth + NEBULA reco columns.
Output: one parquet matching the union of all shards (~4.41M rows).
"""
from __future__ import annotations
import argparse, glob
from pathlib import Path
import pandas as pd
import uproot


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--summary-glob", required=True,
                    help="e.g. /path/to/summary/pxpy_scan_shard*.root")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    files = sorted(glob.glob(args.summary_glob))
    if not files:
        raise SystemExit(f"no files match {args.summary_glob}")
    dfs = []
    for f in files:
        with uproot.open(f) as fh:
            t = fh["summary"]
            dfs.append(t.arrays(library="pd"))
    df = pd.concat(dfs, ignore_index=True)
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    df.to_parquet(args.out, index=False)
    print(f"[summarize] {len(df)} rows from {len(files)} shards -> {args.out}")


if __name__ == "__main__":
    main()
