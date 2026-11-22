#include "../enemy_navigation.h"
#include "../world_navigation.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void test_agents_route_around_village_buildings(void) {
    PlayerState player; EnemyStateData enemies[GAME_ENEMY_MAX]; NavGrid grid;
    int i, moved=0;
    gameplayInit(&player,enemies,(WeaponState[GAME_WEAPON_MAX]){{0}},
                 &(InventoryState){{0}},&(CampaignState){0});
    worldNavBuild(STAGE_VILLAGE,18.0f,&grid);
    player.x=0.0f;player.z=0.0f;
    enemies[0].active=1;enemies[0].state=ENEMY_IDLE;
    enemies[0].x=-14.0f;enemies[0].z=7.0f;enemies[0].speed=1.6f;
    enemyNavReset();
    for(i=0;i<360;i++) {
        float oldx=enemies[0].x,oldz=enemies[0].z;
        enemyNavUpdate(&player,enemies,&grid,STAGE_VILLAGE,18.0f,1.0f/60.0f);
        assert(!worldNavBlocked(STAGE_VILLAGE,18.0f,enemies[0].x,enemies[0].z,0.42f));
        if(fabsf(enemies[0].x-oldx)+fabsf(enemies[0].z-oldz)>0.0001f)moved++;
    }
    assert(moved>30);
    assert(enemies[0].state==ENEMY_CHASE);
    assert(hypotf(enemies[0].x-player.x,enemies[0].z-player.z)<hypotf(-14.0f,7.0f));
    assert(enemyNavGetRoute(0)!=0);
}
static void test_route_reset_and_inactive_agents(void) {
    PlayerState player={0};EnemyStateData enemies[GAME_ENEMY_MAX]={{0}};
    NavGrid grid;worldNavBuild(STAGE_CASTLE,22.0f,&grid);
    enemies[0].active=1;enemies[0].state=ENEMY_IDLE;
    enemies[0].x=-15.0f;enemies[0].z=5.0f;enemies[0].speed=1.0f;
    enemyNavReset();
    enemyNavUpdate(&player,enemies,&grid,STAGE_CASTLE,22.0f,1.0f/60.0f);
    assert(enemyNavGetRoute(0)->count>0);
    enemies[0].active=0;
    enemyNavUpdate(&player,enemies,&grid,STAGE_CASTLE,22.0f,1.0f/60.0f);
    assert(enemyNavGetRoute(0)->count==0);
}
int main(void) {
    test_agents_route_around_village_buildings();
    test_route_reset_and_inactive_agents();
    puts("enemy navigation tests: PASS");return 0;
}
