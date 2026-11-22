#ifndef ENEMY_NAVIGATION_H
#define ENEMY_NAVIGATION_H

#include "gameplay.h"
#include "navigation.h"

/* Cached, compact waypoint route for one non-player character. */
typedef struct {
    unsigned char x[NAV_MAX_CELLS];
    unsigned char y[NAV_MAX_CELLS];
    unsigned short count;
    unsigned short cursor;
    float last_goal_x;
    float last_goal_z;
    int repath_frames;
} EnemyRoute;

void enemyNavReset(void);
void enemyNavUpdate(const PlayerState *target,
                    EnemyStateData enemies[GAME_ENEMY_MAX],
                    const NavGrid *grid, StageId stage, float stage_limit,
                    float dt);
const EnemyRoute *enemyNavGetRoute(int enemy_index);

#endif
