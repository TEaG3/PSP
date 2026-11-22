#include "gameplay.h"
#include <math.h>
#include <string.h>

#define PI_F 3.14159265358979323846f
#define ENEMY_ATTACK_RANGE 1.35f

static float clampf(float v,float lo,float hi){return v<lo?lo:(v>hi?hi:v);}
static float angle_delta(float a,float b){
    float d=b-a;
    while(d>PI_F)d-=2.0f*PI_F;
    while(d<-PI_F)d+=2.0f*PI_F;
    return d;
}

void gameplayInit(PlayerState *p, EnemyStateData enemies[GAME_ENEMY_MAX],
                  WeaponState weapons[GAME_WEAPON_MAX], InventoryState *bag,
                  CampaignState *campaign) {
    int i;
    memset(p,0,sizeof(*p)); memset(enemies,0,sizeof(EnemyStateData)*GAME_ENEMY_MAX);
    memset(weapons,0,sizeof(WeaponState)*GAME_WEAPON_MAX); memset(bag,0,sizeof(*bag));
    memset(campaign,0,sizeof(*campaign));
    p->hp=100; p->speed=3.0f; p->anim=PLAYER_IDLE; p->money=300;
    weapons[WEAPON_HANDGUN]=(WeaponState){10,50,10,1,1,0};
    weapons[WEAPON_SHOTGUN]=(WeaponState){0,0,6,3,0,800};
    weapons[WEAPON_RIFLE]=(WeaponState){0,0,20,2,0,1200};
    bag->amount[ITEM_HEALTH]=1;
    campaign->stage=STAGE_VILLAGE; campaign->boss_hp=30; campaign->objective_total=3;
    for(i=0;i<GAME_ENEMY_MAX;i++) enemies[i].state=ENEMY_DEAD;
}

void gameplayLoadStage(StageId id, StageData *s, EnemyStateData enemies[GAME_ENEMY_MAX]) {
    static const StageData stages[3]={
        {STAGE_VILLAGE,"القرية",0xFF526344,0xFF62513B,0xFFB58B50,18.0f,6,3},
        {STAGE_CASTLE,"القلعة",0xFF343442,0xFF77716B,0xFFB9A36C,22.0f,8,4},
        {STAGE_ISLAND,"الجزيرة",0xFF46545A,0xFF555B60,0xFF8B9A9C,24.0f,10,5}
    };
    static const float sx[6]={-8,-3,4,9,-10,2};
    static const float sz[6]={-9,-12,-8,3,8,10};
    int i,n;
    if(id<STAGE_VILLAGE||id>STAGE_ISLAND)id=STAGE_VILLAGE;
    *s=stages[id]; n=s->enemy_total;
    for(i=0;i<GAME_ENEMY_MAX;i++){
        EnemyStateData *e=&enemies[i];
        memset(e,0,sizeof(*e));
        if(i<n){
            e->x=sx[i%6]+(float)(id*2); e->z=sz[i%6];
            e->hp=(i==n-1 && id>0)?8:3; e->speed=1.0f+(float)id*0.18f;
            e->active=1; e->state=ENEMY_IDLE;
            e->boss=(i==n-1 && id>0);
        } else e->state=ENEMY_DEAD;
    }
}

void gameplayUpdateEnemies(PlayerState *p, EnemyStateData enemies[GAME_ENEMY_MAX], float dt) {
    int i;
    for(i=0;i<GAME_ENEMY_MAX;i++){
        EnemyStateData *e=&enemies[i]; float dx,dz,d,desired,step;
        if(!e->active||e->state==ENEMY_DEAD)continue;
        dx=p->x-e->x; dz=p->z-e->z; d=sqrtf(dx*dx+dz*dz);
        if(e->attack_cooldown>0)e->attack_cooldown--;
        if(d<10.0f && e->state==ENEMY_IDLE)e->state=ENEMY_ALERT;
        if(e->state==ENEMY_STAGGER){ if(++e->alert_frames>12){e->alert_frames=0;e->state=ENEMY_CHASE;} continue; }
        if(e->state==ENEMY_ALERT){ if(++e->alert_frames>20)e->state=ENEMY_CHASE; }
        if(e->state==ENEMY_CHASE||e->state==ENEMY_ATTACK){
            desired=atan2f(dx,dz); e->angle+=clampf(angle_delta(e->angle,desired),-2.8f*dt,2.8f*dt);
            if(d>ENEMY_ATTACK_RANGE){
                step=e->speed*dt; e->x+=sinf(e->angle)*step; e->z+=cosf(e->angle)*step;
                e->state=ENEMY_CHASE;
            } else if(e->attack_cooldown==0){
                p->hp-=e->boss?12:6; if(p->hp<0)p->hp=0;
                e->attack_cooldown=55; e->state=ENEMY_ATTACK;
            }
        }
    }
}

int gameplayFire(PlayerState *p, WeaponState *w, EnemyStateData enemies[GAME_ENEMY_MAX],
                 int count,float range,float tolerance) {
    int i,hit=-1; float best=range;
    if(!w||!w->owned||w->mag<=0||p->reloading)return -1;
    w->mag--;
    for(i=0;i<count&&i<GAME_ENEMY_MAX;i++){
        EnemyStateData *e=&enemies[i]; float dx,dz,d,a;
        if(!e->active||e->state==ENEMY_DEAD)continue;
        dx=e->x-p->x; dz=e->z-p->z; d=sqrtf(dx*dx+dz*dz);
        a=angle_delta(p->angle,atan2f(dx,dz));
        if(d<best&&fabsf(a)<tolerance){best=d;hit=i;}
    }
    if(hit>=0){
        enemies[hit].hp-=w->damage;
        if(enemies[hit].hp<=0){enemies[hit].hp=0;enemies[hit].active=0;enemies[hit].state=ENEMY_DEAD;}
        else enemies[hit].state=ENEMY_STAGGER;
    }
    return hit;
}

int gameplayReload(PlayerState *p,WeaponState *w){
    if(!w||!w->owned||p->reloading||w->mag>=w->capacity||w->reserve<=0)return 0;
    p->reloading=1;p->reload_frames=45;p->anim=PLAYER_RELOAD;return 1;
}
void gameplayUpdateReload(PlayerState *p,WeaponState *w){
    int need,take;
    if(!p->reloading)return;
    if(--p->reload_frames>0)return;
    need=w->capacity-w->mag;take=need<w->reserve?need:w->reserve;
    w->mag+=take;w->reserve-=take;p->reloading=0;p->anim=PLAYER_IDLE;
}
int gameplayBuyAmmo(PlayerState *p,WeaponState *w,int amount,int price){
    int cost;
    if(!w||!w->owned||amount<=0||price<=0)return 0;
    cost=amount*price;if(p->money<cost)return 0;
    p->money-=cost;w->reserve+=amount;return 1;
}
int gameplayBuyHealth(PlayerState *p,InventoryState *b,int price){
    if(p->money<price||price<=0)return 0;
    p->money-=price;b->amount[ITEM_HEALTH]++;return 1;
}
int gameplayBuyWeapon(PlayerState *p,WeaponState weapons[GAME_WEAPON_MAX],WeaponId id){
    WeaponState *w;
    if(id<WEAPON_HANDGUN||id>=GAME_WEAPON_MAX)return 0;
    w=&weapons[id]; if(w->owned||p->money<w->price)return 0;
    p->money-=w->price;w->owned=1;w->mag=w->capacity;w->reserve=w->capacity*3;return 1;
}
int gameplayUpgradeWeapon(PlayerState *p,WeaponState *w,int price){
    if(!w||!w->owned||price<=0||p->money<price||w->damage>=8)return 0;
    p->money-=price;w->damage++;return 1;
}
int gameplayUseHealth(PlayerState *p,InventoryState *b){
    if(b->amount[ITEM_HEALTH]<=0||p->hp>=100)return 0;
    b->amount[ITEM_HEALTH]--;p->hp+=40;if(p->hp>100)p->hp=100;return 1;
}
void gameplayUpdateCampaign(CampaignState *c,EnemyStateData enemies[GAME_ENEMY_MAX]){
    int i,remaining=0;
    for(i=0;i<GAME_ENEMY_MAX;i++)if(enemies[i].active&&enemies[i].state!=ENEMY_DEAD)remaining++;
    c->defeated=GAME_ENEMY_MAX-remaining;
    if(remaining==0 && c->boss_active==0)c->boss_active=1;
}
int gameplayBossDamage(CampaignState *c,int damage){
    if(!c->boss_active||damage<=0)return 0;
    c->boss_hp-=damage;if(c->boss_hp<=0){c->boss_hp=0;c->boss_active=0;return 1;}return 0;
}
