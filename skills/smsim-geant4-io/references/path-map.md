# Path Map

## Source Of Truth Paths

- `apps/sim_deuteron/main.cc`
- `libs/smg4lib/src/action/src/ActionBasicMessenger.cc`
- `libs/smg4lib/src/action/src/PrimaryGeneratorActionBasic.cc`
- `libs/smg4lib/src/action/src/RunActionBasic.cc`
- `libs/smg4lib/src/action/src/EventActionBasic.cc`
- `scripts/event_generator/dpol_eventgen/GenInputRoot_qmdrawdata.cc`
- `scripts/simulation/run_g4input_batch.sh`
- `configs/simulation/macros/`
- `libs/analysis/src/EventDataReader.cc`

## Update Rule

If any path above is moved, update this file first, then update `scripts/check_sync.sh`.
