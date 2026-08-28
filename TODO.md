# Shrink City implementation backlog

This file is the continuation plan for GPT/agents working on the project. The C simulation remains authoritative; Godot owns presentation, input, construction UI, and platform integration. Do not silently replace the C core with Godot-only gameplay.

## Current status

Implemented and verified:

- deterministic seeded C11 headless simulation
- contiguous customer storage and opaque C API
- inventory, purchase, theft, checkout queues, metrics, BFS movement
- CLI day simulation and snapshot stream
- CTest invariants, determinism, pathfinding, and stream protocol checks
- Godot 4 process bridge to the C simulation
- isometric store view, panning, zoom, selection, tooltips, fixture placement
- irregular rooms, floor grid, walls, departments, shelf product markers
- generated full-color shopper/product/security raster assets

Prototype-only and not yet authoritative:

- departments, entrances, addons, guard patrol, FOV, and inspector properties are still prototype/data work; fixture and wall construction authority is in C
- customers are still one simple product trip; staff/security are mostly visual
- Godot process bridge must eventually become a GDExtension or equivalent native FFI

## Priority 1 — authoritative world/construction model

- [x] Add C world dimensions, walkable-cell map, wall segments, fixtures, entrances, exits, and registers/checklanes (rooms/departments remain Godot/data work).
- [x] Add C commands/API for place, move, rotate, remove, and add/remove wall segments.
- [x] Validate construction commands before applying them: bounds, collisions, required entrance, required exit, and connected checkout/exit route.
- [ ] Support multiple entrances and exits with per-door properties: capacity, direction, open/closed state, detector upgrades, and customer flow.
- [ ] Add store expansion purchases: adjacent parcels/portions, cost, construction time, new walls, parking, loading dock, and unlockable departments.
- [x] Add a read-only world snapshot containing dimensions, walls, fixtures, doors, customers, and stable IDs for Godot.
- [ ] Replace the temporary Godot process bridge with a native GDExtension/FFI adapter using the same C API.

Acceptance: a headless command sequence can build an irregular store, move every fixture, add/remove doors safely, and serialize/replay the same result from the same seed.

## Priority 2 — people and heuristics

- [x] Add a bounded employee roster with id, role, wage, skill, fatigue, morale, assignment position, and snapshot data.
- [ ] Add hiring, firing, wages, schedules, training/upgrades, employee statistics, and selectable employee inspector data.
- [ ] Add roaming security guards with grid pathfinding, patrol routes, detection skill, deterrence, response time, fatigue, and coverage.
- [ ] Add security policy toggles globally and per guard: patrol intensity, receipt checks, intervention threshold, camera monitoring, and customer-friction settings.
- [x] Add deterministic customer archetypes and starter traits: shopping speed, budget, patience, theft tendency, and product choice.
- [ ] Add behavior heuristics: browse, compare, seek assistance, abandon, purchase, steal, conceal, react to congestion, and react to security friction.
- [ ] Make all simulated people move along valid authoritative grid paths and expose their current path/intent in snapshots. (Customer product target fixture/access-cell intent is implemented; full paths remain.)

Acceptance: two identical seeds produce identical people, assignments, routes, decisions, thefts, waits, and metrics; different seeds produce varied but reproducible populations.

## Priority 3 — inventory, departments, and merchandise

- [ ] Load validated `products.json`, `fixtures.json`, `security.json`, and `scenarios.json` into internal structs.
- [x] Add starter product costs, prices, demand, theft risk, and cost-of-goods accounting.
- [x] Route customers to product fixtures and deterministic reachable interaction cells; revalidate targets after construction changes.
- [ ] Add shelf variants: gondola, short shelf, bin, locked shelf, clear case, clearance rack, sale display, endcap, cooler, and high-value case.
- [ ] Add shelf capacity, facing count, item placement, stock levels, restock thresholds, and employee restocking tasks.
- [ ] Render product sprites/icons on shelves and expose item/departments/value/stock as tooltip properties.
- [ ] Add RFID workflows: tag individual items, tag departments, tagging stations, tag cost, random sampling, full tagging, tag coverage, and tag removal/maintenance.
- [ ] Add inventory checks by employees and discrepancies from miscounts, damage, and shrink.

Acceptance: moving a shelf or department changes pathing, display capacity, demand access, inventory, labor, and theft outcomes in headless runs.

## Priority 4 — security and ordinances

- [ ] Add camera fixtures with ceiling-layer rendering, configurable range/cone, occlusion/coverage, monitoring assignment, detection chance, and maintenance cost.
- [ ] Add guard/camera FOV snapshots for Godot visualization and selection overlays.
- [ ] Add entrance/exit detector types: EAS, metal detector, receipt gate, RFID gate, and configurable false-positive/friction rates.
- [ ] Add ordinances/policies: receipt checks, bag checks, locked cases, camera retention, guard ratios, accessibility rules, and customer-rights/friction modifiers.
- [ ] Support global policy toggles and overrides for individual guards, doors, departments, and checklanes.
- [ ] Test security tradeoffs against shrink, sales, throughput, satisfaction, labor, false positives, and complaints.

Acceptance: every security control changes measurable simulation outcomes, and excessive security can lower satisfaction/throughput rather than merely increasing numbers.

## Priority 5 — tycoon construction and presentation

- [ ] Replace temporary Godot fixture arrays with C snapshot-backed fixtures.
- [ ] Implement proper drag placement, rotation, multi-select, copy/paste, delete confirmation, and undo/redo.
- [ ] Implement wall drawing with segment snapping, doors/gaps, room connectivity preview, and “would trap customers” validation.
- [ ] Improve isometric camera scale, zoom limits, tile readability, wall height, ceiling toggle, lighting, shadows, and Z-order.
- [ ] Add parking, road, sidewalks, landscaping, loading dock, delivery vehicles, dumpsters, staff entrance, and neighboring parcels.
- [ ] Add multiple views: sales floor, stockroom, staff/security, roof/camera coverage, and construction overlay.
- [ ] Keep full-color generated sprite assets, but add atlas metadata and fallback silhouettes for platforms without the required texture support.

Acceptance: the player can build and visually inspect a non-rectangular store, understand walls/doors/FOV/grid, and rearrange fixtures without losing simulation consistency.

## Testing and tooling

- [ ] Add C unit tests for wall/door connectivity and no-trap invariants.
- [ ] Add tests for multiple entrances/exits and detector upgrades.
- [ ] Add tests for employee hiring/firing/wages/assignments.
- [ ] Add tests for customer archetype repeatability and behavior distributions.
- [ ] Add tests for RFID per-item/per-department/random tagging costs.
- [ ] Add property/fuzz tests for arbitrary construction commands and bounded arrays.
- [x] Add a deterministic headless balance runner with CSV output.
- [ ] Add Godot smoke tests for snapshot parsing, fixture selection, drag placement, wall drawing, and alternate seeds.
- [x] Add CI on Linux for normal + sanitizer CTest and Godot headless validation.

## Suggested next implementation order

1. Add multi-door properties, expansion parcels, and construction-time topology previews.
2. Add employee hiring/firing and roaming guard pathfinding.
3. Load validated product/fixture/security data and add restocking/department demand.
4. Add Godot native FFI and replace the temporary process bridge.
5. Add RFID/detector/ordinance systems and balance tests.
