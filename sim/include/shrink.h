#ifndef SHRINK_H
#define SHRINK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ShrinkWorld ShrinkWorld;

typedef enum ShrinkEntityState {
    SHRINK_ENTITY_TO_PRODUCT = 1,
    SHRINK_ENTITY_WAITING,
    SHRINK_ENTITY_CHECKOUT,
    SHRINK_ENTITY_LEAVING
} ShrinkEntityState;

typedef struct ShrinkEntitySnapshot {
    uint64_t id;
    double x;
    double y;
    unsigned product;
    ShrinkEntityState state;
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
} ShrinkMetrics;

ShrinkWorld *shrink_create(uint64_t seed);
void shrink_destroy(ShrinkWorld *world);
void shrink_tick(ShrinkWorld *world, double dt_seconds);
uint64_t shrink_tick_count(const ShrinkWorld *world);
void shrink_metrics(const ShrinkWorld *world, ShrinkMetrics *out_metrics);
size_t shrink_entity_count(const ShrinkWorld *world);
int shrink_entity_snapshot(const ShrinkWorld *world, size_t index,
                           ShrinkEntitySnapshot *out_snapshot);

#ifdef __cplusplus
}
#endif

#endif
