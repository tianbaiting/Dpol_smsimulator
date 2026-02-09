#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import shlex
from pathlib import Path


SRC_EXTS = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".c++",
    ".C",
    ".CXX",
    ".CPP",
}


def load_commands(path: Path) -> list[dict]:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def args_from_entry(entry: dict) -> list[str]:
    if "arguments" in entry:
        return list(entry["arguments"])
    return shlex.split(entry["command"])


def is_source_arg(arg: str) -> bool:
    p = Path(arg)
    if p.suffix not in SRC_EXTS:
        return False
    return p.exists()


def strip_io_flags(args: list[str]) -> list[str]:
    cleaned: list[str] = []
    i = 0
    while i < len(args):
        a = args[i]
        if a in ("-o", "-MF", "-MT", "-MQ", "-x"):
            i += 2
            continue
        if a in ("-c",):
            i += 1
            continue
        if a.startswith(("-o", "-MF", "-MT", "-MQ", "-x")) and len(a) > 2:
            i += 1
            continue
        if is_source_arg(a):
            i += 1
            continue
        cleaned.append(a)
        i += 1
    return cleaned


def has_std_flag(args: list[str]) -> bool:
    for a in args:
        if a == "-std":
            return True
        if a.startswith("-std="):
            return True
    return False


def include_count(args: list[str]) -> int:
    count = 0
    i = 0
    while i < len(args):
        a = args[i]
        if a in ("-I", "-isystem", "-iquote", "-idirafter"):
            count += 1
            i += 2
            continue
        if a.startswith("-I") and len(a) > 2:
            count += 1
        elif a.startswith("-isystem") and len(a) > 8:
            count += 1
        elif a.startswith("-iquote") and len(a) > 7:
            count += 1
        elif a.startswith("-idirafter") and len(a) > 10:
            count += 1
        i += 1
    return count


def choose_template(entries: list[dict]) -> dict:
    best = None
    best_count = -1
    for e in entries:
        args = args_from_entry(e)
        cnt = include_count(args)
        if cnt > best_count:
            best = e
            best_count = cnt
    if best is None:
        raise RuntimeError("No compile commands found in input database.")
    return best


def find_c_macros(root: Path, ignore_dirs: set[str]) -> list[Path]:
    results: list[Path] = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in ignore_dirs]
        for name in filenames:
            if name.endswith(".C"):
                results.append(Path(dirpath) / name)
    return sorted(results)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate a clangd-only compilation database that includes ROOT .C macros."
    )
    parser.add_argument(
        "--input",
        default="compile_commands.json",
        help="Path to the existing CMake compile_commands.json",
    )
    parser.add_argument(
        "--output-dir",
        # default="clangd_db",
        default="build",
        help="Directory to write the clangd-only compile_commands.json",
    )
    parser.add_argument(
        "--root",
        default=".",
        help="Project root for scanning .C macros",
    )
    args = parser.parse_args()

    input_path = Path(args.input).resolve()
    root = Path(args.root).resolve()
    output_dir = Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    entries = load_commands(input_path)
    template = choose_template(entries)
    base_args = strip_io_flags(args_from_entry(template))

    if not has_std_flag(base_args):
        base_args.append("-std=c++20")

    macro_files = find_c_macros(
        root,
        ignore_dirs={
            ".git",
            ".cache",
            "build",
            "clangd_db",
            "cmake-build",
        },
    )

    existing_files = {Path(e["file"]).resolve() for e in entries if "file" in e}

    added = 0
    for macro in macro_files:
        macro_abs = macro.resolve()
        if macro_abs in existing_files:
            continue
        new_entry = {
            "directory": template["directory"],
            "file": str(macro_abs),
            "arguments": base_args
            + [
                "-x",
                "c++",
                "-c",
                str(macro_abs),
            ],
        }
        entries.append(new_entry)
        added += 1

    output_path = output_dir / "compile_commands.json"
    with output_path.open("w", encoding="utf-8") as f:
        json.dump(entries, f, indent=2)

    print(f"Added {added} .C macro entries.")
    print(f"Wrote {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
