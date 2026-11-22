#include "../world_navigation.h"
#include <assert.h>
#include <stdio.h>

static void test_stage_grids(void) {
    NavGrid g; NavPath p; float x,z;
    worldNavBuild(STAGE_VILLAGE,18.0f,&g);
    assert(g.width==NAV_MAX_WIDTH && g.height==NAV_MAX_HEIGHT);
    assert(worldNavFindPath(&g,18.0f,-14.0f,0.0f,14.0f,0.0f,&p));
    assert(p.count>0);
    worldNavBuild(STAGE_CASTLE,22.0f,&g);
    assert(worldNavFindPath(&g,22.0f,-14.0f,4.0f,14.0f,4.0f,&p));
    worldNavBuild(STAGE_ISLAND,24.0f,&g);
    assert(worldNavFindPath(&g,24.0f,-17.0f,0.0f,17.0f,0.0f,&p));
    worldNavCellCenter(18.0f,16,16,&x,&z);
    assert(x>0.0f && z>0.0f);
}
static void test_collision_matches_grid(void) {
    NavGrid g; NavPath p;
    worldNavBuild(STAGE_VILLAGE,18.0f,&g);
    assert(worldNavBlocked(STAGE_VILLAGE,18.0f,-9.0f,7.0f,0.42f));
    assert(!worldNavBlocked(STAGE_VILLAGE,18.0f,0.0f,0.0f,0.42f));
    assert(worldNavFindPath(&g,18.0f,0.0f,0.0f,-14.0f,7.0f,&p));
}
int main(void) {
    test_stage_grids();test_collision_matches_grid();
    puts("world navigation tests: PASS");return 0;
}
