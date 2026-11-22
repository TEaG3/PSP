#include "savedata.h"

#include <pspiofilemgr.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SAVE_DIR  "ms0:/PSP/SAVEDATA/RE4PROTO"
#define SAVE_FILE SAVE_DIR "/SAVE.BIN"
#define TEMP_FILE SAVE_DIR "/SAVE.TMP"

typedef struct {
    uint32_t magic;
    float player_x, player_z, player_angle;
    int hp, score, ammo, reserve_ammo;
} LegacyGameState;

static int valid_state(const GameState *s) {
    int i;
    if (!s || !isfinite(s->player_x) || !isfinite(s->player_z) ||
        !isfinite(s->player_angle)) return 0;
    if (s->player_x < -24.0f || s->player_x > 24.0f ||
        s->player_z < -24.0f || s->player_z > 24.0f) return 0;
    if (s->hp < 0 || s->hp > 100 || s->score < 0 || s->score > 9999999 ||
        s->stage < 0 || s->stage > 2 || s->money < 0 || s->money > 999999 ||
        s->selected_weapon < 0 || s->selected_weapon > 2 ||
        s->health_items < 0 || s->health_items > 99 ||
        s->objective < 0 || s->objective > 99 ||
        s->boss_active < 0 || s->boss_active > 2 ||
        s->defeated < 0 || s->defeated > 10 || s->boss_hp < 0 || s->boss_hp > 300 ||
        s->completed < 0 || s->completed > 1) return 0;
    for (i=0;i<3;i++) {
        int cap = (i==0)?10:((i==1)?6:20);
        if (s->weapon_mag[i] < 0 || s->weapon_mag[i] > cap ||
            s->weapon_reserve[i] < 0 || s->weapon_reserve[i] > 999 ||
            s->weapon_damage[i] < 0 || s->weapon_damage[i] > 8 ||
            s->weapon_owned[i] < 0 || s->weapon_owned[i] > 1) return 0;
    }
    return s->ammo >= 0 && s->ammo <= 20 &&
           s->reserve_ammo >= 0 && s->reserve_ammo <= 999;
}

static int write_save_file(const char *path, const GameState *copy) {
    FILE *fp=fopen(path,"wb");
    int ok;
    if(!fp)return -1;
    ok=fwrite(copy,sizeof(*copy),1,fp)==1;
    if(fflush(fp)!=0)ok=0;
    if(fclose(fp)!=0)ok=0;
    return ok?0:-1;
}

int saveGame(const GameState *state) {
    GameState copy;
    if(!valid_state(state))return -1;
    sceIoMkdir("ms0:/PSP",0777);
    sceIoMkdir("ms0:/PSP/SAVEDATA",0777);
    sceIoMkdir(SAVE_DIR,0777);
    copy=*state;copy.magic=SAVE_MAGIC;
    (void)sceIoRemove(TEMP_FILE);
    if(write_save_file(TEMP_FILE,&copy)!=0)return -1;
    if(sceIoRename(TEMP_FILE,SAVE_FILE)>=0)return 0;
    (void)sceIoRemove(TEMP_FILE);
    return write_save_file(SAVE_FILE,&copy);
}

int loadGame(GameState *state) {
    FILE *fp;
    uint32_t magic;
    int extra;
    if(!state)return -1;
    fp=fopen(SAVE_FILE,"rb");
    if(!fp)return -1;
    if(fread(&magic,sizeof(magic),1,fp)!=1){fclose(fp);return -1;}
    if(magic==SAVE_MAGIC){
        GameState copy;
        memset(&copy,0,sizeof(copy));copy.magic=magic;
        if(fread((char*)&copy+sizeof(magic),sizeof(copy)-sizeof(magic),1,fp)!=1){
            fclose(fp);return -1;
        }
        extra=fgetc(fp);
        if(ferror(fp)||extra!=EOF){fclose(fp);return -1;}
        if(fclose(fp)!=0||!valid_state(&copy))return -1;
        *state=copy;return 0;
    }
    if(magic==SAVE_MAGIC_V1){
        LegacyGameState old;
        GameState copy;
        memset(&old,0,sizeof(old));old.magic=magic;
        if(fread((char*)&old+sizeof(magic),sizeof(old)-sizeof(magic),1,fp)!=1){
            fclose(fp);return -1;
        }
        extra=fgetc(fp);
        if(ferror(fp)||extra!=EOF){fclose(fp);return -1;}
        if(fclose(fp)!=0)return -1;
        memset(&copy,0,sizeof(copy));
        copy.magic=SAVE_MAGIC;
        copy.player_x=old.player_x;copy.player_z=old.player_z;copy.player_angle=old.player_angle;
        copy.hp=old.hp;copy.score=old.score;copy.ammo=old.ammo;copy.reserve_ammo=old.reserve_ammo;
        copy.stage=0;copy.money=300;copy.selected_weapon=0;
        copy.weapon_mag[0]=old.ammo;copy.weapon_reserve[0]=old.reserve_ammo;
        copy.weapon_damage[0]=1;copy.weapon_owned[0]=1;copy.health_items=1;copy.defeated=0;copy.boss_hp=30;copy.completed=0;
        if(!valid_state(&copy))return -1;
        *state=copy;return 0;
    }
    fclose(fp);
    return -1;
}

int saveExists(void) {
    GameState state;
    return loadGame(&state)==0;
}
