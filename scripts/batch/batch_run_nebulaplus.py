#!/usr/bin/env python3
"""
Batch run script for NEBULA-Plus geometry simulation.
Processes y-pol and z-pol Sn112/Sn124 data with 3deg1.15Tnebulaplus.mac geometry.

Usage:
    python3 batch_run_nebulaplus.py all                     # run all 176 jobs
    python3 batch_run_nebulaplus.py all --mode y            # y-pol only (16 jobs)
    python3 batch_run_nebulaplus.py all --mode z            # z-pol only (160 jobs)
    python3 batch_run_nebulaplus.py 1000                    # 1000 events per file
    python3 batch_run_nebulaplus.py all --dry-run           # print jobs without running
"""

import os
import sys
import subprocess
import glob
import re
import argparse
from pathlib import Path
from datetime import datetime

TARGETS = ["d+Sn112E190", "d+Sn124E190"]
GAMMAS = ["g050", "g060", "g070", "g080"]
YPOL_POLS = ["ynp", "ypn"]
ZPOL_POLS = ["znp", "zpn"]


class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    CYAN = '\033[0;36m'
    NC = '\033[0m'


def log(msg, color=Colors.NC):
    print(f"{color}{msg}{Colors.NC}")


def create_macro(input_file, output_dir, run_name, geometry_mac, num_events):
    return f"""# Auto-generated macro for {run_name}
# Generated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}

# File settings
/action/file/OverWrite y
/action/file/RunName {run_name}
/action/file/SaveDirectory {output_dir}

# Load geometry
/control/execute {geometry_mac}

# Gun settings - use Tree input
/action/gun/Type Tree
/action/gun/tree/InputFileName {input_file}
/action/gun/tree/TreeName tree

# Run simulation
/action/gun/tree/beamOn {num_events}

# Exit
exit
"""


def detect_entries(input_file, macro_dir, filename):
    try:
        import uproot
        with uproot.open(input_file) as f:
            return int(f['tree'].num_entries)
    except Exception:
        pass
    try:
        tmp_macro = os.path.join(macro_dir, f"_get_entries_{filename}.C")
        with open(tmp_macro, 'w') as mf:
            mf.write(
                '{\n'
                '  TFile *f = TFile::Open("' + input_file.replace('\\', '\\\\') + '");\n'
                '  if (!f) { std::cout << -1 << std::endl; return; }\n'
                '  TTree *t = (TTree*)f->Get("tree");\n'
                '  if (!t) { std::cout << -1 << std::endl; return; }\n'
                '  std::cout << t->GetEntries() << std::endl;\n'
                '}'
            )
        res = subprocess.run(['root', '-l', '-b', '-q', tmp_macro],
                             capture_output=True, text=True)
        out = res.stdout.strip() if res.stdout else res.stderr.strip()
        m = re.search(r"(\d+)", out)
        if m:
            return int(m.group(1))
    except Exception:
        pass
    return 0


def collect_ypol_jobs(smsim_dir):
    jobs = []
    for target in TARGETS:
        for gamma in GAMMAS:
            for pol in YPOL_POLS:
                config_name = f"{target}{gamma}{pol}"
                input_file = os.path.join(
                    smsim_dir,
                    f"data/simulation/g4input/20260413ypol/{target}/{config_name}-RP360/dbreak.root"
                )
                output_dir = os.path.join(
                    smsim_dir,
                    f"data/simulation/g4output/y_pol_nebulaplus/phi_random/{config_name}"
                )
                if os.path.isfile(input_file):
                    jobs.append((input_file, output_dir, config_name, "dbreak"))
                else:
                    log(f"  SKIP (missing): {input_file}", Colors.YELLOW)
    return jobs


def collect_zpol_jobs(smsim_dir):
    jobs = []
    for target in TARGETS:
        for gamma in GAMMAS:
            for pol in ZPOL_POLS:
                config_name = f"{target}{gamma}{pol}"
                input_dir = os.path.join(
                    smsim_dir,
                    f"data/simulation/g4input/z_pol/b_discrete/{config_name}"
                )
                output_dir = os.path.join(
                    smsim_dir,
                    f"data/simulation/g4output/z_pol_nebulaplus/b_discrete/{config_name}"
                )
                root_files = sorted(glob.glob(os.path.join(input_dir, "dbreakb*.root")))
                if not root_files:
                    log(f"  SKIP (empty): {input_dir}", Colors.YELLOW)
                    continue
                for rf in root_files:
                    stem = Path(rf).stem
                    jobs.append((rf, output_dir, config_name, stem))
    return jobs


def run_jobs(jobs, smsim_dir, geometry_mac, exe, macro_dir, num_events_arg, dry_run=False):
    success = 0
    failed = 0
    total = len(jobs)

    for idx, (input_file, output_dir, config_name, stem) in enumerate(jobs, 1):
        run_name = stem
        tag = f"{config_name}/{stem}"

        log(f"[{idx}/{total}] {tag}", Colors.CYAN)
        log(f"    Input:  {input_file}")
        log(f"    Output: {output_dir}/")

        if num_events_arg <= 0:
            events = detect_entries(input_file, macro_dir, stem)
            if events <= 0:
                log(f"    WARNING: could not detect entries, using 1000", Colors.YELLOW)
                events = 1000
        else:
            events = num_events_arg

        log(f"    Events: {events}")

        macro_content = create_macro(input_file, output_dir, run_name,
                                     geometry_mac, events)
        macro_file = os.path.join(macro_dir, f"run_{config_name}_{stem}.mac")

        with open(macro_file, 'w') as f:
            f.write(macro_content)

        if dry_run:
            log(f"    DRY RUN", Colors.YELLOW)
            success += 1
            continue

        log_file = os.path.join(output_dir, f"{stem}.log")
        os.makedirs(output_dir, exist_ok=True)

        log(f"    Running...", Colors.YELLOW)
        try:
            with open(log_file, 'w') as lf:
                result = subprocess.run(
                    [exe, macro_file],
                    cwd=smsim_dir,
                    stdout=lf,
                    stderr=subprocess.STDOUT,
                    timeout=3600
                )
            if result.returncode == 0:
                log(f"    OK", Colors.GREEN)
                success += 1
            else:
                log(f"    FAILED (rc={result.returncode}, see {log_file})", Colors.RED)
                failed += 1
        except FileNotFoundError:
            log(f"    FAILED: executable not found: {exe}", Colors.RED)
            failed += 1
        except subprocess.TimeoutExpired:
            log(f"    FAILED: timeout (>1h)", Colors.RED)
            failed += 1
        except Exception as e:
            log(f"    FAILED: {e}", Colors.RED)
            failed += 1

    return success, failed


def main():
    parser = argparse.ArgumentParser(
        description="Batch run NEBULA-Plus geometry simulation"
    )
    parser.add_argument(
        "events",
        nargs="?",
        default="all",
        help="Number of events per file, or 'all' to auto-detect (default: all)"
    )
    parser.add_argument(
        "--mode", "-m",
        choices=["y", "z", "both"],
        default="both",
        help="Which polarization to run: y, z, or both (default: both)"
    )
    parser.add_argument(
        "--dry-run", "-n",
        action="store_true",
        help="Print job list and write macros without executing"
    )
    args = parser.parse_args()

    smsim_dir = os.getenv('SMSIMDIR')
    if not smsim_dir:
        log("Error: SMSIMDIR not set", Colors.RED)
        sys.exit(1)

    if args.events.lower() == 'all':
        num_events = -1
    else:
        try:
            num_events = int(args.events)
        except ValueError:
            log(f"Invalid events argument: {args.events}", Colors.RED)
            sys.exit(1)

    geometry_mac = os.path.join(
        smsim_dir,
        "configs/simulation/geometry/3deg1.15Tnebulaplus.mac"
    )
    exe = os.path.join(smsim_dir, "bin/sim_deuteron")
    macro_dir = os.path.join(smsim_dir, "build/macros_temp_nebulaplus")

    os.makedirs(macro_dir, exist_ok=True)

    if not os.path.isfile(exe):
        log(f"Error: simulator not found: {exe}", Colors.RED)
        sys.exit(1)
    if not os.path.isfile(geometry_mac):
        log(f"Error: geometry mac not found: {geometry_mac}", Colors.RED)
        sys.exit(1)

    y_jobs = []
    z_jobs = []
    if args.mode in ("y", "both"):
        log("Collecting y-pol jobs...", Colors.BLUE)
        y_jobs = collect_ypol_jobs(smsim_dir)
        log(f"  Found {len(y_jobs)} y-pol jobs", Colors.CYAN)

    if args.mode in ("z", "both"):
        log("Collecting z-pol jobs...", Colors.BLUE)
        z_jobs = collect_zpol_jobs(smsim_dir)
        log(f"  Found {len(z_jobs)} z-pol jobs", Colors.CYAN)

    all_jobs = y_jobs + z_jobs
    total = len(all_jobs)
    log(f"\nTotal jobs: {total}", Colors.BLUE)
    log(f"Geometry:   {geometry_mac}")
    log(f"Events:     {'auto-detect' if num_events < 0 else num_events}")
    log(f"Dry run:    {args.dry_run}")
    log("")

    if total == 0:
        log("No jobs to run.", Colors.YELLOW)
        sys.exit(0)

    success, failed = run_jobs(all_jobs, smsim_dir, geometry_mac, exe,
                               macro_dir, num_events, args.dry_run)

    log("\n" + "=" * 60, Colors.BLUE)
    log("  Batch Run Summary", Colors.BLUE)
    log("=" * 60, Colors.BLUE)
    log(f"Total:    {total}")
    log(f"Success:  {success}", Colors.GREEN)
    if failed > 0:
        log(f"Failed:   {failed}", Colors.RED)
    log("=" * 60, Colors.BLUE)

    if failed > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
