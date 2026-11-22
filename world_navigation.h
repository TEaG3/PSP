#ifndef WORLD_NAVIGATION_H
#define WORLD_NAVIGATION_H

#include "gameplay.h"
#include "navigation.h"

void worldNavBuild(StageId stage, float limit, NavGrid *grid);
int worldNavBlocked(StageId stage, float limit, float x, float z, float radius);
int worldNavFindPath(const NavGrid *grid, float limit,
                     float start_x, float start_z,
                     float goal_x, float goal_z, NavPath *path);
void worldNavCellCenter(float limit, int cell_x, int cell_y, float *x, float *z);

#endif
