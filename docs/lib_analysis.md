# libs/analysis Overview

This document summarizes the `libs/analysis` module structure and runtime flow based on the C++ headers and sources referenced by `build/compile_commands.json`.

## Purpose
`libs/analysis` provides ROOT-based analysis tools for:
- loading simulation events from ROOT trees,
- parsing geometry and magnetic field maps,
- reconstructing charged tracks and neutrons,
- back-propagating tracks to a target,
- visualizing events in ROOT EVE.

## High-Level Data Flow
1. `EventDataReader` opens a ROOT file and exposes event branches (`FragSimData`, optional `NEBULAPla`, optional `beam`).
2. `GeometryManager` parses detector and target geometry from a Geant4 macro.
3. `PDCSimAna` reconstructs charged-track points (PDC1/PDC2) into a `RecoEvent`.
4. `NEBULAReconstructor` clusters NEBULA hits and reconstructs neutron kinematics into `RecoEvent`.
5. `TargetReconstructor` back-propagates reconstructed tracks through `MagneticField` to estimate target momentum.
6. `EventDisplay` renders geometry, hits, tracks, and reconstruction results in ROOT EVE.

## Core Types
### RecoEvent (libs/analysis/include/RecoEvent.hh)
Container for reconstructed data per event:
- `rawHits`, `smearedHits` (`RecoHit`)
- `tracks` (`RecoTrack`)
- `neutrons` (`RecoNeutron`)
- `eventID`

## Key Components
### EventDataReader (libs/analysis/include/EventDataReader.hh)
Loads ROOT files and exposes event navigation:
- `NextEvent()`, `PrevEvent()`, `GoToEvent()`
- `GetHits()`, `GetNEBULAHits()`
- Beam access via `GetBeamData()` when the `beam` branch exists.

### GeometryManager (libs/analysis/include/GeometryManager.hh)
Parses geometry parameters from a macro file:
- PDC angle and positions
- Target position and angle
All stored in mm and radians for consistency.

### MagneticField (libs/analysis/include/MagneticField.hh)
Loads a 3D field map and provides:
- `GetField()` in lab coordinates (with rotation)
- `GetFieldRaw()` in magnet coordinates
- trilinear interpolation
- optional symmetry handling and ROOT file I/O

### ParticleTrajectory (libs/analysis/include/ParticleTrajectory.hh)
Runge-Kutta integration in a non-uniform magnetic field:
- `CalculateTrajectory()` returns a list of `TrajectoryPoint`
- Supports charged and neutral propagation

### PDCSimAna (libs/analysis/include/PDCSimAna.hh)
Reconstructs PDC hits into a `RecoEvent`:
- Computes centers-of-mass for U/V planes
- Provides smeared hits for detector resolution studies

### NEBULAReconstructor (libs/analysis/include/NEBULAReconstructor.hh)
Neutron reconstruction pipeline:
- hit extraction, clustering, TOF-based energy estimate
- `ReconstructNeutrons()` returns a list of `RecoNeutron`

### TargetReconstructor (libs/analysis/include/TargetReconstructor.hh)
Back-propagates reconstructed tracks to a target:
- brute-force search, gradient descent, or ROOT TMinuit
- returns `TargetReconstructionResult` with trajectories and optimization history

### EventDisplay (libs/analysis/include/EventDisplay.hh)
ROOT EVE visualization:
- geometry loading
- event and track drawing
- target reconstruction overlays

### EventProcessor (libs/analysis/include/EventProcessor.hh)
Entry point skeleton for custom analysis loops over ROOT trees.

## Notes
- ROOT dictionaries are required for classes used in trees (`TBeamSimData`, `RecoEvent`, etc.).
- `libs/analysis` is ROOT-heavy; build and runtime require proper ROOT setup.
