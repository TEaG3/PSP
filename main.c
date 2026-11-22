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
#define PI_F 3.14159265358979323846f
#define ARENA_LIMIT 18.0f
#define TARGET_COUNT 4

typedef struct {
    unsigned int color;
    float x, y, z;
} ColorVertex;

static unsigned int __attribute__((aligned(16))) display_list[262144];
static float player_x, player_z, player_angle;
static int hp, score, ammo, reserve_ammo;
static int game_started, paused, previous_buttons;
static int target_active[TARGET_COUNT];
static int target_hp[TARGET_COUNT];
static const float target_x[TARGET_COUNT] = {-7.0f, 7.0f, -7.0f, 7.0f};
static const float target_z[TARGET_COUNT] = {-8.0f, -8.0f, 8.0f, 8.0f};
static ObjModel *player_model, *enemy_model;
static Texture *player_texture, *enemy_texture;
static PlayerState player_state;
static EnemyStateData enemies[GAME_ENEMY_MAX];
static WeaponState weapons[GAME_WEAPON_MAX];
static InventoryState inventory;
static CampaignState campaign;
static StageData current_stage;
static int selected_weapon = WEAPON_HANDGUN;

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
    (void)saveGame(&state);
}

static void reset_game(void) {
    int i;
    GameState saved;
    player_x = 0.0f;
    player_z = 0.0f;
    player_angle = 0.0f;
    hp = 100;
    score = 0;
    ammo = 10;
    reserve_ammo = 40;
    gameplayInit(&player_state, enemies, weapons, &inventory, &campaign);
    gameplayLoadStage(STAGE_VILLAGE, &current_stage, enemies);
    player_state.x=player_x; player_state.z=player_z; player_state.angle=player_angle;
    for (i = 0; i < TARGET_COUNT; ++i) { target_active[i] = 0; target_hp[i] = 2; }
    if (loadGame(&saved) == 0) {
        player_x = saved.player_x;
        player_z = saved.player_z;
        player_angle = saved.player_angle;
        hp = saved.hp;
        score = saved.score;
        ammo = saved.ammo;
        reserve_ammo = saved.reserve_ammo;
    }
    paused = 0;
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

static void draw_model(ObjModel *model, Texture *texture,
                       float x, float y, float z, float scale) {
    ScePspFVector3 pos = {x, y, z};
    ScePspFVector3 scl = {scale, scale, scale};
    if (!model) {
        draw_cube(x, y, z, scale);
        return;
    }
    sceGumPushMatrix();
    sceGumTranslate(&pos);
    sceGumScale(&scl);
    sceGuColor(0xFFFFFFFF);
    if (texture) bindTexture(texture);
    else sceGuDisable(GU_TEXTURE_2D);
    objDraw(model);
    sceGumPopMatrix();
    sceGuDisable(GU_TEXTURE_2D);
}

static void update_game(const SceCtrlData *pad) {
    float move=0.0f;
    unsigned int buttons=pad->Buttons;
    int hit;
    if(pressed(buttons,PSP_CTRL_START)){paused=1;return;}
    if(buttons&PSP_CTRL_LEFT)player_angle-=0.045f;
    if(buttons&PSP_CTRL_RIGHT)player_angle+=0.045f;
    if(buttons&PSP_CTRL_UP)move=(buttons&PSP_CTRL_RTRIGGER)?0.24f:0.14f;
    if(buttons&PSP_CTRL_DOWN)move=-0.12f;
    if(pad->Lx>150)player_angle+=0.035f;
    else if(pad->Lx<105)player_angle-=0.035f;
    if(pad->Ly<110)move=(buttons&PSP_CTRL_RTRIGGER)?0.24f:0.14f;
    else if(pad->Ly>145)move=-0.12f;
    player_x+=sinf(player_angle)*move; player_z+=cosf(player_angle)*move;
    if(player_x>ARENA_LIMIT)player_x=ARENA_LIMIT;
    if(player_x<-ARENA_LIMIT)player_x=-ARENA_LIMIT;
    if(player_z>ARENA_LIMIT)player_z=ARENA_LIMIT;
    if(player_z<-ARENA_LIMIT)player_z=-ARENA_LIMIT;
    player_state.x=player_x; player_state.z=player_z; player_state.angle=player_angle;
    player_state.aiming=(buttons&PSP_CTRL_LTRIGGER)!=0;
    player_state.anim=move==0.0f?(player_state.aiming?PLAYER_AIM:PLAYER_IDLE):
                     ((buttons&PSP_CTRL_RTRIGGER)?PLAYER_RUN:PLAYER_WALK);
    if(pressed(buttons,PSP_CTRL_SQUARE)&&gameplayReload(&player_state,&weapons[selected_weapon]))playSound(SFX_RELOAD);
    gameplayUpdateReload(&player_state,&weapons[selected_weapon]);
    gameplayUpdateEnemies(&player_state,enemies,1.0f/30.0f);
    if(pressed(buttons,PSP_CTRL_CROSS)||pressed(buttons,PSP_CTRL_RTRIGGER)){
        int had_ammo=weapons[selected_weapon].mag;
        hit=gameplayFire(&player_state,&weapons[selected_weapon],enemies,GAME_ENEMY_MAX,16.0f,
                         player_state.aiming?0.28f:0.18f);
        if(had_ammo>0){playSound(SFX_GUNSHOT);if(hit>=0){score+=25;playSound(SFX_HIT);}}
        else playSound(SFX_EMPTY);
    }
    if(pressed(buttons,PSP_CTRL_TRIANGLE))(void)gameplayUseHealth(&player_state,&inventory);
    if((buttons&PSP_CTRL_LTRIGGER)&&pressed(buttons,PSP_CTRL_LEFT)){
        int next=(selected_weapon+GAME_WEAPON_MAX-1)%GAME_WEAPON_MAX;
        if(weapons[next].owned)selected_weapon=next;
    }
    if((buttons&PSP_CTRL_LTRIGGER)&&pressed(buttons,PSP_CTRL_RIGHT)){
        int next=(selected_weapon+1)%GAME_WEAPON_MAX;
        if(weapons[next].owned)selected_weapon=next;
    }
    if(pressed(buttons,PSP_CTRL_CIRCLE)){
        if(gameplayBuyAmmo(&player_state,&weapons[selected_weapon],10,2))playSound(SFX_PICKUP);
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
                campaign.boss_active=2;
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
            } else campaign.objective=campaign.objective_total;
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
    arabicDrawTextCenter(240.0f, 48.0f, 1.4f, 0xFFFFFFFF, "ريزيدنت إيفل 4");
    arabicDrawTextCenter(240.0f, 94.0f, 1.0f, 0xFFB8D8FF, "القرية - القلعة - الجزيرة");
    arabicDrawTextCenter(240.0f, 154.0f, 1.0f, 0xFFFFFFFF, "زر البدء لبدء اللعب");
    arabicDrawTextCenter(240.0f, 184.0f, 0.9f, 0xFFFFFFFF, "زر الدائرة للخروج");
    arabicDrawTextCenter(240.0f, 238.0f, 0.7f, 0xFFB0B8C0, "نسخة تطوير قيد الاختبار");
    sceGuFinish();
    sceGuSync(0, 0);
    sceGuSwapBuffers();
}

static void render_pause(void) {
    sceGuStart(GU_DIRECT, display_list);
    sceGuClearColor(0xFF101820);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);
    arabicDrawTextCenter(240.0f, 62.0f, 1.3f, 0xFFFFFFFF, "اللعبة متوقفة");
    arabicDrawTextCenter(240.0f, 126.0f, 0.95f, 0xFFFFFFFF, "زر البدء للمتابعة");
    arabicDrawTextCenter(240.0f, 162.0f, 0.9f, 0xFFFFFFFF, "زر الدائرة للقائمة الرئيسية");
    arabicDrawTextCenter(240.0f, 198.0f, 0.85f, 0xFFB8D8FF, "زر المثلث لحفظ التقدم");
    sceGuFinish();
    sceGuSync(0, 0);
    sceGuSwapBuffers();
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
    ScePspFVector3 camera = {
        player_x - sinf(player_angle) * (aiming ? 3.5f : 5.8f) + cosf(player_angle)*0.9f,
        aiming ? 2.6f : 3.8f,
        player_z - cosf(player_angle) * (aiming ? 3.5f : 5.8f) - sinf(player_angle)*0.9f
    };
    ScePspFVector3 target = {player_x, 1.0f, player_z};
    ScePspFVector3 up = {0.0f, 1.0f, 0.0f};
    char hud[128];

    sceGuStart(GU_DIRECT, display_list);
    sceGuClearColor(current_stage.ground_color ? current_stage.ground_color : 0xFF101820);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_CULL_FACE);
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

    for (i = -4; i <= 4; ++i) {
        draw_cube((float)i * 4.0f, -1.0f, -18.0f, 3.8f);
        draw_cube((float)i * 4.0f, -1.0f, 18.0f, 3.8f);
        draw_cube(-18.0f, -1.0f, (float)i * 4.0f, 3.8f);
        draw_cube(18.0f, -1.0f, (float)i * 4.0f, 3.8f);
    }

    draw_model(player_model, player_texture, player_x, 0.0f, player_z, 1.0f);
    for (i = 0; i < GAME_ENEMY_MAX; ++i) {
        if (enemies[i].active && enemies[i].state != ENEMY_DEAD)
            draw_model(enemy_model, enemy_texture, enemies[i].x, 0.0f, enemies[i].z, enemies[i].boss ? 1.7f : 1.2f);
    }
    if (aiming) draw_aim_line();

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_TEXTURE_2D);
    (void)snprintf(hud, sizeof(hud), "الصحة %d - النقاط %d - الذخيرة %d/%d - المال %d",
                   hp, score, ammo, reserve_ammo, player_state.money);
    arabicDrawTextRTL(472.0f, 8.0f, 0.72f, 0xFFFFFFFF, hud);
    arabicDrawTextRTL(472.0f, 52.0f, 0.68f, 0xFFFFD080, current_stage.name);
    arabicDrawTextRTL(472.0f, 30.0f, 0.62f, 0xFFB8D8FF,
                      "L تصويب | مربع تلقيم | مثلث علاج | دائرة شراء ذخيرة");
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
            if (pressed(pad.Buttons, PSP_CTRL_START)) paused = 0;
            if (pressed(pad.Buttons, PSP_CTRL_CIRCLE)) {
                game_started = 0;
                paused = 0;
            }
            if (pressed(pad.Buttons, PSP_CTRL_TRIANGLE)) save_current_game();
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
