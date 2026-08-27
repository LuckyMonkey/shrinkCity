# Shrink City

The current milestone is a headless C11 retail simulation: customers enter a small store, navigate to merchandise, purchase or steal, queue at two registers, pay, and exit. Metrics are reproducible from a seed.

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

See [ARCHITECTURE.md](ARCHITECTURE.md) for ownership, determinism, and the planned Godot boundary. `data/` contains deliberately small external content examples; v0 uses equivalent validated constants while the schema is still settling.


## Godot prototype

Godot 4 is not required to build the C core. If Godot 4 is installed, open `game/` and run the project after building `build/shrink-sim`. The prototype launches `shrink-sim --stream` and renders its read-only snapshots. This process bridge is intentionally temporary and can later be replaced by GDExtension without changing simulation ownership.


For alternate visual test runs, pass arguments after `--`: `godot --path game -- --seed=7 --ticks=120`. The C test suite also validates the snapshot stream with `ctest`.


## Authoritative construction

The C core now owns the store geometry and validates construction commands. The Godot builder renders C geometry snapshots and sends commands over the temporary bidirectional process bridge. Stream commands include `PLACE`, `MOVE`, `ROTATE`, `REMOVE`, `WALL`, and `UNWALL`; rejected commands return a nonzero build status and leave the C world unchanged.
