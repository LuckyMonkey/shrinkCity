#include "geometry.h"

#include <assert.h>
#include <stdio.h>

static void test_multicell_footprints(void)
{
    ShrinkGeometry geometry;
    shrink_geometry_init(&geometry);

    uint64_t shelf_id = 0U;
    assert(shrink_geometry_place_fixture(&geometry, SHRINK_FIXTURE_SHELF, 20, 4, 0U, &shelf_id) == SHRINK_BUILD_OK);
    const ShrinkFixture *shelf = shrink_geometry_find_fixture(&geometry, shelf_id);
    assert(shelf != NULL);
    assert(shelf->width == 3U && shelf->height == 1U);
    assert(shrink_geometry_blocked(&geometry, 20, 4) == 1);
    assert(shrink_geometry_blocked(&geometry, 21, 4) == 1);
    assert(shrink_geometry_blocked(&geometry, 22, 4) == 1);
    assert(shrink_geometry_blocked(&geometry, 20, 5) == 0);

    assert(shrink_geometry_rotate_fixture(&geometry, shelf_id, 1U) == SHRINK_BUILD_OK);
    shelf = shrink_geometry_find_fixture(&geometry, shelf_id);
    assert(shelf != NULL);
    assert(shelf->width == 1U && shelf->height == 3U);
    assert(shrink_geometry_blocked(&geometry, 20, 4) == 1);
    assert(shrink_geometry_blocked(&geometry, 20, 5) == 1);
    assert(shrink_geometry_blocked(&geometry, 20, 6) == 1);
    assert(shrink_geometry_blocked(&geometry, 21, 4) == 0);

    assert(shrink_geometry_place_fixture(&geometry, SHRINK_FIXTURE_BIN, 20, 6, 0U, NULL) == SHRINK_BUILD_COLLISION);
    assert(shrink_geometry_move_fixture(&geometry, shelf_id, 26, 20) == SHRINK_BUILD_COLLISION);
}

static void test_merchandise_access(void)
{
    ShrinkGeometry geometry;
    shrink_geometry_init(&geometry);

    uint64_t shelf_id = 0U;
    assert(shrink_geometry_place_fixture(&geometry, SHRINK_FIXTURE_LOCKED_SHELF, 24, 5, 0U, &shelf_id) == SHRINK_BUILD_OK);
    assert(shrink_geometry_fixture_accessible(&geometry, shelf_id) == 1);

    /* Locked shelves are front-access only. Facing the south perimeter leaves no interaction aisle. */
    assert(shrink_geometry_move_fixture(&geometry, shelf_id, 24, 20) == SHRINK_BUILD_BLOCKS_ROUTE);
    assert(shrink_geometry_fixture_accessible(&geometry, shelf_id) == 1);

    /* Rejected placement must not leave inaccessible merchandise in authoritative state. */
    assert(shrink_geometry_place_fixture(&geometry, SHRINK_FIXTURE_LOCKED_SHELF, 24, 20, 0U, NULL) == SHRINK_BUILD_BLOCKS_ROUTE);
}

static void test_access_cell_selection(void)
{
    ShrinkGeometry geometry;
    shrink_geometry_init(&geometry);
    const ShrinkFixture *fixture = NULL;
    for (size_t i = 0U; i < geometry.fixture_count; ++i) {
        if (geometry.fixtures[i].product_id == 0) { fixture = &geometry.fixtures[i]; break; }
    }
    assert(fixture != NULL);
    int x1 = -1, y1 = -1, x2 = -1, y2 = -1;
    assert(shrink_geometry_best_access_cell(&geometry, fixture->id, 1, 10, &x1, &y1) == 1);
    assert(shrink_geometry_best_access_cell(&geometry, fixture->id, 1, 10, &x2, &y2) == 1);
    assert(x1 == x2 && y1 == y2);
    assert(!shrink_geometry_blocked(&geometry, x1, y1));
    assert(!(x1 >= fixture->x && x1 < fixture->x + (int)fixture->width && y1 >= fixture->y && y1 < fixture->y + (int)fixture->height));

    assert(shrink_geometry_move_fixture(&geometry, fixture->id, 20, 5) == SHRINK_BUILD_OK);
    assert(shrink_geometry_best_access_cell(&geometry, fixture->id, 1, 10, &x1, &y1) == 1);
    assert(x1 >= 20 && x1 <= 22 && (y1 == 4 || y1 == 6));
    assert(shrink_geometry_rotate_fixture(&geometry, fixture->id, 1U) == SHRINK_BUILD_OK);
    assert(shrink_geometry_best_access_cell(&geometry, fixture->id, 1, 10, &x1, &y1) == 1);
    assert((x1 == 19 || x1 == 21) && y1 >= 5 && y1 <= 7);
}

static void test_replay(void)
{
    ShrinkGeometry a, b;
    shrink_geometry_init(&a);
    shrink_geometry_init(&b);

    uint64_t a_id = 0U, b_id = 0U;
    assert(shrink_geometry_place_fixture(&a, SHRINK_FIXTURE_SHORT_SHELF, 19, 10, 1U, &a_id) == SHRINK_BUILD_OK);
    assert(shrink_geometry_place_fixture(&b, SHRINK_FIXTURE_SHORT_SHELF, 19, 10, 1U, &b_id) == SHRINK_BUILD_OK);
    assert(a_id == b_id);
    assert(shrink_geometry_move_fixture(&a, a_id, 20, 10) == shrink_geometry_move_fixture(&b, b_id, 20, 10));
    assert(shrink_geometry_rotate_fixture(&a, a_id, 2U) == shrink_geometry_rotate_fixture(&b, b_id, 2U));

    const ShrinkFixture *fa = shrink_geometry_find_fixture(&a, a_id);
    const ShrinkFixture *fb = shrink_geometry_find_fixture(&b, b_id);
    assert(fa != NULL && fb != NULL);
    assert(fa->x == fb->x && fa->y == fb->y);
    assert(fa->rotation == fb->rotation);
    assert(fa->width == fb->width && fa->height == fb->height);
    assert(fa->access_mask == fb->access_mask);
}

int main(void)
{
    test_multicell_footprints();
    test_merchandise_access();
    test_access_cell_selection();
    test_replay();
    puts("shrink-geometry-tests: all assertions passed");
    return 0;
}
