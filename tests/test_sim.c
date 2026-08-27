#include "shrink.h"
#include "pathfinding.h"

#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

static ShrinkMetrics run(uint64_t seed)
{
    ShrinkWorld *world = shrink_create(seed);
    assert(world != NULL);
    for (unsigned i = 0; i < 600U; ++i) shrink_tick(world, 1.0);
    ShrinkMetrics metrics;
    shrink_metrics(world, &metrics);
    assert(shrink_tick_count(world) == 600U);
    shrink_destroy(world);
    return metrics;
}

static void assert_same(ShrinkMetrics a, ShrinkMetrics b)
{
    assert(a.customers_entered == b.customers_entered);
    assert(a.purchases == b.purchases);
    assert(a.thefts == b.thefts);
    assert(a.abandoned == b.abandoned);
    assert(a.revenue == b.revenue);
    assert(a.stolen_value == b.stolen_value);
    assert(a.labor_cost == b.labor_cost);
}

int main(void)
{
    int x = 0, y = 0;
    assert(shrink_path_next_step(1, 10, 4, 4, &x, &y) == 1);
    assert((x == 2 && y == 10) || (x == 1 && y == 9));
    int path_x = 1, path_y = 10;
    for (unsigned step = 0; step < 100U && (path_x != 4 || path_y != 4); ++step) {
        assert(shrink_path_next_step(path_x, path_y, 4, 4, &x, &y) == 1);
        assert(abs(x - path_x) + abs(y - path_y) == 1);
        path_x = x; path_y = y;
    }
    assert(path_x == 4 && path_y == 4);

    ShrinkWorld *snapshot_world = shrink_create(99U);
    assert(snapshot_world != NULL);
    shrink_tick(snapshot_world, 1.0);
    ShrinkEntitySnapshot entity;
    assert(shrink_entity_count(snapshot_world) == 1U);
    assert(shrink_entity_snapshot(snapshot_world, 0U, &entity) == 1);
    assert(entity.id == 1U);
    assert(entity.x >= 1.0 && entity.x <= 19.0);
    assert(shrink_entity_snapshot(snapshot_world, 1U, &entity) == 0);
    shrink_destroy(snapshot_world);

    const ShrinkMetrics first = run(12345U);
    const ShrinkMetrics second = run(12345U);
    assert_same(first, second);
    assert(first.customers_entered > 0U);
    assert(first.purchases > 0U);
    assert(first.thefts > 0U);
    assert(first.revenue > 0.0);
    assert(first.stolen_value > 0.0);
    assert(fabs(first.profit - (first.revenue - first.stolen_value - first.labor_cost)) < 0.001);
    assert(first.average_checkout_wait >= 0.0);
    assert(first.average_satisfaction >= 0.0 && first.average_satisfaction <= 100.0);
    puts("shrink-tests: all assertions passed");
    return 0;
}
