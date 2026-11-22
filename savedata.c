#include "savedata.h"

#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>

#define SAVE_DIR  "ms0:/PSP/SAVEDATA/RE4PROTO"
#define SAVE_FILE SAVE_DIR "/SAVE.BIN"
#define TEMP_FILE SAVE_DIR "/SAVE.TMP"

int saveGame(const GameState *state) {
    FILE *fp;
    GameState copy;
    if (!state) return -1;
    sceIoMkdir("ms0:/PSP", 0777);
    sceIoMkdir("ms0:/PSP/SAVEDATA", 0777);
    sceIoMkdir(SAVE_DIR, 0777);
    copy = *state;
    copy.magic = SAVE_MAGIC;
    fp = fopen(TEMP_FILE, "wb");
    if (!fp) fp = fopen(SAVE_FILE, "wb");
    if (!fp) return -1;
    if (fwrite(&copy, sizeof(copy), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    if (fclose(fp) != 0) return -1;
    sceIoRename(TEMP_FILE, SAVE_FILE);
    return 0;
}

int loadGame(GameState *state) {
    FILE *fp;
    GameState copy;
    if (!state) return -1;
    fp = fopen(SAVE_FILE, "rb");
    if (!fp) return -1;
    if (fread(&copy, sizeof(copy), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    if (copy.magic != SAVE_MAGIC) return -1;
    *state = copy;
    return 0;
}

int saveExists(void) {
    GameState state;
    return loadGame(&state) == 0;
}
