#ifndef SHRINK_SCENARIO_H
#define SHRINK_SCENARIO_H

#include "shrink.h"
#include <stddef.h>
#include <stdint.h>

typedef enum ShrinkScenarioGoalType {
    SHRINK_SCENARIO_GOAL_PROFIT = 1,
    SHRINK_SCENARIO_GOAL_SHRINK_RATE,
    SHRINK_SCENARIO_GOAL_SATISFACTION,
    SHRINK_SCENARIO_GOAL_CUSTOMERS_SERVED
} ShrinkScenarioGoalType;

typedef struct ShrinkScenarioInfo {
    unsigned id;
    const char *slug;
    const char *title;
    const char *store_type;
    const char *description;
    ShrinkScenarioGoalType goal_type;
    double goal_value;
    unsigned difficulty;
    uint64_t canonical_seed;
} ShrinkScenarioInfo;

size_t shrink_scenario_count(void);
const ShrinkScenarioInfo *shrink_scenario_at(size_t index);
const ShrinkScenarioInfo *shrink_scenario_find(const char *slug);
ShrinkWorld *shrink_scenario_create(const ShrinkScenarioInfo *scenario, uint64_t variation);

#endif
