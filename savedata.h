#ifndef SAVEDATA_H
#define SAVEDATA_H

#include <stdint.h>

#define SAVE_MAGIC 0x52453450u

typedef struct {
    uint32_t magic;
    float player_x;
    float player_z;
    float player_angle;
    int hp;
    int score;
    int ammo;
    int reserve_ammo;
} GameState;

int saveGame(const GameState *state);
int loadGame(GameState *state);
int saveExists(void);

#endif
