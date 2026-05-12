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

    "reco_bin":          "build/bin/reconstruct_target_momentum",
    "reco_backend":      "rk",
    "reco_output_base":  "data/reconstruction/dbreak_elastic",
    "reco_jobs":         16,                          # spana03 has 16 cores
    "reco_field_map":    "configs/simulation/geometry/filed_map/180703-1,15T-3000.table",
    "reco_magnet_rotation_deg": 30,
    "reco_rk_fit_mode":  "three-point-free",
    "reco_max_iterations": 40,

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
    so setup.sh and other login-shell scripts work."""
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
        "source setup.sh",
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

    Note (ypol path quirk): GenInputRoot's recursive_directory_iterator
    resolves the y_pol/phi_random/<dir>/dbreak.dat file symlinks created in
    `prepare` to their canonical 20260413ypol target before computing the
    output path. So ypol Sn .root files land at
    g4input/20260413ypol/d+{ISO}E190/d+{ISO}E190g{XXX}{ynp,ypn}-RP360/dbreak.root,
    NOT g4input/y_pol/phi_random/<dir>/dbreak.root. We use the canonical
    path here.
    """
    assert pol in ("ypol", "zpol")
    energy = cfg["energy"]
    plan = []
    if pol == "ypol":
        src_root = PurePosixPath(cfg["g4input_base"]) / "20260413ypol"
        dst_root = PurePosixPath(cfg["state_dir"]) / "g4input_filtered_ypol"
        for iso in cfg["isotopes"]:
            for gamma in cfg["gammas"]:
                for direction in cfg["ypol_directions"]:
                    tag = f"d+{iso}{energy}{gamma}{direction}"
                    src_path = (src_root / f"d+{iso}{energy}"
                                / f"{tag}-RP360" / "dbreak.root")
                    dst_path = dst_root / tag / "dbreak.root"
                    plan.append((src_path, dst_path))
    else:
        src_root = PurePosixPath(cfg["g4input_base"]) / "z_pol" / "b_discrete"
        dst_root = PurePosixPath(cfg["state_dir"]) / "g4input_filtered_zpol"
        files = [f"dbreakb{i:02d}.root" for i in range(1, 11)]
        for iso in cfg["isotopes"]:
            for gamma in cfg["gammas"]:
                for direction in cfg["zpol_directions"]:
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

    # 2. Build (rootlogon.C is auto-regenerated by setup.sh,
    #    so it always shows dirty in `git status` — we don't try to pull).
    build = (
        f"set -eo pipefail && "
        f"cd {shlex.quote(str(remote_root))} && "
        f"source setup.sh && "
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


def stage_prepare(args) -> None:
    """Create file-level symlinks under y_pol/phi_random/ pointing into 20260413ypol/."""
    log("INFO", "prepare: starting")
    cfg = CONFIG
    plan = plan_ypol_symlinks(cfg)
    log("INFO", f"prepare: {len(plan)} symlinks planned")

    # Build a single bash heredoc that creates all symlinks idempotently.
    # Use absolute symlink targets — simpler than computing relatives, and
    # the data tree never moves after rsync.
    lines = []
    for src, dst in plan:
        abs_src = PurePosixPath(cfg["remote_smsim_dir"]) / src
        abs_dst = PurePosixPath(cfg["remote_smsim_dir"]) / dst
        lines.append(f"mkdir -p {shlex.quote(str(abs_dst.parent))}")
        lines.append(f"ln -snf {shlex.quote(str(abs_src))} "
                     f"{shlex.quote(str(abs_dst))}")
    bash = " && ".join(lines)

    if args.dry_run:
        log("INFO", "prepare: dry-run, would execute:")
        for line in lines:
            print("  " + line)
        return

    run_bash(bash, check=True)

    # Verify each symlink resolves to an existing file
    check = " && ".join(
        f"test -f {shlex.quote(str(PurePosixPath(cfg['remote_smsim_dir']) / dst))}"
        for _, dst in plan
    )
    res = run_bash(check, check=False, capture=True)
    if res.returncode != 0:
        log("ERROR", "prepare: at least one symlink target is missing")
        sys.exit(1)

    # Marker
    state_remote = PurePosixPath(cfg["remote_smsim_dir"]) / cfg["state_dir"]
    marker_payload = json.dumps({"count": len(plan)})
    marker_cmd = (
        f"mkdir -p {state_remote} && "
        f"echo {shlex.quote(marker_payload)} > {state_remote}/prepare.done"
    )
    run_bash(marker_cmd, check=True)
    log("INFO", "prepare: done")


def stage_gen_input(args) -> None:
    """Run GenInputRoot_qmdrawdata 4 times: (ypol|zpol) × (Sn112|Sn124)."""
    log("INFO", "gen-input: starting")
    cfg = CONFIG
    state_remote = PurePosixPath(cfg["remote_smsim_dir"]) / cfg["state_dir"]

    combos = []
    for pol in (["ypol", "zpol"] if args.only is None else [args.only]):
        for iso in (cfg["isotopes"] if args.isotope is None else [args.isotope]):
            combos.append((pol, iso))

    summary = {"runs": []}
    for pol, iso in combos:
        log("INFO", f"gen-input: {pol} {iso}")
        cmd = build_geninput_cmd(cfg, mode=pol, isotope=iso)
        log_path = state_remote / f"geninput_{pol}_{iso}.log"
        wrapped = (
            f"mkdir -p {state_remote} && "
            f"({cmd}) 2>&1 | tee {log_path}"
        )
        rc = run_bash(wrapped, dry_run=args.dry_run, check=False).returncode
        ok = (rc == 0)
        summary["runs"].append({"pol": pol, "isotope": iso, "ok": ok,
                                "log": str(log_path)})
        if not ok and not args.force:
            log("ERROR", f"gen-input: {pol} {iso} failed (rc={rc}); "
                        f"see {log_path}")
            sys.exit(1)

    # Validate g4input was actually produced (≥1 .root file under each pol root)
    if not args.dry_run:
        for pol in {p for p, _ in combos}:
            # GenInputRoot outputs to 20260413ypol/ for ypol, z_pol/b_discrete/ for zpol
            sub = "20260413ypol" if pol == "ypol" else "z_pol/b_discrete"
            check = (
                f"find {cfg['remote_smsim_dir']}/{cfg['g4input_base']}/{sub} "
                f" -name '*.root' -newer {state_remote}/prepare.done 2>/dev/null | wc -l"
            )
            res = run_bash(check, capture=True, check=False)
            count = int(res.stdout.strip()) if res.returncode == 0 else 0
            log("INFO", f"gen-input: {pol} produced {count} .root files")
            if count == 0:
                log("ERROR", f"gen-input: {pol} produced 0 ROOT files; "
                            "cut may be too tight or paths wrong")
                sys.exit(1)

    # Write done marker
    if not args.dry_run:
        marker = json.dumps(summary)
        marker_cmd = (
            f"mkdir -p {state_remote} && "
            f"echo {shlex.quote(marker)} > {state_remote}/geninput.done"
        )
        run_bash(marker_cmd, check=True)
    log("INFO", "gen-input: done")


def stage_gen_output(args) -> None:
    """Build filtered g4input symlink trees, then call run_g4input_batch_parallel.sh
    twice (ypol, zpol)."""
    log("INFO", "gen-output: starting")
    cfg = CONFIG
    remote_root = PurePosixPath(cfg["remote_smsim_dir"])
    state_remote = remote_root / cfg["state_dir"]

    pols = (["ypol", "zpol"] if args.only is None else [args.only])
    summary = {"jobs": []}

    for pol in pols:
        plan = plan_filtered_tree(cfg, pol=pol)
        # Filter by isotope if given
        if args.isotope is not None:
            plan = [(s, d) for s, d in plan if args.isotope in str(s)]
        log("INFO", f"gen-output: {pol} filter tree, {len(plan)} files")

        # 1. Build filter symlink tree
        ln_lines = []
        for src, dst in plan:
            abs_src = remote_root / src
            abs_dst = remote_root / dst
            ln_lines.append(f"mkdir -p {shlex.quote(str(abs_dst.parent))}")
            ln_lines.append(f"ln -snf {shlex.quote(str(abs_src))} "
                            f"{shlex.quote(str(abs_dst))}")
        bash = " && ".join(ln_lines) if ln_lines else "true"
        run_bash(bash, dry_run=args.dry_run, check=True)

        # 2. Invoke run_g4input_batch_parallel.sh
        sub = "y_pol/phi_random" if pol == "ypol" else "z_pol/b_discrete"
        pattern = "dbreak.root" if pol == "ypol" else "dbreakb*.root"
        input_root = state_remote / f"g4input_filtered_{pol}"
        output_base = remote_root / cfg["g4output_base"] / sub
        log_path = state_remote / f"gen_output_{pol}.log"
        env = " ".join(f"{k}={shlex.quote(v)}" for k, v in {
            "INPUT_ROOT":  str(input_root),
            "OUTPUT_BASE": str(output_base),
            "GEOM_MAC":    str(remote_root / cfg["geom_mac"]),
            "MACRO_DIR":   str(remote_root / cfg["macro_dir"]),
            "JOBS":        str(cfg["g4_jobs"]),
            "PATTERN":     pattern,
            "TAG":         f"{pol}_Sn_{cfg['tag']}",
        }.items())
        # sim_deuteron needs Geant4 data env vars (G4ENSDFSTATEDATA, etc.)
        # which setup.sh does NOT export. Pull them from `geant4-config
        # --datasets` after activating the conda env. The conda Geant4 package
        # installs data dirs without the "G4" prefix (ENSDFSTATE3.0/ instead of
        # G4ENSDFSTATE3.0/) while geant4-config still reports the G4-prefixed
        # form, so we strip the leading G4 from each basename via sed.
        cmd = (
            f'eval "$(/home/tbt/.local/bin/micromamba shell hook -s bash)" && '
            f'micromamba activate anaroot-env && '
            f"set -eo pipefail && "
            f"cd {shlex.quote(cfg['remote_smsim_dir'])} && "
            f"source setup.sh && "
            f'eval "$(geant4-config --datasets '
            f"| sed 's|/G4\\([A-Z]\\)|/\\1|g' "
            f"| awk '{{print \"export \" $2 \"=\" $3}}')\" && "
            f"mkdir -p {state_remote} {output_base} && "
            f"{env} bash {cfg['batch_script']} 2>&1 | tee {log_path}"
        )
        run_bash(cmd, dry_run=args.dry_run, check=True)
        summary["jobs"].append({"pol": pol, "log": str(log_path),
                                "input_root": str(input_root),
                                "output_base": str(output_base)})

    if not args.dry_run:
        marker = json.dumps(summary)
        marker_cmd = (
            f"mkdir -p {state_remote} && "
            f"echo {shlex.quote(marker)} > {state_remote}/gen_output.done"
        )
        run_bash(marker_cmd, check=True)
    log("INFO", "gen-output: done")


def stage_reco(args) -> None:
    """Run reconstruct_target_momentum on every g4output ROOT.

    Walks both y_pol/phi_random/<tag>/ and z_pol/b_discrete/<tag>/ trees,
    matches CONFIG.isotopes/gammas/directions, runs RK reco with
    --max-iterations and --rk-fit-mode from CONFIG, JOBS reco_jobs in
    parallel via xargs. Skip-if-exists by reco output > 100 KB.

    Designed for long-running (8+ days) jobs: launch via nohup so SSH
    disconnect doesn't kill it. Per-file [OK/FAIL] lines stream to
    state_dir/reco.log. Resume by re-invoking — done files are skipped.
    """
    log("INFO", "reco: starting")
    cfg = CONFIG
    remote_root = PurePosixPath(cfg["remote_smsim_dir"])
    state_remote = remote_root / cfg["state_dir"]
    g4output_root = remote_root / cfg["g4output_base"]
    reco_root = remote_root / cfg["reco_output_base"]
    log_path = state_remote / "reco.log"

    # Build per-file (input, output, log, run_name) joblist using globs that
    # match CONFIG.isotopes × gammas × directions × b-slices for zpol.
    # Compose patterns from the task plan; one entry per matching .root.
    jobs = []
    pols = (["ypol", "zpol"] if args.only is None else [args.only])
    isotopes = cfg["isotopes"] if args.isotope is None else [args.isotope]
    energy = cfg["energy"]
    for pol in pols:
        sub = "y_pol/phi_random" if pol == "ypol" else "z_pol/b_discrete"
        directions = cfg["ypol_directions"] if pol == "ypol" else cfg["zpol_directions"]
        for iso in isotopes:
            for gamma in cfg["gammas"]:
                for direction in directions:
                    tag = f"d+{iso}{energy}{gamma}{direction}"
                    out_dir = g4output_root / sub / tag
                    if pol == "ypol":
                        files = ["dbreak0000.root"]
                    else:
                        files = [f"dbreakb{i:02d}0000.root" for i in range(1, 11)]
                    for fname in files:
                        in_path = out_dir / fname
                        run_name = fname.replace(".root", "")
                        reco_dir = reco_root / sub / tag
                        reco_path = reco_dir / f"{run_name}_reco.root"
                        jobs.append((str(in_path), str(reco_path), str(reco_dir), run_name))

    log("INFO", f"reco: {len(jobs)} candidate jobs (Sn112+Sn124, ypol+zpol)")

    # Build joblist file on remote: input|output|reco_dir|run_name per line.
    joblist_path = state_remote / "reco_joblist.txt"
    joblist_content = "\n".join("|".join(j) for j in jobs) + "\n"
    write_joblist = (
        f"mkdir -p {shlex.quote(str(state_remote))} && "
        f"cat > {shlex.quote(str(joblist_path))} << 'JOBLIST_EOF'\n"
        f"{joblist_content}"
        f"JOBLIST_EOF"
    )
    run_bash(write_joblist, dry_run=args.dry_run, check=True)

    # The worker bash function: for each line, skip if reco file exists with
    # >100 KB; otherwise run reconstruct_target_momentum and append [OK Ns]/[FAIL]
    # to the global log. xargs runs JOBS workers in parallel.
    reco_bin = remote_root / cfg["reco_bin"]
    geom_mac = remote_root / cfg["geom_mac"]
    field_map = remote_root / cfg["reco_field_map"]
    jobs_n = cfg["reco_jobs"]
    max_iter = cfg["reco_max_iterations"]
    fit_mode = cfg["reco_rk_fit_mode"]
    backend = cfg["reco_backend"]
    rotation = cfg["reco_magnet_rotation_deg"]

    cmd = (
        f'eval "$(/home/tbt/.local/bin/micromamba shell hook -s bash)" && '
        f'micromamba activate anaroot-env && '
        f"renice +15 $$ >/dev/null 2>&1; "  # lower priority for the long-running reco
        f"cd {shlex.quote(cfg['remote_smsim_dir'])} && "
        f"source setup.sh && "
        f'eval "$(geant4-config --datasets '
        f"| sed 's|/G4\\([A-Z]\\)|/\\1|g' "
        f"| awk '{{print \"export \" $2 \"=\" $3}}')\" && "
        f"export RECO_BIN={shlex.quote(str(reco_bin))} && "
        f"export GEOM_MAC={shlex.quote(str(geom_mac))} && "
        f"export FIELD_MAP={shlex.quote(str(field_map))} && "
        f"export RK_FIT_MODE={shlex.quote(fit_mode)} && "
        f"export MAX_ITER={max_iter} && "
        f"export ROTATION_DEG={rotation} && "
        f"export BACKEND={shlex.quote(backend)} && "
        f'run_one() {{ '
        f'  local entry="$1"; '
        f'  local in_path="${{entry%%|*}}"; entry="${{entry#*|}}"; '
        f'  local out_path="${{entry%%|*}}"; entry="${{entry#*|}}"; '
        f'  local out_dir="${{entry%%|*}}"; '
        f'  local run_name="${{entry##*|}}"; '
        f'  if [[ -s "$out_path" ]] && [[ $(stat -c %s "$out_path") -gt 100000 ]]; then '
        f'    echo "[SKIP] $run_name"; return 0; '
        f'  fi; '
        f'  if [[ ! -s "$in_path" ]]; then echo "[MISS-IN] $in_path"; return 0; fi; '
        f'  mkdir -p "$out_dir"; '
        f'  local s=$(date +%s); '
        f'  if "$RECO_BIN" --backend "$BACKEND" --input-file "$in_path" --output-file "$out_path" '
        f'      --geometry-macro "$GEOM_MAC" --magnetic-field-map "$FIELD_MAP" '
        f'      --magnet-rotation-deg "$ROTATION_DEG" --rk-fit-mode "$RK_FIT_MODE" '
        f'      --max-iterations "$MAX_ITER" '
        f'      > "${{out_path%.root}}.log" 2>&1; then '
        f'    local e=$(date +%s); echo "[OK $((e-s))s] $run_name"; '
        f'  else '
        f'    local rc=$?; local e=$(date +%s); '
        f'    echo "[FAIL rc=$rc t=$((e-s))s] $run_name (log ${{out_path%.root}}.log)"; '
        f'  fi; '
        f'}}; '
        f'export -f run_one; '
        f"export RECO_BIN GEOM_MAC FIELD_MAP RK_FIT_MODE MAX_ITER ROTATION_DEG BACKEND && "
        f"echo \"[INIT reco] backend={backend} jobs={jobs_n} max_iter={max_iter} fit_mode={fit_mode}\" && "
        f"xargs -a {shlex.quote(str(joblist_path))} -d '\\n' -I {{}} -P {jobs_n} "
        f"bash -c 'run_one \"$@\"' _ {{}} && "
        f"echo \"[DONE reco] joblist={joblist_path}\""
    )

    full_cmd = f"({cmd}) 2>&1 | tee {shlex.quote(str(log_path))}"
    if args.dry_run:
        log("INFO", "reco: --dry-run, would execute joblist of size " + str(len(jobs)))
        return
    run_bash(full_cmd, dry_run=False, check=False)

    # Write done marker (count completed reco files)
    marker_cmd = (
        f"mkdir -p {shlex.quote(str(state_remote))} && "
        f"count=$(find {shlex.quote(str(reco_root))} -name '*_reco.root' "
        f"-size +100k 2>/dev/null | wc -l) && "
        f"echo \"{{\\\"completed\\\": $count}}\" "
        f" > {shlex.quote(str(state_remote / 'reco.done'))}"
    )
    run_bash(marker_cmd, check=True)
    log("INFO", "reco: done")


# Stage stubs (filled in by later tasks)
def stage_status(args) -> None:
    """Read state files and g4output file counts; print summary. SSH read-only."""
    log("INFO", "status: querying remote state")
    cfg = CONFIG
    remote_root = PurePosixPath(cfg["remote_smsim_dir"])
    state_remote = remote_root / cfg["state_dir"]
    g4output = remote_root / cfg["g4output_base"]

    cmd = (
        f"echo '== state files =='; "
        f"ls -la {state_remote}/*.done 2>/dev/null || echo '(none)'; "
        f"echo; echo '== g4output ypol .root count =='; "
        f"find {g4output}/y_pol/phi_random -name 'dbreak.root' 2>/dev/null | wc -l; "
        f"echo '== g4output zpol .root count =='; "
        f"find {g4output}/z_pol/b_discrete -name 'dbreakb*.root' 2>/dev/null | wc -l; "
        f"echo; echo '== last .done payloads =='; "
        f"for f in {state_remote}/*.done; do "
        f"  echo \"-- $f --\"; cat \"$f\"; echo; "
        f"done"
    )
    run_bash(cmd, capture=False, check=False)


def stage_all(args) -> None:
    for fn in (stage_prebuild, stage_prepare,
               stage_gen_input, stage_gen_output):
        fn(args)


STAGES = {
    "prebuild":   stage_prebuild,
    "prepare":    stage_prepare,
    "gen-input":  stage_gen_input,
    "gen-output": stage_gen_output,
    "reco":       stage_reco,
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


def main(argv=None) -> int:
    args = build_argparser().parse_args(argv)
    STAGES[args.stage](args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
