#ifndef SHRINK_H
#define SHRINK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ShrinkWorld ShrinkWorld;

typedef enum ShrinkFixtureType {
    SHRINK_FIXTURE_SHELF = 1,
    SHRINK_FIXTURE_BIN,
    SHRINK_FIXTURE_SHORT_SHELF,
    SHRINK_FIXTURE_LOCKED_SHELF,
    SHRINK_FIXTURE_CLEARANCE,
    SHRINK_FIXTURE_REGISTER,
    SHRINK_FIXTURE_SELF_CHECKOUT,
    SHRINK_FIXTURE_CAMERA,
    SHRINK_FIXTURE_ENTRANCE,
    SHRINK_FIXTURE_EXIT,
    SHRINK_FIXTURE_RFID_STATION,
    SHRINK_FIXTURE_LOCKED_CASE
} ShrinkFixtureType;

typedef enum ShrinkBuildResult {
    SHRINK_BUILD_OK = 0,
    SHRINK_BUILD_OUT_OF_BOUNDS,
    SHRINK_BUILD_COLLISION,
    SHRINK_BUILD_BLOCKS_ROUTE,
    SHRINK_BUILD_INVALID_DOOR,
    SHRINK_BUILD_INVALID_WALL,
    SHRINK_BUILD_NOT_FOUND
} ShrinkBuildResult;

typedef enum ShrinkRoomType {
    SHRINK_ROOM_SALES_FLOOR = 1,
    SHRINK_ROOM_STOCKROOM,
    SHRINK_ROOM_RECEIVING,
    SHRINK_ROOM_OFFICE,
    SHRINK_ROOM_SECURITY,
    SHRINK_ROOM_VESTIBULE
} ShrinkRoomType;

typedef struct ShrinkGeometryInfo {
    int width;
    int height;
    size_t wall_count;
    size_t fixture_count;
    size_t room_count;
    unsigned layout_id;
} ShrinkGeometryInfo;

typedef struct ShrinkRoomSnapshot {
    uint64_t id;
    ShrinkRoomType type;
    int x, y, width, height;
    int customer_accessible;
    int staff_accessible;
} ShrinkRoomSnapshot;

typedef struct ShrinkWallSnapshot { uint64_t id; int ax, ay, bx, by; } ShrinkWallSnapshot;
typedef struct ShrinkFixtureSnapshot { uint64_t id; ShrinkFixtureType type; int x, y; unsigned rotation; unsigned width, height; unsigned access_mask; int product_id; } ShrinkFixtureSnapshot;

typedef enum ShrinkEntityState {
    SHRINK_ENTITY_TO_PRODUCT = 1,
    SHRINK_ENTITY_WAITING,
    SHRINK_ENTITY_CHECKOUT,
    SHRINK_ENTITY_LEAVING
} ShrinkEntityState;

typedef enum ShrinkCustomerArchetype {
    SHRINK_CUSTOMER_QUICK_STOP = 1,
    SHRINK_CUSTOMER_ROUTINE,
    SHRINK_CUSTOMER_BARGAIN,
    SHRINK_CUSTOMER_BROWSER,
    SHRINK_CUSTOMER_OPPORTUNISTIC
} ShrinkCustomerArchetype;

typedef enum ShrinkEmployeeRole {
    SHRINK_EMPLOYEE_CASHIER = 1,
    SHRINK_EMPLOYEE_ASSOCIATE,
    SHRINK_EMPLOYEE_STOCKER,
    SHRINK_EMPLOYEE_SECURITY
} ShrinkEmployeeRole;

typedef enum ShrinkStaffResult {
    SHRINK_STAFF_OK = 0,
    SHRINK_STAFF_FULL,
    SHRINK_STAFF_INVALID_ROLE,
    SHRINK_STAFF_NOT_FOUND
} ShrinkStaffResult;

typedef struct ShrinkEmployeeSnapshot {
    uint64_t id;
    ShrinkEmployeeRole role;
    double wage;
    double skill;
    double fatigue;
    double morale;
    int x, y;
    uint64_t target_fixture_id;
    int target_x, target_y;
} ShrinkEmployeeSnapshot;

typedef struct ShrinkEntitySnapshot {
    uint64_t id;
    double x;
    double y;
    unsigned product;
    ShrinkEntityState state;
    uint64_t target_fixture_id;
    int target_x;
    int target_y;
    ShrinkCustomerArchetype archetype;
    double walking_speed;
    double patience_seconds;
    double budget;
    double theft_tendency;
} ShrinkEntitySnapshot;

typedef struct ShrinkMetrics {
    uint64_t customers_entered;
    uint64_t customers_served;
    uint64_t purchases;
    uint64_t abandoned;
    uint64_t thefts;
    double stolen_value;
    double revenue;
    double labor_cost;
    double profit;
    double average_checkout_wait;
    double average_satisfaction;
    double cost_of_goods;
    double security_cost;
    uint64_t active_employees;
} ShrinkMetrics;

ShrinkWorld *shrink_create(uint64_t seed);
void shrink_destroy(ShrinkWorld *world);
void shrink_tick(ShrinkWorld *world, double dt_seconds);
uint64_t shrink_tick_count(const ShrinkWorld *world);
void shrink_metrics(const ShrinkWorld *world, ShrinkMetrics *out_metrics);
size_t shrink_entity_count(const ShrinkWorld *world);
int shrink_entity_snapshot(const ShrinkWorld *world, size_t index,
                           ShrinkEntitySnapshot *out_snapshot);
size_t shrink_employee_count(const ShrinkWorld *world);
int shrink_employee_snapshot(const ShrinkWorld *world, size_t index, ShrinkEmployeeSnapshot *out_snapshot);
ShrinkStaffResult shrink_hire_employee(ShrinkWorld *world, ShrinkEmployeeRole role, double wage, uint64_t *out_id);
ShrinkStaffResult shrink_fire_employee(ShrinkWorld *world, uint64_t id);
void shrink_geometry_info(const ShrinkWorld *world, ShrinkGeometryInfo *out_info);
int shrink_room_snapshot(const ShrinkWorld *world, size_t index, ShrinkRoomSnapshot *out_snapshot);
uint32_t shrink_floor_row(const ShrinkWorld *world, int y);
int shrink_wall_snapshot(const ShrinkWorld *world, size_t index, ShrinkWallSnapshot *out_snapshot);
int shrink_fixture_snapshot(const ShrinkWorld *world, size_t index, ShrinkFixtureSnapshot *out_snapshot);
ShrinkBuildResult shrink_try_place_fixture(ShrinkWorld *world, ShrinkFixtureType type, int x, int y, unsigned rotation, uint64_t *out_id);
ShrinkBuildResult shrink_try_move_fixture(ShrinkWorld *world, uint64_t id, int x, int y);
ShrinkBuildResult shrink_try_rotate_fixture(ShrinkWorld *world, uint64_t id, unsigned rotation);
ShrinkBuildResult shrink_try_remove_fixture(ShrinkWorld *world, uint64_t id);
ShrinkBuildResult shrink_try_add_wall(ShrinkWorld *world, int ax, int ay, int bx, int by, uint64_t *out_id);
ShrinkBuildResult shrink_try_remove_wall(ShrinkWorld *world, uint64_t id);
int shrink_geometry_has_routes(const ShrinkWorld *world);

#ifdef __cplusplus
}
#endif

#endif
