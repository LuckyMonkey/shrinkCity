#include "pathfinding.h"
#include <stddef.h>

#define GRID_SIZE 20
#define GRID_CELLS 1024

static int legacy_blocked(int x, int y)
{
    return (x >= 6 && x <= 7 && y >= 3 && y <= 8) ||
           (x >= 6 && x <= 7 && y >= 11 && y <= 16) ||
           (x >= 12 && x <= 13 && y >= 3 && y <= 8) ||
           (x >= 12 && x <= 13 && y >= 11 && y <= 16);
}

static int valid(int x, int y, int width, int height) { return x >= 0 && y >= 0 && x < width && y < height; }

int shrink_path_next_step_grid(int sx, int sy, int gx, int gy, const unsigned char *blocked, int width, int height, int *out_x, int *out_y)
{
    int queue[GRID_CELLS], previous[GRID_CELLS];
    unsigned char visited[GRID_CELLS] = {0};
    const int directions[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    if (blocked == NULL || out_x == NULL || out_y == NULL || width <= 0 || height <= 0 || width * height > GRID_CELLS ||
        !valid(sx, sy, width, height) || !valid(gx, gy, width, height) || blocked[gy * width + gx]) return 0;
    const int start = sy * width + sx, goal = gy * width + gx;
    size_t head = 0U, tail = 0U;
    queue[tail++] = start; visited[start] = 1U; previous[start] = -1;
    while (head < tail) {
        const int current = queue[head++];
        if (current == goal) break;
        const int x = current % width, y = current / width;
        for (size_t d = 0; d < 4U; ++d) {
            const int nx = x + directions[d][0], ny = y + directions[d][1];
            if (!valid(nx, ny, width, height) || blocked[ny * width + nx]) continue;
            const int next = ny * width + nx;
            if (visited[next]) continue;
            visited[next] = 1U; previous[next] = current; queue[tail++] = next;
        }
    }
    if (!visited[goal]) return 0;
    int step = goal;
    while (previous[step] >= 0 && previous[step] != start) step = previous[step];
    *out_x = step % width; *out_y = step / width;
    return 1;
}

int shrink_path_reachable_grid(int sx, int sy, int gx, int gy, const unsigned char *blocked, int width, int height)
{
    int x = 0, y = 0;
    return shrink_path_next_step_grid(sx, sy, gx, gy, blocked, width, height, &x, &y) && (x != sx || y != sy || (sx == gx && sy == gy));
}

int shrink_path_next_step(int sx, int sy, int gx, int gy, int *out_x, int *out_y)
{
    unsigned char blocked[GRID_CELLS] = {0};
    for (int y = 0; y < GRID_SIZE; ++y) for (int x = 0; x < GRID_SIZE; ++x) blocked[y * GRID_SIZE + x] = (unsigned char)legacy_blocked(x, y);
    return shrink_path_next_step_grid(sx, sy, gx, gy, blocked, GRID_SIZE, GRID_SIZE, out_x, out_y);
}
