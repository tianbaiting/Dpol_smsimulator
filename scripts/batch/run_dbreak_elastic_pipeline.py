#!/usr/bin/env python3
"""Run dbreak elastic data (ypol+zpol Sn112/Sn124) through GenInputRoot
and sim_deuteron on remote spana03.

Spec: docs/superpowers/specs/2026-04-30-dbreak-elastic-g4-pipeline-design.md
"""

import argparse
import json
import shlex
import subprocess
import sys
from pathlib import Path
from typing import Sequence

CONFIG = {
    "local_smsim_dir":   "/home/tian/workspace/dpol/smsimulator5.5",
    "remote_host":       "spana03",
    "remote_smsim_dir":  "/home/tbt/workspace/Dpol_smsimulator",
    "remote_git_remote": "origin",
    "remote_git_branch": "main",
    "remote_build_cmd":  "bash build.sh",

    "isotopes":          ["Sn112", "Sn124"],
    "gammas":            ["g050", "g060", "g070", "g080"],
    "energy":            "E190",
    "ypol_directions":   ["ynp", "ypn"],
    "zpol_directions":   ["znp", "zpn"],

    "geninput_bin":      "build/bin/GenInputRoot_qmdrawdata",
    "cut_unphysical":    "on",
    "cut_axis_limit":    150.0,
    "randomize_ypol":    "off",
    "randomize_zpol":    "on",
    "rotation_seed":     0,

    "geom_mac":          "configs/simulation/geometry/3deg_1.15T.mac",
    "g4_jobs":           8,
    "tag":               "elastic",

    "ypol_source_dir":   "data/qmdrawdata/qmdrawdata/20260413ypol",
    "ypol_link_target":  "data/qmdrawdata/qmdrawdata/y_pol/phi_random",
    "zpol_input_dir":    "data/qmdrawdata/qmdrawdata/z_pol/b_discrete",
    "g4input_base":      "data/simulation/g4input",
    "g4output_base":     "data/simulation/g4output",

    "state_dir":         "logs/dbreak_elastic_pipeline",
    "macro_dir":         "scripts/simulation/_macros",

    "batch_script":      "scripts/simulation/run_g4input_batch_parallel.sh",
}


def log(level: str, msg: str) -> None:
    sys.stdout.write(f"[{level}] {msg}\n")
    sys.stdout.flush()


def run_local(cmd: Sequence[str], dry_run: bool = False, check: bool = True,
              capture: bool = False) -> subprocess.CompletedProcess:
    log("CMD ", " ".join(shlex.quote(c) for c in cmd))
    if dry_run:
        return subprocess.CompletedProcess(cmd, 0, "", "")
    return subprocess.run(cmd, check=check, text=True,
                          capture_output=capture)


def run_ssh(remote_cmd: str, dry_run: bool = False, check: bool = True,
            capture: bool = False) -> subprocess.CompletedProcess:
    cmd = ["ssh", CONFIG["remote_host"], "bash", "-lc", remote_cmd]
    return run_local(cmd, dry_run=dry_run, check=check, capture=capture)


# Stage stubs (filled in by later tasks)
def stage_prebuild(args): raise NotImplementedError
def stage_sync(args): raise NotImplementedError
def stage_prepare(args): raise NotImplementedError
def stage_gen_input(args): raise NotImplementedError
def stage_gen_output(args): raise NotImplementedError
def stage_status(args): raise NotImplementedError


def stage_all(args) -> None:
    for fn in (stage_prebuild, stage_sync, stage_prepare,
               stage_gen_input, stage_gen_output):
        fn(args)


STAGES = {
    "prebuild":   stage_prebuild,
    "sync":       stage_sync,
    "prepare":    stage_prepare,
    "gen-input":  stage_gen_input,
    "gen-output": stage_gen_output,
    "all":        stage_all,
    "status":     stage_status,
}


def build_argparser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("stage", choices=list(STAGES))
    p.add_argument("--dry-run", action="store_true")
    p.add_argument("-v", "--verbose", action="store_true")
    p.add_argument("--force", action="store_true")
    p.add_argument("--only", choices=["ypol", "zpol"], default=None)
    p.add_argument("--isotope", choices=["Sn112", "Sn124"], default=None)
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_argparser().parse_args(argv)
    STAGES[args.stage](args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
