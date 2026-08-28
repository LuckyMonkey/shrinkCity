# Shrink City

The current milestone is a headless C11 retail simulation: customers enter a small store, navigate to merchandise, purchase or steal, queue at two registers, pay, and exit. Metrics are reproducible from a seed. The current core also assigns deterministic customer archetypes/traits, employee wages, product costs, and measurable camera/security costs.

## Build and run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/shrink-sim --days 30 --seed 12345
```

For development sanitizers:

```sh
cmake -S . -B build-asan -DSHRINK_ENABLE_SANITIZERS=ON
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for ownership, determinism, and the planned Godot boundary. `data/` contains deliberately small external content examples; v0 uses equivalent validated constants while the schema is still settling. `shrink-balance` runs repeatable seed batches and emits CSV for tuning.

## Godot prototype

Godot 4 is not required to build the C core. If Godot 4 is installed, open `game/` and run the project after building `build/shrink-sim`. The prototype launches `shrink-sim --stream` and renders its read-only snapshots. The active V2 renderer uses a brighter tycoon palette, bounded pan/zoom, eight deterministic C-authored store shells, room tinting, full fixture footprints, and distinct staff markers. This process bridge is intentionally temporary and can later be replaced by GDExtension without changing simulation ownership.

For alternate visual test runs, pass arguments after `--`: `godot --path game -- --seed=7 --ticks=120`. The C test suite also validates the snapshot stream with `ctest`. Geometry snapshots include fixture product assignments; entity snapshots include target fixture IDs and target interaction cells for routing diagnostics.

### Visual direction prototype

`game/main_v2.gd` is the current presentation experiment and is intentionally separate from the older debug-heavy `main.gd`. The V2 pass moves the prototype toward a classic tycoon/readable-management-game presentation: larger store framing, quieter floor grid, visually distinct zones, stronger walls/shadows, reduced fixture label clutter, compact metric chips, a dedicated inspector, and a bottom build palette.

This is presentation only. The C core remains the source of truth for world geometry, construction validation, customers, routing, inventory, and economics. Presentation-only zone names/colors in V2 are temporary until rooms and departments are authoritative snapshot data.

## Authoritative construction

The C core owns the store geometry and validates construction commands. The Godot builder renders C geometry snapshots and sends commands over the temporary bidirectional process bridge. Stream commands include `PLACE`, `MOVE`, `ROTATE`, `REMOVE`, `WALL`, and `UNWALL`; rejected commands return a nonzero build status and leave the C world unchanged.
