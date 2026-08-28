#include "shrink.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FIXTURE_IDS 512U

typedef enum BalanceScenario {
    SCENARIO_AUTHORED = 0,
    SCENARIO_NO_CAMERAS,
    SCENARIO_EXTRA_CAMERAS
} BalanceScenario;

static const char *scenario_name(BalanceScenario scenario)
{
    switch (scenario) {
        case SCENARIO_NO_CAMERAS: return "no_cameras";
        case SCENARIO_EXTRA_CAMERAS: return "extra_cameras";
        case SCENARIO_AUTHORED:
        default: return "authored";
    }
}

static int parse_scenario(const char *name, BalanceScenario *out_scenario, int *out_all)
{
    if (strcmp(name, "all") == 0) {
        *out_all = 1;
        return 1;
    }
    *out_all = 0;
    if (strcmp(name, "authored") == 0) *out_scenario = SCENARIO_AUTHORED;
    else if (strcmp(name, "no-cameras") == 0 || strcmp(name, "no_cameras") == 0) *out_scenario = SCENARIO_NO_CAMERAS;
    else if (strcmp(name, "extra-cameras") == 0 || strcmp(name, "extra_cameras") == 0) *out_scenario = SCENARIO_EXTRA_CAMERAS;
    else return 0;
    return 1;
}

static unsigned remove_all_cameras(ShrinkWorld *world)
{
    uint64_t ids[MAX_FIXTURE_IDS];
    unsigned id_count = 0U;
    ShrinkGeometryInfo info;
    shrink_geometry_info(world, &info);

    for (size_t i = 0U; i < info.fixture_count && id_count < MAX_FIXTURE_IDS; ++i) {
        ShrinkFixtureSnapshot fixture;
        if (shrink_fixture_snapshot(world, i, &fixture) && fixture.type == SHRINK_FIXTURE_CAMERA)
            ids[id_count++] = fixture.id;
    }

    unsigned removed = 0U;
    for (unsigned i = 0U; i < id_count; ++i)
        if (shrink_try_remove_fixture(world, ids[i]) == SHRINK_BUILD_OK) ++removed;
    return removed;
}

static unsigned add_extra_cameras(ShrinkWorld *world, unsigned wanted)
{
    ShrinkGeometryInfo info;
    shrink_geometry_info(world, &info);
    unsigned added = 0U;

    /* Deterministic row-major search keeps strategy comparisons reproducible. */
    for (int y = 1; y < info.height - 1 && added < wanted; ++y) {
        for (int x = 1; x < info.width - 1 && added < wanted; ++x) {
            uint64_t id = 0U;
            if (shrink_try_place_fixture(world, SHRINK_FIXTURE_CAMERA, x, y, 0U, &id) == SHRINK_BUILD_OK)
                ++added;
        }
    }
    return added;
}

static unsigned apply_scenario(ShrinkWorld *world, BalanceScenario scenario)
{
    if (scenario == SCENARIO_NO_CAMERAS) return remove_all_cameras(world);
    if (scenario == SCENARIO_EXTRA_CAMERAS) return add_extra_cameras(world, 2U);
    return 0U;
}

static int run_one(uint64_t seed, unsigned days, BalanceScenario scenario)
{
    ShrinkWorld *world = shrink_create(seed);
    if (world == NULL) return 0;

    const unsigned changed = apply_scenario(world, scenario);
    ShrinkGeometryInfo info;
    shrink_geometry_info(world, &info);

    for (unsigned day = 0U; day < days; ++day)
        for (unsigned second = 0U; second < 600U; ++second) shrink_tick(world, 1.0);

    ShrinkMetrics m;
    shrink_metrics(world, &m);
    printf("%" PRIu64 ",%u,%s,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
           seed, info.layout_id, scenario_name(scenario), changed,
           m.customers_entered, m.purchases, m.thefts, m.revenue,
           m.cost_of_goods, m.stolen_value, m.labor_cost, m.security_cost, m.profit,
           m.average_satisfaction, m.average_checkout_wait);
    shrink_destroy(world);
    return 1;
}

static void usage(const char *name)
{
    fprintf(stderr,
            "Usage: %s [--days N] [--seeds N] [--start-seed N] "
            "[--scenario authored|no-cameras|extra-cameras|all]\n",
            name);
}

int main(int argc, char **argv)
{
    unsigned days = 30U, seeds = 10U;
    uint64_t first_seed = 1U;
    BalanceScenario selected = SCENARIO_AUTHORED;
    int all_scenarios = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--days") == 0 && i + 1 < argc) days = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--seeds") == 0 && i + 1 < argc) seeds = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--start-seed") == 0 && i + 1 < argc) first_seed = (uint64_t)strtoull(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--scenario") == 0 && i + 1 < argc) {
            if (!parse_scenario(argv[++i], &selected, &all_scenarios)) {
                usage(argv[0]);
                return 2;
            }
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (days == 0U || seeds == 0U) {
        fputs("days and seeds must both be greater than zero\n", stderr);
        return 2;
    }

    puts("seed,layout,scenario,security_changes,customers,purchases,thefts,revenue,cost_of_goods,shrink,labor,security,profit,satisfaction,wait");
    for (unsigned offset = 0U; offset < seeds; ++offset) {
        const uint64_t seed = first_seed + offset;
        if (all_scenarios) {
            for (int scenario = (int)SCENARIO_AUTHORED; scenario <= (int)SCENARIO_EXTRA_CAMERAS; ++scenario)
                if (!run_one(seed, days, (BalanceScenario)scenario)) return 1;
        } else if (!run_one(seed, days, selected)) {
            return 1;
        }
    }
    return 0;
}
