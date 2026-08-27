#include "geometry.h"
#include "pathfinding.h"

#include <string.h>
#include <limits.h>

static int valid_cell(const ShrinkGeometry *g, int x, int y)
{
    return g != NULL && x >= 0 && y >= 0 && x < g->width && y < g->height;
}

static unsigned rotate_access_mask(unsigned mask, unsigned rotation)
{
    rotation %= 4U;
    while (rotation-- > 0U) {
        unsigned next = 0U;
        if (mask & SHRINK_ACCESS_NORTH) next |= SHRINK_ACCESS_EAST;
        if (mask & SHRINK_ACCESS_EAST) next |= SHRINK_ACCESS_SOUTH;
        if (mask & SHRINK_ACCESS_SOUTH) next |= SHRINK_ACCESS_WEST;
        if (mask & SHRINK_ACCESS_WEST) next |= SHRINK_ACCESS_NORTH;
        mask = next;
    }
    return mask;
}

static void fixture_metadata(ShrinkFixtureType type, unsigned rotation,
                             unsigned *width, unsigned *height,
                             unsigned *solid, unsigned *access_mask)
{
    unsigned w = 1U, h = 1U, s = 0U, access = 0U;
    switch (type) {
        case SHRINK_FIXTURE_SHELF:
            w = 3U; s = 1U; access = SHRINK_ACCESS_NORTH | SHRINK_ACCESS_SOUTH; break;
        case SHRINK_FIXTURE_SHORT_SHELF:
            w = 2U; s = 1U; access = SHRINK_ACCESS_NORTH | SHRINK_ACCESS_SOUTH; break;
        case SHRINK_FIXTURE_LOCKED_SHELF:
            w = 2U; s = 1U; access = SHRINK_ACCESS_SOUTH; break;
        case SHRINK_FIXTURE_CLEARANCE:
            w = 2U; s = 1U; access = SHRINK_ACCESS_NORTH | SHRINK_ACCESS_EAST | SHRINK_ACCESS_SOUTH | SHRINK_ACCESS_WEST; break;
        case SHRINK_FIXTURE_LOCKED_CASE:
            w = 2U; s = 1U; access = SHRINK_ACCESS_SOUTH; break;
        case SHRINK_FIXTURE_BIN:
            s = 1U; access = SHRINK_ACCESS_NORTH | SHRINK_ACCESS_EAST | SHRINK_ACCESS_SOUTH | SHRINK_ACCESS_WEST; break;
        case SHRINK_FIXTURE_RFID_STATION:
            s = 1U; access = SHRINK_ACCESS_NORTH | SHRINK_ACCESS_EAST | SHRINK_ACCESS_SOUTH | SHRINK_ACCESS_WEST; break;
        default:
            break;
    }
    rotation %= 4U;
    if ((rotation & 1U) != 0U) {
        const unsigned tmp = w; w = h; h = tmp;
    }
    if (width != NULL) *width = w;
    if (height != NULL) *height = h;
    if (solid != NULL) *solid = s;
    if (access_mask != NULL) *access_mask = rotate_access_mask(access, rotation);
}

static int merchandise_fixture(ShrinkFixtureType type)
{
    return type == SHRINK_FIXTURE_SHELF || type == SHRINK_FIXTURE_BIN ||
           type == SHRINK_FIXTURE_SHORT_SHELF || type == SHRINK_FIXTURE_LOCKED_SHELF ||
           type == SHRINK_FIXTURE_CLEARANCE || type == SHRINK_FIXTURE_LOCKED_CASE;
}

static int fixture_contains(const ShrinkFixture *f, int x, int y)
{
    return f != NULL && x >= f->x && y >= f->y &&
           x < f->x + (int)f->width && y < f->y + (int)f->height;
}

static int wall_blocks(const ShrinkGeometry *g, int x, int y)
{
    for (size_t i = 0; i < g->wall_count; ++i) {
        const ShrinkWall *w = &g->walls[i];
        if (w->ay == w->by && y == w->ay && x >= (w->ax < w->bx ? w->ax : w->bx) && x <= (w->ax > w->bx ? w->ax : w->bx)) return 1;
        if (w->ax == w->bx && x == w->ax && y >= (w->ay < w->by ? w->ay : w->by) && y <= (w->ay > w->by ? w->ay : w->by)) return 1;
    }
    return 0;
}

int shrink_geometry_blocked(const ShrinkGeometry *g, int x, int y)
{
    if (!valid_cell(g, x, y) || !g->floor[y * g->width + x]) return 1;
    for (size_t i = 0; i < g->fixture_count; ++i)
        if (g->fixtures[i].solid && fixture_contains(&g->fixtures[i], x, y)) return 1;
    return wall_blocks(g, x, y);
}

static int fixture_footprint_valid(const ShrinkGeometry *g, const ShrinkFixture *candidate, uint64_t ignored_id)
{
    for (unsigned oy = 0; oy < candidate->height; ++oy) {
        for (unsigned ox = 0; ox < candidate->width; ++ox) {
            const int x = candidate->x + (int)ox;
            const int y = candidate->y + (int)oy;
            if (!valid_cell(g, x, y) || !g->floor[y * g->width + x] || wall_blocks(g, x, y)) return 0;
            for (size_t i = 0; i < g->fixture_count; ++i) {
                const ShrinkFixture *other = &g->fixtures[i];
                if (other->id == ignored_id) continue;
                if (fixture_contains(other, x, y)) return 0;
            }
        }
    }
    return 1;
}

static void flood_from_entrances(const ShrinkGeometry *g, unsigned char *reachable)
{
    unsigned short queue[SHRINK_MAX_CELLS];
    size_t head = 0U, tail = 0U;
    memset(reachable, 0, SHRINK_MAX_CELLS);

    for (size_t i = 0; i < g->fixture_count; ++i) {
        const ShrinkFixture *f = &g->fixtures[i];
        if (f->type != SHRINK_FIXTURE_ENTRANCE || shrink_geometry_blocked(g, f->x, f->y)) continue;
        const int idx = f->y * g->width + f->x;
        if (!reachable[idx]) {
            reachable[idx] = 1U;
            queue[tail++] = (unsigned short)idx;
        }
    }

    while (head < tail) {
        const int idx = (int)queue[head++];
        const int x = idx % g->width;
        const int y = idx / g->width;
        static const int dx[4] = {1, 0, -1, 0};
        static const int dy[4] = {0, 1, 0, -1};
        for (unsigned d = 0; d < 4U; ++d) {
            const int nx = x + dx[d], ny = y + dy[d];
            if (!valid_cell(g, nx, ny) || shrink_geometry_blocked(g, nx, ny)) continue;
            const int nidx = ny * g->width + nx;
            if (reachable[nidx]) continue;
            reachable[nidx] = 1U;
            queue[tail++] = (unsigned short)nidx;
        }
    }
}

static int access_cell_reachable(const ShrinkGeometry *g, const ShrinkFixture *f,
                                 const unsigned char *reachable)
{
    if (f->access_mask & SHRINK_ACCESS_NORTH) {
        const int y = f->y - 1;
        for (unsigned ox = 0; ox < f->width; ++ox) {
            const int x = f->x + (int)ox;
            if (valid_cell(g, x, y) && !shrink_geometry_blocked(g, x, y) && reachable[y * g->width + x]) return 1;
        }
    }
    if (f->access_mask & SHRINK_ACCESS_SOUTH) {
        const int y = f->y + (int)f->height;
        for (unsigned ox = 0; ox < f->width; ++ox) {
            const int x = f->x + (int)ox;
            if (valid_cell(g, x, y) && !shrink_geometry_blocked(g, x, y) && reachable[y * g->width + x]) return 1;
        }
    }
    if (f->access_mask & SHRINK_ACCESS_WEST) {
        const int x = f->x - 1;
        for (unsigned oy = 0; oy < f->height; ++oy) {
            const int y = f->y + (int)oy;
            if (valid_cell(g, x, y) && !shrink_geometry_blocked(g, x, y) && reachable[y * g->width + x]) return 1;
        }
    }
    if (f->access_mask & SHRINK_ACCESS_EAST) {
        const int x = f->x + (int)f->width;
        for (unsigned oy = 0; oy < f->height; ++oy) {
            const int y = f->y + (int)oy;
            if (valid_cell(g, x, y) && !shrink_geometry_blocked(g, x, y) && reachable[y * g->width + x]) return 1;
        }
    }
    return 0;
}

static int access_candidate(const ShrinkGeometry *g, const ShrinkFixture *f, int x, int y)
{
    if (!valid_cell(g, x, y) || shrink_geometry_blocked(g, x, y)) return 0;
    if ((f->access_mask & SHRINK_ACCESS_NORTH) && y == f->y - 1 && x >= f->x && x < f->x + (int)f->width) return 1;
    if ((f->access_mask & SHRINK_ACCESS_SOUTH) && y == f->y + (int)f->height && x >= f->x && x < f->x + (int)f->width) return 1;
    if ((f->access_mask & SHRINK_ACCESS_WEST) && x == f->x - 1 && y >= f->y && y < f->y + (int)f->height) return 1;
    if ((f->access_mask & SHRINK_ACCESS_EAST) && x == f->x + (int)f->width && y >= f->y && y < f->y + (int)f->height) return 1;
    return 0;
}

int shrink_geometry_best_access_cell(const ShrinkGeometry *g, uint64_t id, int from_x, int from_y, int *out_x, int *out_y, int *out_distance)
{
    const ShrinkFixture *fixture = shrink_geometry_find_fixture(g, id);
    unsigned short distance[SHRINK_MAX_CELLS];
    unsigned short queue[SHRINK_MAX_CELLS];
    size_t head = 0U, tail = 0U;
    int best_distance = INT_MAX, best_x = -1, best_y = -1;
    if (g == NULL || fixture == NULL || !valid_cell(g, from_x, from_y) || shrink_geometry_blocked(g, from_x, from_y)) return 0;
    for (int i = 0; i < g->width * g->height; ++i) distance[i] = USHRT_MAX;
    distance[from_y * g->width + from_x] = 0U;
    queue[tail++] = (unsigned short)(from_y * g->width + from_x);
    while (head < tail) {
        const int index = (int)queue[head++];
        const int x = index % g->width, y = index / g->width;
        const int next_distance = (int)distance[index] + 1;
        if (access_candidate(g, fixture, x, y) && ((int)distance[index] < best_distance ||
            ((int)distance[index] == best_distance && (y < best_y || (y == best_y && x < best_x))))) {
            best_distance = distance[index]; best_x = x; best_y = y;
        }
        if ((int)distance[index] >= best_distance) continue;
        static const int dx[4] = {1, 0, -1, 0};
        static const int dy[4] = {0, 1, 0, -1};
        for (unsigned d = 0U; d < 4U; ++d) {
            const int nx = x + dx[d], ny = y + dy[d];
            if (!valid_cell(g, nx, ny) || shrink_geometry_blocked(g, nx, ny)) continue;
            const int nindex = ny * g->width + nx;
            if (distance[nindex] != USHRT_MAX) continue;
            distance[nindex] = (unsigned short)next_distance;
            queue[tail++] = (unsigned short)nindex;
        }
    }
    if (best_x < 0) return 0;
    if (out_x != NULL) *out_x = best_x;
    if (out_y != NULL) *out_y = best_y;
    if (out_distance != NULL) *out_distance = best_distance;
    return 1;
}

void shrink_geometry_set_fixture_product(ShrinkGeometry *g, uint64_t id, int product_id)
{
    ShrinkFixture *fixture;
    for (size_t i = 0U; g != NULL && i < g->fixture_count; ++i) {
        fixture = &g->fixtures[i];
        if (fixture->id == id) { fixture->product_id = product_id; return; }
    }
}

int shrink_geometry_fixture_accessible(const ShrinkGeometry *g, uint64_t id)
{
    unsigned char reachable[SHRINK_MAX_CELLS];
    const ShrinkFixture *fixture = shrink_geometry_find_fixture(g, id);
    if (fixture == NULL) return 0;
    if (!merchandise_fixture(fixture->type)) return 1;
    flood_from_entrances(g, reachable);
    return access_cell_reachable(g, fixture, reachable);
}

static int routes_after_change(const ShrinkGeometry *g)
{
    size_t entrances = 0U, exits = 0U, registers = 0U;
    int reachable_exit = 0, reachable_register = 0;
    unsigned char reachable[SHRINK_MAX_CELLS];

    for (size_t i = 0; i < g->fixture_count; ++i) {
        entrances += g->fixtures[i].type == SHRINK_FIXTURE_ENTRANCE;
        exits += g->fixtures[i].type == SHRINK_FIXTURE_EXIT;
        registers += g->fixtures[i].type == SHRINK_FIXTURE_REGISTER || g->fixtures[i].type == SHRINK_FIXTURE_SELF_CHECKOUT;
    }
    if (entrances == 0U || exits == 0U || registers == 0U) return 1;

    flood_from_entrances(g, reachable);
    for (size_t i = 0; i < g->fixture_count; ++i) {
        const ShrinkFixture *f = &g->fixtures[i];
        if (valid_cell(g, f->x, f->y) && reachable[f->y * g->width + f->x]) {
            if (f->type == SHRINK_FIXTURE_EXIT) reachable_exit = 1;
            if (f->type == SHRINK_FIXTURE_REGISTER || f->type == SHRINK_FIXTURE_SELF_CHECKOUT) reachable_register = 1;
        }
        if (merchandise_fixture(f->type) && !access_cell_reachable(g, f, reachable)) return 0;
    }
    return reachable_exit && reachable_register;
}

void shrink_geometry_init(ShrinkGeometry *g)
{
    memset(g, 0, sizeof(*g));
    g->width = 28; g->height = 22; g->next_id = 1U;
    for (int y = 0; y < g->height; ++y)
        for (int x = 0; x < g->width; ++x) g->floor[y * g->width + x] = 1U;
    /* The C-owned footprint has clipped corners, leaving room for the site layer. */
    for (int y = 1; y <= 3; ++y) g->floor[y * g->width + 1] = 0U;
    for (int y = 18; y <= 20; ++y) g->floor[y * g->width + 1] = 0U;
    for (int y = 1; y <= 2; ++y) g->floor[y * g->width + 26] = 0U;
    for (int y = 19; y <= 20; ++y) g->floor[y * g->width + 26] = 0U;
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_ENTRANCE, 1, 10, 0U, NULL);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_EXIT, 2, 10, 0U, NULL);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_REGISTER, 14, 8, 0U, NULL);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_REGISTER, 14, 12, 0U, NULL);
    uint64_t product_fixture_ids[4] = {0U, 0U, 0U, 0U};
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_SHELF, 4, 5, 0U, &product_fixture_ids[0]);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_SHELF, 8, 13, 0U, &product_fixture_ids[1]);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_SHELF, 11, 5, 0U, &product_fixture_ids[2]);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_BIN, 6, 8, 0U, &product_fixture_ids[3]);
    for (int product = 0; product < 4; ++product) shrink_geometry_set_fixture_product(g, product_fixture_ids[product], product);
    uint64_t second_product_fixture_id = 0U;
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_LOCKED_SHELF, 12, 8, 0U, &second_product_fixture_id);
    shrink_geometry_set_fixture_product(g, second_product_fixture_id, 0);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_CLEARANCE, 10, 12, 0U, NULL);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_SELF_CHECKOUT, 15, 8, 0U, NULL);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_CAMERA, 8, 3, 0U, NULL);
    (void)shrink_geometry_place_fixture(g, SHRINK_FIXTURE_CAMERA, 8, 17, 0U, NULL);
    (void)shrink_geometry_add_wall(g, 0, 0, 27, 0, NULL);
    (void)shrink_geometry_add_wall(g, 0, 21, 27, 21, NULL);
    (void)shrink_geometry_add_wall(g, 0, 0, 0, 21, NULL);
    (void)shrink_geometry_add_wall(g, 27, 0, 27, 21, NULL);
}

ShrinkBuildResult shrink_geometry_place_fixture(ShrinkGeometry *g, ShrinkFixtureType type, int x, int y, unsigned rotation, uint64_t *out_id)
{
    if (g == NULL || g->fixture_count >= SHRINK_MAX_FIXTURES) return SHRINK_BUILD_OUT_OF_BOUNDS;
    ShrinkFixture candidate;
    memset(&candidate, 0, sizeof(candidate));
    candidate.id = g->next_id++;
    candidate.type = type;
    candidate.x = x; candidate.y = y; candidate.rotation = rotation % 4U;
    candidate.product_id = -1;
    fixture_metadata(type, candidate.rotation, &candidate.width, &candidate.height, &candidate.solid, &candidate.access_mask);
    if (!fixture_footprint_valid(g, &candidate, 0U)) return valid_cell(g, x, y) ? SHRINK_BUILD_COLLISION : SHRINK_BUILD_OUT_OF_BOUNDS;
    if ((type == SHRINK_FIXTURE_ENTRANCE || type == SHRINK_FIXTURE_EXIT) && !g->floor[y * g->width + x]) return SHRINK_BUILD_INVALID_DOOR;
    g->fixtures[g->fixture_count++] = candidate;
    if (!routes_after_change(g)) { --g->fixture_count; return SHRINK_BUILD_BLOCKS_ROUTE; }
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
    for (size_t i = 0; g != NULL && i < g->fixture_count; ++i) {
        ShrinkFixture *f = &g->fixtures[i];
        if (f->id != id) continue;
        ShrinkFixture saved = *f;
        f->x = x; f->y = y;
        if (!fixture_footprint_valid(g, f, id)) { *f = saved; return valid_cell(g, x, y) ? SHRINK_BUILD_COLLISION : SHRINK_BUILD_OUT_OF_BOUNDS; }
        if (!routes_after_change(g)) { *f = saved; return SHRINK_BUILD_BLOCKS_ROUTE; }
        return SHRINK_BUILD_OK;
    }
    return SHRINK_BUILD_NOT_FOUND;
}

ShrinkBuildResult shrink_geometry_rotate_fixture(ShrinkGeometry *g, uint64_t id, unsigned rotation)
{
    for (size_t i = 0; g != NULL && i < g->fixture_count; ++i) {
        ShrinkFixture *f = &g->fixtures[i];
        if (f->id != id) continue;
        ShrinkFixture saved = *f;
        f->rotation = rotation % 4U;
        fixture_metadata(f->type, f->rotation, &f->width, &f->height, &f->solid, &f->access_mask);
        if (!fixture_footprint_valid(g, f, id)) { *f = saved; return SHRINK_BUILD_COLLISION; }
        if (!routes_after_change(g)) { *f = saved; return SHRINK_BUILD_BLOCKS_ROUTE; }
        return SHRINK_BUILD_OK;
    }
    return SHRINK_BUILD_NOT_FOUND;
}

ShrinkBuildResult shrink_geometry_remove_fixture(ShrinkGeometry *g, uint64_t id)
{
    for (size_t i = 0; g != NULL && i < g->fixture_count; ++i) {
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
    for (size_t i = 0; g != NULL && i < g->wall_count; ++i) if (g->walls[i].id == id) { memmove(&g->walls[i], &g->walls[i + 1], (g->wall_count - i - 1U) * sizeof(g->walls[0])); --g->wall_count; return SHRINK_BUILD_OK; }
    return SHRINK_BUILD_NOT_FOUND;
}

int shrink_geometry_routes_valid(const ShrinkGeometry *g)
{
    size_t entrances = 0U, exits = 0U, registers = 0U;
    if (g == NULL) return 0;
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
