"""Generate per-shard sim_deuteron mac files for the (px,py) scan.

Each shard input root (from gen_pxpy_grid_input.py) gets a dedicated mac.
The macs disable target and load 3deg_1.15T.mac for full SAMURAI/NEBULA
geometry with the 1.15T dipole field.

Output paths in the macs are remote-machine absolute paths (labenpg's
/data/tian tree). The macros are generated locally and rsynced to labenpg.
"""
from __future__ import annotations
import argparse
from pathlib import Path


MAC_TEMPLATE = """\
/control/execute {smsim}/configs/simulation/geometry/3deg_1.15T.mac
/samurai/geometry/Target/SetTarget false
/samurai/geometry/Update
/action/file/OverWrite y
/action/file/SaveDirectory {save_dir}/
/action/file/RunName {run_name}
/action/gun/Type Tree
/action/gun/tree/InputFileName {input_root}
/action/gun/tree/TreeName tree
/action/gun/tree/beamOn {n_events}
exit
"""


def main():
    ap = argparse.ArgumentParser(
        description="Emit per-shard sim_deuteron macros for the (px,py) acceptance scan."
    )
    ap.add_argument("--smsim", default="/data/tian/workspace/dpol/smsimulator5.5",
                    help="SMSIMDIR on the simulation host (default: labenpg path)")
    ap.add_argument("--input-prefix", required=True,
                    help="Shard prefix matching gen_pxpy_grid_input --out-prefix, "
                         "e.g. /data/tian/.../inputs/neutron_pxpy")
    ap.add_argument("--n-shards", type=int, default=32)
    ap.add_argument("--save-dir", required=True,
                    help="Where sim_deuteron writes g4output (on the sim host)")
    ap.add_argument("--out-dir", required=True,
                    help="Local dir for the generated .mac files")
    ap.add_argument("--n-events-per-shard", type=int, default=137813,
                    help="ceil(total_events / n_shards). Tree gun consumes "
                         "exactly this many entries per job. Slightly larger "
                         "than the shard size is OK — gun stops at EOF.")
    args = ap.parse_args()

    out = Path(args.out_dir); out.mkdir(parents=True, exist_ok=True)
    for j in range(args.n_shards):
        input_root = f"{args.input_prefix}_shard{j:02d}.root"
        run_name = f"pxpy_scan_shard{j:02d}"
        mac = MAC_TEMPLATE.format(
            smsim=args.smsim,
            save_dir=args.save_dir,
            run_name=run_name,
            input_root=input_root,
            n_events=args.n_events_per_shard,
        )
        (out / f"{run_name}.mac").write_text(mac)
    print(f"[gen_scan_macros] wrote {args.n_shards} mac files under {out}")


if __name__ == "__main__":
    main()
