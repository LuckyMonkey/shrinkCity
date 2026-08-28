# Shrink City

Shrink City is a C11 + Godot retail loss-prevention tycoon prototype. Customers enter an authoritative C-authored store, navigate to merchandise, purchase or steal, queue, pay, and exit. The simulation is deterministic from its seed and includes customer archetypes/traits, dynamic employee records, product economics, spatial camera/guard deterrence, construction, and measurable security/labor costs.

The design target is not simply “catch shoplifters.” The player balances shrink against sales, customer friction, labor, throughput, inventory availability, and capital cost. See [GAMEPLAY.md](GAMEPLAY.md) for the broader scenario roadmap and real-retail systems backlog.

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

## Scenarios

Scenarios are now first-class C-side presets rather than Godot-only labels. List them with:

```sh
./build/shrink-sim --list-scenarios
```

Run one headlessly:

```sh
./build/shrink-sim --scenario electronics --days 10 --seed 0
```

Or stream it for presentation:

```sh
./build/shrink-sim --scenario corner-market --stream --realtime --ticks 3600
```

Initial implemented presets are:

- `corner-market` — lean convenience/grocery operation
- `electronics` — high-value merchandise, extra cameras, locked cases, and heavier LP staffing
- `big-box` — larger staffing footprint and rush-oriented checkout pressure
- `pharmacy` — smaller concealable merchandise/security-friction scenario

Scenario variation seeds advance in multiples of the layout count so a scenario keeps its intended curated shell while remaining deterministic.

## Authoritative stream events

The temporary text bridge now emits event records derived from authoritative simulation changes, including shopper entry, completed purchases, recorded thefts, and abandoned trips. These are intentionally small for the first pass; the next milestone is to move richer incident events directly into the C core (attempted theft, detection, intervention, assistance, stockouts, receiving, returns, spills, and other operations).

## Godot prototype

Godot 4 is not required to build the C core. If Godot 4 is installed, open `game/` and run the project after building `build/shrink-sim`.

The default scene is now an RCT-style scenario launcher. A real **Corner Market** simulation is already running behind the menu. The launcher rotates through storefront demos while visible and lets the player switch among Corner Market, Electronics, Big Box, and Pharmacy. Selecting **RUN THIS STORE** leaves the selected authoritative scenario running as the playable prototype. Press Escape to reopen the scenario browser.

`game/main_v3.gd` wraps the V2 renderer with scenario restarts and a small live-event feed. `game/main_v2.gd` remains the underlying current tycoon renderer, while `game/main.gd` remains the older debug-heavy reference implementation.

The process bridge is intentionally temporary and can later be replaced by GDExtension without changing simulation ownership.

### Visual direction

The current presentation uses a brighter tycoon palette, bounded pan/zoom, eight deterministic C-authored store shells, room tinting, full fixture footprints, distinct staff markers, site/parking context, an inspector, and a bottom build palette.

C remains the source of truth for world geometry, construction validation, customers, routing, staffing, security effects, inventory, economics, and scenario setup. Presentation-only department colors/names are still temporary until departments become authoritative snapshot data.

## Authoritative construction and staffing

The C core owns store geometry and validates construction commands. Godot renders C geometry snapshots and sends commands over the temporary bidirectional process bridge. Stream commands include `PLACE`, `MOVE`, `ROTATE`, `REMOVE`, `WALL`, `UNWALL`, `HIRE`, and `FIRE`; rejected commands leave authoritative state unchanged.
