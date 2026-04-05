# IPS Scan Workflow

## Current study definition

- Geometry macro: `configs/simulation/geometry/3deg_1.15T.mac`
- Target handling: `Target/SetTarget false`
- IPS purpose: detect deep inelastic-like small-`b` events and provide veto, while suppressing elastic-breakup leakage
- Official scan script: `scripts/analysis/run_ips_position_scan_3deg115T_smallb_all_noforward.sh`
- Full-statistics scan script: `scripts/analysis/run_ips_position_scan_3deg115T_smallb_all_noforward_fullstats.sh`
- Official WRL export script: `scripts/analysis/export_ips_wrl_examples_3deg115T_offset0.sh`
- Detailed run note: `docs/mechanic/veto_impactPrameterSelector/ips_scan_3deg_1p15T_no_forward.zh.md`

## Event selection rules

### Elastic background

- Source files: cleaned `dbreakb01..b10.root` merged from `d+Sn124-ypolE190g050ypn` and `...ynp`
- Remove unphysical breakup in `GenInputRoot_qmdrawdata.cc`
- For `ypol`, apply `|pyp - pyn| < 150 MeV/c`
- For `zpol`, apply `|pzp - pzn| < 150 MeV/c`

### All-event signal

- Source files: `geminioutb01..b07.root` merged from `d+Sn124-ypolE190g050ypn` and `...ynp`
- Small-`b` definition: `bimp <= 7`
- No forward gate in the current official scan

### Sampling caveat in the current official run

- The current script uses `--beam-on 300`
- That means each merged ROOT contributes at most the first 300 tree entries
- The current published `-40 mm` recommendation is therefore a sampled scan result, not a full-statistics result
- The detailed bucket counts are documented in `docs/mechanic/veto_impactPrameterSelector/ips_scan_3deg_1p15T_no_forward.zh.md`

## Geometry and direction conventions

- Target normal equals the local beam direction
- IPS barrel axis is parallel to that same local beam direction
- Signed IPS offset is measured from target center along the rotated beam axis
- Negative offset means the `-ips_axis` side of the target-centered local beam axis

## Key commands

### Convert qmdrawdata text to g4input ROOT

```bash
build/bin/GenInputRoot_qmdrawdata   --mode ypol   --source both   --input-base data/qmdrawdata/qmdrawdata/allevent   --output-base /tmp/ips_sn124_g4input   --target-filter d+Sn124-ypolE190g050
```

### Run the official sampled IPS scan

```bash
scripts/analysis/run_ips_position_scan_3deg115T_smallb_all_noforward.sh
```

### Run the full-statistics IPS scan

```bash
scripts/analysis/run_ips_position_scan_3deg115T_smallb_all_noforward_fullstats.sh
```

### Export WRL examples at offset 0 mm

```bash
scripts/analysis/export_ips_wrl_examples_3deg115T_offset0.sh
```

## Output locations

- Scan outputs: `data/simulation/ips_scan/sn124_3deg_1.15T/results/smallb_all_noForward_beamOn300`
- Full-statistics outputs: `data/simulation/ips_scan/sn124_3deg_1.15T/results/smallb_all_noForward_fullstats`
- Per-offset progress log: `data/simulation/ips_scan/sn124_3deg_1.15T/results/smallb_all_noForward_beamOn300/scan_progress.log`
- Full summary table: `data/simulation/ips_scan/sn124_3deg_1.15T/results/smallb_all_noForward_beamOn300/ips_scan_summary.csv`
- ROOT summary table: `data/simulation/ips_scan/sn124_3deg_1.15T/results/smallb_all_noForward_beamOn300/ips_scan_summary.root`
- Shortlist text: `data/simulation/ips_scan/sn124_3deg_1.15T/results/smallb_all_noForward_beamOn300/best_ips_positions.txt`
- WRL outputs: `data/simulation/ips_wrl/sn124_3deg_1.15T_offset0`
- Geometry-only export macro: `configs/simulation/macros/export_ips_geometry_example.mac`

## Logging and summary behavior

- `scan_progress.log` appends one aggregated line per processed coarse, refine, or validation offset
- `scan_progress.log` includes `cache_hit=1` when a refine-stage offset reuses cached metrics instead of rerunning Geant4
- Each line includes `elastic_leakage`, `smallb_selected_rate`, and `smallb_raw_rate`
- `ips_scan_summary.csv/root` now contain all evaluated scan offsets, not only the top shortlist, and add a `stage` field
- `best_ips_positions.txt` remains the compact shortlist view

## When results change

Update all of the following together:

1. `docs/mechanic/veto_impactPrameterSelector/ips_scan_3deg_1p15T_no_forward.zh.md`
2. `scripts/analysis/run_ips_position_scan_3deg115T_smallb_all_noforward.sh`
3. `scripts/analysis/export_ips_wrl_examples_3deg115T_offset0.sh`
4. `configs/simulation/macros/export_ips_geometry_example.mac` if the reference export changes
