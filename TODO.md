# Shrink City implementation checklist

This is the continuing project checklist for GPT/Codex passes. Items are checked only when the behavior is implemented in the authoritative C simulation or verified in Godot. C11 owns gameplay state; Godot owns presentation, input, camera, construction UI, and platform integration. Do not introduce an ECS or move gameplay authority into GDScript.

## Current verified baseline

- [x] Deterministic seeded C11 simulation with bounded contiguous customer/employee storage.
- [x] Opaque public C API, fixed-step ticks, headless CLI, repeatable metrics, and CTest.
- [x] Authoritative irregular geometry: 8 layouts, floor cells, walls, fixtures, rooms, doors, stable IDs, footprints, rotations, access cells, and route validation.
- [x] Construction commands: place, move, rotate, remove fixtures; add/remove walls; collision, bounds, access, and required-route validation.
- [x] Customers route to product fixtures and reachable interaction cells; targets revalidate after construction changes.
- [x] Customer archetypes/traits: speed, patience, budget, theft tendency, and deterministic product choice.
- [x] Employee records, hiring/firing API, roles, wages, skill, fatigue, morale, positions, and spatial guard patrol.
- [x] Product prices/costs/theft risk, checkout queues, staffed versus self-checkout timing, revenue, COGS, labor, security cost, shrink, and profit.
- [x] Spatial camera/guard deterrence, camera maintenance, authoritative staged incident events, and event-driven Godot incident feed.
- [x] Theft lifecycle: attempt, detection, security response, intervention/recovery, or completed loss at exit.
- [x] Four C-authored scenario presets: corner-market, electronics, big-box, and pharmacy.
- [x] Scenario CLI, scenario metadata, attract-mode launcher, live simulation behind the menu, scenario reset, and event stream.
- [x] Godot V2/V3 isometric presentation: panning, zoom, selection, tooltips, construction feedback, rooms, site context, and full fixture footprints.
- [x] Linux CI with normal and sanitizer builds, CTest, stream checks, balance strategy checks, scenario checks, and Godot headless validation.

## Milestone A — authoritative incidents and operations

### Event and incident foundation

- [x] Replace CLI/Godot metric-delta event inference with a bounded C event ring.
- [x] Expose deterministic event records with tick, type, entity ID, fixture ID, product, position, and value.
- [x] Distinguish theft attempt, detection, response, intervention/recovery, and completed exit loss.
- [ ] Add event queue overflow diagnostics and a documented consumer/acknowledgement contract.
- [ ] Add authoritative event replay tests covering complete event sequences, not only aggregate metrics.

### Security and loss prevention

- [ ] Separate camera deterrence, observation, detection confidence, and intervention confidence in tunable C configuration.
- [ ] Add directional/cone camera coverage and expose coverage cells/FOV in snapshots.
- [ ] Add roaming guard incident assignment: patrol → investigate/respond → intercept → return to patrol.
- [ ] Add guard response distance/time and deterministic success/failure outcomes.
- [ ] Make locked cases reduce theft while creating assistance requests, labor load, wait, and lost conversion.
- [ ] Implement self-checkout missed-scan/under-scan events separately from aisle concealment.
- [ ] Make RFID/EAS tags and exit gates detect unpaid tagged merchandise with false negatives.
- [ ] Add optional receipt-check station with labor, friction, throughput, and detection tradeoffs.
- [ ] Add global and per-guard/door/department security policy toggles.
- [ ] Add security metrics: prevented loss, recovered value, false positives, friction loss, and intervention count.

### Inventory, deliveries, and employee tasks

- [ ] Split inventory into sales-floor quantity, backroom quantity, fixture capacity, and product totals.
- [ ] Add stockout events, lost-sale value, shelf availability, and alternate-product/abandon behavior.
- [ ] Add deterministic deliveries, receiving inventory, delivery backlog, and delivery events.
- [ ] Add bounded employee tasks: cashier, locked-case associate, stocker, security response, and current target/task snapshots.
- [ ] Add stocker restocking from backroom to fixtures and `RESTOCK_STARTED`/`RESTOCKED` events.
- [ ] Add employee availability effects, fatigue/workload, and assistance abandonment.
- [ ] Add product departments and 8–16 useful scenario-specific products.
- [ ] Load validated `data/products.json`, `fixtures.json`, `security.json`, and `scenarios.json` into internal C structs.
- [ ] Add product shelf capacity, facings, replenishment thresholds, and employee inventory checks.
- [ ] Add item/department/random RFID tagging workflows and tagging costs.

### Customers and sales

- [ ] Add bounded multi-item baskets by archetype and route customers between several fixtures.
- [ ] Add browse/compare/assistance/impulse/congestion/security-friction heuristics.
- [ ] Add impulse sales near endcaps, promotions, and checkout with archetype/budget effects.
- [ ] Add returns/refunds and `RETURN_STARTED`/`RETURN_COMPLETED` events.
- [ ] Add customer security sensitivity and basket-size tendency to snapshots.
- [ ] Add customer-level satisfaction/friction accounting for assistance, receipt checks, locked cases, alarms, and delays.

## Milestone B — real scenarios and progression

### Scenario authority

- [ ] Add scenario objective progress, target/current values, completion, failure, and end-of-day report in C.
- [ ] Add scenario-specific catalog, traffic, staffing, security, inventory, starting cash, and goals.
- [ ] Add `grocery-fresh` only after delivery/restocking meaningfully differ from the existing scenarios.
- [ ] Add `troubled-store` only after capital, morale, staffing, and layout penalties are real.
- [ ] Add Holiday Electronics and Downtown Express only when scenario differences are mechanical, not label-only.
- [ ] Make launcher metadata load from `shrink-sim --list-scenarios` or another authoritative source instead of duplicated IDs/text.

### Scripted official incidents

Official scenario scripts are separate from ordinary emergent events and are not exposed to user-created levels initially.

- [x] Add bounded C-authored scripted-event records with trigger tick, type, target cell, duration, and severity.
- [ ] Add remaining deterministic scenario incident tracks: car impact, sprinkler/power, robbery, delivery accident, flood, alarm malfunction, theft surge, and severe weather.
- [ ] Add simple validated conditions such as shrink/traffic/time thresholds; do not create a general scripting language.
- [x] Add bounded authoritative fire/smoke hazard cells with expiration and snapshot support.
- [ ] Add debris, water, damaged-floor, and temporary-closure hazard variants.
- [x] Integrate fire/debris-style blocking hazards into authoritative walkability and stream snapshots.
- [x] Implement a deterministic fire track with unsafe cells, customer evacuation, merchandise/incident damage cost, expiration, and resolution events.
- [ ] Add sprinklers, employee emergency tasks, deeper merchandise damage, and repair operations.
- [ ] Implement car-impact consequences: breached storefront, damaged/disabled fixtures, debris, entrance change, evacuation/panic, and repair cost.
- [ ] Implement robbery consequences without combat: panic, employee task interruption, temporary closure, guard response, and cash/inventory loss.
- [ ] Add authoritative damage states: normal, damaged, disabled, destroyed for walls, windows, fixtures, and hazards.
- [ ] Add scripted-event replay/determinism tests and scenario-specific event-order tests.

### Capital and balance

- [ ] Add authoritative starting cash and capital ledger.
- [ ] Charge fixture purchase, construction, hiring, security maintenance, repair, tagging, and operating costs.
- [ ] Reject unaffordable construction/hiring deterministically, with development sandbox/unlimited-money mode if needed.
- [ ] Track retail shrink, product-cost shrink, recovered merchandise, prevented loss, security spending, lost sales from friction, stockouts, and locked-case abandonment.
- [ ] Extend `shrink-balance` to compare scenario strategy presets and objective outcomes over many seeds.
- [ ] Add acceptance comparisons: cameras/guards reduce shrink but cost money; excessive security can reduce profit/satisfaction; too few lanes increase waits; excess capacity costs labor/capital.

## Milestone C — store/level designer

### Versioned level format

- [ ] Define versioned JSON format with name/type, width/height, floor, floor surfaces, walls, windows, doors, rooms, departments, fixtures, merchandise, staff, inventory, starting cash, and goals.
- [ ] Validate malformed JSON, unknown types, bad dimensions, duplicate/unsafe IDs, overlaps, invalid doors, unreachable routes, capacity violations, and invalid scenario starts.
- [ ] Load user levels into C-owned state without trusting arbitrary coordinates or IDs.
- [ ] Keep official scripted incident tracks unavailable to user-created levels initially.
- [ ] Add deterministic save/load/replay tests: same level + seed + commands produces identical snapshots, events, and metrics.

### Editor capabilities

- [ ] Create separate Godot editor mode for paint floor, erase floor, floor surface, wall, door/window, room, department, fixture, merchandise, security, entrances/exits, staff, inventory, cash, and goals.
- [ ] Send all edits to C validation; Godot must never mutate authoritative arrays first.
- [ ] Add full footprint/rotation previews, valid/invalid reasons, access/path preview, snapping, multi-select, copy/paste, delete confirmation, and undo/redo.
- [ ] Add multiple entrances/exits with capacity, direction, open/closed state, detector upgrades, and flow logic.
- [ ] Add expansion parcels, purchase cost, construction time, new walls, parking, loading dock, and department unlocks.
- [ ] Add room connectivity preview and staff/customer accessibility validation.
- [ ] Add authoritative floor-surface types independent from walkability: vinyl light, warm/cool tile, polished concrete, concrete, carpet, entrance mat, stockroom, receiving.

## Milestone D — tycoon presentation and platform

- [ ] Replace temporary process bridge with a thin GDExtension/FFI adapter linking the same C library.
- [ ] Add authoritative fixture vocabulary: gondolas, wall shelves, coolers/freezers, produce/dump/endcap/promo displays, locked cases, service counters, carts/baskets, pallets, cameras, EAS/RFID gates, and receipt stations.
- [ ] Ensure every rendered asset matches the exact C footprint, rotation, isometric angle, height convention, lighting, shadow, and line weight.
- [ ] Improve canonical isometric store-object assets; keep shoppers simpler and readable.
- [ ] Add storefront windows, facade sections, glass/door treatment, decals, loading/receiving doors, ceiling-layer cameras, and damaged/breached presentation.
- [ ] Add environmental dressing: sidewalks, parking stalls, roads, woods/trees, neighboring parcels, bollards, dumpsters, carts, baskets, pallets, benches, posters, signs, lights, and delivery vehicles.
- [ ] Add sales-floor, stockroom, staff/security, roof/camera-coverage, and construction views.
- [ ] Keep normal gameplay free of debug labels; retain F3 overlays for IDs, grid, paths, FOV, hazards, rooms, and footprints.
- [ ] Add restrained event camera focus for detected theft, interventions, alarms, assistance, deliveries, stockouts, and long queues.
- [ ] Add pause/1x/2x/4x controls and safe scenario restart/menu lifecycle with no orphan processes or stale pipes.
- [ ] Verify responsive layouts at 1280×720, 1366×768, 1920×1080, and tablet-sized viewports.
- [ ] Add Android/iOS shell integration only after the headless/core boundary remains stable.

## Testing and release gates

- [x] Normal Debug build and CTest.
- [x] Sanitizer build and CTest.
- [x] Linux CI normal + sanitizer jobs.
- [x] Godot headless editor/runtime smoke validation.
- [x] Deterministic scenario, staffing, geometry, stream, balance, and event lifecycle coverage.
- [ ] Event ring overflow/order/consumer tests.
- [ ] Theft detection/intervention matrix tests for coverage, distance, guard skill, and recovery.
- [ ] Inventory conservation, delivery, restock, stockout, basket, return, and task determinism tests.
- [ ] Scripted incident hazard/pathfinding/damage/objective tests.
- [ ] Level JSON malformed-input and round-trip tests.
- [ ] Fuzz/property tests for arbitrary construction and level commands.
- [ ] Godot interactive validation: launcher buttons, scenario switching, Escape/menu, panning, construction, selection, event feed, repeated restart, and process cleanup.
- [ ] Screenshot comparison for Corner Market, Electronics, Big Box, Pharmacy, and later Grocery/Turnaround.
- [ ] Release checklist: `git diff --check`, clean worktree, all local tests, sanitizer tests, CI green, commit pushed to `origin/main`.

## Next implementation order

1. Inventory locations, deliveries, restocking, stockouts, and employee task assignments.
2. Locked-case assistance, self-checkout under-scan, EAS/receipt-check tradeoffs, and richer incident metrics.
3. Bounded official scripted hazards and scenario objective progression.
4. Versioned JSON levels and the separate Godot store designer.
5. Authoritative floor surfaces, richer fixtures/windows/damage, and final interactive presentation validation.
