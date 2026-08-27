#include "geometry.h"
#include "pathfinding.h"

#include <string.h>

static int valid_cell(const ShrinkGeometry *g, int x, int y)
{
    return g != NULL && x >= 0 && y >= 0 && x < g->width && y < g->height;
}

static int route(const ShrinkGeometry *g, int sx, int sy, int gx, int gy)
{
    unsigned char blocked[SHRINK_MAX_CELLS];
    if (!valid_cell(g, sx, sy) || !valid_cell(g, gx, gy)) return 0;
    for (int y = 0; y < g->height; ++y)
        for (int x = 0; x < g->width; ++x)
            blocked[y * g->width + x] = (unsigned char)shrink_geometry_blocked(g, x, y);
    return shrink_path_reachable_grid(sx, sy, gx, gy, blocked, g->width, g->height);
}

static int routes_after_change(const ShrinkGeometry *g)
{
    int entrance_x = -1, entrance_y = -1, exit_x = -1, exit_y = -1, register_x = -1, register_y = -1;
    for (size_t i = 0; i < g->fixture_count; ++i) {
        const ShrinkFixture *f = &g->fixtures[i];
        if (f->type == SHRINK_FIXTURE_ENTRANCE && entrance_x < 0) { entrance_x = f->x; entrance_y = f->y; }
        if (f->type == SHRINK_FIXTURE_EXIT && exit_x < 0) { exit_x = f->x; exit_y = f->y; }
        if (f->type == SHRINK_FIXTURE_REGISTER && register_x < 0) { register_x = f->x; register_y = f->y; }
    }
    if (entrance_x < 0 || exit_x < 0 || register_x < 0) return 1;
    return route(g, entrance_x, entrance_y, register_x, register_y) && route(g, register_x, register_y, exit_x, exit_y);
}

void shrink_geometry_init(ShrinkGeometry *g)
{
    memset(g, 0, sizeof(*g));
    g->width = 28; g->height = 22; g->next_id = 1U;
    for (int y = 0; y < g->height; ++y)
        for (int x = 0; x < g->width; ++x) g->floor[y * g->width + x] = 1U;
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_ENTRANCE, 1, 10, 0U, NULL);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_EXIT, 2, 10, 0U, NULL);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_REGISTER, 14, 8, 0U, NULL);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_REGISTER, 14, 12, 0U, NULL);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_SHELF, 4, 5, 0U, NULL);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_SHELF, 8, 13, 0U, NULL);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_SHELF, 11, 5, 0U, NULL);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_BIN, 6, 8, 0U, NULL);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_LOCKED_SHELF, 12, 8, 0U, NULL);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_CLEARANCE, 10, 12, 0U, NULL);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_SELF_CHECKOUT, 15, 8, 0U, NULL);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_CAMERA, 8, 3, 0U, NULL);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_CAMERA, 8, 17, 0U, NULL);
    /* Keep the initial footprint legible while leaving the customer doors open. */
    (void)shrink_geometry_add_wall(g, 0, 0, 27, 0, NULL);
    (void)shrink_geometry_add_wall(g, 0, 21, 27, 21, NULL);
    (void)shrink_geometry_add_wall(g, 0, 0, 0, 21, NULL);
    (void)shrink_geometry_add_wall(g, 27, 0, 27, 21, NULL);
}

int shrink_geometry_blocked(const ShrinkGeometry *g, int x, int y)
{
    if (!valid_cell(g, x, y) || !g->floor[y * g->width + x]) return 1;
    for (size_t i = 0; i < g->fixture_count; ++i)
        if (g->fixtures[i].solid && g->fixtures[i].x == x && g->fixtures[i].y == y) return 1;
    for (size_t i = 0; i < g->wall_count; ++i) {
        const ShrinkWall *w = &g->walls[i];
        if (w->ay == w->by && y == w->ay && x >= (w->ax < w->bx ? w->ax : w->bx) && x <= (w->ax > w->bx ? w->ax : w->bx)) return 1;
        if (w->ax == w->bx && x == w->ax && y >= (w->ay < w->by ? w->ay : w->by) && y <= (w->ay > w->by ? w->ay : w->by)) return 1;
    }
    return 0;
}

static int fixture_collision(const ShrinkGeometry *g, int x, int y, uint64_t ignored_id)
{
    for (size_t i = 0; i < g->fixture_count; ++i)
        if (g->fixtures[i].id != ignored_id && g->fixtures[i].x == x && g->fixtures[i].y == y) return 1;
    return 0;
}

static unsigned is_solid(ShrinkFixtureType type)
{
    return type == SHRINK_FIXTURE_SHELF || type == SHRINK_FIXTURE_BIN || type == SHRINK_FIXTURE_SHORT_SHELF ||
           type == SHRINK_FIXTURE_LOCKED_SHELF || type == SHRINK_FIXTURE_CLEARANCE || type == SHRINK_FIXTURE_LOCKED_CASE ||
           0U;
}

ShrinkBuildResult shrink_geometry_place_fixture(ShrinkGeometry *g, ShrinkFixtureType type, int x, int y, unsigned rotation, uint64_t *out_id)
{
    if (g == NULL || g->fixture_count >= SHRINK_MAX_FIXTURES || !valid_cell(g, x, y)) return SHRINK_BUILD_OUT_OF_BOUNDS;
    if (fixture_collision(g, x, y, 0U)) return SHRINK_BUILD_COLLISION;
    if ((type == SHRINK_FIXTURE_ENTRANCE || type == SHRINK_FIXTURE_EXIT) && !g->floor[y * g->width + x]) return SHRINK_BUILD_INVALID_DOOR;
    ShrinkFixture candidate = {g->next_id++, type, x, y, rotation % 4U, is_solid(type)};
    g->fixtures[g->fixture_count++] = candidate;
    if (!routes_after_change(g)) { g->fixture_count--; return SHRINK_BUILD_BLOCKS_ROUTE; }
    if (out_id != NULL) *out_id = candidate.id;
    return SHRINK_BUILD_OK;
}

const ShrinkFixture *shrink_geometry_find_fixture(const ShrinkGeometry *g, uint64_t id)
{
    if (g == NULL) return NULL;
    for (size_t i = 0; i < g->fixture_count; ++i) if (g->fixtures[i].id == id) return &g->fixtures[i];
    return NULL;
}

ShrinkBuildResult shrink_geometry_move_fixture(ShrinkGeometry *g, uint64_t id, int x, int y)
{
    if (!valid_cell(g, x, y)) return SHRINK_BUILD_OUT_OF_BOUNDS;
    for (size_t i = 0; i < g->fixture_count; ++i) {
        ShrinkFixture *f = &g->fixtures[i];
        if (f->id != id) continue;
        if (fixture_collision(g, x, y, id)) return SHRINK_BUILD_COLLISION;
        const int old_x = f->x, old_y = f->y;
        f->x = x; f->y = y;
        if (!routes_after_change(g)) { f->x = old_x; f->y = old_y; return SHRINK_BUILD_BLOCKS_ROUTE; }
        return SHRINK_BUILD_OK;
    }
    return SHRINK_BUILD_NOT_FOUND;
}

ShrinkBuildResult shrink_geometry_rotate_fixture(ShrinkGeometry *g, uint64_t id, unsigned rotation)
{
    for (size_t i = 0; i < g->fixture_count; ++i) if (g->fixtures[i].id == id) { g->fixtures[i].rotation = rotation % 4U; return SHRINK_BUILD_OK; }
    return SHRINK_BUILD_NOT_FOUND;
}

ShrinkBuildResult shrink_geometry_remove_fixture(ShrinkGeometry *g, uint64_t id)
{
    for (size_t i = 0; i < g->fixture_count; ++i) {
        if (g->fixtures[i].id != id) continue;
        if (g->fixtures[i].type == SHRINK_FIXTURE_ENTRANCE || g->fixtures[i].type == SHRINK_FIXTURE_EXIT) {
            size_t entrances = 0U, exits = 0U;
            for (size_t j = 0; j < g->fixture_count; ++j) { entrances += g->fixtures[j].type == SHRINK_FIXTURE_ENTRANCE; exits += g->fixtures[j].type == SHRINK_FIXTURE_EXIT; }
            if ((g->fixtures[i].type == SHRINK_FIXTURE_ENTRANCE && entrances <= 1U) || (g->fixtures[i].type == SHRINK_FIXTURE_EXIT && exits <= 1U)) return SHRINK_BUILD_INVALID_DOOR;
        }
        ShrinkFixture saved = g->fixtures[i];
        memmove(&g->fixtures[i], &g->fixtures[i + 1], (g->fixture_count - i - 1U) * sizeof(g->fixtures[0]));
        --g->fixture_count;
        if (!routes_after_change(g)) { memmove(&g->fixtures[i + 1], &g->fixtures[i], (g->fixture_count - i) * sizeof(g->fixtures[0])); g->fixtures[i] = saved; ++g->fixture_count; return SHRINK_BUILD_BLOCKS_ROUTE; }
        return SHRINK_BUILD_OK;
    }
    return SHRINK_BUILD_NOT_FOUND;
}

ShrinkBuildResult shrink_geometry_add_wall(ShrinkGeometry *g, int ax, int ay, int bx, int by, uint64_t *out_id)
{
    if (g == NULL || g->wall_count >= SHRINK_MAX_WALLS || !valid_cell(g, ax, ay) || !valid_cell(g, bx, by)) return SHRINK_BUILD_OUT_OF_BOUNDS;
    if (ax != bx && ay != by) return SHRINK_BUILD_INVALID_WALL;
    if (ax == bx && ay == by) return SHRINK_BUILD_INVALID_WALL;
    ShrinkWall wall = {g->next_id++, ax, ay, bx, by};
    g->walls[g->wall_count++] = wall;
    if (!shrink_geometry_routes_valid(g) && g->fixture_count > 0U) { --g->wall_count; return SHRINK_BUILD_BLOCKS_ROUTE; }
    if (out_id != NULL) *out_id = wall.id;
    return SHRINK_BUILD_OK;
}

ShrinkBuildResult shrink_geometry_remove_wall(ShrinkGeometry *g, uint64_t id)
{
    for (size_t i = 0; i < g->wall_count; ++i) if (g->walls[i].id == id) { memmove(&g->walls[i], &g->walls[i + 1], (g->wall_count - i - 1U) * sizeof(g->walls[0])); --g->wall_count; return SHRINK_BUILD_OK; }
    return SHRINK_BUILD_NOT_FOUND;
}

int shrink_geometry_routes_valid(const ShrinkGeometry *g)
{
    size_t entrances = 0U, exits = 0U, registers = 0U;
    for (size_t i = 0; i < g->fixture_count; ++i) {
        entrances += g->fixtures[i].type == SHRINK_FIXTURE_ENTRANCE;
        exits += g->fixtures[i].type == SHRINK_FIXTURE_EXIT;
        registers += g->fixtures[i].type == SHRINK_FIXTURE_REGISTER || g->fixtures[i].type == SHRINK_FIXTURE_SELF_CHECKOUT;
    }
    return entrances > 0U && exits > 0U && registers > 0U && routes_after_change(g);
}

void shrink_geometry_blocked_map(const ShrinkGeometry *g, unsigned char *out_blocked)
{
    if (g == NULL || out_blocked == NULL) return;
    for (int y = 0; y < g->height; ++y)
        for (int x = 0; x < g->width; ++x) out_blocked[y * g->width + x] = (unsigned char)shrink_geometry_blocked(g, x, y);
}
