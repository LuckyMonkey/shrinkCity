#include "shrink.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    unsigned days = 30U, seeds = 10U;
    uint64_t first_seed = 1U;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--days") == 0 && i + 1 < argc) days = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--seeds") == 0 && i + 1 < argc) seeds = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--start-seed") == 0 && i + 1 < argc) first_seed = (uint64_t)strtoull(argv[++i], NULL, 10);
        else { fprintf(stderr, "Usage: %s [--days N] [--seeds N] [--start-seed N]\n", argv[0]); return 2; }
    }
    puts("seed,customers,purchases,thefts,revenue,cost_of_goods,shrink,labor,security,profit,satisfaction,wait");
    for (unsigned offset = 0U; offset < seeds; ++offset) {
        ShrinkWorld *world = shrink_create(first_seed + offset);
        if (world == NULL) return 1;
        for (unsigned day = 0U; day < days; ++day)
            for (unsigned second = 0U; second < 600U; ++second) shrink_tick(world, 1.0);
        ShrinkMetrics m;
        shrink_metrics(world, &m);
        printf("%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
               first_seed + offset, m.customers_entered, m.purchases, m.thefts, m.revenue,
               m.cost_of_goods, m.stolen_value, m.labor_cost, m.security_cost, m.profit,
               m.average_satisfaction, m.average_checkout_wait);
        shrink_destroy(world);
    }
    return 0;
}
