#include "savedata.h"

#include <pspiofilemgr.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SAVE_DIR  "ms0:/PSP/SAVEDATA/RE4PROTO"
#define SAVE_FILE SAVE_DIR "/SAVE.BIN"
#define TEMP_FILE SAVE_DIR "/SAVE.TMP"

static int valid_state(const GameState *state) {
    if (!state) return 0;
    if (!isfinite(state->player_x) || !isfinite(state->player_z) ||
        !isfinite(state->player_angle)) return 0;
    if (state->player_x < -18.0f || state->player_x > 18.0f ||
        state->player_z < -18.0f || state->player_z > 18.0f) return 0;
    if (state->hp < 0 || state->hp > 100 ||
        state->score < 0 || state->score > 9999999 ||
        state->ammo < 0 || state->ammo > 10 ||
        state->reserve_ammo < 0 || state->reserve_ammo > 999) return 0;
    return 1;
}

static int write_save_file(const char *path, const GameState *copy) {
    FILE *fp = fopen(path, "wb");
    int ok;
    if (!fp) return -1;
    ok = fwrite(copy, sizeof(*copy), 1, fp) == 1;
    if (fflush(fp) != 0) ok = 0;
    if (fclose(fp) != 0) ok = 0;
    return ok ? 0 : -1;
}

int saveGame(const GameState *state) {
    GameState copy;
    if (!valid_state(state)) return -1;

    sceIoMkdir("ms0:/PSP", 0777);
    sceIoMkdir("ms0:/PSP/SAVEDATA", 0777);
    sceIoMkdir(SAVE_DIR, 0777);

    copy = *state;
    copy.magic = SAVE_MAGIC;
    (void)sceIoRemove(TEMP_FILE);
    if (write_save_file(TEMP_FILE, &copy) != 0) return -1;

    if (sceIoRename(TEMP_FILE, SAVE_FILE) >= 0) return 0;

    /* Some memory-stick filesystems cannot replace an existing file by rename. */
    (void)sceIoRemove(TEMP_FILE);
    return write_save_file(SAVE_FILE, &copy);
}

int loadGame(GameState *state) {
    FILE *fp;
    GameState copy;
    int extra;

    if (!state) return -1;
    fp = fopen(SAVE_FILE, "rb");
    if (!fp) return -1;
    if (fread(&copy, sizeof(copy), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    extra = fgetc(fp);
    if (ferror(fp) || extra != EOF) {
        fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0) return -1;
    if (copy.magic != SAVE_MAGIC || !valid_state(&copy)) return -1;

    *state = copy;
    return 0;
}

int saveExists(void) {
    GameState state;
    return loadGame(&state) == 0;
}
