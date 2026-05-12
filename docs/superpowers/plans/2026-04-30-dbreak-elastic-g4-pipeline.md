# dbreak Elastic G4 Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `scripts/batch/run_dbreak_elastic_pipeline.py` (runs on spana03) + `scripts/batch/sync_dbreak_to_spana.sh` (runs on local), an end-to-end pipeline that pushes ypol Sn data, then runs GenInputRoot_qmdrawdata → sim_deuteron, preserving b_phi metadata in both input and output ROOT.

**Architecture (revised 2026-04-30):** Two artifacts — a small **local** bash helper does the rsync data push, and a Python driver runs **on spana03** (after `ssh spana03`) with all stages calling subprocess directly (no inner ssh). Code travels via `git remote spana03` push. Pure planning logic (rsync cmd, symlink list, geninput cmd, filtered-tree plan) is unit-tested locally with pytest; integration tests run on spana03.

**Tech Stack:** Python 3.10+, pytest for unit tests, rsync over ssh (local→remote), bash on remote, existing remote binaries `build/bin/{GenInputRoot_qmdrawdata,sim_deuteron}` and existing script `scripts/simulation/run_g4input_batch_parallel.sh`.

**Python interpreter conventions (important — do NOT downgrade syntax):**
- Local pytest: `/home/tian/micromamba/envs/anaroot-env/bin/python3` (3.10.19)
- Remote driver / pytest: `/home/tbt/.local/share/mamba/envs/anaroot-env/bin/python3` (3.10.19)
- **NEVER use `ssh spana03 'python3 ...'`** because that resolves to system `/usr/bin/python3` (3.6) and modern syntax fails. Always use the absolute conda-env path above, OR `bash -lc "source setup.sh && python3 ..."` after PATH setup.

**Spec:** `docs/superpowers/specs/2026-04-30-dbreak-elastic-g4-pipeline-design.md`

---

## File Structure

| Path | Role |
|---|---|
| `scripts/batch/run_dbreak_elastic_pipeline.py` | Main driver (~500 lines): CONFIG dict, argparse, stage functions, helpers. Runs ON remote. |
| `scripts/batch/sync_dbreak_to_spana.sh` | Tiny rsync wrapper. Runs on local, pushes 20260413ypol Sn data to spana03. |
| `scripts/batch/tests/__init__.py` | Empty package marker |
| `scripts/batch/tests/test_run_dbreak_elastic_pipeline.py` | Unit tests for pure planning functions |
| `scripts/batch/tests/conftest.py` | pytest path setup |
| `docs/superpowers/plans/2026-04-30-dbreak-elastic-g4-pipeline.md` | This plan |

Code is committed to local feature branch and pushed to spana03 via `git remote spana03`.

---

## Task 1: Driver skeleton + CONFIG + CLI scaffold

**Files:**
- Create: `scripts/batch/run_dbreak_elastic_pipeline.py`
- Create: `scripts/batch/tests/__init__.py`
- Create: `scripts/batch/tests/conftest.py`

- [ ] **Step 1.1: Create empty test package marker**

```bash
mkdir -p scripts/batch/tests
touch scripts/batch/tests/__init__.py
```

- [ ] **Step 1.2: Create pytest conftest**

`scripts/batch/tests/conftest.py`:
```python
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
```

- [ ] **Step 1.3: Create driver skeleton with CONFIG and argparse**

`scripts/batch/run_dbreak_elastic_pipeline.py`:
```python
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


def run(cmd: Sequence[str], dry_run: bool = False, check: bool = True,
              capture: bool = False) -> subprocess.CompletedProcess:
    log("CMD ", " ".join(shlex.quote(c) for c in cmd))
    if dry_run:
        return subprocess.CompletedProcess(cmd, 0, "", "")
    return subprocess.run(cmd, check=check, text=True,
                          capture_output=capture)


def run_bash(remote_cmd: str, dry_run: bool = False, check: bool = True,
            capture: bool = False) -> subprocess.CompletedProcess:
    cmd = ["ssh", CONFIG["remote_host"], "bash", "-lc", remote_cmd]
    return run(cmd, dry_run=dry_run, check=check, capture=capture)


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
```

- [ ] **Step 1.4: Verify argparse works on every stage**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
python3 scripts/batch/run_dbreak_elastic_pipeline.py --help
for s in prebuild sync prepare gen-input gen-output all status; do
  python3 scripts/batch/run_dbreak_elastic_pipeline.py "$s" --dry-run 2>&1 | tail -3
done
```
Expected: `--help` prints stages list; each stage prints `NotImplementedError` and exits non-zero.

- [ ] **Step 1.5: Commit**

```bash
git add scripts/batch/run_dbreak_elastic_pipeline.py scripts/batch/tests/__init__.py scripts/batch/tests/conftest.py
git commit -m "scripts/batch: scaffold run_dbreak_elastic_pipeline driver"
```

---

## Task 2: Pure planning functions (TDD)

These are the testable units: rsync command builder, ypol symlink planner, geninput command builder, filtered-tree planner. Each has a deterministic input/output, no IO.

**Files:**
- Modify: `scripts/batch/run_dbreak_elastic_pipeline.py`
- Create: `scripts/batch/tests/test_run_dbreak_elastic_pipeline.py`

- [ ] **Step 2.1: Write failing test for `plan_ypol_symlinks`**

`scripts/batch/tests/test_run_dbreak_elastic_pipeline.py`:
```python
from pathlib import PurePosixPath

from run_dbreak_elastic_pipeline import (
    CONFIG, plan_ypol_symlinks, build_rsync_cmd, build_geninput_cmd,
    plan_filtered_tree,
)


def _cfg(**overrides):
    cfg = dict(CONFIG)
    cfg.update(overrides)
    return cfg


def test_plan_ypol_symlinks_emits_one_per_isotope_gamma_direction():
    cfg = _cfg(isotopes=["Sn112"], gammas=["g050", "g060"],
               ypol_directions=["ynp", "ypn"])
    plan = plan_ypol_symlinks(cfg)

    assert len(plan) == 4  # 1 iso * 2 gammas * 2 dirs
    src0, dst0 = plan[0]
    assert src0 == PurePosixPath(
        "data/qmdrawdata/qmdrawdata/20260413ypol/d+Sn112E190/"
        "d+Sn112E190g050ynp-RP360/dbreak.dat"
    )
    assert dst0 == PurePosixPath(
        "data/qmdrawdata/qmdrawdata/y_pol/phi_random/"
        "d+Sn112E190g050ynp/dbreak.dat"
    )
```

- [ ] **Step 2.2: Run, see it fail**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
python3 -m pytest scripts/batch/tests/test_run_dbreak_elastic_pipeline.py::test_plan_ypol_symlinks_emits_one_per_isotope_gamma_direction -v
```
Expected: ImportError on `plan_ypol_symlinks`.

- [ ] **Step 2.3: Implement `plan_ypol_symlinks`**

Add to `scripts/batch/run_dbreak_elastic_pipeline.py` (above `STAGES`):
```python
from pathlib import PurePosixPath


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
```

- [ ] **Step 2.4: Run test, expect pass**

```bash
python3 -m pytest scripts/batch/tests/test_run_dbreak_elastic_pipeline.py::test_plan_ypol_symlinks_emits_one_per_isotope_gamma_direction -v
```
Expected: PASS.

- [ ] **Step 2.5: Add full-CONFIG smoke test**

Append to test file:
```python
def test_plan_ypol_symlinks_default_config_has_16_entries():
    plan = plan_ypol_symlinks(CONFIG)
    assert len(plan) == 2 * 4 * 2  # 2 iso * 4 gammas * 2 dirs

    # All sources point under 20260413ypol; all dests under y_pol/phi_random
    for src, dst in plan:
        assert "20260413ypol" in str(src)
        assert "y_pol/phi_random" in str(dst)
        assert src.name == "dbreak.dat"
        assert dst.name == "dbreak.dat"
```

Run: `pytest scripts/batch/tests/ -v` → both pass.

- [ ] **Step 2.6: Write failing test for `build_rsync_cmd`**

```python
def test_build_rsync_cmd_only_includes_dbreak_dat():
    cfg = _cfg()
    cmd = build_rsync_cmd(cfg, dry_run=False)
    # Must be a list of args, not a shell string
    assert isinstance(cmd, list)
    assert cmd[0] == "rsync"
    assert "-avz" in cmd
    # Filter pattern: only dbreak.dat files come through
    assert "--include=*/" in cmd
    assert "--include=dbreak.dat" in cmd
    assert "--exclude=*" in cmd
    # Source ends with trailing slash (rsync semantic for dir contents)
    src = next(a for a in cmd if a.endswith("/20260413ypol/"))
    assert "/home/tian/workspace/dpol/smsimulator5.5/data/qmdrawdata/qmdrawdata/20260413ypol/" == src
    # Destination uses ssh alias
    dst = cmd[-1]
    assert dst.startswith("spana03:")
    assert dst.endswith("/20260413ypol/")


def test_build_rsync_cmd_dry_run_adds_flag():
    cfg = _cfg()
    cmd = build_rsync_cmd(cfg, dry_run=True)
    assert "--dry-run" in cmd
```

- [ ] **Step 2.7: Run, see fail**

```bash
python3 -m pytest scripts/batch/tests/test_run_dbreak_elastic_pipeline.py::test_build_rsync_cmd_only_includes_dbreak_dat -v
```
Expected: ImportError.

- [ ] **Step 2.8: Implement `build_rsync_cmd`**

Add to driver:
```python
def build_rsync_cmd(cfg: dict, dry_run: bool) -> list[str]:
    """Build rsync command pushing 20260413ypol/Sn{iso}E190 to remote, only dbreak.dat."""
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
```

- [ ] **Step 2.9: Run, expect pass**

```bash
python3 -m pytest scripts/batch/tests/ -v
```
Expected: 4 passed.

- [ ] **Step 2.10: Write failing test for `build_geninput_cmd`**

```python
def test_build_geninput_cmd_ypol_sn112():
    cfg = _cfg()
    cmd = build_geninput_cmd(cfg, mode="ypol", isotope="Sn112")
    # Returns a single shell string suitable for ssh "bash -lc <string>"
    assert isinstance(cmd, str)
    assert "source setup.sh" in cmd
    assert "build/bin/GenInputRoot_qmdrawdata" in cmd
    assert "--mode ypol" in cmd
    assert "--source elastic" in cmd
    assert "--target-filter Sn112" in cmd
    assert "--cut-unphysical on" in cmd
    assert "--cut-ypol-axis-limit 150" in cmd
    assert "--cut-zpol-axis-limit 150" in cmd
    assert "--randomize-ypol off" in cmd
    assert "--randomize-zpol on" in cmd
    assert "--rotation-seed 0" in cmd


def test_build_geninput_cmd_zpol_sn124():
    cmd = build_geninput_cmd(_cfg(), mode="zpol", isotope="Sn124")
    assert "--mode zpol" in cmd
    assert "--target-filter Sn124" in cmd
```

- [ ] **Step 2.11: Run, see fail. Implement.**

```python
def build_geninput_cmd(cfg: dict, mode: str, isotope: str) -> str:
    """Build the bash command that runs GenInputRoot_qmdrawdata once on remote.

    Returns a single string suitable for `ssh ... bash -lc '<cmd>'`.
    Caller embeds via run_bash().
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
```

Run tests: `pytest scripts/batch/tests/ -v` → 6 passed.

- [ ] **Step 2.12: Write failing test for `plan_filtered_tree`**

```python
def test_plan_filtered_tree_ypol_picks_target_isotope_gamma_dirs():
    cfg = _cfg(isotopes=["Sn112"], gammas=["g050"],
               ypol_directions=["ynp"])
    plan = plan_filtered_tree(cfg, pol="ypol")
    # 1 iso * 1 gamma * 1 dir * 1 dbreak.root file
    assert len(plan) == 1
    src, dst = plan[0]
    assert str(src) == ("data/simulation/g4input/y_pol/phi_random/"
                        "d+Sn112E190g050ynp/dbreak.root")
    assert str(dst) == ("logs/dbreak_elastic_pipeline/g4input_filtered_ypol/"
                        "d+Sn112E190g050ynp/dbreak.root")


def test_plan_filtered_tree_zpol_emits_10_b_slices_per_dir():
    cfg = _cfg(isotopes=["Sn112"], gammas=["g050"],
               zpol_directions=["znp"])
    plan = plan_filtered_tree(cfg, pol="zpol")
    # 1 iso * 1 gamma * 1 dir * 10 b-slices
    assert len(plan) == 10
    assert all("dbreakb" in str(src) for src, _ in plan)
    # b01..b10 distinct
    bnums = sorted({str(src).split("dbreakb")[1].split(".")[0] for src, _ in plan})
    assert bnums == [f"{i:02d}" for i in range(1, 11)]


def test_plan_filtered_tree_default_config_total_176():
    plan_y = plan_filtered_tree(CONFIG, pol="ypol")
    plan_z = plan_filtered_tree(CONFIG, pol="zpol")
    assert len(plan_y) == 16     # 2 iso * 4 gamma * 2 dir * 1 file
    assert len(plan_z) == 160    # 2 iso * 4 gamma * 2 dir * 10 files
    assert len(plan_y) + len(plan_z) == 176
```

- [ ] **Step 2.13: Run, see fail. Implement.**

```python
def plan_filtered_tree(cfg: dict, pol: str) -> list[tuple[PurePosixPath, PurePosixPath]]:
    """Plan the (g4input → state_dir/g4input_filtered_<pol>) symlink farm.

    The filtered tree is what `run_g4input_batch_parallel.sh` will read as
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
```

Run: `pytest scripts/batch/tests/ -v` → 9 passed.

- [ ] **Step 2.14: Commit**

```bash
git add scripts/batch/run_dbreak_elastic_pipeline.py scripts/batch/tests/
git commit -m "scripts/batch: pure planning fns (rsync, symlinks, geninput, filter, parse) + tests"
```

---

## Task 3: `prebuild` stage

**Files:**
- Modify: `scripts/batch/run_dbreak_elastic_pipeline.py` (replace stub)

- [ ] **Step 3.1: Implement `stage_prebuild`**

Driver runs **on spana03** (after the user `ssh spana03 && git checkout feature/...`); code is delivered via `git push spana03` from local, so prebuild does NOT pull or fetch — that would race with the local push. It just verifies the branch is right, runs the build, and confirms the EventTruthConverter symbol landed.

Replace the stub:
```python
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

    # 2. Build (rootlogon.C / setup.sh are auto-regenerated by setup.sh,
    #    so they always show dirty in `git status` — we don't try to pull).
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
```

- [ ] **Step 3.2: Smoke test prebuild dry-run (run from local via ssh)**

```bash
ssh spana03 'cd /home/tbt/workspace/Dpol_smsimulator && /home/tbt/.local/share/mamba/envs/anaroot-env/bin/python3 scripts/batch/run_dbreak_elastic_pipeline.py prebuild --dry-run'
```
Expected: prints CMD lines for each `bash -lc ...` invocation; no real bash calls.

- [ ] **Step 3.3: Real prebuild (run from local via ssh)**

```bash
ssh spana03 'cd /home/tbt/workspace/Dpol_smsimulator && /home/tbt/.local/share/mamba/envs/anaroot-env/bin/python3 scripts/batch/run_dbreak_elastic_pipeline.py prebuild'
```
Expected (interactive observation):
- git pull / build runs and ends with build success message
- "libsmdata.so contains N EventTruthConverter symbols" with N≥10
- state_dir/prebuild.done exists on remote

Verify:
```bash
ls -la /home/tian/workspace/sshDir/spana03/Dpol_smsimulator/logs/dbreak_elastic_pipeline/prebuild.done
nm -D /home/tian/workspace/sshDir/spana03/Dpol_smsimulator/build/lib/libsmdata.so | grep -c EventTruthConverter
```
Expected: file exists; count ≥ 10.

- [ ] **Step 3.4: Commit and push to spana03**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
git add scripts/batch/run_dbreak_elastic_pipeline.py
git commit -m "scripts/batch: prebuild stage — verify branch, build, symbol check"
git push spana03 feature/dbreak-elastic-pipeline
```

---

## Task 4: `sync_dbreak_to_spana.sh` local bash helper

**Architecture revision (2026-04-30):** Originally Task 4 added a `stage_sync` to the Python driver. The driver now runs ON the remote, so a local-side bash helper does the rsync push from outside the driver. Already created in commit `<refactor>` — this task is to verify it exists, run a dry-run smoke test, and unit-test it from Python (import the rsync command shape via the existing `build_rsync_cmd` planning fn — kept for testability even though the driver doesn't call it).

**Files:**
- Verify exists (already created): `scripts/batch/sync_dbreak_to_spana.sh`
- Test (already added in T2): `build_rsync_cmd` in driver covers the rsync shape

- [ ] **Step 4.1: Verify the bash helper exists and is executable**

```bash
ls -la /home/tian/workspace/dpol/smsimulator5.5/scripts/batch/sync_dbreak_to_spana.sh
test -x /home/tian/workspace/dpol/smsimulator5.5/scripts/batch/sync_dbreak_to_spana.sh && echo OK
head -10 /home/tian/workspace/dpol/smsimulator5.5/scripts/batch/sync_dbreak_to_spana.sh
```
Expected: file present, executable, shebang `#!/usr/bin/env bash`.

- [ ] **Step 4.2: Dry-run sync (does not transfer)**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
bash scripts/batch/sync_dbreak_to_spana.sh --dry-run
```
Expected: rsync prints "would send" listings. **Only `dbreak.dat` files appear; geminiout.dat and reactiontype.dat must NOT appear.**

- [ ] **Step 4.3: Real sync (transfers data)**

```bash
bash scripts/batch/sync_dbreak_to_spana.sh
```
Verify on remote:
```bash
ls /home/tian/workspace/sshDir/spana03/Dpol_smsimulator/data/qmdrawdata/qmdrawdata/20260413ypol/d+Sn112E190/d+Sn112E190g050ynp-RP360/
```
Expected: contains `dbreak.dat` (and only that — no geminiout/reactiontype).

- [ ] **Step 4.4: No code change required** — sync helper landed in the architecture-refactor commit. If editing the bash script, commit normally.

---

## Task 5: `prepare` stage

**Files:**
- Modify: `scripts/batch/run_dbreak_elastic_pipeline.py` (replace stub)

- [ ] **Step 5.1: Implement `stage_prepare` reusing `plan_ypol_symlinks`**

```python
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
```

- [ ] **Step 5.2: Dry-run**

```bash
python3 scripts/batch/run_dbreak_elastic_pipeline.py prepare --dry-run
```
Expected: prints 16 mkdir + 16 ln commands.

- [ ] **Step 5.3: Real prepare**

```bash
python3 scripts/batch/run_dbreak_elastic_pipeline.py prepare
```
Verify:
```bash
ls -la /home/tian/workspace/sshDir/spana03/Dpol_smsimulator/data/qmdrawdata/qmdrawdata/y_pol/phi_random/d+Sn112E190g050ynp/
```
Expected: `dbreak.dat -> /home/tbt/workspace/Dpol_smsimulator/data/qmdrawdata/qmdrawdata/20260413ypol/d+Sn112E190/d+Sn112E190g050ynp-RP360/dbreak.dat` and the link resolves.

- [ ] **Step 5.4: Commit**

```bash
git add scripts/batch/run_dbreak_elastic_pipeline.py
git commit -m "scripts/batch: prepare stage — file symlinks for ypol Sn into y_pol/phi_random"
```

---

## Task 6: `gen-input` stage

**Files:**
- Modify: `scripts/batch/run_dbreak_elastic_pipeline.py` (replace stub)

- [ ] **Step 6.1: Implement `stage_gen_input`**

```python
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
            sub = "y_pol/phi_random" if pol == "ypol" else "z_pol/b_discrete"
            check = (
                f"find {cfg['remote_smsim_dir']}/{cfg['g4input_base']}/{sub} "
                f" -name '*.root' -newer {state_remote}/sync.done | wc -l"
            )
            res = run_bash(check, capture=True, check=True)
            count = int(res.stdout.strip())
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
```

- [ ] **Step 6.2: Smoke test with single isotope**

```bash
python3 scripts/batch/run_dbreak_elastic_pipeline.py gen-input \
    --only ypol --isotope Sn112
```
Verify:
```bash
ls /home/tian/workspace/sshDir/spana03/Dpol_smsimulator/data/simulation/g4input/y_pol/phi_random/d+Sn112E190g050ynp/
```
Expected: contains `dbreak.root`.

Also open in ROOT to confirm b_phi present (this is part of Task 9 verification, listed here as context only):
```bash
ssh spana03 'cd /home/tbt/workspace/Dpol_smsimulator && source setup.sh && root -l -b -q "data/simulation/g4input/y_pol/phi_random/d+Sn112E190g050ynp/dbreak.root" -e "tree->Show(0)"' 2>&1 | grep -A2 fUserDouble
```
Expected: fUserDouble has ≥3 elements; element [1] is a finite number.

- [ ] **Step 6.3: Full gen-input**

```bash
python3 scripts/batch/run_dbreak_elastic_pipeline.py gen-input
```
Expected: 4 runs complete; geninput.done written.

- [ ] **Step 6.4: Commit**

```bash
git add scripts/batch/run_dbreak_elastic_pipeline.py
git commit -m "scripts/batch: gen-input stage — 4 GenInputRoot calls, log per (pol, iso)"
```

---

## Task 7: `gen-output` stage

**Files:**
- Modify: `scripts/batch/run_dbreak_elastic_pipeline.py` (replace stub)

- [ ] **Step 7.1: Add filtered-tree builder**

```python
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
        cmd = (
            f"set -eo pipefail && "
            f"cd {shlex.quote(cfg['remote_smsim_dir'])} && "
            f"source setup.sh && "
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
```

- [ ] **Step 7.2: Smoke test ypol single isotope**

```bash
python3 scripts/batch/run_dbreak_elastic_pipeline.py gen-output \
    --only ypol --isotope Sn112
```
Watch the log — expect 8 jobs (1 iso × 4 γ × 2 dirs). Successful jobs end with `[OK Ns]`.

Verify a result:
```bash
ls /home/tian/workspace/sshDir/spana03/Dpol_smsimulator/data/simulation/g4output/y_pol/phi_random/d+Sn112E190g050ynp/
```
Expected: `dbreak.root` and `dbreak.log` present.

- [ ] **Step 7.3: Full gen-output**

```bash
python3 scripts/batch/run_dbreak_elastic_pipeline.py gen-output
```
Expected: 16 + 160 = 176 jobs run; gen_output.done written.

- [ ] **Step 7.4: Commit**

```bash
git add scripts/batch/run_dbreak_elastic_pipeline.py
git commit -m "scripts/batch: gen-output stage — filtered tree + run_g4input_batch_parallel"
```

---

## Task 8: `status` stage

**Files:**
- Modify: `scripts/batch/run_dbreak_elastic_pipeline.py` (replace stub)

- [ ] **Step 8.1: Implement `stage_status`**

```python
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
```

- [ ] **Step 8.2: Run after smoke runs**

```bash
python3 scripts/batch/run_dbreak_elastic_pipeline.py status
```
Expected output includes:
```
== state files ==
... prebuild.done sync.done prepare.done geninput.done gen_output.done
== g4output ypol .root count ==
16
== g4output zpol .root count ==
160
```

- [ ] **Step 8.3: Commit**

```bash
git add scripts/batch/run_dbreak_elastic_pipeline.py
git commit -m "scripts/batch: status stage — print remote state + output counts"
```

---

## Task 9: End-to-end smoke run + b_phi verification

This is an integration verification, not a code task. The driver code is done after Task 8. This task **proves** the pipeline meets the spec's verification §.

**Files:**
- Create: `scripts/batch/tests/verify_b_phi_smoke.sh` (small bash wrapper around root macros)

- [ ] **Step 9.1: Create verification script**

`scripts/batch/tests/verify_b_phi_smoke.sh`:
```bash
#!/usr/bin/env bash
# Verify b_phi is correctly populated in input and output ROOT for one
# (ypol Sn112 g050 ynp) smoke pair and one (zpol Sn112 g050 znp b01) pair.
set -euo pipefail

REMOTE_HOST="${REMOTE_HOST:-spana03}"
REMOTE_DIR="${REMOTE_DIR:-/home/tbt/workspace/Dpol_smsimulator}"

ssh "$REMOTE_HOST" "cd $REMOTE_DIR && source setup.sh && \
  root -l -b -q -e '
    auto check = [](const char* in, const char* out){
      TFile *fi = TFile::Open(in);
      TTree *ti = (TTree*)fi->Get(\"tree\");
      TClonesArray *arr=0; ti->SetBranchAddress(\"TBeamSimData\", &arr);
      ti->GetEntry(0);
      auto *first = (TBeamSimData*) arr->At(0);
      double in_b_phi = first->fUserDouble[1];

      TFile *fo = TFile::Open(out);
      TTree *to = (TTree*)fo->Get(\"tree\");
      double out_b_phi=0; to->SetBranchAddress(\"truth_b_phi\", &out_b_phi);
      to->GetEntry(0);
      printf(\"%s ↔ %s : in=%.6f out=%.6f delta=%.3e\\n\",
             in, out, in_b_phi, out_b_phi, in_b_phi-out_b_phi);
    };
    check(
      \"data/simulation/g4input/y_pol/phi_random/d+Sn112E190g050ynp/dbreak.root\",
      \"data/simulation/g4output/y_pol/phi_random/d+Sn112E190g050ynp/dbreak.root\"
    );
    check(
      \"data/simulation/g4input/z_pol/b_discrete/d+Sn112E190g050znp/dbreakb01.root\",
      \"data/simulation/g4output/z_pol/b_discrete/d+Sn112E190g050znp/dbreakb01.root\"
    );
  '"
```

- [ ] **Step 9.2: Make it executable and run**

```bash
chmod +x scripts/batch/tests/verify_b_phi_smoke.sh
bash scripts/batch/tests/verify_b_phi_smoke.sh
```
Expected output (two lines, one per check):
```
.../dbreak.root ↔ .../dbreak.root : in=1.234567 out=1.234567 delta=0.000e+00
.../dbreakb01.root ↔ .../dbreakb01.root : in=2.345678 out=2.345678 delta=0.000e+00
```
Both `delta` < 1e-9. **If delta is large or NaN, the pipeline is broken.**

- [ ] **Step 9.3: Verify zpol Δ↔b_phi reproducibility (spec §verification step 5b)**

Run gen-input twice with the same seed, diff the b_phi distribution:
```bash
ssh spana03 'cd /home/tbt/workspace/Dpol_smsimulator && source setup.sh && \
  build/bin/GenInputRoot_qmdrawdata --mode zpol --source elastic \
    --target-filter Sn112E190g050znp \
    --cut-unphysical on --cut-ypol-axis-limit 150 --cut-zpol-axis-limit 150 \
    --randomize-zpol on --rotation-seed 12345 \
    --output-base /tmp/seed_test_a 2>&1 | tail -5 && \
  build/bin/GenInputRoot_qmdrawdata --mode zpol --source elastic \
    --target-filter Sn112E190g050znp \
    --cut-unphysical on --cut-ypol-axis-limit 150 --cut-zpol-axis-limit 150 \
    --randomize-zpol on --rotation-seed 12345 \
    --output-base /tmp/seed_test_b 2>&1 | tail -5'
```
Then compare b_phi byte-for-byte:
```bash
ssh spana03 'source /home/tbt/workspace/Dpol_smsimulator/setup.sh && \
  root -l -b -q -e "
    TFile *fa=TFile::Open(\"/tmp/seed_test_a/z_pol/b_discrete/d+Sn112E190g050znp/dbreakb01.root\");
    TFile *fb=TFile::Open(\"/tmp/seed_test_b/z_pol/b_discrete/d+Sn112E190g050znp/dbreakb01.root\");
    TTree *ta=(TTree*)fa->Get(\"tree\"), *tb=(TTree*)fb->Get(\"tree\");
    TClonesArray *aa=0,*ab=0;
    ta->SetBranchAddress(\"TBeamSimData\",&aa); tb->SetBranchAddress(\"TBeamSimData\",&ab);
    Long64_t n = ta->GetEntries();
    int mismatches=0;
    for(Long64_t i=0;i<n;i++){
      ta->GetEntry(i); tb->GetEntry(i);
      double pa=((TBeamSimData*)aa->At(0))->fUserDouble[1];
      double pb=((TBeamSimData*)ab->At(0))->fUserDouble[1];
      if(std::fabs(pa-pb)>1e-12) mismatches++;
    }
    printf(\"events=%lld mismatches=%d\\n\", n, mismatches);
  "'
```
Expected: `mismatches=0` (proves Δ stamping is reproducible per seed; ergo Δ IS what gets recorded as b_phi).

- [ ] **Step 9.4: Final commit**

```bash
git add scripts/batch/tests/verify_b_phi_smoke.sh
git commit -m "scripts/batch: b_phi smoke verification script (input ↔ output, seed reproducibility)"
```

---

## Task 10: README + status summary improvement

**Files:**
- Modify: `scripts/batch/run_dbreak_elastic_pipeline.py` (add module docstring example)
- Create: `scripts/batch/README_dbreak_elastic_pipeline.md`

- [ ] **Step 10.1: Write quick-start README**

`scripts/batch/README_dbreak_elastic_pipeline.md`:
```markdown
# dbreak elastic g4 pipeline runner

Drives the end-to-end ypol+zpol Sn112/Sn124 elastic dbreak →
g4input → g4output flow on remote spana03.

Spec: `docs/superpowers/specs/2026-04-30-dbreak-elastic-g4-pipeline-design.md`

## Quick start

```bash
cd /home/tian/workspace/dpol/smsimulator5.5

# Verify wiring with a dry-run of every stage
for s in prebuild sync prepare gen-input gen-output; do
  python3 scripts/batch/run_dbreak_elastic_pipeline.py "$s" --dry-run
done

# Full run (≈ tens of minutes on remote, depending on cuts)
python3 scripts/batch/run_dbreak_elastic_pipeline.py all

# Status check
python3 scripts/batch/run_dbreak_elastic_pipeline.py status
```

## Smoke run (one isotope, one γ, one direction)

Edit CONFIG at the top of `run_dbreak_elastic_pipeline.py`:
```python
"isotopes":        ["Sn112"],
"gammas":          ["g050"],
"ypol_directions": ["ynp"],
"zpol_directions": ["znp"],
```
Then `python3 ... all` produces 1 + 1 = 2 outputs and is fast enough
to validate the wiring before scaling up.

Verify b_phi after the smoke run:
```bash
bash scripts/batch/tests/verify_b_phi_smoke.sh
```

## Tunable parameters

All in `CONFIG` at the top of `run_dbreak_elastic_pipeline.py`. Most
common edits:
- `g4_jobs`: parallel sim_deuteron jobs on remote (default 8)
- `cut_axis_limit`: virtual-breakup cut threshold (MeV/c, default 150)
- `rotation_seed`: 0 for system seed, positive int for reproducibility

## Re-running

Each stage is idempotent. To force a re-run pass `--force`:
- `prebuild --force`: `git reset --hard origin/main` before build
- `gen-input --force`: skip the "≥1 .root present" guard
- `gen-output --force`: not yet wired; manually `rm -rf` g4output/$pol/...
```

- [ ] **Step 10.2: Run unit tests one final time**

```bash
cd /home/tian/workspace/dpol/smsimulator5.5
python3 -m pytest scripts/batch/tests/ -v
```
Expected: all tests pass.

- [ ] **Step 10.3: Final commit**

```bash
git add scripts/batch/README_dbreak_elastic_pipeline.md
git commit -m "scripts/batch: add quick-start README for dbreak elastic pipeline"
```

---

## Verification Checklist (executed during/after Task 9)

Direct mapping to spec §verification:

| Spec step | Implemented in | How to run |
|---|---|---|
| 1. prebuild self-check | Task 3 step 3.3 | `... prebuild` then check libsmdata.so symbol |
| 2. sync --dry-run | Task 4 step 4.2 | `... sync --dry-run` |
| 3. prepare --dry-run | Task 5 step 5.2 | `... prepare --dry-run` |
| 4. ypol smoke (1 file) | Task 6 step 6.2 + Task 9 step 9.2 | Edit CONFIG to single iso/γ/dir → `... all` → `verify_b_phi_smoke.sh` |
| 5. zpol smoke + b_phi uniformity | Task 9 step 9.2 | `verify_b_phi_smoke.sh` |
| 5b. zpol Δ↔b_phi equivalence | Task 9 step 9.3 | seed-reproducibility check |
| 6. ypol b_phi == raw rpphi | Task 9 step 9.2 | input fUserDouble[1] equals rad-converted rpphi |
| 7. cut on/off comparison | Manual | Toggle `cut_unphysical` in CONFIG, rerun gen-input twice, diff GetEntries |
| 8. status: 5 stages done, 176 outputs | Task 8 step 8.2 | `... status` |
| 9. regression vs polarization_validation_sn124_zpol_g060 | Manual, optional | `root` open both files, compare branch list |

---

## Risk register (from spec §Open questions)

- ypol γ caps at g085 in source data. CONFIG.gammas already excludes anything above g080, so no impact on this run.
- gen-input over-produces zpol γ files (g090, g100 also created since GenInputRoot has no γ filter). They occupy ~tens of MB; gen-output filtered tree drops them. **No code change in this plan.**
- `recursive_directory_iterator` file-symlink resolution is Linux glibc semantics. Verified working on spana03 (Debian); re-test if migrating elsewhere.
- `rotation_seed=0` (system-derived) means re-running gen-input changes truth_b_phi values. Set `rotation_seed` to a positive int to freeze.
