#include "pathfinding.h"
#include <stddef.h>

#define GRID_SIZE 20
#define GRID_CELLS (GRID_SIZE * GRID_SIZE)

static int blocked(int x, int y)
{
    return (x >= 6 && x <= 7 && y >= 3 && y <= 8) ||
           (x >= 6 && x <= 7 && y >= 11 && y <= 16) ||
           (x >= 12 && x <= 13 && y >= 3 && y <= 8) ||
           (x >= 12 && x <= 13 && y >= 11 && y <= 16);
}

static int valid(int x, int y) { return x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE; }

int shrink_path_next_step(int start_x, int start_y, int goal_x, int goal_y, int *out_x, int *out_y)
{
    int queue[GRID_CELLS], previous[GRID_CELLS];
    unsigned char visited[GRID_CELLS] = {0};
    const int directions[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    if (!valid(start_x, start_y) || !valid(goal_x, goal_y) || blocked(goal_x, goal_y) ||
        out_x == NULL || out_y == NULL) return 0;
    const int start = start_y * GRID_SIZE + start_x, goal = goal_y * GRID_SIZE + goal_x;
    size_t head = 0U, tail = 0U;
    queue[tail++] = start; visited[start] = 1U; previous[start] = -1;
    while (head < tail) {
        const int current = queue[head++];
        if (current == goal) break;
        const int x = current % GRID_SIZE, y = current / GRID_SIZE;
        for (size_t d = 0; d < 4U; ++d) {
            const int nx = x + directions[d][0], ny = y + directions[d][1];
            if (!valid(nx, ny) || blocked(nx, ny)) continue;
            const int next = ny * GRID_SIZE + nx;
            if (visited[next]) continue;
            visited[next] = 1U; previous[next] = current; queue[tail++] = next;
        }
    }
    if (!visited[goal]) return 0;
    int step = goal;
    while (previous[step] >= 0 && previous[step] != start) step = previous[step];
    *out_x = step % GRID_SIZE; *out_y = step / GRID_SIZE;
    return 1;
}
