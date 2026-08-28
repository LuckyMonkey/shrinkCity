# Shrink City

Shrink City is a C11 + Godot retail loss-prevention tycoon prototype. Customers enter an authoritative C-authored store, navigate to merchandise, purchase or steal, queue, pay, and exit. The simulation is deterministic from its seed and now includes customer archetypes/traits, dynamic employee records, product economics, spatial camera/guard deterrence, construction, and measurable security/labor costs.

The design target is not simply “catch shoplifters.” The player balances shrink against sales, customer friction, labor, throughput, inventory availability, and capital cost. See [GAMEPLAY.md](GAMEPLAY.md) for the scenario/attract-mode plan and the growing list of real retail systems.

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

See [ARCHITECTURE.md](ARCHITECTURE.md) for ownership, determinism, and the planned Godot boundary. `data/` contains deliberately small external content examples while schemas settle.

`shrink-balance` runs controlled, repeatable seed batches. It can compare authored stores against camera and staffing strategies (`authored`, `no_cameras`, `extra_cameras`, `no_guard`, `extra_guard`, and `lean_staff`) so balance changes can be tested instead of tuned only by feel.

## Godot prototype

Godot 4 is not required to build the C core. If Godot 4 is installed, open `game/` and run the project after building `build/shrink-sim`. The prototype launches `shrink-sim --stream` and renders its read-only snapshots. The active V2 renderer uses a brighter tycoon palette, bounded pan/zoom, eight deterministic C-authored store shells, room tinting, full fixture footprints, and distinct staff markers. This process bridge is intentionally temporary and can later be replaced by GDExtension without changing simulation ownership.

For alternate visual test runs, pass arguments after `--`: `godot --path game -- --seed=7 --ticks=120`. The C test suite also validates the snapshot stream with `ctest`. Geometry snapshots include fixture product assignments; entity snapshots include target fixture IDs and target interaction cells for routing diagnostics.

### Next presentation milestone: attract mode + scenario browser

Launching the finished prototype should feel closer to RollerCoaster Tycoon than a level editor: a real store should already be running behind a scenario browser, with the camera surfacing queues, shoppers, guard patrols and loss-prevention incidents. Choosing a prefab such as **Corner Market**, **Electronics / High Value**, **Big Box Saturday Rush**, **Pharmacy**, or **Troubled Store Turnaround** should restart the authoritative C simulation with that scenario's layout, assortment, staff, security, traffic and goals. The demo must use real simulation state rather than scripted fake incidents.

### Visual direction prototype

`game/main_v2.gd` is the current presentation experiment and is intentionally separate from the older debug-heavy `main.gd`. V2 moves toward classic tycoon/readable-management-game presentation: larger store framing, quieter floor grid, visually distinct zones, stronger walls/shadows, reduced fixture label clutter, compact metric chips, a dedicated inspector, and a bottom build palette.

This is presentation only. The C core remains the source of truth for world geometry, construction validation, customers, routing, staffing, security effects, inventory, and economics. Presentation-only zone names/colors in V2 are temporary until departments are authoritative snapshot data.

## Authoritative construction

The C core owns store geometry and validates construction commands. Godot renders C geometry snapshots and sends commands over the temporary bidirectional process bridge. Stream commands include `PLACE`, `MOVE`, `ROTATE`, `REMOVE`, `WALL`, `UNWALL`, `HIRE`, and `FIRE`; rejected commands leave authoritative state unchanged.
