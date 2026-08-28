#include "shrink.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static ShrinkEmployeeSnapshot find_role(const ShrinkWorld *world, ShrinkEmployeeRole role)
{
    ShrinkEmployeeSnapshot found = {0};
    for (size_t i = 0U; i < shrink_employee_count(world); ++i) {
        ShrinkEmployeeSnapshot employee;
        assert(shrink_employee_snapshot(world, i, &employee) == 1);
        if (employee.role == role) return employee;
    }
    return found;
}

static void test_hire_fire_and_capacity(void)
{
    ShrinkWorld *world = shrink_create(12345U);
    assert(world != NULL);
    assert(shrink_employee_count(world) == 4U);

    uint64_t hired_id = 0U;
    assert(shrink_hire_employee(world, SHRINK_EMPLOYEE_SECURITY, 0.0, &hired_id) == SHRINK_STAFF_OK);
    assert(hired_id > 4U);
    assert(shrink_employee_count(world) == 5U);

    ShrinkEmployeeSnapshot hired = {0};
    int found = 0;
    for (size_t i = 0U; i < shrink_employee_count(world); ++i) {
        ShrinkEmployeeSnapshot employee;
        assert(shrink_employee_snapshot(world, i, &employee) == 1);
        if (employee.id == hired_id) { hired = employee; found = 1; break; }
    }
    assert(found == 1);
    assert(hired.role == SHRINK_EMPLOYEE_SECURITY);
    assert(fabs(hired.wage - 22.0) < 0.001);

    assert(shrink_fire_employee(world, hired_id) == SHRINK_STAFF_OK);
    assert(shrink_employee_count(world) == 4U);
    assert(shrink_fire_employee(world, hired_id) == SHRINK_STAFF_NOT_FOUND);
    assert(shrink_hire_employee(world, (ShrinkEmployeeRole)99, 10.0, NULL) == SHRINK_STAFF_INVALID_ROLE);

    for (unsigned i = 0U; i < 12U; ++i)
        assert(shrink_hire_employee(world, SHRINK_EMPLOYEE_ASSOCIATE, 17.0, NULL) == SHRINK_STAFF_OK);
    assert(shrink_employee_count(world) == 16U);
    assert(shrink_hire_employee(world, SHRINK_EMPLOYEE_ASSOCIATE, 17.0, NULL) == SHRINK_STAFF_FULL);

    shrink_destroy(world);
}

static void test_labor_changes_with_staffing(void)
{
    ShrinkWorld *full = shrink_create(9001U);
    ShrinkWorld *lean = shrink_create(9001U);
    assert(full != NULL && lean != NULL);

    ShrinkEmployeeSnapshot cashier = find_role(lean, SHRINK_EMPLOYEE_CASHIER);
    assert(cashier.id != 0U);
    assert(shrink_fire_employee(lean, cashier.id) == SHRINK_STAFF_OK);

    for (unsigned i = 0U; i < 120U; ++i) {
        shrink_tick(full, 1.0);
        shrink_tick(lean, 1.0);
    }

    ShrinkMetrics full_metrics, lean_metrics;
    shrink_metrics(full, &full_metrics);
    shrink_metrics(lean, &lean_metrics);
    assert(full_metrics.active_employees == 4U);
    assert(lean_metrics.active_employees == 3U);
    assert(lean_metrics.labor_cost < full_metrics.labor_cost);

    shrink_destroy(full);
    shrink_destroy(lean);
}

static void test_guard_patrol_is_authoritative(void)
{
    ShrinkWorld *world = shrink_create(12345U);
    assert(world != NULL);

    ShrinkEmployeeSnapshot before = find_role(world, SHRINK_EMPLOYEE_SECURITY);
    assert(before.id != 0U);
    assert(before.target_fixture_id != 0U);

    for (unsigned i = 0U; i < 24U; ++i) shrink_tick(world, 1.0);

    ShrinkEmployeeSnapshot after = {0};
    for (size_t i = 0U; i < shrink_employee_count(world); ++i) {
        ShrinkEmployeeSnapshot employee;
        assert(shrink_employee_snapshot(world, i, &employee) == 1);
        if (employee.id == before.id) { after = employee; break; }
    }
    assert(after.id == before.id);
    assert(after.target_fixture_id != 0U);
    assert(after.x != before.x || after.y != before.y || after.target_fixture_id != before.target_fixture_id);
    assert(after.fatigue > before.fatigue);

    shrink_destroy(world);
}

static void test_staff_determinism(void)
{
    ShrinkWorld *a = shrink_create(77U);
    ShrinkWorld *b = shrink_create(77U);
    assert(a != NULL && b != NULL);

    uint64_t a_id = 0U, b_id = 0U;
    assert(shrink_hire_employee(a, SHRINK_EMPLOYEE_SECURITY, 23.5, &a_id) == SHRINK_STAFF_OK);
    assert(shrink_hire_employee(b, SHRINK_EMPLOYEE_SECURITY, 23.5, &b_id) == SHRINK_STAFF_OK);
    assert(a_id == b_id);

    for (unsigned tick = 0U; tick < 40U; ++tick) {
        shrink_tick(a, 1.0);
        shrink_tick(b, 1.0);
    }

    assert(shrink_employee_count(a) == shrink_employee_count(b));
    for (size_t i = 0U; i < shrink_employee_count(a); ++i) {
        ShrinkEmployeeSnapshot ea, eb;
        assert(shrink_employee_snapshot(a, i, &ea) == 1);
        assert(shrink_employee_snapshot(b, i, &eb) == 1);
        assert(ea.id == eb.id && ea.role == eb.role);
        assert(ea.x == eb.x && ea.y == eb.y);
        assert(ea.target_fixture_id == eb.target_fixture_id);
        assert(ea.target_x == eb.target_x && ea.target_y == eb.target_y);
        assert(ea.wage == eb.wage && ea.skill == eb.skill);
        assert(ea.fatigue == eb.fatigue && ea.morale == eb.morale);
    }

    shrink_destroy(a);
    shrink_destroy(b);
}

int main(void)
{
    test_hire_fire_and_capacity();
    test_labor_changes_with_staffing();
    test_guard_patrol_is_authoritative();
    test_staff_determinism();
    puts("shrink-staffing-tests: all assertions passed");
    return 0;
}
