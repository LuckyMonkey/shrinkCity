# Shrink City simulation architecture

The simulation owns all world state. Godot will own presentation, input, saves, and platform lifecycle; it will never own customer or inventory truth. The public C API exposes an opaque `ShrinkWorld`, tick advancement, and aggregate metrics. A later read-only snapshot/iteration API can serve rendering without exposing mutable internals.

The first slice uses bounded, contiguous arrays: customers are fixed-capacity records and products/registers are small arrays. This is data-oriented enough for the current scale without inventing an ECS. A customer is a state plus numeric fields (position, target, product, wait, satisfaction); rendering can map those states to sprites or emoji externally.

Updates are deterministic fixed one-second steps in the CLI. `shrink_tick` accepts a positive `dt` for future real-time integration, but gameplay behavior is designed around fixed steps. Each tick spawns, moves, resolves product decisions, updates registers, assigns queues, and updates metrics.

Randomness is owned by `ShrinkWorld` and uses a small integer generator. No global RNG or wall-clock input is consulted. Equal seed, starting state, and command sequence produce equal metrics.

Navigation in v0 is deterministic grid BFS movement in the 20x20 store layout: entrance at `(1,10)`, four merchandise positions, and two register positions. The next navigation task is making fixture geometry data-driven while retaining the same simulation-facing coordinates.

Security is currently represented by two fixed camera coverage assumptions in the theft probability. The data files establish the intended direction for tunable content; loading/validation will be added once the first gameplay variables stabilize.

The future Godot boundary should use GDExtension or a small C wrapper that links the same `shrink_sim` library. Keep it pull-based: Godot advances ticks and requests snapshots/metrics. No Godot headers or concepts belong in `sim/`.
