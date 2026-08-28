# Shrink City simulation architecture

The simulation owns all world state. Godot will own presentation, input, saves, and platform lifecycle; it will never own customer or inventory truth. The public C API exposes an opaque `ShrinkWorld`, tick advancement, and aggregate metrics. Read-only snapshots serve rendering without exposing mutable internals.

The first slice uses bounded, contiguous arrays: customers are fixed-capacity records, geometry cells are a compact byte grid, and walls/fixtures are fixed-capacity records with monotonic stable IDs. This is data-oriented enough for the current scale without inventing an ECS. A customer is a state plus numeric fields (position, target, product, wait, satisfaction) plus seeded archetype traits (speed, patience, budget, theft tendency); rendering can map those states to sprites or emoji externally. Employees are a bounded roster with role, wage, skill, fatigue, morale, and position.

Updates are deterministic fixed one-second steps in the CLI. `shrink_tick` accepts a positive `dt` for future real-time integration, but gameplay behavior is designed around fixed steps. Each tick spawns, moves, resolves product decisions, updates registers, assigns queues, and updates metrics.

Randomness is owned by `ShrinkWorld` and uses a small integer generator. No global RNG or wall-clock input is consulted. Equal seed, starting state, and command sequence produce equal metrics.

Navigation uses deterministic grid BFS over the authoritative geometry. Customers select a seeded product, resolve it to a fixture product assignment, and choose the nearest reachable interaction cell with distance/y/x tie-breaking. Register and exit targets are also resolved from fixture records.

Security uses quantitative starter effects: camera count and security staff provide deterministic deterrence, cameras add maintenance cost, and employee wages contribute to labor. Product cost-of-goods is tracked separately from sales revenue so profit is not gross revenue. The data files establish the intended direction for tunable content; loading/validation will be added once the first gameplay variables stabilize.

The current process bridge is bidirectional: C streams geometry/entities and Godot sends text construction commands back for C validation. Godot never mutates authoritative fixture/wall state; its next snapshot is the source of truth. The future boundary should use GDExtension or a small C wrapper that links the same `shrink_sim` library. No Godot headers or concepts belong in `sim/`.


## Construction authority

`ShrinkGeometry` owns the 28×22 walkability grid, bounded wall records, bounded fixture records, doors, checkouts, and stable IDs. `shrink_try_*` commands validate collisions and preserve a reachable entrance → checkout → exit route before mutation. The stream exposes versionable geometry records (`GEOMETRY`, `WALL`, `FIXTURE`) and command results. `FIXTURE` includes its product assignment when present; `ENTITY` includes target fixture and interaction cell fields.

## Customer routing authority

Product destination coordinates are not stored in customer logic. Initial content assigns products 0–3 to stable fixture IDs; each customer resolves its product through the current geometry and recalculates its access cell when construction changes. Missing or inaccessible merchandise causes a deterministic leave/abandon path.

## Curated layouts and balance

The C geometry initializer selects one of eight curated store shells from the requested layout ID/seed. Each uses the same bounded cell grid and required-route validation, but differs in footprint: convenience, strip-mall, L-shaped, bump-out, T-shaped, corner, warehouse, and two-wing sites. The Godot renderer adds inexpensive surrounding site presentation and clamps camera movement around the authoritative site.

`shrink-balance` is a headless CSV runner. It creates fresh authoritative worlds for a seed range and reports revenue, cost of goods, shrink, labor, security, profit, satisfaction, and wait, making tuning comparisons reproducible without Godot.


## Authoritative event ring

The simulation owns a bounded 256-entry event ring alongside snapshots. Events are immutable facts with tick, type, entity/fixture context, product, position, and value. The CLI drains and serializes this ring as `EVENT` records; Godot consumes those same records for its incident feed. The ring drops the oldest record deterministically on overflow rather than allocating in a tick. Current events include entry, item selection, purchase completion, theft attempt, detection, security response/intervention, and theft exit.

Theft is deliberately staged: merchandise is decremented when acquired, an attempted theft emits immediately, camera observation may mark it detected, and shrink is only finalized when the customer reaches the exit. A nearby guard can recover a detected attempt. This keeps attempted, detected, recovered, and completed loss distinct without creating a special criminal entity type.

## Official scripts and user-authored levels

Curated scenarios are the intended home for future deterministic scripted incident tracks (fire, impact, outage, robbery, delivery accident, and similar hazards). These will be bounded C records with authoritative consequences; Godot will only present them. User-created store levels will initially author geometry, fixtures, inventory, staff, cash, and goals, but will not supply arbitrary gameplay scripts. A versioned JSON level format and authoritative floor-surface metadata are planned before exposing the designer as a saveable tool.
