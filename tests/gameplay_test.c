#include "../gameplay.h"
#include <assert.h>
#include <stdio.h>

static void test_stage_setup(void) {
    PlayerState p; EnemyStateData enemies[GAME_ENEMY_MAX];
    WeaponState weapons[GAME_WEAPON_MAX]; InventoryState bag; CampaignState c;
    StageData stage;
    gameplayInit(&p,enemies,weapons,&bag,&c);
    gameplayLoadStage(STAGE_VILLAGE,&stage,enemies);
    gameplaySetStage(&c,STAGE_VILLAGE);
    assert(stage.enemy_total==6);
    assert(c.stage_total==6);
    assert(p.hp==100 && weapons[WEAPON_HANDGUN].owned);
    gameplayLoadStage(STAGE_CASTLE,&stage,enemies);
    gameplaySetStage(&c,STAGE_CASTLE);
    assert(stage.id==STAGE_CASTLE && c.stage_total==8);
    gameplayLoadStage(STAGE_ISLAND,&stage,enemies);
    gameplaySetStage(&c,STAGE_ISLAND);
    assert(stage.id==STAGE_ISLAND && c.stage_total==10);
}

static void test_weapons_and_inventory(void) {
    PlayerState p; EnemyStateData enemies[GAME_ENEMY_MAX];
    WeaponState weapons[GAME_WEAPON_MAX]; InventoryState bag; CampaignState c;
    gameplayInit(&p,enemies,weapons,&bag,&c);
    p.money=2000;
    assert(gameplayBuyWeapon(&p,weapons,WEAPON_SHOTGUN)==1);
    assert(weapons[WEAPON_SHOTGUN].owned==1);
    assert(weapons[WEAPON_SHOTGUN].mag==6);
    assert(gameplayUpgradeWeapon(&p,&weapons[WEAPON_SHOTGUN],250)==1);
    assert(weapons[WEAPON_SHOTGUN].damage==4);
    assert(gameplayBuyHealth(&p,&bag,120)==1);
    p.hp=50;
    assert(gameplayUseHealth(&p,&bag)==1);
    assert(p.hp==90);
    assert(gameplayUseHealth(&p,&bag)==1);
    assert(p.hp==100);
    assert(gameplayUseHealth(&p,&bag)==0);
    assert(gameplayBuyAmmo(&p,&weapons[WEAPON_SHOTGUN],10,2)==1);
    assert(weapons[WEAPON_SHOTGUN].reserve==28);
}

static void test_fire_and_reload(void) {
    PlayerState p; EnemyStateData enemies[GAME_ENEMY_MAX];
    WeaponState weapons[GAME_WEAPON_MAX]; InventoryState bag; CampaignState c;
    int i,hit;
    gameplayInit(&p,enemies,weapons,&bag,&c);
    enemies[0].x=0.0f;enemies[0].z=4.0f;enemies[0].hp=1;
    enemies[0].active=1;enemies[0].state=ENEMY_CHASE;
    p.angle=0.0f;
    hit=gameplayFire(&p,&weapons[WEAPON_HANDGUN],enemies,1,10.0f,0.2f);
    assert(hit==0);
    assert(enemies[0].active==0);
    assert(weapons[WEAPON_HANDGUN].mag==9);
    assert(p.money==350);
    weapons[WEAPON_HANDGUN].mag=0;
    assert(gameplayReload(&p,&weapons[WEAPON_HANDGUN])==1);
    for(i=0;i<45;i++)gameplayUpdateReload(&p,&weapons[WEAPON_HANDGUN]);
    assert(p.reloading==0);
    assert(weapons[WEAPON_HANDGUN].mag==10);
    assert(weapons[WEAPON_HANDGUN].reserve==40);
}

static void test_enemy_and_campaign(void) {
    PlayerState p; EnemyStateData enemies[GAME_ENEMY_MAX];
    WeaponState weapons[GAME_WEAPON_MAX]; InventoryState bag; CampaignState c;
    int i;
    gameplayInit(&p,enemies,weapons,&bag,&c);
    { StageData stage; gameplayLoadStage(STAGE_VILLAGE,&stage,enemies); }
    gameplaySetStage(&c,STAGE_VILLAGE);
    p.x=enemies[0].x; p.z=enemies[0].z;
    for(i=0;i<30;i++)gameplayUpdateEnemies(&p,enemies,1.0f/60.0f);
    assert(enemies[0].state==ENEMY_CHASE || enemies[0].state==ENEMY_ATTACK);
    for(i=0;i<GAME_ENEMY_MAX;i++){enemies[i].active=0;enemies[i].state=ENEMY_DEAD;}
    gameplayUpdateCampaign(&c,enemies);
    assert(c.boss_active==1);
    c.completed=1;c.boss_active=0;
    gameplayUpdateCampaign(&c,enemies);
    assert(c.boss_active==0);
}

int main(void) {
    test_stage_setup();
    test_weapons_and_inventory();
    test_fire_and_reload();
    test_enemy_and_campaign();
    puts("gameplay tests: PASS");
    return 0;
}
