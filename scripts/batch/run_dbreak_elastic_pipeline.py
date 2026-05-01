#!/usr/bin/env python3
"""Run dbreak elastic data (ypol+zpol Sn112/Sn124) through GenInputRoot
and sim_deuteron. Designed to be invoked ON spana03 (after `ssh spana03`),
so all stages call subprocess directly — no inner ssh wrapping.

Local data push (rsync) is a separate small bash helper at
scripts/batch/sync_dbreak_to_spana.sh, run from local before invoking
the pipeline on remote.

Spec: docs/superpowers/specs/2026-04-30-dbreak-elastic-g4-pipeline-design.md
"""

import argparse
import json
import shlex
import subprocess
import sys
from pathlib import Path, PurePosixPath
from typing import Sequence

CONFIG = {
    "local_smsim_dir":   "/home/tian/workspace/dpol/smsimulator5.5",
    "remote_host":       "spana03",
    "remote_smsim_dir":  "/home/tbt/workspace/Dpol_smsimulator",
    "git_remote":        "origin",
    "git_branch":        "feature/dbreak-elastic-pipeline",
    "build_cmd":         "bash build.sh",

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


def run(cmd: Sequence[str], dry_run: bool = False, check: bool = True,
        capture: bool = False, shell: bool = False) -> subprocess.CompletedProcess:
    """Run a command via subprocess (not over ssh — driver runs on remote)."""
    if shell:
        log("CMD ", cmd if isinstance(cmd, str) else " ".join(cmd))
    else:
        log("CMD ", " ".join(shlex.quote(c) for c in cmd))
    if dry_run:
        return subprocess.CompletedProcess(cmd, 0, "", "")
    return subprocess.run(cmd, check=check, text=True,
                          capture_output=capture, shell=shell)


def run_bash(remote_cmd: str, dry_run: bool = False, check: bool = True,
             capture: bool = False) -> subprocess.CompletedProcess:
    """Run a multi-statement bash snippet locally with `bash -lc <cmd>`,
    so setup_spana.sh and other login-shell scripts work."""
    return run(["bash", "-lc", remote_cmd],
               dry_run=dry_run, check=check, capture=capture)


def plan_ypol_symlinks(cfg: dict) -> list[tuple[PurePosixPath, PurePosixPath]]:
    """Return [(src, dst)] file-symlink plan for ypol Sn data.

    src lives under cfg['ypol_source_dir']/d+{iso}E190/d+{iso}E190g{gamma}{dir}-RP360/dbreak.dat
    dst lives under cfg['ypol_link_target']/d+{iso}E190g{gamma}{dir}/dbreak.dat

    Both returned as PurePosixPath relative to cfg['remote_smsim_dir'].
    """
    src_root = PurePosixPath(cfg["ypol_source_dir"])
    dst_root = PurePosixPath(cfg["ypol_link_target"])
    energy = cfg["energy"]
    plan = []
    for iso in cfg["isotopes"]:
        for gamma in cfg["gammas"]:
            for direction in cfg["ypol_directions"]:
                tag = f"d+{iso}{energy}{gamma}{direction}"  # e.g. d+Sn112E190g050ynp
                src = src_root / f"d+{iso}{energy}" / f"{tag}-RP360" / "dbreak.dat"
                dst = dst_root / tag / "dbreak.dat"
                plan.append((src, dst))
    return plan


def build_rsync_cmd(cfg: dict, dry_run: bool) -> list[str]:
    """Build rsync command pushing 20260413ypol/Sn{iso}E190 to remote, only dbreak.dat.

    Documents the rsync flag shape for sync_dbreak_to_spana.sh.
    Not called by the driver at runtime; covered by tests as documentation-as-code.
    """
    local_root = Path(cfg["local_smsim_dir"]) / cfg["ypol_source_dir"]
    remote_root = (PurePosixPath(cfg["remote_smsim_dir"])
                   / cfg["ypol_source_dir"])
    src = f"{local_root}/"
    dst = f"{cfg['remote_host']}:{remote_root}/"
    cmd = [
        "rsync", "-avz", "--info=progress2",
        "--include=*/", "--include=dbreak.dat", "--exclude=*",
        "--prune-empty-dirs",
    ]
    if dry_run:
        cmd.append("--dry-run")
    cmd += [src, dst]
    return cmd


def build_geninput_cmd(cfg: dict, mode: str, isotope: str) -> str:
    """Build the bash command that runs GenInputRoot_qmdrawdata once.

    Returns a single string suitable for `run_bash(...)` (bash -lc <cmd>).
    Driver invokes this on remote spana03 directly.
    """
    assert mode in ("ypol", "zpol")
    parts = [
        f"cd {shlex.quote(cfg['remote_smsim_dir'])}",
        "source setup_spana.sh",
        " ".join([
            cfg["geninput_bin"],
            f"--mode {mode}",
            "--source elastic",
            f"--target-filter {isotope}",
            f"--cut-unphysical {cfg['cut_unphysical']}",
            f"--cut-ypol-axis-limit {cfg['cut_axis_limit']}",
            f"--cut-zpol-axis-limit {cfg['cut_axis_limit']}",
            f"--randomize-ypol {cfg['randomize_ypol']}",
            f"--randomize-zpol {cfg['randomize_zpol']}",
            f"--rotation-seed {cfg['rotation_seed']}",
        ]),
    ]
    return " && ".join(parts)


def plan_filtered_tree(cfg: dict, pol: str) -> list[tuple[PurePosixPath, PurePosixPath]]:
    """Plan the (g4input → state_dir/g4input_filtered_<pol>) symlink farm.

    The filtered tree is what `run_g4input_batch_parallel.sh` reads as
    INPUT_ROOT, so it only contains the target isotope/gamma/direction subset.
    Both paths are PurePosixPath relative to cfg['remote_smsim_dir'].
    """
    assert pol in ("ypol", "zpol")
    energy = cfg["energy"]
    if pol == "ypol":
        src_root = PurePosixPath(cfg["g4input_base"]) / "y_pol" / "phi_random"
        dst_root = PurePosixPath(cfg["state_dir"]) / "g4input_filtered_ypol"
        directions = cfg["ypol_directions"]
        files = ["dbreak.root"]
    else:
        src_root = PurePosixPath(cfg["g4input_base"]) / "z_pol" / "b_discrete"
        dst_root = PurePosixPath(cfg["state_dir"]) / "g4input_filtered_zpol"
        directions = cfg["zpol_directions"]
        files = [f"dbreakb{i:02d}.root" for i in range(1, 11)]

    plan = []
    for iso in cfg["isotopes"]:
        for gamma in cfg["gammas"]:
            for direction in directions:
                tag = f"d+{iso}{energy}{gamma}{direction}"
                for fname in files:
                    plan.append((src_root / tag / fname,
                                 dst_root / tag / fname))
    return plan


# Stage implementations (T1-T5)
def stage_prebuild(args) -> None:
    """Verify branch + build + check EventTruthConverter symbol. Runs on spana03."""
    log("INFO", "prebuild: starting")
    cfg = CONFIG
    remote_root = PurePosixPath(cfg["remote_smsim_dir"])
    state_remote = remote_root / cfg["state_dir"]

    # 1. Verify we are on the expected feature branch (code arrived via local push).
    verify = (
        f"cd {shlex.quote(str(remote_root))} && "
        f"git rev-parse --abbrev-ref HEAD && "
        f"git rev-parse HEAD"
    )
    res = run_bash(verify, dry_run=args.dry_run, capture=True, check=True)
    head = ""
    if not args.dry_run:
        branch, head = res.stdout.strip().splitlines()[-2:]
        if branch != cfg["git_branch"] and not args.force:
            log("ERROR", f"remote on branch {branch!r}, expected {cfg['git_branch']!r}; "
                        f"checkout the right branch or pass --force")
            sys.exit(1)

    # 2. Build (rootlogon.C / setup.sh are auto-regenerated by setup_spana.sh,
    #    so they always show dirty in `git status` — we don't try to pull).
    build = (
        f"set -eo pipefail && "
        f"cd {shlex.quote(str(remote_root))} && "
        f"source setup_spana.sh && "
        f"{cfg['build_cmd']} 2>&1 | tail -200"
    )
    log("INFO", "prebuild: running build (this may take minutes)")
    run_bash(build, dry_run=args.dry_run, check=True)

    # 3. Verify EventTruthConverter symbol present in libsmdata.so
    verify_sym = (
        f"cd {shlex.quote(str(remote_root))} && "
        f"nm -D build/lib/libsmdata.so | grep -c EventTruthConverter || true"
    )
    res = run_bash(verify_sym, dry_run=args.dry_run, capture=True, check=False)
    if not args.dry_run:
        count = int(res.stdout.strip() or "0")
        if count == 0:
            log("ERROR", "build/lib/libsmdata.so has no EventTruthConverter "
                        "symbol; remote build failed or wrong source version")
            sys.exit(1)
        log("INFO", f"libsmdata.so contains {count} EventTruthConverter symbols")

    # 4. Write state_dir/prebuild.done
    head_short = "" if args.dry_run else head[:12]
    payload = json.dumps({"git_head": head_short, "has_event_truth": True})
    done = (
        f"mkdir -p {shlex.quote(str(state_remote))} && "
        f"echo {shlex.quote(payload)} > {shlex.quote(str(state_remote / 'prebuild.done'))}"
    )
    run_bash(done, dry_run=args.dry_run, check=True)
    log("INFO", "prebuild: done")


# Stage stubs (filled in by later tasks)
def stage_prepare(args): raise NotImplementedError
def stage_gen_input(args): raise NotImplementedError
def stage_gen_output(args): raise NotImplementedError
def stage_status(args): raise NotImplementedError


def stage_all(args) -> None:
    for fn in (stage_prebuild, stage_prepare,
               stage_gen_input, stage_gen_output):
        fn(args)


STAGES = {
    "prebuild":   stage_prebuild,
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
