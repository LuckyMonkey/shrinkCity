#ifndef SHRINK_GEOMETRY_H
#define SHRINK_GEOMETRY_H

#include "shrink.h"
#include <stddef.h>
#include <stdint.h>

#define SHRINK_MAX_WALLS 256U
#define SHRINK_MAX_FIXTURES 512U
#define SHRINK_MAX_CELLS 1024U

#define SHRINK_ACCESS_NORTH 0x1U
#define SHRINK_ACCESS_EAST  0x2U
#define SHRINK_ACCESS_SOUTH 0x4U
#define SHRINK_ACCESS_WEST  0x8U

typedef struct ShrinkWall {
    uint64_t id;
    int ax, ay, bx, by;
} ShrinkWall;

typedef struct ShrinkFixture {
    uint64_t id;
    ShrinkFixtureType type;
    int x, y;
    unsigned rotation;
    unsigned solid;
    unsigned width;
    unsigned height;
    unsigned access_mask;
    int product_id;
} ShrinkFixture;

typedef struct ShrinkGeometry {
    int width, height;
    unsigned char floor[SHRINK_MAX_CELLS];
    ShrinkWall walls[SHRINK_MAX_WALLS];
    size_t wall_count;
    ShrinkFixture fixtures[SHRINK_MAX_FIXTURES];
    size_t fixture_count;
    uint64_t next_id;
} ShrinkGeometry;

void shrink_geometry_init(ShrinkGeometry *geometry);
int shrink_geometry_blocked(const ShrinkGeometry *geometry, int x, int y);
ShrinkBuildResult shrink_geometry_place_fixture(ShrinkGeometry *geometry, ShrinkFixtureType type, int x, int y, unsigned rotation, uint64_t *out_id);
ShrinkBuildResult shrink_geometry_move_fixture(ShrinkGeometry *geometry, uint64_t id, int x, int y);
ShrinkBuildResult shrink_geometry_rotate_fixture(ShrinkGeometry *geometry, uint64_t id, unsigned rotation);
ShrinkBuildResult shrink_geometry_remove_fixture(ShrinkGeometry *geometry, uint64_t id);
ShrinkBuildResult shrink_geometry_add_wall(ShrinkGeometry *geometry, int ax, int ay, int bx, int by, uint64_t *out_id);
ShrinkBuildResult shrink_geometry_remove_wall(ShrinkGeometry *geometry, uint64_t id);
const ShrinkFixture *shrink_geometry_find_fixture(const ShrinkGeometry *geometry, uint64_t id);
int shrink_geometry_routes_valid(const ShrinkGeometry *geometry);
void shrink_geometry_blocked_map(const ShrinkGeometry *geometry, unsigned char *out_blocked);
int shrink_geometry_fixture_accessible(const ShrinkGeometry *geometry, uint64_t id);
int shrink_geometry_best_access_cell(const ShrinkGeometry *geometry, uint64_t id, int from_x, int from_y, int *out_x, int *out_y, int *out_distance);
void shrink_geometry_set_fixture_product(ShrinkGeometry *geometry, uint64_t id, int product_id);

#endif
