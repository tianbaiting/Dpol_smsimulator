#!/usr/bin/env python3
# [EN] Usage: python3 scripts/event_generator/dpol_eventgen/generate_synthetic_px_scan.py --output data/simulation/g4input/synthetic_scan/synthetic_px_scan.dat --events 20000 --xmin -150 --xmax 150 --pz 627 --seed 20260224 / [CN] 用法: python3 scripts/event_generator/dpol_eventgen/generate_synthetic_px_scan.py --output data/simulation/g4input/synthetic_scan/synthetic_px_scan.dat --events 20000 --xmin -150 --xmax 150 --pz 627 --seed 20260224

import argparse
import os
import random


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate synthetic p/n momentum scan dat file: (px,0,627) with px in [xmin,xmax]."
    )
    parser.add_argument("--output", required=True, help="Output .dat path")
    parser.add_argument("--events", type=int, default=20000, help="Number of events")
    parser.add_argument("--xmin", type=float, default=-150.0, help="Minimum px [MeV/c]")
    parser.add_argument("--xmax", type=float, default=150.0, help="Maximum px [MeV/c]")
    parser.add_argument("--pz", type=float, default=627.0, help="Fixed pz [MeV/c]")
    parser.add_argument("--seed", type=int, default=20260224, help="RNG seed")
    args = parser.parse_args()

    if args.events <= 0:
        raise ValueError("--events must be positive")
    if args.xmax <= args.xmin:
        raise ValueError("--xmax must be greater than --xmin")

    random.seed(args.seed)
    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)

    with open(args.output, "w", encoding="utf-8") as f:
        # [EN] Keep the same header/column format as existing np-atime converter input. / [CN] 保持与现有np-atime转换器输入相同的表头和列格式。
        f.write("# synthetic momentum scan for proton+neutron\n")
        f.write("# no pxp pyp pzp pxn pyn pzn\n")

        for i in range(1, args.events + 1):
            pxp = random.uniform(args.xmin, args.xmax)
            pxn = random.uniform(args.xmin, args.xmax)
            f.write(f"{i} {pxp:.6f} 0.0 {args.pz:.6f} {pxn:.6f} 0.0 {args.pz:.6f}\n")

    print(f"[generate_synthetic_px_scan] Wrote {args.events} events to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
