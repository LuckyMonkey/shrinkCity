# Shrink City gameplay direction

Shrink City is a retail loss-prevention tycoon game. The interesting decision is not simply whether to buy more security; it is how much friction, labor, capital, and customer annoyance a store can tolerate while controlling shrink.

## Attract/demo mode

Launching the game should enter an RCT-style attract mode rather than an empty editor. A fully simulated store runs behind a lightweight scenario browser. The camera periodically follows shoppers, guards, queues, receiving, and suspicious activity. Selecting **Play this store** restarts the chosen scenario from its canonical seed/state; selecting another card swaps the demo to that prefab.

The attract loop should deliberately surface events: a normal purchase, a queue forming, a suspicious shopper, a guard patrol, a theft/detection, a stockout/restock, a return, and a receiving delivery. It is presentation over the real C simulation, not a scripted fake simulation.

## Prefabricated scenarios

Each scenario owns a store archetype, curated layout, starting staff/security, merchandise mix, starting cash, traffic profile, loss profile, and goals. Layout seed and simulation seed remain deterministic.

### Corner Market — First Shift
Small neighborhood grocery/convenience store. Food, drinks, household basics, two staffed lanes, modest cameras. High traffic and low-value opportunistic theft. Teaches queues, camera coverage, staffing, stocking, and receipt friction.

Goals: finish profitable; keep shrink below 4%; average wait below 45 seconds; satisfaction above 80%.

### Circuit City-ish — High Value
Electronics store with expensive portable merchandise, locked displays, demo tables, blind spots, and a small high-value cage. Lower traffic, much larger loss per incident. Teaches locked cases, associate response, camera placement, tags/gates, and the cost of making legitimate customers wait for keys.

Goals: protect high-value inventory while maintaining sales conversion and assistance time.

### Big Box — Saturday Rush
Large discount store with long aisles, self-checkout, multiple departments, receiving, and heavy traffic. Organized concealment and self-checkout loss compete with queue pressure and labor cost.

Goals: survive a rush period, maintain throughput, and avoid solving shrink by simply closing self-checkout.

### Pharmacy — Small Items, Big Risk
Compact pharmacy/health store with many small concealable products and a few locked high-risk categories. Teaches category-specific protection and customer-friction tradeoffs.

### Grocery — Fresh & Fast
Larger supermarket emphasizing perishables, restocking, produce, coolers, long shopping trips, checkout peaks, and receiving. Shrink includes theft plus spoilage/damage later.

### Troubled Store — Turnaround
An inherited poorly laid-out store with blind corners, understaffing, bad queue flow, low morale, and excessive shrink. Player has limited capital and must improve operations rather than rebuild everything.

### Holiday Electronics — Doorbuster
High traffic, high-value stock, promotional displays, temporary staff, long queues, and elevated theft pressure. A short challenge scenario intended for score chasing.

### Downtown Express — No Room
Tiny urban footprint, minimal back room, expensive floor space, frequent deliveries, limited checkout area, and heavy grab-and-go traffic. Forces compact security and staffing decisions.

## Real retail systems worth simulating

These systems should arrive incrementally and remain measurable in headless balance runs.

### Loss prevention
- camera coverage, blind spots, deterrence, observation and detection
- uniformed guards versus plain-clothes loss-prevention staff
- EAS/RFID tags and exit pedestals
- locked cases, spider wraps, keeper boxes and tethered demos
- receipt checking and exit congestion
- self-checkout interventions, missed scans and intentional skip-scanning
- fitting-room concealment for applicable stores
- high-risk merchandise placement near staff/checkout
- repeat suspicious behavior and observation confidence
- false positives: intervention should require confidence; bad stops damage satisfaction/reputation

### Shoplifting behavior
Do not model thieves as a binary class. Customers have motives/opportunity/risk tolerance. Theft attempts can include concealment, walkouts, skip-scanning, tag avoidance, distraction, or opportunistic grabbing. Security changes opportunity and perceived risk rather than setting a magic global probability.

### Store operations
- deliveries and receiving windows
- stockroom inventory and shelf replenishment
- stockouts and lost sales
- returns/refunds and return fraud
- damaged merchandise
- misplaced merchandise / recovery (go-backs)
- price checks and customer assistance
- register cash variances and sweethearting later
- employee breaks, fatigue, call-outs and shift coverage
- opening/closing tasks
- cleaning/spills that temporarily block aisles
- carts/baskets and cart collection
- queue abandonment
- service desk workload
- alarms and security interventions

### External events
- lunch/after-work/weekend traffic waves
- weather effects on traffic
- holidays/promotions
- delivery delays
- equipment failures
- local events producing traffic spikes
- shoplifting crews / coordinated incidents as rare advanced events

## Core balancing principle

Every loss-prevention tool should have at least one cost or operational consequence.

Examples:

| Tool | Benefit | Cost / consequence |
| --- | --- | --- |
| Camera | coverage, deterrence, evidence | capital + maintenance; requires useful placement |
| Guard | mobile deterrence/intervention | high labor; cannot cover entire store |
| Plain-clothes LP | observation with low customer friction | specialized labor; limited coverage |
| Locked case | strong protection | assistance workload, lost conversion, frustration |
| EAS/RFID | exit detection | equipment/tag cost, interventions/false alarms |
| Receipt check | reduces walkouts | labor, exit queue, annoyance |
| Self-checkout | throughput, lower cashier labor | interventions and higher shrink risk |
| More cashiers | shorter queues | labor cost |
| Open layout | visibility and throughput | less merchandising density |

A good strategy should depend on store type, merchandise, traffic, layout, and goals. There should not be one universal security build.

## Current incident loop

The live C core now emits a bounded event stream instead of asking the CLI or Godot to infer incidents from metric changes. A suspicious merchandise decision produces `THEFT_ATTEMPTED`; local camera coverage may produce `THEFT_DETECTED` and `SECURITY_RESPONDING`; reaching the exit produces either `SECURITY_INTERVENTION` or `THEFT_EXITED`. Purchases emit `PURCHASE_COMPLETED`. These records carry stable IDs and values for deterministic incident feeds.

Official scripted disasters are separate from emergent events and now have a bounded C-authored track foundation. Grocery Fresh and Troubled Store schedule deterministic fire incidents; C creates expiring fire/smoke hazards, blocks routing, evacuates nearby shoppers, accounts for damage, and emits resolution events. Vehicle impact, robbery, outages, and other incident tracks remain future additions. Sandbox levels remain unscripted.

## Simulation/event architecture

The C simulation should grow an event stream alongside snapshots. Events are immutable facts such as CUSTOMER_ENTERED, ITEM_SELECTED, THEFT_ATTEMPTED, THEFT_DETECTED, THEFT_EXITED, CHECKOUT_STARTED, QUEUE_ABANDONED, ASSISTANCE_REQUESTED, DELIVERY_ARRIVED, STOCKOUT, RESTOCKED, RETURN_STARTED, SPILL_REPORTED, and SECURITY_INTERVENTION.

Godot may use those events for speech/thought bubbles, sound, alerts, camera focus, demo-mode storytelling, and the incident log. Godot must not manufacture gameplay events.

## Scenario data model

Move toward validated scenario/content data rather than scattered constants. A scenario should eventually define:

- id, title, description, store archetype
- layout id and canonical seed
- starting cash
- traffic profile
- merchandise assortment
- starting inventory
- staff roster
- security fixtures/policies
- scenario modifiers
- objectives and optional bonus objectives
- win/lose conditions
- suggested difficulty

The headless runner must be able to execute a named scenario for many seeds and report objective outcomes.

## Near-term implementation order

1. Scenario registry and scenario-selectable C world creation.
2. Godot attract mode + scenario browser using real simulation snapshots.
3. Author distinct Grocery and Electronics prefabs first.
4. Add authoritative simulation event stream and incident log.
5. Add stockroom/restocking/deliveries and stockout-driven lost sales.
6. Add richer theft attempts + detection/intervention lifecycle.
7. Add locked-case assistance and self-checkout interventions.
8. Add scenario objectives, scoring and end-of-day report.
9. Add returns/return fraud, spills, call-outs and other operational events.
10. Continuously use shrink-balance to prevent dominant strategies.
