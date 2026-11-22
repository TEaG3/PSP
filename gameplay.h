#ifndef GAMEPLAY_H
#define GAMEPLAY_H

#define GAME_ENEMY_MAX 12
#define GAME_ITEM_MAX 8
#define GAME_WEAPON_MAX 3

typedef enum { STAGE_VILLAGE=0, STAGE_CASTLE=1, STAGE_ISLAND=2 } StageId;
typedef enum { PLAYER_IDLE=0, PLAYER_WALK, PLAYER_RUN, PLAYER_AIM, PLAYER_RELOAD, PLAYER_HURT } PlayerAnim;
typedef enum { ENEMY_IDLE=0, ENEMY_ALERT, ENEMY_CHASE, ENEMY_ATTACK, ENEMY_STAGGER, ENEMY_DEAD } EnemyState;
typedef enum { WEAPON_HANDGUN=0, WEAPON_SHOTGUN, WEAPON_RIFLE } WeaponId;
typedef enum { ITEM_AMMO=0, ITEM_HEALTH, ITEM_MONEY, ITEM_KEY, ITEM_COUNT } ItemId;

typedef struct {
    float x,z,angle, speed;
    int hp, money, anim, aiming, reloading, reload_frames;
} PlayerState;

typedef struct {
    float x,z,angle, speed;
    int hp, state, attack_cooldown, alert_frames, active, boss;
} EnemyStateData;

typedef struct {
    int mag, reserve, capacity, damage, owned, price;
} WeaponState;

typedef struct {
    int amount[ITEM_COUNT];
} InventoryState;

typedef struct {
    StageId id;
    const char *name;
    unsigned int ground_color, wall_color, accent_color;
    float limit;
    int enemy_total, objective_total;
} StageData;

typedef struct {
    int stage, objective, defeated, boss_hp, boss_active, merchant_open, objective_total, stage_total, completed;
} CampaignState;

void gameplayInit(PlayerState *p, EnemyStateData enemies[GAME_ENEMY_MAX],
                  WeaponState weapons[GAME_WEAPON_MAX], InventoryState *bag,
                  CampaignState *campaign);
void gameplayLoadStage(StageId id, StageData *stage, EnemyStateData enemies[GAME_ENEMY_MAX]);
void gameplayUpdateEnemies(PlayerState *p, EnemyStateData enemies[GAME_ENEMY_MAX], float dt);
int gameplayFire(PlayerState *p, WeaponState *weapon, EnemyStateData enemies[GAME_ENEMY_MAX],
                 int count, float range, float tolerance);
int gameplayReload(PlayerState *p, WeaponState *weapon);
void gameplayUpdateReload(PlayerState *p, WeaponState *weapon);
int gameplayBuyAmmo(PlayerState *p, WeaponState *weapon, int amount, int price_per_round);
int gameplayBuyHealth(PlayerState *p, InventoryState *bag, int price);
int gameplayBuyWeapon(PlayerState *p, WeaponState weapons[GAME_WEAPON_MAX], WeaponId id);
int gameplayUpgradeWeapon(PlayerState *p, WeaponState *weapon, int price);
int gameplayUseHealth(PlayerState *p, InventoryState *bag);
void gameplayUpdateCampaign(CampaignState *c, EnemyStateData enemies[GAME_ENEMY_MAX]);
void gameplaySetStage(CampaignState *c, StageId id);
int gameplayBossDamage(CampaignState *c, int damage);

#endif
