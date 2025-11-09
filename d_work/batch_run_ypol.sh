#!/bin/bash
# Batch run script for ypol ROOT data files
# Author: Auto-generated
# Date: 2025-11-09

# Color definitions for better output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check if SMSIMDIR is set
if [ -z "$SMSIMDIR" ]; then
    echo -e "${RED}Error: SMSIMDIR environment variable is not set${NC}"
    exit 1
fi

# Define directories
INPUT_DIR="$SMSIMDIR/d_work/rootfiles/ypol"
OUTPUT_BASE_DIR="$SMSIMDIR/d_work/output_tree/ypol"
MACRO_DIR="$SMSIMDIR/d_work/macros_temp"
GEOMETRY_MAC="$SMSIMDIR/d_work/geometry/5deg_1.2T.mac"

# Number of events to run per file (default: 1000)
NUM_EVENTS=${1:-1000}

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Batch Running Y-Polarization Data${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "Input directory:  ${INPUT_DIR}"
echo -e "Output directory: ${OUTPUT_BASE_DIR}"
echo -e "Events per file:  ${NUM_EVENTS}"
echo -e "${BLUE}========================================${NC}\n"

# Create necessary directories
mkdir -p "$MACRO_DIR"
mkdir -p "$OUTPUT_BASE_DIR"

# Check if input directory exists
if [ ! -d "$INPUT_DIR" ]; then
    echo -e "${RED}Error: Input directory does not exist: ${INPUT_DIR}${NC}"
    exit 1
fi

# Check if geometry file exists
if [ ! -f "$GEOMETRY_MAC" ]; then
    echo -e "${YELLOW}Warning: Geometry file not found: ${GEOMETRY_MAC}${NC}"
fi

# Counter for successful runs
SUCCESS_COUNT=0
TOTAL_COUNT=0

# Loop through all ROOT files in ypol directory
for input_file in "$INPUT_DIR"/*.root; do
    # Check if files exist (in case directory is empty)
    if [ ! -f "$input_file" ]; then
        echo -e "${YELLOW}No ROOT files found in ${INPUT_DIR}${NC}"
        exit 0
    fi
    
    # Extract filename without path and extension
    filename=$(basename "$input_file" .root)
    
    # Parse target and gamma from filename (e.g., ypol_np_Pb208_g050)
    # Format: ypol_np_{target}_g{gamma}
    target=$(echo "$filename" | sed 's/ypol_np_\(.*\)_g.*/\1/')
    gamma=$(echo "$filename" | sed 's/.*_g\(.*\)/\1/')
    
    # Create output subdirectory for this target and gamma
    output_subdir="${OUTPUT_BASE_DIR}/${target}_g${gamma}"
    mkdir -p "$output_subdir"
    
    # Define output run name
    run_name="${filename}"
    
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    
    echo -e "${GREEN}[${TOTAL_COUNT}] Processing: ${filename}${NC}"
    echo -e "    Target: ${target}, Gamma: ${gamma}"
    echo -e "    Input:  ${input_file}"
    echo -e "    Output: ${output_subdir}/"
    
    # Create temporary macro file for this run
    macro_file="${MACRO_DIR}/run_${filename}.mac"
    
    cat > "$macro_file" <<EOF
# Auto-generated macro for ${filename}
# Generated on: $(date)

# File settings
/action/file/OverWrite y
/action/file/RunName ${run_name}
/action/file/SaveDirectory ${output_subdir}

# Load geometry
/control/execute ${GEOMETRY_MAC}

# Set up visualization (optional, comment out for batch mode)
# /vis/open OGL
# /vis/drawVolume
# /vis/scene/add/trajectories smooth

# Tracking settings
/tracking/storeTrajectory 1

# Gun settings - use Tree input
/action/gun/Type Tree
/action/gun/tree/InputFileName ${input_file}
/action/gun/tree/TreeName tree

# Run simulation
/action/gun/tree/beamOn ${NUM_EVENTS}

# Exit
exit
EOF
    
    echo -e "    Macro:  ${macro_file}"
    
    # Run the simulation
    echo -e "${YELLOW}    Running simulation...${NC}"
    
    # Run smsimulator (adjust the command based on your setup)
    # Assuming the executable is in $SMSIMDIR/bin/smsimulator
    if [ -f "$SMSIMDIR/bin/smsimulator" ]; then
        cd "$SMSIMDIR/d_work"
        "$SMSIMDIR/bin/smsimulator" "$macro_file" > "${output_subdir}/${run_name}.log" 2>&1
        
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}    ✓ Simulation completed successfully${NC}"
            SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
        else
            echo -e "${RED}    ✗ Simulation failed (check log: ${output_subdir}/${run_name}.log)${NC}"
        fi
    else
        echo -e "${RED}    ✗ smsimulator executable not found at $SMSIMDIR/bin/smsimulator${NC}"
        echo -e "${YELLOW}    Please run manually: smsimulator ${macro_file}${NC}"
    fi
    
    echo ""
done

# Summary
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Batch Run Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "Total files processed: ${TOTAL_COUNT}"
echo -e "Successful runs:       ${GREEN}${SUCCESS_COUNT}${NC}"
echo -e "Failed runs:           ${RED}$((TOTAL_COUNT - SUCCESS_COUNT))${NC}"
echo -e "${BLUE}========================================${NC}"

# Clean up temporary macros (optional)
read -p "Delete temporary macro files? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    rm -rf "$MACRO_DIR"
    echo -e "${GREEN}Temporary macros deleted${NC}"
else
    echo -e "${YELLOW}Temporary macros kept in: ${MACRO_DIR}${NC}"
fi

echo -e "\n${GREEN}All done!${NC}"
