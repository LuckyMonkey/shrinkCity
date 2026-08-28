#include "scenario.h"

#include <string.h>

static const ShrinkScenarioInfo SCENARIOS[] = {
    {1U, "corner-market", "Corner Market - First Shift", "Convenience / grocery",
     "A compact neighborhood store with tight margins and frequent opportunistic shrink.",
     SHRINK_SCENARIO_GOAL_PROFIT, 125.0, 1U, UINT64_C(8)},
    {2U, "electronics", "Electronics - High Value", "Electronics",
     "High-value portable merchandise, locked displays, cameras, and expensive loss-prevention decisions.",
     SHRINK_SCENARIO_GOAL_SHRINK_RATE, 8.0, 3U, UINT64_C(13)},
    {3U, "big-box", "Big Box - Saturday Rush", "Discount / big box",
     "Heavy traffic, more checkout capacity, self-checkout pressure, and expensive staffing tradeoffs.",
     SHRINK_SCENARIO_GOAL_CUSTOMERS_SERVED, 80.0, 3U, UINT64_C(14)},
    {4U, "pharmacy", "Pharmacy - Small Items, Big Risk", "Pharmacy / health",
     "Small concealable products, concentrated high-risk zones, and customer-friction-sensitive security.",
     SHRINK_SCENARIO_GOAL_SATISFACTION, 88.0, 2U, UINT64_C(15)},
    {5U, "grocery-fresh", "Fresh Market - Delivery Day", "Full grocery",
     "A larger grocery operation where replenishment, staffing coverage, and long customer trips create loss-prevention blind spots.",
     SHRINK_SCENARIO_GOAL_PROFIT, 180.0, 2U, UINT64_C(12)},
    {6U, "troubled-store", "Troubled Store - Turnaround", "Inherited discount store",
     "An awkward inherited store with thin staffing, weak security coverage, and little room for operational mistakes.",
     SHRINK_SCENARIO_GOAL_SHRINK_RATE, 10.0, 3U, UINT64_C(11)}
};

size_t shrink_scenario_count(void)
{
    return sizeof(SCENARIOS) / sizeof(SCENARIOS[0]);
}

const ShrinkScenarioInfo *shrink_scenario_at(size_t index)
{
    return index < shrink_scenario_count() ? &SCENARIOS[index] : NULL;
}

const ShrinkScenarioInfo *shrink_scenario_find(const char *slug)
{
    if (slug == NULL) return NULL;
    for (size_t i = 0U; i < shrink_scenario_count(); ++i)
        if (strcmp(SCENARIOS[i].slug, slug) == 0) return &SCENARIOS[i];
    return NULL;
}

static int first_open_cell(const ShrinkWorld *world, int *out_x, int *out_y)
{
    ShrinkGeometryInfo info;
    shrink_geometry_info(world, &info);
    for (int y = 1; y < info.height - 1; ++y) {
        const uint32_t row = shrink_floor_row(world, y);
        for (int x = 1; x < info.width - 1 && x < 32; ++x) {
            if ((row & (UINT32_C(1) << x)) == 0U) continue;
            *out_x = x;
            *out_y = y;
            return 1;
        }
    }
    return 0;
}

static unsigned place_some(ShrinkWorld *world, ShrinkFixtureType type, unsigned wanted)
{
    ShrinkGeometryInfo info;
    shrink_geometry_info(world, &info);
    unsigned placed = 0U;
    for (int y = 1; y < info.height - 1 && placed < wanted; ++y) {
        for (int x = 1; x < info.width - 1 && placed < wanted; ++x) {
            if (shrink_try_place_fixture(world, type, x, y, 0U, NULL) == SHRINK_BUILD_OK)
                ++placed;
        }
    }
    return placed;
}

static void fire_one_role(ShrinkWorld *world, ShrinkEmployeeRole role)
{
    for (size_t i = 0U; i < shrink_employee_count(world); ++i) {
        ShrinkEmployeeSnapshot employee;
        if (shrink_employee_snapshot(world, i, &employee) && employee.role == role) {
            (void)shrink_fire_employee(world, employee.id);
            return;
        }
    }
}

static void remove_cameras(ShrinkWorld *world, unsigned wanted)
{
    uint64_t ids[32];
    unsigned count = 0U;
    ShrinkGeometryInfo info;
    shrink_geometry_info(world, &info);
    for (size_t i = 0U; i < info.fixture_count && count < wanted && count < 32U; ++i) {
        ShrinkFixtureSnapshot fixture;
        if (shrink_fixture_snapshot(world, i, &fixture) && fixture.type == SHRINK_FIXTURE_CAMERA)
            ids[count++] = fixture.id;
    }
    for (unsigned i = 0U; i < count; ++i)
        (void)shrink_try_remove_fixture(world, ids[i]);
}

static void configure_corner_market(ShrinkWorld *world)
{
    /* A lean neighborhood operation: one staffed lane plus self-checkout. */
    fire_one_role(world, SHRINK_EMPLOYEE_CASHIER);
}

static void configure_electronics(ShrinkWorld *world)
{
    (void)place_some(world, SHRINK_FIXTURE_CAMERA, 3U);
    (void)place_some(world, SHRINK_FIXTURE_LOCKED_CASE, 2U);
    (void)shrink_hire_employee(world, SHRINK_EMPLOYEE_SECURITY, 24.0, NULL);
    (void)shrink_hire_employee(world, SHRINK_EMPLOYEE_ASSOCIATE, 19.0, NULL);
}

static void configure_big_box(ShrinkWorld *world)
{
    (void)place_some(world, SHRINK_FIXTURE_CAMERA, 2U);
    (void)shrink_hire_employee(world, SHRINK_EMPLOYEE_CASHIER, 19.0, NULL);
    (void)shrink_hire_employee(world, SHRINK_EMPLOYEE_ASSOCIATE, 18.0, NULL);
    (void)shrink_hire_employee(world, SHRINK_EMPLOYEE_STOCKER, 19.0, NULL);
}

static void configure_pharmacy(ShrinkWorld *world)
{
    (void)place_some(world, SHRINK_FIXTURE_CAMERA, 2U);
    (void)place_some(world, SHRINK_FIXTURE_LOCKED_CASE, 1U);
    fire_one_role(world, SHRINK_EMPLOYEE_CASHIER);
    (void)shrink_hire_employee(world, SHRINK_EMPLOYEE_ASSOCIATE, 18.5, NULL);
}

static void configure_grocery_fresh(ShrinkWorld *world)
{
    (void)place_some(world, SHRINK_FIXTURE_CAMERA, 1U);
    (void)shrink_hire_employee(world, SHRINK_EMPLOYEE_STOCKER, 19.5, NULL);
    (void)shrink_hire_employee(world, SHRINK_EMPLOYEE_ASSOCIATE, 18.0, NULL);
}

static void configure_troubled_store(ShrinkWorld *world)
{
    remove_cameras(world, 8U);
    fire_one_role(world, SHRINK_EMPLOYEE_CASHIER);
    fire_one_role(world, SHRINK_EMPLOYEE_SECURITY);
}

ShrinkWorld *shrink_scenario_create(const ShrinkScenarioInfo *scenario, uint64_t variation)
{
    if (scenario == NULL) return NULL;

    /* Variations move by eight so the curated layout ID remains stable. */
    ShrinkWorld *world = shrink_create(scenario->canonical_seed + variation * UINT64_C(8));
    if (world == NULL) return NULL;

    switch (scenario->id) {
        case 1U: configure_corner_market(world); break;
        case 2U: configure_electronics(world); break;
        case 3U: configure_big_box(world); break;
        case 4U: configure_pharmacy(world); break;
        case 5U: configure_grocery_fresh(world); break;
        case 6U: configure_troubled_store(world); break;
        default: break;
    }

    /* Official incidents are bounded authored tracks; user levels do not use this API yet. */
    if (scenario->id == 5U) {
        (void)shrink_schedule_scripted_event(world, (ShrinkScenarioEventDef){SHRINK_SCRIPT_FIRE, 180U, 10, 12, 2U, 45U});
    } else if (scenario->id == 6U) {
        (void)shrink_schedule_scripted_event(world, (ShrinkScenarioEventDef){SHRINK_SCRIPT_FIRE, 120U, 8, 10, 1U, 30U});
    }

    /* Keep the helper referenced in sanitizer builds while layout APIs settle. */
    int x = 0, y = 0;
    (void)first_open_cell(world, &x, &y);
    return world;
}
