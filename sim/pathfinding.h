#ifndef SHRINK_PATHFINDING_H
#define SHRINK_PATHFINDING_H

int shrink_path_next_step(int start_x, int start_y, int goal_x, int goal_y,
                          int *out_x, int *out_y);
int shrink_path_next_step_grid(int start_x, int start_y, int goal_x, int goal_y,
                               const unsigned char *blocked, int width, int height,
                               int *out_x, int *out_y);
int shrink_path_reachable_grid(int start_x, int start_y, int goal_x, int goal_y,
                               const unsigned char *blocked, int width, int height);

#endif
