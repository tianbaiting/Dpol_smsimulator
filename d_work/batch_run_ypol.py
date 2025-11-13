#!/usr/bin/env python3
"""
Batch run script for Y-polarization ROOT data files
Automatically processes all files in rootfiles/ypol/ directory
Outputs organized by target and gamma in output_tree/ypol/
"""

import os
import sys
import subprocess
import glob
from pathlib import Path
from datetime import datetime

# Color codes for terminal output
class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    CYAN = '\033[0;36m'
    NC = '\033[0m'  # No Color

def print_colored(message, color=Colors.NC):
    """Print colored message to terminal"""
    print(f"{color}{message}{Colors.NC}")

def create_macro_file(input_file, output_dir, run_name, geometry_mac, num_events):
    """Create a Geant4 macro file for a single run"""
    macro_content = f"""# Auto-generated macro for {run_name}
# Generated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}

# File settings
/action/file/OverWrite y
/action/file/RunName {run_name}
/action/file/SaveDirectory {output_dir}

# Load geometry
/control/execute {geometry_mac}

# Tracking settings
/tracking/storeTrajectory 1

# Gun settings - use Tree input
/action/gun/Type Tree
/action/gun/tree/InputFileName {input_file}
/action/gun/tree/TreeName tree

# Run simulation
/action/gun/tree/beamOn {num_events}

# Exit
exit
"""
    return macro_content

def main():
    # Get SMSIMDIR from environment
    smsim_dir = os.getenv('SMSIMDIR')
    if not smsim_dir:
        print_colored("Error: SMSIMDIR environment variable is not set", Colors.RED)
        sys.exit(1)
    
    # Define directories
    input_dir = os.path.join(smsim_dir, 'd_work/rootfiles/ypol_slect_rotate_back')
    output_base_dir = os.path.join(smsim_dir, 'd_work/output_tree/ypol_slect_rotate_back')
    macro_dir = os.path.join(smsim_dir, 'd_work/macros_temp')
    geometry_mac = os.path.join(smsim_dir, 'd_work/geometry/5deg_1.2T.mac')
    simulator_exe = os.path.join(smsim_dir, 'bin/sim_deuteron')
    
    # Number of events (can be passed as command line argument)
    # You can pass a positive integer for a fixed number of events per file,
    # or pass the string 'all' (or 0) to run ALL events contained in each
    # input ROOT tree. When 'all' is used the script will auto-detect the
    # number of entries in the input file's TTree named 'tree'.
    raw_arg = sys.argv[1] if len(sys.argv) > 1 else None
    if raw_arg is None:
        num_events = 1000
    elif isinstance(raw_arg, str) and raw_arg.lower() == 'all':
        num_events = -1  # sentinel: detect per-file
    else:
        try:
            num_events = int(raw_arg)
        except Exception:
            print_colored(f"Invalid argument for num_events: {raw_arg}", Colors.RED)
            sys.exit(1)
    
    # Print header
    print_colored("=" * 60, Colors.BLUE)
    print_colored("  Batch Running Y-Polarization Data", Colors.BLUE)
    print_colored("=" * 60, Colors.BLUE)
    print(f"Input directory:  {input_dir}")
    print(f"Output directory: {output_base_dir}")
    print(f"Events per file:  {num_events}")
    print_colored("=" * 60, Colors.BLUE)
    print()
    
    # Create necessary directories
    os.makedirs(macro_dir, exist_ok=True)
    os.makedirs(output_base_dir, exist_ok=True)
    
    # Check if input directory exists
    if not os.path.isdir(input_dir):
        print_colored(f"Error: Input directory does not exist: {input_dir}", Colors.RED)
        sys.exit(1)
    
    # Get all ROOT files in ypol directory
    root_files = sorted(glob.glob(os.path.join(input_dir, '*.root')))
    
    if not root_files:
        print_colored(f"No ROOT files found in {input_dir}", Colors.YELLOW)
        sys.exit(0)
    
    print_colored(f"Found {len(root_files)} ROOT files to process\n", Colors.CYAN)
    
    # Counters
    success_count = 0
    total_count = 0
    
    # Process each ROOT file
    for input_file in root_files:
        total_count += 1
        
        # Extract filename without path and extension
        filename = Path(input_file).stem
        
        # Parse target and gamma from filename (e.g., ypol_np_Pb208_g050)
        # Format: ypol_np_{target}_g{gamma}
        parts = filename.split('_')
        if len(parts) >= 4:
            target = parts[2]  # e.g., Pb208 or Xe130
            gamma = parts[3]   # e.g., g050
        else:
            print_colored(f"Warning: Cannot parse filename: {filename}", Colors.YELLOW)
            target = "unknown"
            gamma = "g000"
        
        # Create output subdirectory for this target and gamma
        output_subdir = os.path.join(output_base_dir, f"{target}_{gamma}")
        os.makedirs(output_subdir, exist_ok=True)
        
        # Define run name
        run_name = filename
        
        print_colored(f"[{total_count}/{len(root_files)}] Processing: {filename}", Colors.GREEN)
        print(f"    Target: {target}, Gamma: {gamma}")
        print(f"    Input:  {input_file}")
        print(f"    Output: {output_subdir}/")
        
        # Determine events to run for this file. If num_events <= 0 we auto-detect
        # the number of entries in the ROOT tree named 'tree'. Try using uproot
        # first (lightweight), otherwise fall back to running a tiny ROOT macro.
        events_to_run = num_events
        if num_events <= 0:
            # Try uproot first
            events_to_run = None
            try:
                import uproot
                with uproot.open(input_file) as f:
                    tree = f['tree']
                    events_to_run = int(tree.num_entries)
            except Exception:
                # Fallback to calling ROOT to get entries
                try:
                    tmp_macro = os.path.join(macro_dir, f"_get_entries_{filename}.C")
                    macro_content_entries = (
                        '{\n'
                        '  TFile *f = TFile::Open("' + input_file.replace('\\', '\\\\') + '");\n'
                        '  if (!f) { std::cout << -1 << std::endl; return; }\n'
                        '  TTree *t = (TTree*)f->Get("tree");\n'
                        '  if (!t) { std::cout << -1 << std::endl; return; }\n'
                        '  std::cout << t->GetEntries() << std::endl;\n'
                        '}'
                    )
                    # Write macro safely
                    with open(tmp_macro, 'w') as mf:
                        mf.write(macro_content_entries)

                    res = subprocess.run(['root', '-l', '-b', '-q', tmp_macro],
                                         capture_output=True, text=True)
                    out = res.stdout.strip() if res.stdout else res.stderr.strip()
                    # Extract first integer from output
                    import re
                    m = re.search(r"(\d+)", out)
                    if m:
                        events_to_run = int(m.group(1))
                    else:
                        events_to_run = 0
                except Exception:
                    events_to_run = 0

        # Safety: if detection failed or returned 0, fall back to 1000
        if events_to_run is None or events_to_run <= 0:
            print_colored(f"    ⚠ Could not detect entries for {filename}, using 1000 events", Colors.YELLOW)
            events_to_run = 1000

        # Create macro file
        macro_file = os.path.join(macro_dir, f"run_{filename}.mac")
        macro_content = create_macro_file(input_file, output_subdir, run_name,
                                         geometry_mac, events_to_run)
        
        with open(macro_file, 'w') as f:
            f.write(macro_content)
        
        print(f"    Macro:  {macro_file}")
        
        # Run the simulation
        print_colored("    Running simulation...", Colors.YELLOW)
        
        log_file = os.path.join(output_subdir, f"{run_name}.log")
        
        try:
            # Change to d_work directory before running
            work_dir = os.path.join(smsim_dir, 'd_work')
            
            # Run smsimulator
            with open(log_file, 'w') as log:
                result = subprocess.run(
                    [simulator_exe, macro_file],
                    cwd=work_dir,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                    timeout=3600  # 1 hour timeout
                )
            
            if result.returncode == 0:
                print_colored("    ✓ Simulation completed successfully", Colors.GREEN)
                success_count += 1
            else:
                print_colored(f"    ✗ Simulation failed (check log: {log_file})", Colors.RED)
        
        except FileNotFoundError:
            print_colored(f"    ✗ smsimulator executable not found at {simulator_exe}", Colors.RED)
            print_colored(f"    Please run manually: {simulator_exe} {macro_file}", Colors.YELLOW)
        
        except subprocess.TimeoutExpired:
            print_colored("    ✗ Simulation timed out (>1 hour)", Colors.RED)
        
        except Exception as e:
            print_colored(f"    ✗ Error: {e}", Colors.RED)
        
        print()
    
    # Print summary
    print_colored("=" * 60, Colors.BLUE)
    print_colored("  Batch Run Summary", Colors.BLUE)
    print_colored("=" * 60, Colors.BLUE)
    print(f"Total files processed: {total_count}")
    print_colored(f"Successful runs:       {success_count}", Colors.GREEN)
    print_colored(f"Failed runs:           {total_count - success_count}", Colors.RED)
    print_colored("=" * 60, Colors.BLUE)
    
    # Ask about cleaning up temporary macros
    try:
        response = input("\nDelete temporary macro files? (y/n): ").strip().lower()
        if response == 'y':
            import shutil
            shutil.rmtree(macro_dir)
            print_colored("Temporary macros deleted", Colors.GREEN)
        else:
            print_colored(f"Temporary macros kept in: {macro_dir}", Colors.YELLOW)
    except KeyboardInterrupt:
        print()
        print_colored(f"Temporary macros kept in: {macro_dir}", Colors.YELLOW)
    
    print_colored("\nAll done!", Colors.GREEN)

if __name__ == "__main__":
    main()
