#define _POSIX_C_SOURCE 200809L
#include "shrink.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>
#include <time.h>

static void usage(const char *name)
{
    fprintf(stderr, "Usage: %s [--days N] [--seed N] [--stream --ticks N]\n", name);
}

static ShrinkFixtureType fixture_type(const char *name)
{
    const char *names[] = {"shelf", "shelf_bin", "short_shelf", "locked_shelf", "clearance", "register", "self_checkout", "camera", "entrance", "exit", "rfid_station", "locked_case"};
    const ShrinkFixtureType types[] = {SHRINK_FIXTURE_SHELF, SHRINK_FIXTURE_BIN, SHRINK_FIXTURE_SHORT_SHELF, SHRINK_FIXTURE_LOCKED_SHELF, SHRINK_FIXTURE_CLEARANCE, SHRINK_FIXTURE_REGISTER, SHRINK_FIXTURE_SELF_CHECKOUT, SHRINK_FIXTURE_CAMERA, SHRINK_FIXTURE_ENTRANCE, SHRINK_FIXTURE_EXIT, SHRINK_FIXTURE_RFID_STATION, SHRINK_FIXTURE_LOCKED_CASE};
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) if (strcmp(name, names[i]) == 0) return types[i];
    return 0;
}

static void process_commands(ShrinkWorld *world)
{
    struct pollfd input = {STDIN_FILENO, POLLIN, 0};
    if (poll(&input, 1, 0) <= 0 || !(input.revents & POLLIN)) return;
    char line[256];
    if (fgets(line, sizeof(line), stdin) == NULL) return;
    uint64_t id = 0U, new_id = 0U;
    int x = 0, y = 0, ax = 0, ay = 0, bx = 0, by = 0, rotation = 0;
    char type[32];
    ShrinkBuildResult result = SHRINK_BUILD_NOT_FOUND;
    if (sscanf(line, "PLACE %31s %d %d %d", type, &x, &y, &rotation) == 4)
        result = shrink_try_place_fixture(world, fixture_type(type), x, y, (unsigned)rotation, &new_id);
    else if (sscanf(line, "MOVE %" SCNu64 " %d %d", &id, &x, &y) == 3)
        result = shrink_try_move_fixture(world, id, x, y);
    else if (sscanf(line, "ROTATE %" SCNu64 " %d", &id, &rotation) == 2)
        result = shrink_try_rotate_fixture(world, id, (unsigned)rotation);
    else if (sscanf(line, "REMOVE %" SCNu64, &id) == 1)
        result = shrink_try_remove_fixture(world, id);
    else if (sscanf(line, "WALL %d %d %d %d", &ax, &ay, &bx, &by) == 4)
        result = shrink_try_add_wall(world, ax, ay, bx, by, &new_id);
    else if (sscanf(line, "UNWALL %" SCNu64, &id) == 1)
        result = shrink_try_remove_wall(world, id);
    printf("COMMAND %d %" PRIu64 "\n", (int)result, new_id);
    fflush(stdout);
}

static void stream_geometry(const ShrinkWorld *world)
{
    ShrinkGeometryInfo info;
    shrink_geometry_info(world, &info);
    printf("GEOMETRY %d %d %zu %zu\n", info.width, info.height, info.wall_count, info.fixture_count);
    for (size_t i = 0; i < info.wall_count; ++i) {
        ShrinkWallSnapshot wall;
        if (shrink_wall_snapshot(world, i, &wall)) printf("WALL %llu %d %d %d %d\n", (unsigned long long)wall.id, wall.ax, wall.ay, wall.bx, wall.by);
    }
    for (size_t i = 0; i < info.fixture_count; ++i) {
        ShrinkFixtureSnapshot fixture;
        if (shrink_fixture_snapshot(world, i, &fixture)) printf("FIXTURE %llu %d %d %d %d\n", (unsigned long long)fixture.id, (int)fixture.type, fixture.x, fixture.y, (int)fixture.rotation);
    }
}

static void stream_frame(const ShrinkWorld *world)
{
    ShrinkMetrics metrics;
    shrink_metrics(world, &metrics);
    const size_t count = shrink_entity_count(world);
    stream_geometry(world);
    printf("TICK %llu %zu %.2f %.2f %.2f %.2f %.2f\n",
           (unsigned long long)shrink_tick_count(world), count, metrics.revenue,
           metrics.stolen_value, metrics.labor_cost, metrics.average_checkout_wait,
           metrics.average_satisfaction);
    for (size_t i = 0; i < count; ++i) {
        ShrinkEntitySnapshot entity;
        if (shrink_entity_snapshot(world, i, &entity))
            printf("ENTITY %llu %d %.3f %.3f %u\n", (unsigned long long)entity.id,
                   (int)entity.state, entity.x, entity.y, entity.product);
    }
    fflush(stdout);
}

int main(int argc, char **argv)
{
    unsigned days = 30U;
    uint64_t seed = 12345U;
    unsigned stream_ticks = 600U;
    int stream = 0;
    int realtime = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--days") == 0 && i + 1 < argc) days = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) seed = (uint64_t)strtoull(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--stream") == 0) stream = 1;
        else if (strcmp(argv[i], "--realtime") == 0) realtime = 1;
        else if (strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) stream_ticks = (unsigned)strtoul(argv[++i], NULL, 10);
        else { usage(argv[0]); return 2; }
    }
    ShrinkWorld *world = shrink_create(seed);
    if (world == NULL) { fputs("Unable to create simulation\n", stderr); return 1; }
    if (stream) {
        for (unsigned tick = 0; tick < stream_ticks; ++tick) {
            process_commands(world);
            shrink_tick(world, 1.0);
            stream_frame(world);
            if (realtime) {
                const struct timespec delay = {0, 50000000L};
                (void)nanosleep(&delay, NULL);
            }
        }
        shrink_destroy(world);
        return 0;
    }
    for (unsigned day = 0; day < days; ++day)
        for (unsigned second = 0; second < 600U; ++second) shrink_tick(world, 1.0);
    ShrinkMetrics m;
    shrink_metrics(world, &m);
    printf("Shrink City simulation\nSeed: %" PRIu64 "\nTicks: %" PRIu64 "\n\n", seed, shrink_tick_count(world));
    printf("Customers entered: %" PRIu64 "\nPurchases: %" PRIu64 "\nAbandoned: %" PRIu64 "\nThefts: %" PRIu64 "\n\n", m.customers_entered, m.purchases, m.abandoned, m.thefts);
    printf("Revenue: $%.2f\nShrink: $%.2f\nLabor: $%.2f\nProfit: $%.2f\n\n", m.revenue, m.stolen_value, m.labor_cost, m.profit);
    printf("Average checkout wait: %.1f sec\nAverage satisfaction: %.1f%%\n", m.average_checkout_wait, m.average_satisfaction);
    shrink_destroy(world);
    return 0;
}
