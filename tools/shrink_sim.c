#define _POSIX_C_SOURCE 200809L
#include "shrink.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void usage(const char *name)
{
    fprintf(stderr, "Usage: %s [--days N] [--seed N] [--stream --ticks N]\n", name);
}

static void stream_frame(const ShrinkWorld *world)
{
    ShrinkMetrics metrics;
    shrink_metrics(world, &metrics);
    const size_t count = shrink_entity_count(world);
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
