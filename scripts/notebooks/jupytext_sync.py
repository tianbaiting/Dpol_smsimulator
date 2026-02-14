#!/usr/bin/env python3
# ---
# jupyter:
#   jupytext:
#     cell_metadata_filter: -all
#     formats: ipynb,py:percent
#     notebook_metadata_filter: kernelspec,language_info,jupytext
#     text_representation:
#       extension: .py
#       format_name: percent
#       format_version: '1.3'
#       jupytext_version: 1.19.1
# ---

# %%
# %%
from __future__ import annotations

# %%
import argparse
import subprocess
import sys
from pathlib import Path
from typing import Iterable


# %%
REPO_ROOT = Path(__file__).resolve().parents[2]
NOTEBOOK_ROOT = REPO_ROOT / "scripts" / "notebooks"


# %%
def run_cmd(cmd: list[str]) -> str:
    result = subprocess.run(
        cmd,
        cwd=REPO_ROOT,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return result.stdout


# %%
def run_jupytext(*args: str) -> None:
    subprocess.run(
        ["jupytext", *args],
        cwd=REPO_ROOT,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


# %%
def relpath(path: Path) -> str:
    return str(path.relative_to(REPO_ROOT))


# %%
def is_notebook_py(path: Path) -> bool:
    try:
        with path.open("r", encoding="utf-8") as handle:
            for index, line in enumerate(handle):
                if index > 120:
                    break
                if "jupytext:" in line or line.startswith("# %%"):
                    return True
    except OSError:
        return False
    return False


# %%
def gather_staged_files() -> list[Path]:
    output = run_cmd(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR"]
    ).strip()
    if not output:
        return []
    files: list[Path] = []
    for line in output.splitlines():
        path = (REPO_ROOT / line).resolve()
        if path.exists():
            files.append(path)
    return files


# %%
def gather_all_files() -> list[Path]:
    files: list[Path] = []
    for ipynb in NOTEBOOK_ROOT.rglob("*.ipynb"):
        files.append(ipynb.resolve())
    for py_file in NOTEBOOK_ROOT.rglob("*.py"):
        if is_notebook_py(py_file.resolve()):
            files.append(py_file.resolve())
    return files


# %%
def as_targets(paths: Iterable[Path]) -> list[Path]:
    targets: dict[str, Path] = {}
    for raw_path in paths:
        path = raw_path.resolve()
        if not path.exists():
            continue
        try:
            path.relative_to(NOTEBOOK_ROOT)
        except ValueError:
            continue
        if path.suffix not in {".ipynb", ".py"}:
            continue
        if path.suffix == ".py" and not (
            is_notebook_py(path) or path.with_suffix(".ipynb").exists()
        ):
            continue
        targets[str(path)] = path
    return list(targets.values())


# %%
def sync_target(path: Path) -> Path | None:
    if path.suffix == ".ipynb":
        pair_path = path.with_suffix(".py")
    else:
        pair_path = path

    run_jupytext("--set-formats", "ipynb,py:percent", relpath(path))
    run_jupytext("--sync", relpath(path))

    if pair_path.exists() and pair_path.suffix == ".py":
        return pair_path
    return None


# %%
def stage_files(paths: Iterable[Path]) -> None:
    unique = sorted({relpath(path) for path in paths if path.exists()})
    if not unique:
        return
    subprocess.run(["git", "add", *unique], cwd=REPO_ROOT, check=True)


# %%
def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Sync notebook ipynb/py pairs with jupytext."
    )
    parser.add_argument("paths", nargs="*", help="Paths to sync")
    parser.add_argument(
        "--all",
        action="store_true",
        help="Sync all notebooks and notebook-paired python files under scripts/notebooks",
    )
    parser.add_argument(
        "--staged",
        action="store_true",
        help="Sync paths currently staged in git",
    )
    parser.add_argument(
        "--stage-updated",
        action="store_true",
        help="Stage generated or updated .py files",
    )
    return parser.parse_args(argv)


# %%
def main(argv: list[str]) -> int:
    args = parse_args(argv)

    if not NOTEBOOK_ROOT.exists():
        return 0

    input_paths: list[Path] = []
    if args.staged:
        input_paths.extend(gather_staged_files())
    if args.all:
        input_paths.extend(gather_all_files())
    if args.paths:
        input_paths.extend((REPO_ROOT / path).resolve() for path in args.paths)

    targets = as_targets(input_paths)
    if not targets:
        return 0

    updated_py: list[Path] = []
    for target in sorted(targets):
        try:
            updated = sync_target(target)
            if updated is not None:
                updated_py.append(updated)
        except subprocess.CalledProcessError as exc:
            sys.stderr.write(exc.stderr)
            return exc.returncode

    if args.stage_updated:
        stage_files(updated_py)

    return 0


# %%
if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
