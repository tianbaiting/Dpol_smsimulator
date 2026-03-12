# Principles

## Geometry First

When PDC reconstruction quality changes, first check geometry consistency before changing algorithms.

Priority order:

1. target position / target angle in the geometry macro
2. PDC placement and rotation convention
3. coordinate-frame interpretation for truth momentum
4. only then model or solver details

## Target Position Must Follow Project Sources Of Truth

Do not silently replace project target-position logic with a private calculation.

Repository notes explicitly call out that target position should come from the project geometry / acceptance flow rather than being re-derived independently. When a task depends on target position, first locate the existing project source of truth and reuse it.

## Understand What NN Solves

The current NN backend predicts target momentum from two reconstructed PDC space points.

It does not:

- solve target vertex coordinates directly
- perform RK stepping internally
- infer geometry on its own

Therefore, NN accuracy is strongly coupled to the geometry and feature distribution used during dataset generation.

## Geometry Domain Match Matters

For NN training and application, geometry mismatch is a first-class failure mode.

Check these together:

- target position
- target angle
- PDC geometry macro
- track-point definition and smearing assumptions
- whether the model was trained on synthetic narrow-window data, domain-matched retraining data, or production campaign data

## PDC Fixed-Position Convention Can Differ By Context

Repository notes also warn that fixed PDC positions may be defined differently across simulation and filtering contexts, especially around rotation conventions. Before using a fixed PDC setup, verify how the position is defined in that subsystem instead of assuming a shared convention.

## Prefer Existing Production Anchors

When the user asks for "the current NN chain" or "the corrected-target setup", prefer the existing production script and corrected-target geometry macro instead of assembling a new path from scratch.
