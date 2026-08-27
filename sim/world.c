#include "shrink.h"
#include "pathfinding.h"
#include "geometry.h"

#include <math.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CUSTOMERS 128U
#define REGISTER_COUNT 2U
#define PRODUCT_COUNT 4U
#define OPEN_SECONDS_PER_DAY 600.0
#define SPAWN_INTERVAL 8.0

typedef enum CustomerState {
    CUSTOMER_UNUSED,
    CUSTOMER_TO_PRODUCT,
    CUSTOMER_DECIDING,
    CUSTOMER_WAITING,
    CUSTOMER_CHECKOUT,
    CUSTOMER_LEAVING
} CustomerState;

typedef struct Product { double price; unsigned stock; } Product;
typedef struct Customer {
    CustomerState state;
    double x, y;
    double target_x, target_y;
    uint64_t target_fixture_id;
    double wait_seconds;
    double satisfaction;
    unsigned product;
    unsigned register_id;
    uint64_t id;
} Customer;

struct ShrinkWorld {
    uint64_t seed;
    uint64_t ticks;
    double time;
    double spawn_timer;
    uint64_t next_customer_id;
    uint64_t active_customers;
    uint64_t register_customer[REGISTER_COUNT];
    double register_timer[REGISTER_COUNT];
    Product products[PRODUCT_COUNT];
    Customer customers[MAX_CUSTOMERS];
    ShrinkGeometry geometry;
    ShrinkMetrics metrics;
    double satisfaction_sum;
    double checkout_wait_sum;
    uint64_t satisfaction_count;
};

static uint64_t rng_next(ShrinkWorld *world)
{
    /* PCG-style xorshift is small, fast, and entirely owned by the world. */
    uint64_t x = world->seed;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    world->seed = x;
    return x * UINT64_C(2685821657736338717);
}

static double random_unit(ShrinkWorld *world)
{
    return (double)(rng_next(world) >> 11) * (1.0 / 9007199254740992.0);
}

static double distance_to(double x, double y, double target_x, double target_y)
{
    return hypot(target_x - x, target_y - y);
}

static void move_toward(Customer *customer, double dt, const unsigned char *blocked, int width, int height)
{
    const double speed = 3.0;
    const int start_x = (int)lround(customer->x), start_y = (int)lround(customer->y);
    const int goal_x = (int)lround(customer->target_x), goal_y = (int)lround(customer->target_y);
    int next_x = goal_x, next_y = goal_y;
    (void)shrink_path_next_step_grid(start_x, start_y, goal_x, goal_y, blocked, width, height, &next_x, &next_y);
    const double dx = (double)next_x - customer->x;
    const double dy = (double)next_y - customer->y;
    const double distance = hypot(dx, dy);
    if (distance <= speed * dt || distance < 0.001) {
        customer->x = (next_x == goal_x && next_y == goal_y) ? customer->target_x : (double)next_x;
        customer->y = (next_x == goal_x && next_y == goal_y) ? customer->target_y : (double)next_y;
    } else {
        customer->x += dx / distance * speed * dt;
        customer->y += dy / distance * speed * dt;
    }
}

static int reached_target(const Customer *customer)
{
    return distance_to(customer->x, customer->y, customer->target_x, customer->target_y) < 0.01;
}

static Customer *find_customer(ShrinkWorld *world, uint64_t id)
{
    for (size_t i = 0; i < MAX_CUSTOMERS; ++i) {
        if (world->customers[i].state != CUSTOMER_UNUSED && world->customers[i].id == id)
            return &world->customers[i];
    }
    return NULL;
}

static void finish_customer(ShrinkWorld *world, Customer *customer)
{
    if (customer->state != CUSTOMER_UNUSED) {
        customer->state = CUSTOMER_UNUSED;
        world->active_customers--;
    }
}

static const ShrinkFixture *first_fixture_of_type(const ShrinkWorld *world, ShrinkFixtureType type)
{
    for (size_t i = 0U; i < world->geometry.fixture_count; ++i)
        if (world->geometry.fixtures[i].type == type) return &world->geometry.fixtures[i];
    return NULL;
}

static const ShrinkFixture *fixture_for_product(const ShrinkWorld *world, unsigned product, int from_x, int from_y)
{
    const ShrinkFixture *best = NULL;
    int best_distance = INT_MAX;
    for (size_t i = 0U; i < world->geometry.fixture_count; ++i) {
        const ShrinkFixture *candidate = &world->geometry.fixtures[i];
        int access_x = 0, access_y = 0, distance = 0;
        if (candidate->product_id != (int)product ||
            !shrink_geometry_best_access_cell(&world->geometry, candidate->id, from_x, from_y, &access_x, &access_y, &distance)) continue;
        if (best == NULL || distance < best_distance ||
            (distance == best_distance && candidate->id < best->id)) {
            best = candidate;
            best_distance = distance;
        }
    }
    return best;
}

static int set_direct_target(ShrinkWorld *world, Customer *customer, ShrinkFixtureType type)
{
    const ShrinkFixture *fixture = first_fixture_of_type(world, type);
    if (fixture == NULL) return 0;
    customer->target_fixture_id = fixture->id;
    customer->target_x = (double)fixture->x;
    customer->target_y = (double)fixture->y;
    return 1;
}

static int set_product_target(ShrinkWorld *world, Customer *customer)
{
    const ShrinkFixture *fixture = fixture_for_product(world, customer->product, (int)lround(customer->x), (int)lround(customer->y));
    int access_x = 0, access_y = 0;
    const int from_x = (int)lround(customer->x), from_y = (int)lround(customer->y);
    if (fixture == NULL || !shrink_geometry_best_access_cell(&world->geometry, fixture->id, from_x, from_y, &access_x, &access_y, NULL)) return 0;
    customer->target_fixture_id = fixture->id;
    customer->target_x = (double)access_x;
    customer->target_y = (double)access_y;
    return 1;
}

static int revalidate_product_target(ShrinkWorld *world, Customer *customer)
{
    const ShrinkFixture *fixture = shrink_geometry_find_fixture(&world->geometry, customer->target_fixture_id);
    int access_x = 0, access_y = 0;
    const int from_x = (int)lround(customer->x), from_y = (int)lround(customer->y);
    if (fixture == NULL || fixture->product_id != (int)customer->product ||
        !shrink_geometry_best_access_cell(&world->geometry, fixture->id, from_x, from_y, &access_x, &access_y, NULL)) return set_product_target(world, customer);
    customer->target_x = (double)access_x;
    customer->target_y = (double)access_y;
    return 1;
}

static void begin_leaving(ShrinkWorld *world, Customer *customer)
{
    customer->state = CUSTOMER_LEAVING;
    (void)set_direct_target(world, customer, SHRINK_FIXTURE_EXIT);
}

static void spawn_customer(ShrinkWorld *world)
{
    if (world->active_customers >= MAX_CUSTOMERS) return;
    for (size_t i = 0U; i < MAX_CUSTOMERS; ++i) {
        Customer *customer = &world->customers[i];
        if (customer->state == CUSTOMER_UNUSED) {
            const ShrinkFixture *entrance = first_fixture_of_type(world, SHRINK_FIXTURE_ENTRANCE);
            if (entrance == NULL) return;
            customer->state = CUSTOMER_TO_PRODUCT;
            customer->x = (double)entrance->x; customer->y = (double)entrance->y;
            customer->target_fixture_id = entrance->id;
            customer->wait_seconds = 0.0;
            customer->satisfaction = 100.0;
            customer->product = (unsigned)(rng_next(world) % PRODUCT_COUNT);
            customer->id = ++world->next_customer_id;
            world->active_customers++;
            world->metrics.customers_entered++;
            if (!set_product_target(world, customer)) {
                world->metrics.abandoned++;
                customer->satisfaction -= 25.0;
                begin_leaving(world, customer);
            }
            return;
        }
    }
}

static void decide_purchase(ShrinkWorld *world, Customer *customer)
{
    Product *product = &world->products[customer->product];
    if (product->stock == 0U) {
        world->metrics.abandoned++;
        customer->satisfaction -= 25.0;
        begin_leaving(world, customer);
        return;
    }
    product->stock--;
    /* Two cameras cover the merchandise aisles; their combined effect is deterministic. */
    const double theft_probability = 0.16 * (1.0 - 0.55);
    if (random_unit(world) < theft_probability) {
        world->metrics.thefts++;
        world->metrics.stolen_value += product->price;
        customer->satisfaction -= 18.0;
        begin_leaving(world, customer);
    } else {
        customer->state = CUSTOMER_WAITING;
        if (!set_direct_target(world, customer, SHRINK_FIXTURE_REGISTER))
            (void)set_direct_target(world, customer, SHRINK_FIXTURE_SELF_CHECKOUT);
    }
}

static void assign_queues(ShrinkWorld *world)
{
    for (unsigned r = 0; r < REGISTER_COUNT; ++r) {
        if (world->register_customer[r] != 0U) continue;
        for (size_t i = 0; i < MAX_CUSTOMERS; ++i) {
            Customer *customer = &world->customers[i];
            if (customer->state == CUSTOMER_WAITING) {
                customer->state = CUSTOMER_CHECKOUT;
                customer->register_id = r;
                world->register_customer[r] = customer->id;
                world->register_timer[r] = 20.0;
                break;
            }
        }
    }
}

static void update_registers(ShrinkWorld *world, double dt)
{
    for (unsigned r = 0; r < REGISTER_COUNT; ++r) {
        if (world->register_customer[r] == 0U) continue;
        world->register_timer[r] -= dt;
        if (world->register_timer[r] <= 0.0) {
            Customer *customer = find_customer(world, world->register_customer[r]);
            if (customer != NULL) {
                Product *product = &world->products[customer->product];
                world->metrics.purchases++;
                world->metrics.customers_served++;
                world->metrics.revenue += product->price;
                world->checkout_wait_sum += customer->wait_seconds;
                begin_leaving(world, customer);
            }
            world->register_customer[r] = 0U;
            world->register_timer[r] = 0.0;
        }
    }
}

ShrinkWorld *shrink_create(uint64_t seed)
{
    ShrinkWorld *world = calloc(1, sizeof(*world));
    if (world == NULL) return NULL;
    world->seed = seed == 0U ? UINT64_C(88172645463393265) : seed;
    shrink_geometry_init(&world->geometry);
    const double prices[PRODUCT_COUNT] = { 2.49, 3.99, 1.79, 5.49 };
    for (size_t i = 0; i < PRODUCT_COUNT; ++i) {
        world->products[i].price = prices[i];
        world->products[i].stock = 100U;
    }
    return world;
}

void shrink_destroy(ShrinkWorld *world) { free(world); }

void shrink_tick(ShrinkWorld *world, double dt_seconds)
{
    if (world == NULL || !(dt_seconds > 0.0) || !isfinite(dt_seconds)) return;
    world->ticks++;
    world->time += dt_seconds;
    world->metrics.labor_cost += 0.0133333333333333 * dt_seconds;
    world->spawn_timer -= dt_seconds;
    unsigned char blocked[SHRINK_MAX_CELLS];
    shrink_geometry_blocked_map(&world->geometry, blocked);
    if (world->time < OPEN_SECONDS_PER_DAY && world->spawn_timer <= 0.0) {
        spawn_customer(world);
        world->spawn_timer += SPAWN_INTERVAL;
    }
    for (size_t i = 0; i < MAX_CUSTOMERS; ++i) {
        Customer *customer = &world->customers[i];
        if (customer->state == CUSTOMER_UNUSED) continue;
        if (customer->state == CUSTOMER_TO_PRODUCT) {
            if (!revalidate_product_target(world, customer)) {
                world->metrics.abandoned++;
                customer->satisfaction -= 25.0;
                begin_leaving(world, customer);
            }
        }
        if (customer->state == CUSTOMER_TO_PRODUCT || customer->state == CUSTOMER_LEAVING)
            move_toward(customer, dt_seconds, blocked, world->geometry.width, world->geometry.height);
        if (customer->state == CUSTOMER_TO_PRODUCT && reached_target(customer)) {
            if (!revalidate_product_target(world, customer)) continue;
            customer->state = CUSTOMER_DECIDING;
            decide_purchase(world, customer);
        } else if (customer->state == CUSTOMER_WAITING) {
            customer->wait_seconds += dt_seconds;
            customer->satisfaction -= 0.025 * dt_seconds;
        } else if (customer->state == CUSTOMER_LEAVING && reached_target(customer)) {
            world->satisfaction_sum += fmax(0.0, customer->satisfaction);
            world->satisfaction_count++;
            finish_customer(world, customer);
        }
    }
    update_registers(world, dt_seconds);
    assign_queues(world);
    world->metrics.labor_cost = round(world->metrics.labor_cost * 100.0) / 100.0;
    world->metrics.profit = world->metrics.revenue - world->metrics.stolen_value - world->metrics.labor_cost;
    world->metrics.average_checkout_wait = world->metrics.purchases == 0U ? 0.0 :
        world->checkout_wait_sum / (double)world->metrics.purchases;
    world->metrics.average_satisfaction = world->satisfaction_count == 0U ? 100.0 :
        world->satisfaction_sum / (double)world->satisfaction_count;
}

uint64_t shrink_tick_count(const ShrinkWorld *world) { return world == NULL ? 0U : world->ticks; }

size_t shrink_entity_count(const ShrinkWorld *world)
{
    return world == NULL ? 0U : (size_t)world->active_customers;
}

int shrink_entity_snapshot(const ShrinkWorld *world, size_t index, ShrinkEntitySnapshot *out_snapshot)
{
    size_t active_index = 0U;
    if (world == NULL || out_snapshot == NULL) return 0;
    for (size_t i = 0; i < MAX_CUSTOMERS; ++i) {
        const Customer *customer = &world->customers[i];
        if (customer->state == CUSTOMER_UNUSED) continue;
        if (active_index++ != index) continue;
        out_snapshot->id = customer->id; out_snapshot->x = customer->x; out_snapshot->y = customer->y;
        out_snapshot->product = customer->product;
        out_snapshot->state = customer->state == CUSTOMER_TO_PRODUCT ? SHRINK_ENTITY_TO_PRODUCT :
            customer->state == CUSTOMER_WAITING ? SHRINK_ENTITY_WAITING :
            customer->state == CUSTOMER_CHECKOUT ? SHRINK_ENTITY_CHECKOUT : SHRINK_ENTITY_LEAVING;
        out_snapshot->target_fixture_id = customer->target_fixture_id;
        out_snapshot->target_x = (int)lround(customer->target_x);
        out_snapshot->target_y = (int)lround(customer->target_y);
        return 1;
    }
    return 0;
}

void shrink_metrics(const ShrinkWorld *world, ShrinkMetrics *out_metrics)
{
    if (world != NULL && out_metrics != NULL) *out_metrics = world->metrics;
}

uint32_t shrink_floor_row(const ShrinkWorld *world, int y)
{
    uint32_t row = 0U;
    if (world == NULL || y < 0 || y >= world->geometry.height) return 0U;
    for (int x = 0; x < world->geometry.width && x < 32; ++x)
        if (world->geometry.floor[y * world->geometry.width + x]) row |= UINT32_C(1) << x;
    return row;
}

void shrink_geometry_info(const ShrinkWorld *world, ShrinkGeometryInfo *out_info)
{
    if (world == NULL || out_info == NULL) return;
    out_info->width = world->geometry.width; out_info->height = world->geometry.height;
    out_info->wall_count = world->geometry.wall_count; out_info->fixture_count = world->geometry.fixture_count;
}

int shrink_wall_snapshot(const ShrinkWorld *world, size_t index, ShrinkWallSnapshot *out_snapshot)
{
    if (world == NULL || out_snapshot == NULL || index >= world->geometry.wall_count) return 0;
    const ShrinkWall *wall = &world->geometry.walls[index];
    *out_snapshot = (ShrinkWallSnapshot){wall->id, wall->ax, wall->ay, wall->bx, wall->by};
    return 1;
}

int shrink_fixture_snapshot(const ShrinkWorld *world, size_t index, ShrinkFixtureSnapshot *out_snapshot)
{
    if (world == NULL || out_snapshot == NULL || index >= world->geometry.fixture_count) return 0;
    const ShrinkFixture *fixture = &world->geometry.fixtures[index];
    *out_snapshot = (ShrinkFixtureSnapshot){fixture->id, fixture->type, fixture->x, fixture->y, fixture->rotation, fixture->product_id};
    return 1;
}

ShrinkBuildResult shrink_try_place_fixture(ShrinkWorld *world, ShrinkFixtureType type, int x, int y, unsigned rotation, uint64_t *out_id) { return world == NULL ? SHRINK_BUILD_NOT_FOUND : shrink_geometry_place_fixture(&world->geometry, type, x, y, rotation, out_id); }
ShrinkBuildResult shrink_try_move_fixture(ShrinkWorld *world, uint64_t id, int x, int y) { return world == NULL ? SHRINK_BUILD_NOT_FOUND : shrink_geometry_move_fixture(&world->geometry, id, x, y); }
ShrinkBuildResult shrink_try_rotate_fixture(ShrinkWorld *world, uint64_t id, unsigned rotation) { return world == NULL ? SHRINK_BUILD_NOT_FOUND : shrink_geometry_rotate_fixture(&world->geometry, id, rotation); }
ShrinkBuildResult shrink_try_remove_fixture(ShrinkWorld *world, uint64_t id) { return world == NULL ? SHRINK_BUILD_NOT_FOUND : shrink_geometry_remove_fixture(&world->geometry, id); }
ShrinkBuildResult shrink_try_add_wall(ShrinkWorld *world, int ax, int ay, int bx, int by, uint64_t *out_id) { return world == NULL ? SHRINK_BUILD_NOT_FOUND : shrink_geometry_add_wall(&world->geometry, ax, ay, bx, by, out_id); }
ShrinkBuildResult shrink_try_remove_wall(ShrinkWorld *world, uint64_t id) { return world == NULL ? SHRINK_BUILD_NOT_FOUND : shrink_geometry_remove_wall(&world->geometry, id); }
int shrink_geometry_has_routes(const ShrinkWorld *world) { return world != NULL && shrink_geometry_routes_valid(&world->geometry); }
