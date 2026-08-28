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
#define MAX_EMPLOYEES 16U
#define GUARD_STEP_TICKS 2U
#define MAX_EVENTS 256U
#define MAX_HAZARDS 128U
#define MAX_SCRIPTED_EVENTS 8U

typedef struct ShrinkBalanceConfig {
    double spawn_interval;
    double camera_deterrence;
    double guard_deterrence;
    double camera_maintenance_per_second;
    double checkout_seconds;
    double self_checkout_seconds;
    int camera_range;
    int guard_range;
} ShrinkBalanceConfig;

static const ShrinkBalanceConfig BALANCE = {6.0, 0.11, 0.18, 0.006, 20.0, 15.0, 8, 6};

typedef enum CustomerState {
    CUSTOMER_UNUSED,
    CUSTOMER_TO_PRODUCT,
    CUSTOMER_DECIDING,
    CUSTOMER_WAITING,
    CUSTOMER_CHECKOUT,
    CUSTOMER_LEAVING,
    CUSTOMER_EVACUATING
} CustomerState;

typedef struct Product {
    double cost;
    double price;
    double demand;
    double theft_risk;
    unsigned stock;
} Product;

typedef struct Customer {
    CustomerState state;
    ShrinkCustomerArchetype archetype;
    double walking_speed;
    double patience_seconds;
    double budget;
    double theft_tendency;
    double x, y;
    double target_x, target_y;
    uint64_t target_fixture_id;
    double wait_seconds;
    double satisfaction;
    unsigned product;
    unsigned register_id;
    uint64_t id;
    unsigned theft_attempted;
    unsigned theft_detected;
} Customer;

typedef struct Employee {
    uint64_t id;
    ShrinkEmployeeRole role;
    double wage;
    double skill;
    double fatigue;
    double morale;
    int x, y;
    uint64_t target_fixture_id;
    int target_x, target_y;
} Employee;

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
    Employee employees[MAX_EMPLOYEES];
    size_t employee_count;
    uint64_t next_employee_id;
    ShrinkHazardSnapshot hazards[MAX_HAZARDS];
    size_t hazard_count;
    uint64_t next_hazard_id;
    struct {
        ShrinkScenarioEventDef def;
        unsigned triggered;
    } scripted_events[MAX_SCRIPTED_EVENTS];
    size_t scripted_event_count;
    unsigned fire_active;
    ShrinkEvent events[MAX_EVENTS];
    size_t event_head;
    size_t event_count;
};

static void emit_event_at(ShrinkWorld *world, ShrinkEventType type, const Customer *customer, uint64_t fixture_id, int x, int y, double value)
{
    const size_t slot = (world->event_head + world->event_count) % MAX_EVENTS;
    if (world->event_count == MAX_EVENTS) {
        world->event_head = (world->event_head + 1U) % MAX_EVENTS;
        --world->event_count;
    }
    world->events[slot] = (ShrinkEvent){world->ticks, type,
        customer != NULL ? customer->id : 0U, fixture_id,
        customer != NULL ? (int)customer->product : -1,
        customer != NULL ? (int)lround(customer->x) : x,
        customer != NULL ? (int)lround(customer->y) : y, value};
    ++world->event_count;
}

static void emit_event(ShrinkWorld *world, ShrinkEventType type, const Customer *customer, uint64_t fixture_id, double value)
{
    const int x = customer != NULL ? (int)lround(customer->x) : 0;
    const int y = customer != NULL ? (int)lround(customer->y) : 0;
    emit_event_at(world, type, customer, fixture_id, x, y, value);
}

static int manhattan(int ax, int ay, int bx, int by);

static int hazard_blocks(ShrinkHazardType type)
{
    return type == SHRINK_HAZARD_FIRE || type == SHRINK_HAZARD_DEBRIS ||
           type == SHRINK_HAZARD_WATER || type == SHRINK_HAZARD_CLOSED;
}

static void create_hazard(ShrinkWorld *world, ShrinkHazardType type, int x, int y, unsigned severity, uint64_t expires_tick)
{
    if (world->hazard_count >= MAX_HAZARDS || x < 0 || y < 0 || x >= world->geometry.width || y >= world->geometry.height ||
        !world->geometry.floor[y * world->geometry.width + x]) return;
    ShrinkHazardSnapshot hazard = {++world->next_hazard_id, type, x, y, severity, expires_tick};
    world->hazards[world->hazard_count++] = hazard;
    emit_event_at(world, SHRINK_EVENT_HAZARD_CREATED, NULL, hazard.id, x, y, (double)severity);
}

static unsigned hazard_count_of(const ShrinkWorld *world, ShrinkHazardType type)
{
    unsigned count = 0U;
    for (size_t i = 0U; i < world->hazard_count; ++i) count += world->hazards[i].type == type;
    return count;
}

static int hazard_near(const ShrinkWorld *world, int x, int y, unsigned radius)
{
    for (size_t i = 0U; i < world->hazard_count; ++i) {
        const ShrinkHazardSnapshot *hazard = &world->hazards[i];
        if (hazard_blocks(hazard->type) && manhattan(x, y, hazard->x, hazard->y) <= (int)radius) return 1;
    }
    return 0;
}

static void expire_hazards(ShrinkWorld *world)
{
    size_t write = 0U;
    for (size_t i = 0U; i < world->hazard_count; ++i) {
        const ShrinkHazardSnapshot hazard = world->hazards[i];
        if (hazard.expires_tick != 0U && world->ticks >= hazard.expires_tick) {
            emit_event_at(world, SHRINK_EVENT_HAZARD_CLEARED, NULL, hazard.id, hazard.x, hazard.y, 0.0);
            continue;
        }
        world->hazards[write++] = hazard;
    }
    world->hazard_count = write;
    if (world->fire_active && hazard_count_of(world, SHRINK_HAZARD_FIRE) == 0U) {
        world->fire_active = 0U;
        emit_event(world, SHRINK_EVENT_FIRE_RESOLVED, NULL, 0U, 0.0);
    }
}

static uint64_t rng_next(ShrinkWorld *world)
{
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

static int manhattan(int ax, int ay, int bx, int by)
{
    return abs(ax - bx) + abs(ay - by);
}

static void move_toward(Customer *customer, double dt, const unsigned char *blocked, int width, int height)
{
    const double speed = customer->walking_speed;
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

static unsigned fixture_count_of_type(const ShrinkWorld *world, ShrinkFixtureType type)
{
    unsigned count = 0U;
    for (size_t i = 0U; i < world->geometry.fixture_count; ++i)
        count += world->geometry.fixtures[i].type == type;
    return count;
}

static unsigned employee_count_of_role(const ShrinkWorld *world, ShrinkEmployeeRole role)
{
    unsigned count = 0U;
    for (size_t i = 0U; i < world->employee_count; ++i)
        count += world->employees[i].role == role;
    return count;
}

static double default_wage(ShrinkEmployeeRole role)
{
    switch (role) {
        case SHRINK_EMPLOYEE_CASHIER: return 18.0;
        case SHRINK_EMPLOYEE_ASSOCIATE: return 17.0;
        case SHRINK_EMPLOYEE_STOCKER: return 18.5;
        case SHRINK_EMPLOYEE_SECURITY: return 22.0;
        default: return 0.0;
    }
}

static int valid_employee_role(ShrinkEmployeeRole role)
{
    return role >= SHRINK_EMPLOYEE_CASHIER && role <= SHRINK_EMPLOYEE_SECURITY;
}

static const ShrinkFixture *next_camera_fixture(const ShrinkWorld *world, uint64_t current_id)
{
    const ShrinkFixture *first = NULL;
    const ShrinkFixture *next = NULL;
    for (size_t i = 0U; i < world->geometry.fixture_count; ++i) {
        const ShrinkFixture *fixture = &world->geometry.fixtures[i];
        if (fixture->type != SHRINK_FIXTURE_CAMERA) continue;
        if (first == NULL || fixture->id < first->id) first = fixture;
        if (fixture->id > current_id && (next == NULL || fixture->id < next->id)) next = fixture;
    }
    return next != NULL ? next : first;
}

static void set_employee_target(Employee *employee, const ShrinkFixture *fixture)
{
    if (fixture == NULL) {
        employee->target_fixture_id = 0U;
        employee->target_x = employee->x;
        employee->target_y = employee->y;
        return;
    }
    employee->target_fixture_id = fixture->id;
    employee->target_x = fixture->x;
    employee->target_y = fixture->y;
}

static ShrinkStaffResult hire_employee_internal(ShrinkWorld *world, ShrinkEmployeeRole role, double wage, uint64_t *out_id)
{
    if (world == NULL || !valid_employee_role(role)) return SHRINK_STAFF_INVALID_ROLE;
    if (world->employee_count >= MAX_EMPLOYEES) return SHRINK_STAFF_FULL;

    const uint64_t id = ++world->next_employee_id;
    Employee employee;
    memset(&employee, 0, sizeof(employee));
    employee.id = id;
    employee.role = role;
    employee.wage = wage > 0.0 ? wage : default_wage(role);
    employee.skill = 0.72 + 0.035 * (double)((id * 7U + (uint64_t)role * 3U) % 7U);
    employee.morale = 0.88 + 0.02 * (double)(id % 5U);

    const ShrinkFixture *spawn = NULL;
    if (role == SHRINK_EMPLOYEE_CASHIER) spawn = first_fixture_of_type(world, SHRINK_FIXTURE_REGISTER);
    else if (role == SHRINK_EMPLOYEE_SECURITY) spawn = first_fixture_of_type(world, SHRINK_FIXTURE_CAMERA);
    if (spawn == NULL) spawn = first_fixture_of_type(world, SHRINK_FIXTURE_ENTRANCE);
    if (spawn != NULL) {
        employee.x = spawn->x;
        employee.y = spawn->y;
    }

    if (role == SHRINK_EMPLOYEE_SECURITY)
        set_employee_target(&employee, next_camera_fixture(world, 0U));
    else
        set_employee_target(&employee, spawn);

    world->employees[world->employee_count++] = employee;
    if (out_id != NULL) *out_id = id;
    return SHRINK_STAFF_OK;
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

static void cancel_checkout(ShrinkWorld *world, uint64_t customer_id)
{
    for (unsigned lane = 0U; lane < REGISTER_COUNT; ++lane) {
        if (world->register_customer[lane] != customer_id) continue;
        world->register_customer[lane] = 0U;
        world->register_timer[lane] = 0.0;
    }
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
            customer->x = (double)entrance->x;
            customer->y = (double)entrance->y;
            customer->archetype = (ShrinkCustomerArchetype)(SHRINK_CUSTOMER_QUICK_STOP + (rng_next(world) % 5U));
            customer->walking_speed = 2.3 + random_unit(world) * 1.8;
            customer->patience_seconds = 35.0 + random_unit(world) * 90.0;
            customer->budget = 5.0 + random_unit(world) * 25.0;
            customer->theft_tendency = 0.05 + random_unit(world) * 0.38;
            if (customer->archetype == SHRINK_CUSTOMER_QUICK_STOP) {
                customer->walking_speed += 0.7;
                customer->patience_seconds -= 10.0;
            } else if (customer->archetype == SHRINK_CUSTOMER_BARGAIN) {
                customer->budget += 8.0;
            } else if (customer->archetype == SHRINK_CUSTOMER_OPPORTUNISTIC) {
                customer->theft_tendency += 0.25;
            }
            customer->target_fixture_id = entrance->id;
            customer->theft_attempted = 0U;
            customer->theft_detected = 0U;
            customer->wait_seconds = 0.0;
            customer->satisfaction = 100.0;
            customer->product = (unsigned)(rng_next(world) % PRODUCT_COUNT);
            customer->id = ++world->next_customer_id;
            world->active_customers++;
            world->metrics.customers_entered++;
            emit_event(world, SHRINK_EVENT_CUSTOMER_ENTERED, customer, entrance->id, 0.0);
            emit_event(world, SHRINK_EVENT_ITEM_SELECTED, customer, 0U, 0.0);
            if (!set_product_target(world, customer)) {
                world->metrics.abandoned++;
                customer->satisfaction -= 25.0;
                begin_leaving(world, customer);
            }
            return;
        }
    }
}

static double local_security_deterrence(const ShrinkWorld *world, int x, int y)
{
    double deterrence = 0.0;
    for (size_t i = 0U; i < world->geometry.fixture_count; ++i) {
        const ShrinkFixture *fixture = &world->geometry.fixtures[i];
        if (fixture->type != SHRINK_FIXTURE_CAMERA) continue;
        const int distance = manhattan(x, y, fixture->x, fixture->y);
        if (distance <= BALANCE.camera_range) {
            const double proximity = 1.0 - (double)distance / (double)(BALANCE.camera_range + 1);
            deterrence += BALANCE.camera_deterrence * proximity;
        }
    }
    for (size_t i = 0U; i < world->employee_count; ++i) {
        const Employee *employee = &world->employees[i];
        if (employee->role != SHRINK_EMPLOYEE_SECURITY) continue;
        const int distance = manhattan(x, y, employee->x, employee->y);
        if (distance <= BALANCE.guard_range) {
            const double proximity = 1.0 - (double)distance / (double)(BALANCE.guard_range + 1);
            const double fatigue_factor = 1.0 - 0.35 * employee->fatigue;
            deterrence += BALANCE.guard_deterrence * employee->skill * employee->morale * fatigue_factor * proximity;
        }
    }
    return fmin(0.85, deterrence);
}

static void decide_purchase(ShrinkWorld *world, Customer *customer)
{
    Product *product = &world->products[customer->product];
    if (product->stock == 0U || product->price > customer->budget) {
        world->metrics.abandoned++;
        customer->satisfaction -= 25.0;
        begin_leaving(world, customer);
        return;
    }
    product->stock--;
    const double deterrence = local_security_deterrence(world, (int)lround(customer->x), (int)lround(customer->y));
    const double theft_probability = product->theft_risk * (1.0 - deterrence) * (0.65 + customer->theft_tendency);
    if (random_unit(world) < theft_probability) {
        customer->theft_attempted = 1U;
        world->metrics.theft_attempts++;
        emit_event(world, SHRINK_EVENT_THEFT_ATTEMPTED, customer, customer->target_fixture_id, product->price);
        unsigned cameras = 0U;
        for (size_t i = 0U; i < world->geometry.fixture_count; ++i) {
            const ShrinkFixture *camera = &world->geometry.fixtures[i];
            if (camera->type == SHRINK_FIXTURE_CAMERA && manhattan((int)lround(customer->x), (int)lround(customer->y), camera->x, camera->y) <= BALANCE.camera_range) ++cameras;
        }
        if (cameras > 0U && random_unit(world) < fmin(0.90, 0.35 + 0.18 * (double)cameras)) {
            customer->theft_detected = 1U;
            world->metrics.thefts_detected++;
            emit_event(world, SHRINK_EVENT_THEFT_DETECTED, customer, customer->target_fixture_id, product->price);
            emit_event(world, SHRINK_EVENT_SECURITY_RESPONDING, customer, customer->target_fixture_id, 0.0);
        }
        customer->satisfaction -= 18.0;
        begin_leaving(world, customer);
    } else {
        customer->state = CUSTOMER_WAITING;
        if (!set_direct_target(world, customer, SHRINK_FIXTURE_REGISTER))
            (void)set_direct_target(world, customer, SHRINK_FIXTURE_SELF_CHECKOUT);
    }
}

static unsigned open_checkout_lanes(const ShrinkWorld *world, unsigned *out_staffed_lanes)
{
    unsigned staffed = employee_count_of_role(world, SHRINK_EMPLOYEE_CASHIER);
    const unsigned registers = fixture_count_of_type(world, SHRINK_FIXTURE_REGISTER);
    const unsigned self_checkouts = fixture_count_of_type(world, SHRINK_FIXTURE_SELF_CHECKOUT);
    if (staffed > registers) staffed = registers;
    if (staffed > REGISTER_COUNT) staffed = REGISTER_COUNT;
    unsigned self_lanes = self_checkouts;
    if (self_lanes > REGISTER_COUNT - staffed) self_lanes = REGISTER_COUNT - staffed;
    if (out_staffed_lanes != NULL) *out_staffed_lanes = staffed;
    return staffed + self_lanes;
}

static double average_cashier_skill(const ShrinkWorld *world)
{
    double total = 0.0;
    unsigned count = 0U;
    for (size_t i = 0U; i < world->employee_count; ++i) {
        if (world->employees[i].role != SHRINK_EMPLOYEE_CASHIER) continue;
        total += world->employees[i].skill;
        ++count;
    }
    return count == 0U ? 0.75 : total / (double)count;
}

static void assign_queues(ShrinkWorld *world)
{
    unsigned staffed_lanes = 0U;
    const unsigned open_lanes = open_checkout_lanes(world, &staffed_lanes);
    const double cashier_speed = 0.65 + 0.60 * average_cashier_skill(world);
    for (unsigned r = 0; r < open_lanes; ++r) {
        if (world->register_customer[r] != 0U) continue;
        for (size_t i = 0; i < MAX_CUSTOMERS; ++i) {
            Customer *customer = &world->customers[i];
            if (customer->state == CUSTOMER_WAITING) {
                customer->state = CUSTOMER_CHECKOUT;
                customer->register_id = r;
                world->register_customer[r] = customer->id;
                world->register_timer[r] = r < staffed_lanes ? BALANCE.checkout_seconds / cashier_speed : BALANCE.self_checkout_seconds;
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
                emit_event(world, SHRINK_EVENT_PURCHASE_COMPLETED, customer, customer->target_fixture_id, product->price);
                world->metrics.revenue += product->price;
                world->metrics.cost_of_goods += product->cost;
                world->checkout_wait_sum += customer->wait_seconds;
                begin_leaving(world, customer);
            }
            world->register_customer[r] = 0U;
            world->register_timer[r] = 0.0;
        }
    }
}

static void trigger_scripted_event(ShrinkWorld *world, const ShrinkScenarioEventDef *def)
{
    const uint64_t expires = world->ticks + (uint64_t)def->duration;
    if (def->type == SHRINK_SCRIPT_FIRE) {
        world->fire_active = 1U;
        emit_event_at(world, SHRINK_EVENT_FIRE_STARTED, NULL, 0U, def->target_x, def->target_y, (double)def->severity);
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (abs(dx) + abs(dy) <= 1) create_hazard(world, SHRINK_HAZARD_FIRE, def->target_x + dx, def->target_y + dy, def->severity, expires);
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
                if (abs(dx) + abs(dy) == 2) create_hazard(world, SHRINK_HAZARD_SMOKE, def->target_x + dx, def->target_y + dy, def->severity, expires);
    }
}

static void process_scripted_events(ShrinkWorld *world)
{
    for (size_t i = 0U; i < world->scripted_event_count; ++i) {
        if (world->scripted_events[i].triggered || world->ticks < world->scripted_events[i].def.trigger_tick) continue;
        world->scripted_events[i].triggered = 1U;
        trigger_scripted_event(world, &world->scripted_events[i].def);
    }
}

static void update_fire_damage(ShrinkWorld *world, double dt)
{
    for (size_t h = 0U; h < world->hazard_count; ++h) {
        const ShrinkHazardSnapshot *hazard = &world->hazards[h];
        if (hazard->type != SHRINK_HAZARD_FIRE) continue;
        world->metrics.merchandise_damage += 0.12 * (double)hazard->severity * dt;
        world->metrics.incident_damage_cost += 0.08 * (double)hazard->severity * dt;
    }
}

static void update_security_staff(ShrinkWorld *world, const unsigned char *blocked)
{
    if (world->ticks % GUARD_STEP_TICKS != 0U) return;
    for (size_t i = 0U; i < world->employee_count; ++i) {
        Employee *employee = &world->employees[i];
        if (employee->role != SHRINK_EMPLOYEE_SECURITY) continue;

        const ShrinkFixture *target = shrink_geometry_find_fixture(&world->geometry, employee->target_fixture_id);
        if (target == NULL || target->type != SHRINK_FIXTURE_CAMERA) {
            target = next_camera_fixture(world, 0U);
            if (target == NULL) target = first_fixture_of_type(world, SHRINK_FIXTURE_ENTRANCE);
            set_employee_target(employee, target);
        }

        if (employee->x == employee->target_x && employee->y == employee->target_y) {
            const ShrinkFixture *next = next_camera_fixture(world, employee->target_fixture_id);
            if (next != NULL && next->id != employee->target_fixture_id) set_employee_target(employee, next);
        }

        int next_x = employee->x;
        int next_y = employee->y;
        if (shrink_path_next_step_grid(employee->x, employee->y, employee->target_x, employee->target_y,
                                       blocked, world->geometry.width, world->geometry.height, &next_x, &next_y)) {
            employee->x = next_x;
            employee->y = next_y;
        }
    }
}

ShrinkWorld *shrink_create(uint64_t seed)
{
    ShrinkWorld *world = calloc(1, sizeof(*world));
    if (world == NULL) return NULL;
    world->seed = seed == 0U ? UINT64_C(88172645463393265) : seed;
    shrink_geometry_init_layout(&world->geometry, (unsigned)(world->seed % 8U));

    const double prices[PRODUCT_COUNT] = {2.49, 3.99, 1.79, 5.49};
    const double costs[PRODUCT_COUNT] = {1.05, 1.55, 0.62, 2.20};
    const double risks[PRODUCT_COUNT] = {0.16, 0.11, 0.13, 0.27};
    for (size_t i = 0; i < PRODUCT_COUNT; ++i) {
        world->products[i].cost = costs[i];
        world->products[i].price = prices[i];
        world->products[i].demand = 1.0;
        world->products[i].theft_risk = risks[i];
        world->products[i].stock = 100U;
    }

    (void)hire_employee_internal(world, SHRINK_EMPLOYEE_CASHIER, 18.0, NULL);
    (void)hire_employee_internal(world, SHRINK_EMPLOYEE_CASHIER, 18.0, NULL);
    (void)hire_employee_internal(world, SHRINK_EMPLOYEE_ASSOCIATE, 17.0, NULL);
    (void)hire_employee_internal(world, SHRINK_EMPLOYEE_SECURITY, 22.0, NULL);
    return world;
}

void shrink_destroy(ShrinkWorld *world)
{
    free(world);
}

void shrink_tick(ShrinkWorld *world, double dt_seconds)
{
    if (world == NULL || !(dt_seconds > 0.0) || !isfinite(dt_seconds)) return;
    world->ticks++;
    world->time += dt_seconds;

    for (size_t i = 0U; i < world->employee_count; ++i) {
        Employee *employee = &world->employees[i];
        employee->fatigue = fmin(1.0, employee->fatigue + dt_seconds / 36000.0);
        employee->morale = fmax(0.55, employee->morale - dt_seconds / 180000.0);
        world->metrics.labor_cost += employee->wage / 3600.0 * dt_seconds;
    }

    const unsigned camera_count = fixture_count_of_type(world, SHRINK_FIXTURE_CAMERA);
    world->metrics.security_cost += camera_count * BALANCE.camera_maintenance_per_second * dt_seconds;
    world->spawn_timer -= dt_seconds;

    process_scripted_events(world);
    expire_hazards(world);
    update_fire_damage(world, dt_seconds);
    unsigned char blocked[SHRINK_MAX_CELLS];
    shrink_geometry_blocked_map(&world->geometry, blocked);
    for (size_t h = 0U; h < world->hazard_count; ++h)
        if (hazard_blocks(world->hazards[h].type)) blocked[world->hazards[h].y * world->geometry.width + world->hazards[h].x] = 1U;
    update_security_staff(world, blocked);

    if (world->time < OPEN_SECONDS_PER_DAY && world->spawn_timer <= 0.0) {
        spawn_customer(world);
        world->spawn_timer += BALANCE.spawn_interval;
    }

    for (size_t i = 0; i < MAX_CUSTOMERS; ++i) {
        Customer *customer = &world->customers[i];
        if (customer->state == CUSTOMER_UNUSED) continue;
        if (customer->state != CUSTOMER_LEAVING && customer->state != CUSTOMER_EVACUATING &&
            hazard_near(world, (int)lround(customer->x), (int)lround(customer->y), 2U)) {
            cancel_checkout(world, customer->id);
            customer->state = CUSTOMER_EVACUATING;
            (void)set_direct_target(world, customer, SHRINK_FIXTURE_EXIT);
            customer->satisfaction -= 12.0;
            emit_event(world, SHRINK_EVENT_CUSTOMER_EVACUATING, customer, customer->target_fixture_id, 0.0);
        }
        if (customer->state == CUSTOMER_TO_PRODUCT) {
            if (!revalidate_product_target(world, customer)) {
                world->metrics.abandoned++;
                customer->satisfaction -= 25.0;
                begin_leaving(world, customer);
            }
        }
        if (customer->state == CUSTOMER_TO_PRODUCT || customer->state == CUSTOMER_LEAVING || customer->state == CUSTOMER_EVACUATING)
            move_toward(customer, dt_seconds, blocked, world->geometry.width, world->geometry.height);
        if (customer->state == CUSTOMER_TO_PRODUCT && reached_target(customer)) {
            if (!revalidate_product_target(world, customer)) continue;
            customer->state = CUSTOMER_DECIDING;
            decide_purchase(world, customer);
        } else if (customer->state == CUSTOMER_WAITING) {
            customer->wait_seconds += dt_seconds;
            customer->patience_seconds -= dt_seconds;
            customer->satisfaction -= 0.025 * dt_seconds;
            if (customer->patience_seconds <= 0.0) {
                world->metrics.abandoned++;
                begin_leaving(world, customer);
            }
        } else if ((customer->state == CUSTOMER_LEAVING || customer->state == CUSTOMER_EVACUATING) && reached_target(customer)) {
            if (customer->theft_attempted) {
                if (customer->theft_detected) {
                    int guard_nearby = 0;
                    for (size_t e = 0U; e < world->employee_count; ++e)
                        if (world->employees[e].role == SHRINK_EMPLOYEE_SECURITY && manhattan(world->employees[e].x, world->employees[e].y, (int)lround(customer->x), (int)lround(customer->y)) <= 5) guard_nearby = 1;
                    if (guard_nearby && random_unit(world) < 0.55) {
                        world->metrics.thefts_recovered++;
                        emit_event(world, SHRINK_EVENT_SECURITY_INTERVENTION, customer, customer->target_fixture_id, world->products[customer->product].price);
                    } else {
                        world->metrics.thefts++;
                        world->metrics.stolen_value += world->products[customer->product].price;
                        emit_event(world, SHRINK_EVENT_THEFT_EXITED, customer, customer->target_fixture_id, world->products[customer->product].price);
                    }
                } else {
                    world->metrics.thefts++;
                    world->metrics.stolen_value += world->products[customer->product].price;
                    emit_event(world, SHRINK_EVENT_THEFT_EXITED, customer, customer->target_fixture_id, world->products[customer->product].price);
                }
            }
            world->satisfaction_sum += fmax(0.0, customer->satisfaction);
            world->satisfaction_count++;
            finish_customer(world, customer);
        }
    }

    update_registers(world, dt_seconds);
    assign_queues(world);
    world->metrics.emergency_closure_seconds += hazard_count_of(world, SHRINK_HAZARD_FIRE) > 0U ? dt_seconds : 0.0;
    world->metrics.profit = world->metrics.revenue - world->metrics.cost_of_goods - world->metrics.stolen_value - world->metrics.labor_cost - world->metrics.security_cost - world->metrics.incident_damage_cost - world->metrics.merchandise_damage;
    world->metrics.average_checkout_wait = world->metrics.purchases == 0U ? 0.0 : world->checkout_wait_sum / (double)world->metrics.purchases;
    world->metrics.average_satisfaction = world->satisfaction_count == 0U ? 100.0 : world->satisfaction_sum / (double)world->satisfaction_count;
    world->metrics.active_employees = (uint64_t)world->employee_count;
}

uint64_t shrink_tick_count(const ShrinkWorld *world)
{
    return world == NULL ? 0U : world->ticks;
}

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
        out_snapshot->id = customer->id;
        out_snapshot->x = customer->x;
        out_snapshot->y = customer->y;
        out_snapshot->product = customer->product;
        out_snapshot->state = customer->state == CUSTOMER_TO_PRODUCT ? SHRINK_ENTITY_TO_PRODUCT :
            customer->state == CUSTOMER_WAITING ? SHRINK_ENTITY_WAITING :
            customer->state == CUSTOMER_CHECKOUT ? SHRINK_ENTITY_CHECKOUT :
            customer->state == CUSTOMER_EVACUATING ? SHRINK_ENTITY_EVACUATING : SHRINK_ENTITY_LEAVING;
        out_snapshot->target_fixture_id = customer->target_fixture_id;
        out_snapshot->target_x = (int)lround(customer->target_x);
        out_snapshot->target_y = (int)lround(customer->target_y);
        out_snapshot->archetype = customer->archetype;
        out_snapshot->walking_speed = customer->walking_speed;
        out_snapshot->patience_seconds = customer->patience_seconds;
        out_snapshot->budget = customer->budget;
        out_snapshot->theft_tendency = customer->theft_tendency;
        return 1;
    }
    return 0;
}

void shrink_metrics(const ShrinkWorld *world, ShrinkMetrics *out_metrics)
{
    if (world != NULL && out_metrics != NULL) *out_metrics = world->metrics;
}

int shrink_room_snapshot(const ShrinkWorld *world, size_t index, ShrinkRoomSnapshot *out_snapshot)
{
    if (world == NULL || out_snapshot == NULL || index >= world->geometry.room_count) return 0;
    const ShrinkRoom *room = &world->geometry.rooms[index];
    *out_snapshot = (ShrinkRoomSnapshot){room->id, room->type, room->x, room->y, room->width, room->height, (int)room->customer_accessible, (int)room->staff_accessible};
    return 1;
}

size_t shrink_employee_count(const ShrinkWorld *world)
{
    return world == NULL ? 0U : world->employee_count;
}

int shrink_employee_snapshot(const ShrinkWorld *world, size_t index, ShrinkEmployeeSnapshot *out_snapshot)
{
    if (world == NULL || out_snapshot == NULL || index >= world->employee_count) return 0;
    const Employee *employee = &world->employees[index];
    *out_snapshot = (ShrinkEmployeeSnapshot){employee->id, employee->role, employee->wage, employee->skill,
                                             employee->fatigue, employee->morale, employee->x, employee->y,
                                             employee->target_fixture_id, employee->target_x, employee->target_y};
    return 1;
}

ShrinkStaffResult shrink_hire_employee(ShrinkWorld *world, ShrinkEmployeeRole role, double wage, uint64_t *out_id)
{
    return hire_employee_internal(world, role, wage, out_id);
}

ShrinkStaffResult shrink_fire_employee(ShrinkWorld *world, uint64_t id)
{
    if (world == NULL) return SHRINK_STAFF_NOT_FOUND;
    for (size_t i = 0U; i < world->employee_count; ++i) {
        if (world->employees[i].id != id) continue;
        memmove(&world->employees[i], &world->employees[i + 1U],
                (world->employee_count - i - 1U) * sizeof(world->employees[0]));
        --world->employee_count;
        return SHRINK_STAFF_OK;
    }
    return SHRINK_STAFF_NOT_FOUND;
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
    out_info->width = world->geometry.width;
    out_info->height = world->geometry.height;
    out_info->wall_count = world->geometry.wall_count;
    out_info->fixture_count = world->geometry.fixture_count;
    out_info->room_count = world->geometry.room_count;
    out_info->layout_id = world->geometry.layout_id;
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
    *out_snapshot = (ShrinkFixtureSnapshot){fixture->id, fixture->type, fixture->x, fixture->y, fixture->rotation,
                                            fixture->width, fixture->height, fixture->access_mask, fixture->product_id};
    return 1;
}

ShrinkBuildResult shrink_try_place_fixture(ShrinkWorld *world, ShrinkFixtureType type, int x, int y, unsigned rotation, uint64_t *out_id)
{
    return world == NULL ? SHRINK_BUILD_NOT_FOUND : shrink_geometry_place_fixture(&world->geometry, type, x, y, rotation, out_id);
}

ShrinkBuildResult shrink_try_move_fixture(ShrinkWorld *world, uint64_t id, int x, int y)
{
    return world == NULL ? SHRINK_BUILD_NOT_FOUND : shrink_geometry_move_fixture(&world->geometry, id, x, y);
}

ShrinkBuildResult shrink_try_rotate_fixture(ShrinkWorld *world, uint64_t id, unsigned rotation)
{
    return world == NULL ? SHRINK_BUILD_NOT_FOUND : shrink_geometry_rotate_fixture(&world->geometry, id, rotation);
}

ShrinkBuildResult shrink_try_remove_fixture(ShrinkWorld *world, uint64_t id)
{
    return world == NULL ? SHRINK_BUILD_NOT_FOUND : shrink_geometry_remove_fixture(&world->geometry, id);
}

ShrinkBuildResult shrink_try_add_wall(ShrinkWorld *world, int ax, int ay, int bx, int by, uint64_t *out_id)
{
    return world == NULL ? SHRINK_BUILD_NOT_FOUND : shrink_geometry_add_wall(&world->geometry, ax, ay, bx, by, out_id);
}

ShrinkBuildResult shrink_try_remove_wall(ShrinkWorld *world, uint64_t id)
{
    return world == NULL ? SHRINK_BUILD_NOT_FOUND : shrink_geometry_remove_wall(&world->geometry, id);
}

int shrink_geometry_has_routes(const ShrinkWorld *world)
{
    return world != NULL && shrink_geometry_routes_valid(&world->geometry);
}

size_t shrink_event_count(const ShrinkWorld *world)
{
    return world == NULL ? 0U : world->event_count;
}

int shrink_event_snapshot(const ShrinkWorld *world, size_t index, ShrinkEvent *out_event)
{
    if (world == NULL || out_event == NULL || index >= world->event_count) return 0;
    *out_event = world->events[(world->event_head + index) % MAX_EVENTS];
    return 1;
}

void shrink_events_clear(ShrinkWorld *world)
{
    if (world != NULL) { world->event_head = 0U; world->event_count = 0U; }
}

size_t shrink_hazard_count(const ShrinkWorld *world)
{
    return world == NULL ? 0U : world->hazard_count;
}

int shrink_hazard_snapshot(const ShrinkWorld *world, size_t index, ShrinkHazardSnapshot *out_hazard)
{
    if (world == NULL || out_hazard == NULL || index >= world->hazard_count) return 0;
    *out_hazard = world->hazards[index];
    return 1;
}

int shrink_schedule_scripted_event(ShrinkWorld *world, ShrinkScenarioEventDef event_def)
{
    if (world == NULL || world->scripted_event_count >= MAX_SCRIPTED_EVENTS || event_def.type < SHRINK_SCRIPT_FIRE || event_def.type > SHRINK_SCRIPT_THEFT_SURGE || event_def.duration == 0U) return 0;
    world->scripted_events[world->scripted_event_count++].def = event_def;
    return 1;
}
