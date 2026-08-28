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

static ShrinkEmployeeRole employee_role(const char *name)
{
    if (strcmp(name, "cashier") == 0) return SHRINK_EMPLOYEE_CASHIER;
    if (strcmp(name, "associate") == 0) return SHRINK_EMPLOYEE_ASSOCIATE;
    if (strcmp(name, "stocker") == 0) return SHRINK_EMPLOYEE_STOCKER;
    if (strcmp(name, "security") == 0 || strcmp(name, "guard") == 0) return SHRINK_EMPLOYEE_SECURITY;
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
    double wage = 0.0;
    char type[32];
    ShrinkBuildResult build_result = SHRINK_BUILD_NOT_FOUND;

    if (sscanf(line, "HIRE %31s %lf", type, &wage) >= 1) {
        const ShrinkStaffResult staff_result = shrink_hire_employee(world, employee_role(type), wage, &new_id);
        printf("STAFF %d %" PRIu64 "\n", (int)staff_result, new_id);
        fflush(stdout);
        return;
    }
    if (sscanf(line, "FIRE %" SCNu64, &id) == 1) {
        const ShrinkStaffResult staff_result = shrink_fire_employee(world, id);
        printf("STAFF %d %" PRIu64 "\n", (int)staff_result, id);
        fflush(stdout);
        return;
    }

    if (sscanf(line, "PLACE %31s %d %d %d", type, &x, &y, &rotation) == 4)
        build_result = shrink_try_place_fixture(world, fixture_type(type), x, y, (unsigned)rotation, &new_id);
    else if (sscanf(line, "MOVE %" SCNu64 " %d %d", &id, &x, &y) == 3)
        build_result = shrink_try_move_fixture(world, id, x, y);
    else if (sscanf(line, "ROTATE %" SCNu64 " %d", &id, &rotation) == 2)
        build_result = shrink_try_rotate_fixture(world, id, (unsigned)rotation);
    else if (sscanf(line, "REMOVE %" SCNu64, &id) == 1)
        build_result = shrink_try_remove_fixture(world, id);
    else if (sscanf(line, "WALL %d %d %d %d", &ax, &ay, &bx, &by) == 4)
        build_result = shrink_try_add_wall(world, ax, ay, bx, by, &new_id);
    else if (sscanf(line, "UNWALL %" SCNu64, &id) == 1)
        build_result = shrink_try_remove_wall(world, id);
    printf("COMMAND %d %" PRIu64 "\n", (int)build_result, new_id);
    fflush(stdout);
}

static void stream_geometry(const ShrinkWorld *world)
{
    ShrinkGeometryInfo info;
    shrink_geometry_info(world, &info);
    printf("GEOMETRY %d %d %zu %zu %zu %u\n", info.width, info.height, info.wall_count, info.fixture_count, info.room_count, info.layout_id);
    for (int y = 0; y < info.height; ++y) printf("FLOOR %d %u\n", y, shrink_floor_row(world, y));
    for (size_t i = 0; i < info.room_count; ++i) {
        ShrinkRoomSnapshot room;
        if (shrink_room_snapshot(world, i, &room)) printf("ROOM %llu %d %d %d %d %d %d %d\n", (unsigned long long)room.id, (int)room.type, room.x, room.y, room.width, room.height, room.customer_accessible, room.staff_accessible);
    }
    for (size_t i = 0; i < info.wall_count; ++i) {
        ShrinkWallSnapshot wall;
        if (shrink_wall_snapshot(world, i, &wall)) printf("WALL %llu %d %d %d %d\n", (unsigned long long)wall.id, wall.ax, wall.ay, wall.bx, wall.by);
    }
    for (size_t i = 0; i < info.fixture_count; ++i) {
        ShrinkFixtureSnapshot fixture;
        if (shrink_fixture_snapshot(world, i, &fixture)) printf("FIXTURE %llu %d %d %d %d %u %u %u %d\n", (unsigned long long)fixture.id, (int)fixture.type, fixture.x, fixture.y, (int)fixture.rotation, fixture.width, fixture.height, fixture.access_mask, fixture.product_id);
    }
}

static void stream_frame(const ShrinkWorld *world)
{
    ShrinkMetrics metrics;
    shrink_metrics(world, &metrics);
    const size_t count = shrink_entity_count(world);
    stream_geometry(world);
    for (size_t i = 0U; i < shrink_employee_count(world); ++i) {
        ShrinkEmployeeSnapshot employee;
        if (shrink_employee_snapshot(world, i, &employee))
            printf("EMPLOYEE %llu %d %.2f %.2f %.2f %.2f %d %d %llu %d %d\n",
                   (unsigned long long)employee.id, (int)employee.role, employee.wage, employee.skill,
                   employee.fatigue, employee.morale, employee.x, employee.y,
                   (unsigned long long)employee.target_fixture_id, employee.target_x, employee.target_y);
    }
    printf("TICK %llu %zu %.2f %.2f %.2f %.2f %.2f\n",
           (unsigned long long)shrink_tick_count(world), count, metrics.revenue,
           metrics.stolen_value, metrics.labor_cost, metrics.average_checkout_wait,
           metrics.average_satisfaction);
    printf("BALANCE %.2f %.2f %llu\n", metrics.cost_of_goods, metrics.security_cost, (unsigned long long)metrics.active_employees);
    for (size_t i = 0; i < count; ++i) {
        ShrinkEntitySnapshot entity;
        if (shrink_entity_snapshot(world, i, &entity))
            printf("ENTITY %llu %d %.3f %.3f %u %llu %d %d %d %.2f %.1f %.2f %.2f\n", (unsigned long long)entity.id,
                   (int)entity.state, entity.x, entity.y, entity.product,
                   (unsigned long long)entity.target_fixture_id, entity.target_x, entity.target_y,
                   (int)entity.archetype, entity.walking_speed, entity.patience_seconds, entity.budget, entity.theft_tendency);
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
    printf("Revenue: $%.2f\nCOGS: $%.2f\nShrink: $%.2f\nLabor: $%.2f\nSecurity: $%.2f\nProfit: $%.2f\n\n", m.revenue, m.cost_of_goods, m.stolen_value, m.labor_cost, m.security_cost, m.profit);
    printf("Average checkout wait: %.1f sec\nAverage satisfaction: %.1f%%\n", m.average_checkout_wait, m.average_satisfaction);
    shrink_destroy(world);
    return 0;
}
