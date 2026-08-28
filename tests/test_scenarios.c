#include "scenario.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned count_type(const ShrinkWorld *world, ShrinkFixtureType type)
{
    ShrinkGeometryInfo info;
    shrink_geometry_info(world, &info);
    unsigned count = 0U;
    for (size_t i = 0U; i < info.fixture_count; ++i) {
        ShrinkFixtureSnapshot fixture;
        if (shrink_fixture_snapshot(world, i, &fixture) && fixture.type == type) ++count;
    }
    return count;
}

static int has_event(const ShrinkWorld *world, ShrinkEventType type)
{
    for (size_t i = 0U; i < shrink_event_count(world); ++i) {
        ShrinkEvent event;
        if (shrink_event_snapshot(world, i, &event) && event.type == type) return 1;
    }
    return 0;
}

static unsigned count_role(const ShrinkWorld *world, ShrinkEmployeeRole role)
{
    unsigned count = 0U;
    for (size_t i = 0U; i < shrink_employee_count(world); ++i) {
        ShrinkEmployeeSnapshot employee;
        if (shrink_employee_snapshot(world, i, &employee) && employee.role == role) ++count;
    }
    return count;
}

int main(void)
{
    assert(shrink_scenario_count() >= 6U);
    assert(shrink_scenario_find("corner-market") != NULL);
    assert(shrink_scenario_find("electronics") != NULL);
    assert(shrink_scenario_find("big-box") != NULL);
    assert(shrink_scenario_find("pharmacy") != NULL);
    assert(shrink_scenario_find("grocery-fresh") != NULL);
    assert(shrink_scenario_find("troubled-store") != NULL);
    assert(shrink_scenario_find("not-a-store") == NULL);

    for (size_t i = 0U; i < shrink_scenario_count(); ++i) {
        const ShrinkScenarioInfo *scenario = shrink_scenario_at(i);
        assert(scenario != NULL);
        assert(scenario->slug != NULL && scenario->slug[0] != '\0');
        for (size_t j = i + 1U; j < shrink_scenario_count(); ++j)
            assert(strcmp(scenario->slug, shrink_scenario_at(j)->slug) != 0);

        ShrinkWorld *a = shrink_scenario_create(scenario, 3U);
        ShrinkWorld *b = shrink_scenario_create(scenario, 3U);
        assert(a != NULL && b != NULL);
        assert(shrink_geometry_has_routes(a));
        assert(shrink_geometry_has_routes(b));

        ShrinkGeometryInfo ga, gb;
        shrink_geometry_info(a, &ga);
        shrink_geometry_info(b, &gb);
        assert(ga.layout_id == gb.layout_id);
        assert(ga.fixture_count == gb.fixture_count);
        assert(shrink_employee_count(a) == shrink_employee_count(b));

        shrink_destroy(a);
        shrink_destroy(b);
    }

    ShrinkWorld *corner = shrink_scenario_create(shrink_scenario_find("corner-market"), 0U);
    ShrinkWorld *electronics = shrink_scenario_create(shrink_scenario_find("electronics"), 0U);
    ShrinkWorld *big_box = shrink_scenario_create(shrink_scenario_find("big-box"), 0U);
    ShrinkWorld *grocery = shrink_scenario_create(shrink_scenario_find("grocery-fresh"), 0U);
    ShrinkWorld *troubled = shrink_scenario_create(shrink_scenario_find("troubled-store"), 0U);
    assert(corner != NULL && electronics != NULL && big_box != NULL && grocery != NULL && troubled != NULL);
    assert(count_role(corner, SHRINK_EMPLOYEE_CASHIER) == 1U);
    assert(count_role(electronics, SHRINK_EMPLOYEE_SECURITY) >= 2U);
    assert(count_type(electronics, SHRINK_FIXTURE_CAMERA) > count_type(corner, SHRINK_FIXTURE_CAMERA));
    assert(shrink_employee_count(big_box) > shrink_employee_count(corner));
    assert(count_role(grocery, SHRINK_EMPLOYEE_STOCKER) >= 1U);
    assert(shrink_employee_count(grocery) > shrink_employee_count(troubled));
    assert(count_type(troubled, SHRINK_FIXTURE_CAMERA) < count_type(electronics, SHRINK_FIXTURE_CAMERA));
    assert(count_role(troubled, SHRINK_EMPLOYEE_SECURITY) == 0U);

    for (unsigned tick = 0U; tick < 180U; ++tick) shrink_tick(grocery, 1.0);
    assert(has_event(grocery, SHRINK_EVENT_FIRE_STARTED));
    assert(shrink_hazard_count(grocery) > 0U);
    ShrinkHazardSnapshot hazard;
    assert(shrink_hazard_snapshot(grocery, 0U, &hazard) == 1);
    assert(hazard.type == SHRINK_HAZARD_FIRE && hazard.expires_tick > 180U);
    for (unsigned tick = 0U; tick < 45U; ++tick) shrink_tick(grocery, 1.0);
    assert(shrink_hazard_count(grocery) == 0U);
    assert(has_event(grocery, SHRINK_EVENT_FIRE_RESOLVED));

    shrink_destroy(corner);
    shrink_destroy(electronics);
    shrink_destroy(big_box);
    shrink_destroy(grocery);
    shrink_destroy(troubled);
    puts("scenario registry and deterministic presets: ok");
    return 0;
}
