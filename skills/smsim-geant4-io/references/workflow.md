# Workflow

## Scope

- Geant4 macro input commands (`/action/file/*`, `/action/gun/tree/*`)
- tree input (`TBeamSimData`)
- simulation ROOT output tree writing (`FragSimData`, optional `NEBULAPla`, optional `beam`)
- QMD rawdata -> g4input ROOT conversion

## Entry Points

- `apps/sim_deuteron/main.cc`
- `libs/smg4lib/src/action/src/ActionBasicMessenger.cc`
- `libs/smg4lib/src/action/src/PrimaryGeneratorActionBasic.cc`
- `libs/smg4lib/src/action/src/RunActionBasic.cc`
- `libs/smg4lib/src/action/src/EventActionBasic.cc`
- `scripts/event_generator/dpol_eventgen/GenInputRoot_qmdrawdata.cc`
- `scripts/simulation/run_g4input_batch.sh`
- `libs/analysis/src/EventDataReader.cc`

## Focused Validation

```bash
cd build && make -j$(nproc) sim_deuteron GenInputRoot_qmdrawdata
./bin/sim_deuteron ../configs/simulation/macros/test_g4input_ypol_np_Pb208_g050.mac
```

If reader compatibility changed, add a minimal read check through `EventDataReader` paths above.
