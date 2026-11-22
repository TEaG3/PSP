#ifndef NAVIGATION_H
#define NAVIGATION_H

/* Original compact grid navigation for PSP. No heap allocation at runtime. */
#define NAV_MAX_WIDTH 32
#define NAV_MAX_HEIGHT 32
#define NAV_MAX_CELLS (NAV_MAX_WIDTH * NAV_MAX_HEIGHT)

typedef struct {
    int width;
    int height;
    unsigned char blocked[NAV_MAX_CELLS];
} NavGrid;

typedef struct {
    int x;
    int y;
} NavPoint;

typedef struct {
    NavPoint points[NAV_MAX_CELLS];
    int count;
} NavPath;

void navClear(NavGrid *grid, int width, int height);
int navSetBlocked(NavGrid *grid, int x, int y, int blocked);
int navFindPath(const NavGrid *grid, NavPoint start, NavPoint goal, NavPath *out);

#endif
