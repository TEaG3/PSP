#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "arabic_font.h"
#include "audio.h"
#include "obj_loader.h"
#include "texture.h"
#include "savedata.h"
#include "gameplay.h"

PSP_MODULE_INFO("RE4 Proto", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272
#define ARENA_LIMIT 18.0f

typedef struct {
    unsigned int color;
    float x, y, z;
} ColorVertex;

static unsigned int __attribute__((aligned(16))) display_list[262144];
static float player_x, player_z, player_angle;
static int hp, score, ammo, reserve_ammo;
static int game_started, paused, previous_buttons;
static ObjModel *player_model, *enemy_model;
static Texture *player_texture, *enemy_texture;
static PlayerState player_state;
static EnemyStateData enemies[GAME_ENEMY_MAX];
static WeaponState weapons[GAME_WEAPON_MAX];
static InventoryState inventory;
static CampaignState campaign;
static StageData current_stage;
static int selected_weapon = WEAPON_HANDGUN;
static int pause_mode, pause_selection, merchant_selection, inventory_selection;
static int animation_frame, muzzle_flash_frames, hit_flash_frames, player_previous_hp;
static float camera_x, camera_y=3.8f, camera_z;
static int camera_ready;

static const ColorVertex cube_vertices[] = {
    {0xFF4D8C35,-0.5f,-0.5f, 0.5f},{0xFF4D8C35, 0.5f,-0.5f, 0.5f},{0xFF4D8C35, 0.5f, 0.5f, 0.5f},
    {0xFF4D8C35,-0.5f,-0.5f, 0.5f},{0xFF4D8C35, 0.5f, 0.5f, 0.5f},{0xFF4D8C35,-0.5f, 0.5f, 0.5f},
    {0xFF356A9A, 0.5f,-0.5f,-0.5f},{0xFF356A9A,-0.5f,-0.5f,-0.5f},{0xFF356A9A,-0.5f, 0.5f,-0.5f},
    {0xFF356A9A, 0.5f,-0.5f,-0.5f},{0xFF356A9A,-0.5f, 0.5f,-0.5f},{0xFF356A9A, 0.5f, 0.5f,-0.5f},
    {0xFF9A5B35,-0.5f,-0.5f,-0.5f},{0xFF9A5B35,-0.5f,-0.5f, 0.5f},{0xFF9A5B35,-0.5f, 0.5f, 0.5f},
    {0xFF9A5B35,-0.5f,-0.5f,-0.5f},{0xFF9A5B35,-0.5f, 0.5f, 0.5f},{0xFF9A5B35,-0.5f, 0.5f,-0.5f},
    {0xFF8A7130, 0.5f,-0.5f, 0.5f},{0xFF8A7130, 0.5f,-0.5f,-0.5f},{0xFF8A7130, 0.5f, 0.5f,-0.5f},
    {0xFF8A7130, 0.5f,-0.5f, 0.5f},{0xFF8A7130, 0.5f, 0.5f,-0.5f},{0xFF8A7130, 0.5f, 0.5f, 0.5f},
    {0xFF666666,-0.5f, 0.5f, 0.5f},{0xFF666666, 0.5f, 0.5f, 0.5f},{0xFF666666, 0.5f, 0.5f,-0.5f},
    {0xFF666666,-0.5f, 0.5f, 0.5f},{0xFF666666, 0.5f, 0.5f,-0.5f},{0xFF666666,-0.5f, 0.5f,-0.5f},
    {0xFF444444,-0.5f,-0.5f,-0.5f},{0xFF444444, 0.5f,-0.5f,-0.5f},{0xFF444444, 0.5f,-0.5f, 0.5f},
    {0xFF444444,-0.5f,-0.5f,-0.5f},{0xFF444444, 0.5f,-0.5f, 0.5f},{0xFF444444,-0.5f,-0.5f, 0.5f}
};

static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1; (void)arg2; (void)common;
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp) {
    SceUID cbid;
    (void)args; (void)argp;
    cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    if (cbid >= 0) sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void) {
    SceUID thread = sceKernelCreateThread("callback_thread", callback_thread,
        0x11, 0x1000, PSP_THREAD_ATTR_USER, NULL);
    if (thread >= 0) sceKernelStartThread(thread, 0, NULL);
}

static int pressed(unsigned int buttons, unsigned int mask) {
    return (buttons & mask) && !(previous_buttons & mask);
}

static void save_current_game(void) {
    GameState state;
    state.player_x = player_x;
    state.player_z = player_z;
    state.player_angle = player_angle;
    state.hp = hp;
    state.score = score;
    state.ammo = ammo;
    state.reserve_ammo = reserve_ammo;
    state.stage = campaign.stage; state.money = player_state.money;
    state.selected_weapon = selected_weapon;
    state.health_items = inventory.amount[ITEM_HEALTH];
    state.objective = campaign.objective; state.boss_active = campaign.boss_active;
    state.defeated=campaign.defeated; state.boss_hp=campaign.boss_hp; state.completed=campaign.completed;
    { int i; for(i=0;i<GAME_ENEMY_MAX;i++)if(enemies[i].active&&enemies[i].boss){state.boss_hp=enemies[i].hp;break;} }
    { int i; for(i=0;i<GAME_WEAPON_MAX;i++) {
        state.weapon_mag[i]=weapons[i].mag; state.weapon_reserve[i]=weapons[i].reserve;
        state.weapon_damage[i]=weapons[i].damage; state.weapon_owned[i]=weapons[i].owned;
    }}
    (void)saveGame(&state);
}

static void reset_game(void) {
    GameState saved;
    camera_ready = 0;
    player_x = 0.0f;
    player_z = 0.0f;
    player_angle = 0.0f;
    hp = 100;
    score = 0;
    ammo = 10;
    reserve_ammo = 40;
    gameplayInit(&player_state, enemies, weapons, &inventory, &campaign);
    gameplayLoadStage(STAGE_VILLAGE, &current_stage, enemies);
    gameplaySetStage(&campaign,STAGE_VILLAGE);
    player_state.x=player_x; player_state.z=player_z; player_state.angle=player_angle;
    if (loadGame(&saved) == 0) {
        player_x = saved.player_x;
        player_z = saved.player_z;
        player_angle = saved.player_angle;
        hp = saved.hp;
        score = saved.score;
        ammo = saved.ammo;
        reserve_ammo = saved.reserve_ammo;
        player_state.x=player_x; player_state.z=player_z; player_state.angle=player_angle;
        player_state.hp=hp; weapons[WEAPON_HANDGUN].mag=ammo;
        weapons[WEAPON_HANDGUN].reserve=reserve_ammo;
        { int i;
          if(saved.stage>=STAGE_VILLAGE && saved.stage<=STAGE_ISLAND){
              gameplayLoadStage((StageId)saved.stage,&current_stage,enemies);
              gameplaySetStage(&campaign,(StageId)saved.stage);
          }
          player_state.money=saved.money; selected_weapon=saved.selected_weapon;
          inventory.amount[ITEM_HEALTH]=saved.health_items;
          campaign.objective=saved.objective; campaign.boss_active=saved.boss_active;
          campaign.defeated=saved.defeated; campaign.boss_hp=saved.boss_hp; campaign.completed=saved.completed;
          if(saved.boss_active==2){
              int j; for(j=0;j<GAME_ENEMY_MAX;j++){enemies[j].active=0;enemies[j].state=ENEMY_DEAD;}
              enemies[GAME_ENEMY_MAX-1].x=0.0f;enemies[GAME_ENEMY_MAX-1].z=12.0f;
              enemies[GAME_ENEMY_MAX-1].hp=saved.boss_hp>0?saved.boss_hp:campaign.boss_hp;
              enemies[GAME_ENEMY_MAX-1].speed=1.0f+(float)saved.stage*0.2f;
              enemies[GAME_ENEMY_MAX-1].active=1;enemies[GAME_ENEMY_MAX-1].boss=1;
              enemies[GAME_ENEMY_MAX-1].state=ENEMY_ALERT;
          } else {
              int j,remaining=saved.defeated;
              for(j=0;j<GAME_ENEMY_MAX&&remaining>0;j++)if(enemies[j].active){
                  enemies[j].active=0;enemies[j].state=ENEMY_DEAD;remaining--;
              }
          }
          for(i=0;i<GAME_WEAPON_MAX;i++){
              weapons[i].mag=saved.weapon_mag[i];weapons[i].reserve=saved.weapon_reserve[i];
              weapons[i].damage=saved.weapon_damage[i];weapons[i].owned=saved.weapon_owned[i];
          }
        }
    }
    paused = 0; pause_mode=0; pause_selection=0;
    player_previous_hp=hp;
    game_started = 1;
}

static void load_assets(void) {
    player_model = objLoad("assets/player.obj");
    enemy_model = objLoad("assets/enemy.obj");
    player_texture = loadTexturePNG("assets/player.png");
    enemy_texture = loadTexturePNG("assets/enemy.png");
}

static void draw_cube(float x, float y, float z, float scale) {
    ScePspFVector3 pos = {x, y, z};
    ScePspFVector3 scl = {scale, scale, scale};
    sceGumPushMatrix();
    sceGumTranslate(&pos);
    sceGumScale(&scl);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
        36, NULL, cube_vertices);
    sceGumPopMatrix();
}

static void draw_box(float x,float y,float z,float sx,float sy,float sz) {
    ScePspFVector3 pos={x,y,z}, scale={sx,sy,sz};
    sceGumPushMatrix();
    sceGumTranslate(&pos);
    sceGumScale(&scale);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D,
                   36,NULL,cube_vertices);
    sceGumPopMatrix();
}

static void draw_local_box(float x,float y,float z,float sx,float sy,float sz,float rz) {
    ScePspFVector3 pos={x,y,z}, scale={sx,sy,sz};
    sceGumPushMatrix();
    sceGumTranslate(&pos);
    sceGumRotateZ(rz);
    sceGumScale(&scale);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D,
                   36,NULL,cube_vertices);
    sceGumPopMatrix();
}

typedef struct { float x, z, hx, hz; } WorldBlock;
static int circle_hits_block(float x,float z,float radius,const WorldBlock *b) {
    float nx=x<b->x-b->hx?b->x-b->hx:(x>b->x+b->hx?b->x+b->hx:x);
    float nz=z<b->z-b->hz?b->z-b->hz:(z>b->z+b->hz?b->z+b->hz:z);
    float dx=x-nx,dz=z-nz;
    return dx*dx+dz*dz<radius*radius;
}
static int world_blocked(float x,float z,float radius) {
    WorldBlock blocks[12]; int n=0,i;
    if(current_stage.id==STAGE_VILLAGE) {
        blocks[n++]=(WorldBlock){-9.0f,7.0f,2.2f,1.75f};
        blocks[n++]=(WorldBlock){0.0f,7.0f,2.2f,1.75f};
        blocks[n++]=(WorldBlock){9.0f,7.0f,2.2f,1.75f};
        blocks[n++]=(WorldBlock){-12.0f,-9.0f,0.8f,0.8f};
        blocks[n++]=(WorldBlock){-6.0f,-9.0f,0.8f,0.8f};
        blocks[n++]=(WorldBlock){0.0f,-9.0f,0.8f,0.8f};
        blocks[n++]=(WorldBlock){6.0f,-9.0f,0.8f,0.8f};
        blocks[n++]=(WorldBlock){12.0f,-9.0f,0.8f,0.8f};
    } else if(current_stage.id==STAGE_CASTLE) {
        blocks[n++]=(WorldBlock){-9.0f,0.0f,1.4f,1.4f};
        blocks[n++]=(WorldBlock){9.0f,0.0f,1.4f,1.4f};
        blocks[n++]=(WorldBlock){-16.0f,-current_stage.limit+1.0f,1.1f,1.1f};
        blocks[n++]=(WorldBlock){-8.0f,-current_stage.limit+1.0f,1.1f,1.1f};
        blocks[n++]=(WorldBlock){0.0f,-current_stage.limit+1.0f,1.1f,1.1f};
        blocks[n++]=(WorldBlock){8.0f,-current_stage.limit+1.0f,1.1f,1.1f};
        blocks[n++]=(WorldBlock){16.0f,-current_stage.limit+1.0f,1.1f,1.1f};
    } else {
        blocks[n++]=(WorldBlock){-12.0f,0.0f,1.75f,1.75f};
        blocks[n++]=(WorldBlock){12.0f,0.0f,1.75f,1.75f};
        blocks[n++]=(WorldBlock){-14.0f,-8.0f,1.4f,1.4f};
        blocks[n++]=(WorldBlock){-7.0f,-8.0f,1.4f,1.4f};
        blocks[n++]=(WorldBlock){0.0f,-8.0f,1.4f,1.4f};
        blocks[n++]=(WorldBlock){7.0f,-8.0f,1.4f,1.4f};
        blocks[n++]=(WorldBlock){14.0f,-8.0f,1.4f,1.4f};
    }
    for(i=0;i<n;i++) if(circle_hits_block(x,z,radius,&blocks[i])) return 1;
    return 0;
}
static void move_player(float dx,float dz) {
    float nx=player_x+dx,nz=player_z+dz,limit=current_stage.limit-0.65f;
    if(nx>limit)nx=limit;if(nx<-limit)nx=-limit;
    if(nz>limit)nz=limit;if(nz<-limit)nz=-limit;
    /* Axis-separated resolution keeps the player sliding along walls. */
    if(!world_blocked(nx,player_z,0.42f))player_x=nx;
    if(!world_blocked(player_x,nz,0.42f))player_z=nz;
}
static void draw_stage_environment(void) {
    int i;
    float edge=current_stage.limit;
    draw_box(0.0f,-0.75f,0.0f,edge*2.0f,0.7f,edge*2.0f);
    if(current_stage.id==STAGE_VILLAGE){
        for(i=-1;i<=1;i++){
            float x=(float)i*9.0f;
            draw_box(x,0.8f,7.0f,4.4f,2.2f,3.5f);
            draw_box(x,2.4f,7.0f,4.9f,0.7f,3.9f);
            draw_box(x+0.4f,0.0f,5.2f,0.9f,1.4f,0.25f);
            /* Roof ridge, chimney and front lintel give each house a readable silhouette. */
            draw_box(x,2.95f,7.0f,0.22f,0.28f,3.65f);
            draw_box(x-1.55f,3.05f,8.15f,0.42f,0.95f,0.42f);
            draw_box(x,1.85f,5.18f,1.35f,0.18f,0.18f);
        }
        for(i=-2;i<=2;i++){
            float x=(float)i*6.0f;
            draw_box(x,1.7f,-9.0f,0.7f,3.3f,0.7f);
            draw_box(x,0.5f,-9.0f,2.0f,0.6f,1.5f);
            draw_box(x+1.45f,0.8f,-9.0f,2.7f,0.16f,0.16f);
        }
        /* Low stone well and market crates. */
        draw_box(-5.0f,0.25f,0.0f,1.8f,0.5f,1.8f);
        draw_box(5.0f,0.35f,-2.0f,1.0f,0.7f,0.9f);
        draw_box(6.1f,0.35f,-2.0f,0.9f,0.7f,0.9f);
    } else if(current_stage.id==STAGE_CASTLE){
        draw_box(0.0f,2.0f,-edge+1.0f,edge*1.8f,4.0f,1.5f);
        draw_box(0.0f,2.0f,edge-1.0f,edge*1.8f,4.0f,1.5f);
        for(i=-2;i<=2;i++){
            draw_box((float)i*8.0f,3.2f,-edge+1.0f,2.2f,6.5f,2.2f);
            draw_box((float)i*8.0f,3.2f,edge-1.0f,2.2f,6.5f,2.2f);
        }
        draw_box(-9.0f,0.6f,0.0f,2.8f,1.2f,2.8f);
        draw_box(9.0f,0.6f,0.0f,2.8f,1.2f,2.8f);
        /* Gatehouse, buttresses and raised central platform. */
        draw_box(0.0f,3.7f,-edge+2.0f,7.0f,1.0f,2.0f);
        draw_box(-4.1f,2.1f,-edge+2.0f,0.8f,4.2f,1.5f);
        draw_box(4.1f,2.1f,-edge+2.0f,0.8f,4.2f,1.5f);
        draw_box(0.0f,0.18f,0.0f,5.8f,0.35f,4.2f);
        draw_box(-13.0f,0.65f,7.0f,1.8f,1.3f,1.8f);
        draw_box(13.0f,0.65f,7.0f,1.8f,1.3f,1.8f);
    } else {
        for(i=-2;i<=2;i++){
            draw_box((float)i*7.0f,0.4f,-8.0f,2.8f,0.8f,2.8f);
            draw_box((float)i*7.0f,0.7f,8.0f,2.8f,1.4f,2.8f);
        }
        draw_box(-12.0f,1.0f,0.0f,3.5f,2.0f,3.5f);
        draw_box(12.0f,1.0f,0.0f,3.5f,2.0f,3.5f);
        draw_box(0.0f,2.0f,-edge+2.0f,edge*1.7f,4.0f,1.2f);
        /* Industrial island silhouettes: containers, vents and a radio mast. */
        draw_box(-6.0f,1.15f,2.0f,4.2f,2.3f,2.5f);
        draw_box(6.0f,1.15f,2.0f,4.2f,2.3f,2.5f);
        draw_box(-6.0f,2.45f,2.0f,4.35f,0.18f,2.65f);
        draw_box(6.0f,2.45f,2.0f,4.35f,0.18f,2.65f);
        draw_box(0.0f,3.2f,9.0f,0.22f,6.4f,0.22f);
        draw_box(0.0f,5.9f,9.0f,2.1f,0.18f,0.18f);
        draw_box(-2.0f,0.55f,-2.0f,1.8f,1.1f,1.8f);
        draw_box(2.0f,0.55f,-2.0f,1.8f,1.1f,1.8f);
    }
}

static void draw_actor(ObjModel *model,Texture *texture,float x,float y,float z,
                       float scale,float angle,float phase,int is_player,int boss) {
    ScePspFVector3 pos={x,y+(sinf(phase*2.0f)*0.045f),z};
    ScePspFVector3 size={scale,scale,scale};
    if(model){
        sceGumPushMatrix();
        sceGumTranslate(&pos);sceGumRotateY(angle);sceGumScale(&size);
        if(texture)bindTexture(texture);else sceGuDisable(GU_TEXTURE_2D);
        objDraw(model);
        sceGumPopMatrix();sceGuDisable(GU_TEXTURE_2D);
        return;
    }
    sceGumPushMatrix();
    sceGumTranslate(&pos);sceGumRotateY(angle);sceGumScale(&size);
    draw_local_box(0.0f,0.98f,0.0f,0.62f,0.92f,0.38f,0.0f);
    draw_local_box(0.0f,1.65f,0.0f,0.44f,0.48f,0.42f,0.0f);
    draw_local_box(-0.43f,1.15f,0.13f,0.22f,0.75f,0.24f,sinf(phase)*0.22f);
    draw_local_box(0.43f,1.15f,0.13f,0.22f,0.75f,0.24f,-sinf(phase)*0.22f);
    draw_local_box(-0.19f,0.34f,0.0f,0.25f,0.72f,0.28f,sinf(phase)*0.3f);
    draw_local_box(0.19f,0.34f,0.0f,0.25f,0.72f,0.28f,-sinf(phase)*0.3f);
    if(is_player){
        draw_local_box(0.17f,0.97f,0.53f,0.13f,0.13f,0.72f,0.0f);
    } else if(boss){
        draw_local_box(-0.55f,1.85f,0.0f,0.22f,0.42f,0.22f,-0.35f);
        draw_local_box(0.55f,1.85f,0.0f,0.22f,0.42f,0.22f,0.35f);
    }
    sceGumPopMatrix();
}

static void update_game(const SceCtrlData *pad) {
    float move=0.0f, turn=0.0f, step;
    unsigned int buttons=pad->Buttons;
    int hit;
    if(pressed(buttons,PSP_CTRL_START)){paused=1;pause_mode=0;return;}
    if(!(buttons&PSP_CTRL_LTRIGGER)){
        if(buttons&PSP_CTRL_LEFT)turn-=0.045f;
        if(buttons&PSP_CTRL_RIGHT)turn+=0.045f;
    }
    if(pad->Lx>150)turn+=((float)pad->Lx-150.0f)/105.0f*0.055f;
    else if(pad->Lx<105)turn-=((105.0f-(float)pad->Lx)/105.0f)*0.055f;
    player_angle+=turn;
    if(player_angle>3.14159265f)player_angle-=6.2831853f;
    if(player_angle<-3.14159265f)player_angle+=6.2831853f;
    if(buttons&PSP_CTRL_UP)move=1.0f;
    if(buttons&PSP_CTRL_DOWN)move=-0.72f;
    if(pad->Ly<110)move=-((110.0f-(float)pad->Ly)/110.0f);
    else if(pad->Ly>145)move=( (float)pad->Ly-145.0f)/110.0f;
    if(move>1.0f)move=1.0f;if(move<-1.0f)move=-1.0f;
    step=(buttons&PSP_CTRL_RTRIGGER)?0.105f:0.065f;
    move_player(sinf(player_angle)*move*step,cosf(player_angle)*move*step);
    player_state.x=player_x; player_state.z=player_z; player_state.angle=player_angle;
    player_state.aiming=(buttons&PSP_CTRL_LTRIGGER)!=0;
    player_state.anim=player_state.reloading?PLAYER_RELOAD:
                     (hit_flash_frames>0?PLAYER_HURT:
                     (move==0.0f?(player_state.aiming?PLAYER_AIM:PLAYER_IDLE):
                     ((buttons&PSP_CTRL_RTRIGGER)?PLAYER_RUN:PLAYER_WALK)));
    if(pressed(buttons,PSP_CTRL_SQUARE)&&gameplayReload(&player_state,&weapons[selected_weapon]))playSound(SFX_RELOAD);
    gameplayUpdateReload(&player_state,&weapons[selected_weapon]);
    gameplayUpdateEnemies(&player_state,enemies,1.0f/60.0f);
    if(animation_frame<1000000)animation_frame++;
    if(muzzle_flash_frames>0)muzzle_flash_frames--;
    if(hit_flash_frames>0)hit_flash_frames--;
    if(player_state.hp<player_previous_hp){playSound(SFX_HURT);hit_flash_frames=8;player_state.anim=PLAYER_HURT;}
    player_previous_hp=player_state.hp;
    if(player_state.hp<=0){paused=1;pause_mode=4;}
    if(pressed(buttons,PSP_CTRL_CROSS)){
        int had_ammo=weapons[selected_weapon].mag;
        hit=gameplayFire(&player_state,&weapons[selected_weapon],enemies,GAME_ENEMY_MAX,
                         selected_weapon==WEAPON_RIFLE?24.0f:(selected_weapon==WEAPON_SHOTGUN?11.0f:16.0f),
                         selected_weapon==WEAPON_RIFLE?0.08f:(selected_weapon==WEAPON_SHOTGUN?0.36f:
                         (player_state.aiming?0.10f:0.20f)));
        if(had_ammo>0){playSound(SFX_GUNSHOT);muzzle_flash_frames=3;if(hit>=0){score+=25;playSound(SFX_HIT);}}
        else playSound(SFX_EMPTY);
    }
    if(pressed(buttons,PSP_CTRL_TRIANGLE)){
        if(buttons&PSP_CTRL_LTRIGGER)(void)gameplayUpgradeWeapon(&player_state,&weapons[selected_weapon],250);
        else (void)gameplayUseHealth(&player_state,&inventory);
    }
    if((buttons&PSP_CTRL_LTRIGGER)&&pressed(buttons,PSP_CTRL_LEFT)){
        int next=(selected_weapon+GAME_WEAPON_MAX-1)%GAME_WEAPON_MAX;
        if(weapons[next].owned)selected_weapon=next;
    }
    if((buttons&PSP_CTRL_LTRIGGER)&&pressed(buttons,PSP_CTRL_RIGHT)){
        int next=(selected_weapon+1)%GAME_WEAPON_MAX;
        if(weapons[next].owned)selected_weapon=next;
    }
    if(pressed(buttons,PSP_CTRL_CIRCLE)){
        if(buttons&PSP_CTRL_LTRIGGER){
            int id;
            for(id=WEAPON_SHOTGUN;id<GAME_WEAPON_MAX;id++)if(!weapons[id].owned){
                if(gameplayBuyWeapon(&player_state,weapons,(WeaponId)id))playSound(SFX_PICKUP);
                break;
            }
        } else if(gameplayBuyAmmo(&player_state,&weapons[selected_weapon],10,2))playSound(SFX_PICKUP);
    }
    gameplayUpdateCampaign(&campaign,enemies);
    if(campaign.boss_active==1){
        int i,alive=0;
        for(i=0;i<GAME_ENEMY_MAX;i++)if(enemies[i].active)alive++;
        if(alive==0){
            for(i=GAME_ENEMY_MAX-1;i>=0;i--)if(!enemies[i].active){
                enemies[i].x=0.0f; enemies[i].z=12.0f;
                enemies[i].hp=18+campaign.stage*8;
                enemies[i].speed=0.85f+campaign.stage*0.2f;
                enemies[i].active=1; enemies[i].boss=1;
                enemies[i].state=ENEMY_ALERT;
                campaign.boss_hp=enemies[i].hp;
                campaign.boss_active=2;
                playSound(SFX_BOSS);
                break;
            }
        }
    } else if(campaign.boss_active==2){
        int i,alive=0;
        for(i=0;i<GAME_ENEMY_MAX;i++)if(enemies[i].active)alive++;
        if(alive==0){
            campaign.boss_active=0;
            if(campaign.stage<STAGE_ISLAND){
                campaign.stage++;
                gameplayLoadStage((StageId)campaign.stage,&current_stage,enemies);
                gameplaySetStage(&campaign,(StageId)campaign.stage);
                player_x=0.0f;player_z=0.0f;player_state.x=player_x;player_state.z=player_z;
                playSound(SFX_STAGE);
                save_current_game();
            } else {
                campaign.objective=campaign.objective_total;
                campaign.completed=1;campaign.boss_active=0;
                paused=1;pause_mode=3;
                save_current_game();
            }
        }
    }
    hp=player_state.hp; ammo=weapons[selected_weapon].mag; reserve_ammo=weapons[selected_weapon].reserve;
    if(pressed(buttons,PSP_CTRL_SELECT))save_current_game();
}

static void render_menu(void) {
    sceGuStart(GU_DIRECT, display_list);
    sceGuClearColor(0xFF101820);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);
    arabicDrawTextCenter(240.0f, 48.0f, 1.4f, 0xFFFFFFFF, "SURVIVAL PROTOCOL");
    arabicDrawTextCenter(240.0f, 94.0f, 1.0f, 0xFFB8D8FF, "VILLAGE  -  CASTLE  -  ISLAND");
    arabicDrawTextCenter(240.0f, 154.0f, 1.0f, 0xFFFFFFFF, "PRESS X TO START");
    arabicDrawTextCenter(240.0f, 184.0f, 0.9f, 0xFFFFFFFF, "PRESS O TO EXIT");
    arabicDrawTextCenter(240.0f, 238.0f, 0.7f, 0xFFB0B8C0, "DEVELOPMENT BUILD");
    sceGuFinish();
    sceGuSync(0, 0);
    sceGuSwapBuffers();
}

static void render_pause(void) {
    sceGuStart(GU_DIRECT,display_list);
    sceGuClearColor(0xFF101820);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuDisable(GU_DEPTH_TEST);sceGuDisable(GU_CULL_FACE);sceGuDisable(GU_TEXTURE_2D);
    if(pause_mode==3){
        arabicDrawTextCenter(240.0f,84.0f,1.35f,0xFFFFD080,"CAMPAIGN COMPLETE");
        arabicDrawTextCenter(240.0f,132.0f,0.92f,0xFFFFFFFF,"ALL THREE STAGES CLEARED");
        arabicDrawTextCenter(240.0f,202.0f,0.82f,0xFFB8D8FF,"PRESS X OR O TO RETURN TO MAIN MENU");
    } else if(pause_mode==4){
        arabicDrawTextCenter(240.0f,84.0f,1.35f,0xFFFF8080,"GAME OVER");
        arabicDrawTextCenter(240.0f,132.0f,0.92f,0xFFFFFFFF,"YOUR LAST CHECKPOINT WAS SAVED");
        arabicDrawTextCenter(240.0f,202.0f,0.82f,0xFFB8D8FF,"PRESS X OR O TO RETURN TO MAIN MENU");
    } else if(pause_mode==0){
        arabicDrawTextCenter(240.0f,32.0f,1.15f,0xFFFFFFFF,"PAUSE MENU");
        arabicDrawTextCenter(240.0f,66.0f,0.82f,0xFFFFFFFF,pause_selection==0?"> RESUME":"RESUME");
        arabicDrawTextCenter(240.0f,96.0f,0.82f,0xFFFFFFFF,pause_selection==1?"> SAVE GAME":"SAVE GAME");
        arabicDrawTextCenter(240.0f,126.0f,0.82f,0xFFFFFFFF,pause_selection==2?"> MERCHANT":"MERCHANT");
        arabicDrawTextCenter(240.0f,156.0f,0.82f,0xFFFFFFFF,pause_selection==3?"> INVENTORY":"INVENTORY");
        arabicDrawTextCenter(240.0f,186.0f,0.82f,0xFFFFFFFF,pause_selection==4?"> MAIN MENU":"MAIN MENU");
        arabicDrawTextCenter(240.0f,238.0f,0.68f,0xFFB8D8FF,"D-PAD: SELECT   X: CONFIRM   O: BACK");
    } else if(pause_mode==1){
        arabicDrawTextCenter(240.0f,25.0f,1.1f,0xFFFFD080,"MERCHANT");
        {
            char money[64];
            (void)snprintf(money,sizeof(money),"CASH: %d",player_state.money);
            arabicDrawTextCenter(240.0f,53.0f,0.75f,0xFFFFFFFF,money);
        }
        arabicDrawTextCenter(240.0f,85.0f,0.78f,0xFFFFFFFF,merchant_selection==0?"> BUY 10 AMMO - 20":"BUY 10 AMMO - 20");
        arabicDrawTextCenter(240.0f,115.0f,0.78f,0xFFFFFFFF,merchant_selection==1?"> BUY MEDKIT - 120":"BUY MEDKIT - 120");
        arabicDrawTextCenter(240.0f,145.0f,0.78f,0xFFFFFFFF,merchant_selection==2?"> BUY SHOTGUN - 800":"BUY SHOTGUN - 800");
        arabicDrawTextCenter(240.0f,175.0f,0.78f,0xFFFFFFFF,merchant_selection==3?"> BUY RIFLE - 1200":"BUY RIFLE - 1200");
        arabicDrawTextCenter(240.0f,205.0f,0.78f,0xFFFFFFFF,merchant_selection==4?"> UPGRADE WEAPON - 250":"UPGRADE WEAPON - 250");
        arabicDrawTextCenter(240.0f,237.0f,0.72f,0xFFB8D8FF,merchant_selection==5?"> BACK":"BACK - PRESS O");
    } else {
        char item[80];
        arabicDrawTextCenter(240.0f,36.0f,1.1f,0xFFFFFFFF,"INVENTORY");
        (void)snprintf(item,sizeof(item),"MEDKITS: %d",inventory.amount[ITEM_HEALTH]);
        arabicDrawTextCenter(240.0f,90.0f,0.9f,0xFFFFFFFF,item);
        arabicDrawTextCenter(240.0f,140.0f,0.85f,0xFFFFFFFF,"PRESS X TO USE MEDKIT");
        arabicDrawTextCenter(240.0f,190.0f,0.8f,0xFFB8D8FF,"O: BACK");
    }
    sceGuFinish();sceGuSync(0,0);sceGuSwapBuffers();
}

static void update_pause_menu(unsigned int buttons) {
    if(pause_mode==3||pause_mode==4){
        if(pressed(buttons,PSP_CTRL_CROSS)||pressed(buttons,PSP_CTRL_CIRCLE)){
            game_started=0;paused=0;pause_mode=0;
        }
        return;
    }
    if(pause_mode==0){
        if(pressed(buttons,PSP_CTRL_UP))pause_selection=(pause_selection+4)%5;
        if(pressed(buttons,PSP_CTRL_DOWN))pause_selection=(pause_selection+1)%5;
        if(pressed(buttons,PSP_CTRL_CIRCLE)||pressed(buttons,PSP_CTRL_START)){paused=0;return;}
        if(pressed(buttons,PSP_CTRL_TRIANGLE)){save_current_game();return;}
        if(pressed(buttons,PSP_CTRL_CROSS)){
            switch(pause_selection){
                case 0: paused=0;break;
                case 1: save_current_game();break;
                case 2: pause_mode=1;merchant_selection=0;break;
                case 3: pause_mode=2;inventory_selection=0;break;
                case 4: game_started=0;paused=0;break;
                default: break;
            }
        }
    } else if(pause_mode==1){
        if(pressed(buttons,PSP_CTRL_UP))merchant_selection=(merchant_selection+5)%6;
        if(pressed(buttons,PSP_CTRL_DOWN))merchant_selection=(merchant_selection+1)%6;
        if(pressed(buttons,PSP_CTRL_CIRCLE)||pressed(buttons,PSP_CTRL_TRIANGLE)){pause_mode=0;return;}
        if(pressed(buttons,PSP_CTRL_CROSS)){
            int ok=0;
            switch(merchant_selection){
                case 0: ok=gameplayBuyAmmo(&player_state,&weapons[selected_weapon],10,2);break;
                case 1: ok=gameplayBuyHealth(&player_state,&inventory,120);break;
                case 2: ok=gameplayBuyWeapon(&player_state,weapons,WEAPON_SHOTGUN);break;
                case 3: ok=gameplayBuyWeapon(&player_state,weapons,WEAPON_RIFLE);break;
                case 4: ok=gameplayUpgradeWeapon(&player_state,&weapons[selected_weapon],250);break;
                case 5: pause_mode=0;break;
                default: break;
            }
            if(ok)playSound(SFX_PICKUP);
        }
    } else {
        if(pressed(buttons,PSP_CTRL_CROSS))(void)gameplayUseHealth(&player_state,&inventory);
        if(pressed(buttons,PSP_CTRL_CIRCLE)||pressed(buttons,PSP_CTRL_TRIANGLE))pause_mode=0;
    }
}

static void draw_aim_line(void) {
    ColorVertex line[2];
    line[0].color = 0xFF2020FF;
    line[0].x = player_x;
    line[0].y = 0.65f;
    line[0].z = player_z;
    line[1].color = 0xFF2020FF;
    line[1].x = player_x + sinf(player_angle) * 11.0f;
    line[1].y = 0.65f;
    line[1].z = player_z + cosf(player_angle) * 11.0f;
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_LINES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
        2, NULL, line);
}

static void render_scene(int aiming) {
    int i;
    float desired_x=player_x-sinf(player_angle)*(aiming?3.15f:5.35f)+cosf(player_angle)*0.82f;
    float desired_y=aiming?2.45f:3.45f;
    float desired_z=player_z-cosf(player_angle)*(aiming?3.15f:5.35f)-sinf(player_angle)*0.82f;
    float camera_lerp=aiming?0.22f:0.14f;
    ScePspFVector3 camera;
    ScePspFVector3 target = {player_x+sinf(player_angle)*1.15f, 1.12f, player_z+cosf(player_angle)*1.15f};
    ScePspFVector3 up = {0.0f, 1.0f, 0.0f};
    char hud[128];

    if(!camera_ready){camera_x=desired_x;camera_y=desired_y;camera_z=desired_z;camera_ready=1;}
    camera_x+=(desired_x-camera_x)*camera_lerp;
    camera_y+=(desired_y-camera_y)*camera_lerp;
    camera_z+=(desired_z-camera_z)*camera_lerp;
    camera.x=camera_x;camera.y=camera_y;camera.z=camera_z;

    sceGuStart(GU_DIRECT, display_list);
    sceGuClearColor(current_stage.id==STAGE_CASTLE?0xFF252734:(current_stage.id==STAGE_ISLAND?0xFF35494F:0xFF293426));
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuEnable(GU_DEPTH_TEST);
    /* Keep both triangle windings visible until every model's winding is verified. */
    sceGuDisable(GU_CULL_FACE);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuShadeModel(GU_SMOOTH);
    sceGuDisable(GU_TEXTURE_2D);

    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(55.0f, 16.0f / 9.0f, 0.5f, 100.0f);
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    sceGumLookAt(&camera, &target, &up);
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();

    draw_stage_environment();
    for (i = -4; i <= 4; ++i) {
        float edge=current_stage.limit;
        draw_cube((float)i * 4.0f, -1.0f, -edge, 3.8f);
        draw_cube((float)i * 4.0f, -1.0f, edge, 3.8f);
        draw_cube(-edge, -1.0f, (float)i * 4.0f, 3.8f);
        draw_cube(edge, -1.0f, (float)i * 4.0f, 3.8f);
    }

    draw_actor(player_model,player_texture,player_x,0.0f,player_z,1.0f,player_angle,
               (float)animation_frame*0.16f,1,0);
    for (i = 0; i < GAME_ENEMY_MAX; ++i) {
        if (enemies[i].active && enemies[i].state != ENEMY_DEAD)
            draw_actor(enemy_model,enemy_texture,enemies[i].x,0.0f,enemies[i].z,
                       enemies[i].boss?1.55f:1.0f,enemies[i].angle,
                       (float)animation_frame*0.13f,0,enemies[i].boss);
    }
    if (aiming) draw_aim_line();
    if(muzzle_flash_frames>0){
        draw_box(player_x+sinf(player_angle)*0.8f,1.05f,player_z+cosf(player_angle)*0.8f,0.22f,0.22f,0.22f);
    }

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);
    (void)snprintf(hud, sizeof(hud), "HP %d  |  SCORE %d  |  AMMO %d/%d  |  CASH %d",
                   hp, score, ammo, reserve_ammo, player_state.money);
    arabicDrawTextRTL(472.0f, 8.0f, 0.72f, 0xFFFFFFFF, hud);
    arabicDrawTextRTL(472.0f, 52.0f, 0.68f, 0xFFFFD080, current_stage.name);
    (void)snprintf(hud,sizeof(hud),"OBJECTIVE %d/%d  |  ENEMIES %d/%d",campaign.objective,campaign.objective_total,campaign.defeated,current_stage.enemy_total);
    arabicDrawTextRTL(472.0f,74.0f,0.62f,0xFFFFFFFF,hud);
    if(hit_flash_frames>0)arabicDrawTextCenter(240.0f,104.0f,0.9f,0xFFFF7777,"PLAYER HIT");
    arabicDrawTextRTL(472.0f, 30.0f, 0.62f, 0xFFB8D8FF,
                      "L: AIM  |  SQUARE: RELOAD  |  TRIANGLE: HEAL  |  L+TRIANGLE: UPGRADE  |  CIRCLE: FIRE  |  L+CIRCLE: SWITCH");
    sceGuFinish();
    sceGuSync(0, 0);
    sceGuSwapBuffers();
}

static void init_graphics(void) {
    sceGuInit();
    sceGuStart(GU_DIRECT, display_list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_WIDTH);
    sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, (void *)0x88000, BUF_WIDTH);
    sceGuDepthBuffer((void *)0x110000, BUF_WIDTH);
    sceGuOffset(2048 - (SCR_WIDTH / 2), 2048 - (SCR_HEIGHT / 2));
    sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
    sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuEnable(GU_CULL_FACE);
    sceGuFrontFace(GU_CW);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

int main(void) {
    int running = 1;
    setup_callbacks();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    arabicFontInit();
    init_graphics();
    initAudio();
    load_assets();

    while (running) {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(&pad, 1);
        if (!game_started) {
            render_menu();
            if (pressed(pad.Buttons, PSP_CTRL_CROSS)) reset_game();
            if (pressed(pad.Buttons, PSP_CTRL_CIRCLE)) running = 0;
        } else if (paused) {
            render_pause();
            update_pause_menu(pad.Buttons);
            hp=player_state.hp; ammo=weapons[selected_weapon].mag;
            reserve_ammo=weapons[selected_weapon].reserve;
        } else {
            update_game(&pad);
            render_scene((pad.Buttons & PSP_CTRL_LTRIGGER) != 0);
        }
        if (pad.Buttons & PSP_CTRL_HOME) running = 0;
        previous_buttons = pad.Buttons;
        sceDisplayWaitVblankStart();
    }

    if (player_model) objFree(player_model);
    if (enemy_model) objFree(enemy_model);
    if (player_texture) freeTexture(player_texture);
    if (enemy_texture) freeTexture(enemy_texture);
    releaseAudio();
    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
