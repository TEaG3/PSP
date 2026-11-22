#ifndef SAVEDATA_H
#define SAVEDATA_H

#include <stdint.h>

#define SAVE_MAGIC 0x52453451u
#define SAVE_MAGIC_V1 0x52453450u

typedef struct {
    uint32_t magic;
    float player_x;
    float player_z;
    float player_angle;
    int hp;
    int score;
    int ammo;
    int reserve_ammo;
    int stage;
    int money;
    int selected_weapon;
    int weapon_mag[3];
    int weapon_reserve[3];
    int weapon_damage[3];
    int weapon_owned[3];
    int health_items;
    int objective;
    int boss_active;
    int defeated;
    int boss_hp;
    int completed;
} GameState;

int saveGame(const GameState *state);
int loadGame(GameState *state);
int saveExists(void);

#endif
