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

static void test_customer_uses_authoritative_target(void)
{
    ShrinkWorld *world = shrink_create(123U);
    assert(world != NULL);
    shrink_tick(world, 0.1);
    ShrinkEntitySnapshot before;
    assert(shrink_entity_snapshot(world, 0U, &before) == 1);
    assert(before.target_fixture_id != 0U);

    ShrinkGeometryInfo info;
    shrink_geometry_info(world, &info);
    ShrinkFixtureSnapshot fixture;
    int found = 0;
    for (size_t i = 0U; i < info.fixture_count; ++i) {
        assert(shrink_fixture_snapshot(world, i, &fixture) == 1);
        if (fixture.id == before.target_fixture_id) { found = 1; break; }
    }
    assert(found && fixture.product_id == (int)before.product);

    assert(shrink_try_move_fixture(world, fixture.id, 20, 5) == SHRINK_BUILD_OK);
    shrink_tick(world, 0.1);
    ShrinkEntitySnapshot after;
    assert(shrink_entity_snapshot(world, 0U, &after) == 1);
    assert(after.target_fixture_id == fixture.id);
    assert(after.target_x >= 20 && after.target_x <= 22);

    assert(shrink_try_remove_fixture(world, fixture.id) == SHRINK_BUILD_OK);
    shrink_tick(world, 0.1);
    assert(shrink_entity_snapshot(world, 0U, &after) == 1);
    assert(after.state == SHRINK_ENTITY_LEAVING);
    shrink_destroy(world);
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

    test_customer_uses_authoritative_target();

    ShrinkWorld *geometry_world = shrink_create(77U);
    assert(geometry_world != NULL);
    ShrinkGeometryInfo geometry_info;
    shrink_geometry_info(geometry_world, &geometry_info);
    assert(geometry_info.width == 28 && geometry_info.height == 22);
    assert(geometry_info.fixture_count >= 7U);
    assert(shrink_geometry_has_routes(geometry_world) == 1);
    uint64_t placed_id = 0U;
    assert(shrink_try_place_fixture(geometry_world, SHRINK_FIXTURE_SHELF, 3, 10, 0U, &placed_id) == SHRINK_BUILD_OK);
    assert(placed_id > 0U);
    assert(shrink_try_move_fixture(geometry_world, placed_id, 3, 11) == SHRINK_BUILD_OK);
    assert(shrink_try_rotate_fixture(geometry_world, placed_id, 1U) == SHRINK_BUILD_OK);
    ShrinkFixtureSnapshot fixture_snapshot;
    assert(shrink_fixture_snapshot(geometry_world, geometry_info.fixture_count, &fixture_snapshot) == 1);
    assert(fixture_snapshot.id == placed_id && fixture_snapshot.rotation == 1U);
    assert(shrink_try_place_fixture(geometry_world, SHRINK_FIXTURE_SHELF, -1, 4, 0U, NULL) == SHRINK_BUILD_OUT_OF_BOUNDS);
    assert(shrink_try_place_fixture(geometry_world, SHRINK_FIXTURE_SHELF, 14, 8, 0U, NULL) == SHRINK_BUILD_COLLISION);
    assert(shrink_try_remove_fixture(geometry_world, placed_id) == SHRINK_BUILD_OK);
    assert(shrink_try_remove_fixture(geometry_world, 1U) == SHRINK_BUILD_INVALID_DOOR);
    uint64_t wall_id = 0U;
    assert(shrink_try_add_wall(geometry_world, 2, 2, 4, 4, &wall_id) == SHRINK_BUILD_INVALID_WALL);
    assert(shrink_try_add_wall(geometry_world, 0, 10, 27, 10, &wall_id) == SHRINK_BUILD_BLOCKS_ROUTE);
    shrink_destroy(geometry_world);

    ShrinkWorld *replay_a = shrink_create(88U);
    ShrinkWorld *replay_b = shrink_create(88U);
    assert(replay_a != NULL && replay_b != NULL);
    uint64_t replay_id_a = 0U, replay_id_b = 0U;
    assert(shrink_try_place_fixture(replay_a, SHRINK_FIXTURE_BIN, 3, 3, 0U, &replay_id_a) == SHRINK_BUILD_OK);
    assert(shrink_try_place_fixture(replay_b, SHRINK_FIXTURE_BIN, 3, 3, 0U, &replay_id_b) == SHRINK_BUILD_OK);
    assert(replay_id_a == replay_id_b);
    assert(shrink_try_move_fixture(replay_a, replay_id_a, 4, 3) == shrink_try_move_fixture(replay_b, replay_id_b, 4, 3));
    ShrinkGeometryInfo replay_info_a, replay_info_b;
    shrink_geometry_info(replay_a, &replay_info_a); shrink_geometry_info(replay_b, &replay_info_b);
    assert(replay_info_a.fixture_count == replay_info_b.fixture_count && replay_info_a.wall_count == replay_info_b.wall_count);
    shrink_destroy(replay_a); shrink_destroy(replay_b);

    ShrinkWorld *snapshot_world = shrink_create(99U);
    assert(snapshot_world != NULL);
    shrink_tick(snapshot_world, 1.0);
    ShrinkEntitySnapshot entity;
    assert(shrink_entity_count(snapshot_world) == 1U);
    assert(shrink_entity_snapshot(snapshot_world, 0U, &entity) == 1);
    assert(entity.id == 1U);
    assert(entity.x >= 1.0 && entity.x <= 19.0);
    assert(entity.archetype >= SHRINK_CUSTOMER_QUICK_STOP && entity.archetype <= SHRINK_CUSTOMER_OPPORTUNISTIC);
    assert(entity.walking_speed > 0.0 && entity.patience_seconds > 0.0 && entity.budget > 0.0);
    ShrinkWorld *same_world = shrink_create(99U);
    assert(same_world != NULL);
    shrink_tick(same_world, 1.0);
    ShrinkEntitySnapshot same_entity;
    assert(shrink_entity_snapshot(same_world, 0U, &same_entity) == 1);
    assert(entity.product == same_entity.product && entity.archetype == same_entity.archetype);
    assert(entity.walking_speed == same_entity.walking_speed);
    assert(shrink_employee_count(snapshot_world) == 4U);
    ShrinkEmployeeSnapshot employee;
    assert(shrink_employee_snapshot(snapshot_world, 3U, &employee) == 1);
    assert(employee.role == SHRINK_EMPLOYEE_SECURITY && employee.wage > 0.0);
    assert(shrink_entity_snapshot(snapshot_world, 1U, &entity) == 0);
    shrink_destroy(same_world);
    shrink_destroy(snapshot_world);

    const ShrinkMetrics first = run(12345U);
    const ShrinkMetrics second = run(12345U);
    assert_same(first, second);
    assert(first.customers_entered > 0U);
    assert(first.purchases > 0U);
    assert(first.thefts > 0U);
    assert(first.revenue > 0.0);
    assert(first.stolen_value > 0.0);
    assert(fabs(first.profit - (first.revenue - first.cost_of_goods - first.stolen_value - first.labor_cost - first.security_cost)) < 0.001);
    assert(first.average_checkout_wait >= 0.0);
    assert(first.average_satisfaction >= 0.0 && first.average_satisfaction <= 100.0);
    puts("shrink-tests: all assertions passed");
    return 0;
}
