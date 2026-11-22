#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <psprtc.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "obj_loader.h"
#include "texture.h"
#include "savedata.h"

PSP_MODULE_INFO("RE4 Proto", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272
#define PI_F 3.14159265358979323846f
#define ARENA_LIMIT 18.0f

typedef struct {
    unsigned int color;
    float x, y, z;
} ColorVertex;

static unsigned int __attribute__((aligned(16))) display_list[262144];
static float player_x = 0.0f, player_z = 0.0f, player_angle = 0.0f;
static int hp = 100, score = 0, ammo = 10, reserve_ammo = 40;
static int game_started = 0, paused = 0;
static int target_active[4] = {1, 1, 1, 1};
static float target_x[4] = {-7.0f, 7.0f, -7.0f, 7.0f};
static float target_z[4] = {-8.0f, -8.0f, 8.0f, 8.0f};
static ObjModel *player_model = NULL, *enemy_model = NULL;
static Texture *player_texture = NULL, *enemy_texture = NULL;

static const ColorVertex cube_vertices[] = {
    {0xFF4D8C35, -0.5f,-0.5f, 0.5f},{0xFF4D8C35, 0.5f,-0.5f, 0.5f},{0xFF4D8C35, 0.5f,0.5f,0.5f},
    {0xFF4D8C35, -0.5f,-0.5f, 0.5f},{0xFF4D8C35, 0.5f,0.5f,0.5f},{0xFF4D8C35, -0.5f,0.5f,0.5f},
    {0xFF356A9A, 0.5f,-0.5f,-0.5f},{0xFF356A9A, -0.5f,-0.5f,-0.5f},{0xFF356A9A, -0.5f,0.5f,-0.5f},
    {0xFF356A9A, 0.5f,-0.5f,-0.5f},{0xFF356A9A, -0.5f,0.5f,-0.5f},{0xFF356A9A, 0.5f,0.5f,-0.5f},
    {0xFF9A5B35, -0.5f,-0.5f,-0.5f},{0xFF9A5B35, -0.5f,-0.5f,0.5f},{0xFF9A5B35, -0.5f,0.5f,0.5f},
    {0xFF9A5B35, -0.5f,-0.5f,-0.5f},{0xFF9A5B35, -0.5f,0.5f,0.5f},{0xFF9A5B35, -0.5f,0.5f,-0.5f},
    {0xFF8A7130, 0.5f,-0.5f,0.5f},{0xFF8A7130, 0.5f,-0.5f,-0.5f},{0xFF8A7130, 0.5f,0.5f,-0.5f},
    {0xFF8A7130, 0.5f,-0.5f,0.5f},{0xFF8A7130, 0.5f,0.5f,-0.5f},{0xFF8A7130, 0.5f,0.5f,0.5f},
    {0xFF666666, -0.5f,0.5f,0.5f},{0xFF666666, 0.5f,0.5f,0.5f},{0xFF666666, 0.5f,0.5f,-0.5f},
    {0xFF666666, -0.5f,0.5f,0.5f},{0xFF666666, 0.5f,0.5f,-0.5f},{0xFF666666, -0.5f,0.5f,-0.5f},
    {0xFF444444, -0.5f,-0.5f,-0.5f},{0xFF444444, 0.5f,-0.5f,-0.5f},{0xFF444444, 0.5f,-0.5f,0.5f},
    {0xFF444444, -0.5f,-0.5f,-0.5f},{0xFF444444, 0.5f,-0.5f,0.5f},{0xFF444444, -0.5f,-0.5f,0.5f}
};

static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1; (void)arg2; (void)common;
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp) {
    int cbid;
    (void)args; (void)argp;
    cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    if (cbid >= 0) sce月KernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void) {
    SceUID thid = sceKernelCreateThread("callback_thread", callback_thread, 0x11, 0x1000, PSP_THREAD_ATTR_USER, NULL);
    if (thid >= 0) sceKernelStartThread(thid, 0, NULL);
}

static void draw_cube(float x, float y, float z, float scale, unsigned int tint) {
    ScePspFVector3 pos = {x, y, z};
    ScePspFVector3 scl = {scale, scale, scale};
    (void)tint;
    sceGumPushMatrix();
    sceGumTranslate(&pos);
    sceGumScale(&scl);
    sceGumDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
                    36, NULL, cube_vertices);
    sceGumPopMatrix();
}

static void reset_game(void) {
    int i;
    player_x = 0.0f; player_z = 0.0f; player_angle = 0.0f;
    hp = 100; score = 0; ammo = 10; reserve_ammo = 40;
    for (i = 0; i < 4; ++i) target_active[i] = 1;
    paused = 0; game_started = 1;
}

static void load_assets(void) {
    player_model = objLoad("assets/player.obj");
    enemy_model = objLoad("assets/enemy.obj");
    player_texture = loadTexturePNG("assets/player.png");
    enemy_texture = loadTexturePNG("assets/enemy.png");
}

static void update_game(void) {
    SceCtrlData pad;
    float move = 0.0f;
    sceCtrlPeekBufferPositive(&pad, 1);
    if (pad.Buttons & PSP_CTRL_START) { paused = !paused; sceKernelDelayThread(180000); }
    if (paused) return;
    if (pad.Buttons & PSP_CTRL_LEFT) player_angle -= 0.045f;
    if (pad.Buttons & PSP_CTRL_RIGHT) player_angle += 0.045f;
    if (pad.Buttons & PSP_CTRL_UP) move = 0.16f;
    if (pad.Buttons & PSP_CTRL_DOWN) move = -0.16f;
    if (pad.Lx > 150) player_angle += 0.035f;
    if (pad.Lx < 105) player_angle -= 0.035f;
    if (pad.Ly < 110) move = 0.13f;
    if (pad.Ly > 145) move = -0.13f;
    player_x += sinf(player_angle) * move;
    player_z += cosf(player_angle) * move;
    if (player_x > ARENA_LIMIT) player_x = ARENA_LIMIT;
    if (player_x < -ARENA_LIMIT) player_x = -ARENA_LIMIT;
    if (player_z > ARENA_LIMIT) player_z = ARENA_LIMIT;
    if (player_z < -ARENA_LIMIT) player_z = -ARENA_LIMIT;
    if (pad.Buttons & PSP_CTRL_SQUARE) {
        int need = 10 - ammo;
        if (need > reserve_ammo) need = reserve_ammo;
        if (need > 0) { ammo += need; reserve_ammo -= need; playSound(SFX_RELOAD); }
        sceKernelDelayThread(180000);
    }
    if (pad.Buttons & PSP_CTRL_CROSS) {
        int i, hit = 0;
        if (ammo > 0) {
            --ammo; playSound(SFX_GUNSHOT);
            for (i = 0; i < 4; ++i) {
                float dx = target_x[i] - player_x, dz = target_z[i] - player_z;
                float distance = sqrtf(dx * dx + dz * dz);
                float target_angle = atan2f(dx, dz);
                float diff = target_angle - player_angle;
                while (diff > PI_F) diff -= 2.0f * PI_F;
                while (diff < -PI_F) diff += 2.0f * PI_F;
                if (target_active[i] && distance < 14.0f && fabsf(diff) < 0.24f) {
                    target_active[i] = 0; score += 100; hit = 1; break;
                }
            }
            if (hit) playSound(SFX_HIT);
        } else playSound(SFX_EMPTY);
        sceKernelDelayThread(160000);
    }
    if (pad.Buttons & PSP_CTRL_SELECT) {
        GameState s;
        s.player_x = player_x; s.player_z = player_z; s.player_angle = player_angle;
        s.hp = hp; s.score = score; s.ammo = ammo; s.reserve_ammo = reserve_ammo;
        saveGame(&s);
        sceKernelDelayThread(200000);
    }
}

static void render_scene(void) {
    int i;
    ScePspFVector3 cam = {player_x - sinf(player_angle) * 7.0f, 5.0f, player_z - cosf(player_angle) * 7.0f};
    ScePspFVector3 target = {player_x, 1.0f, player_z};
    ScePspFVector3 up = {0.0f, 1.0f, 0.0f};
    sceGuStart(GU_DIRECT, display_list);
    sceGuClearColor(0xFF101820);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuShadeModel(GU_SMOOTH);
    sceGuDisable(GU_TEXTURE_2D);
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(55.0f, 16.0f / 9.0f, 0.5f, 100.0f);
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    sceGumLookAt(&cam, &target, &up);
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    for (i = -4; i <= 4; ++i) {
        draw_cube((float)i * 4.0f, -1.0f, -18.0f, 3.8f, 0);
        draw_cube((float)i * 4.0f, -1.0f, 18.0f, 3.8f, 0);
        draw_cube(-18.0f, -1.0f, (float)i * 4.0f, 3.8f, 0);
        draw_cube(18.0f, -1.0f, (float)i * 4.0f, 3.8f, 0);
    }
    draw_cube(player_x, 0.0f, player_z, 1.0f, 0);
    for (i = 0; i < 4; ++i) {
        if (target_active[i]) {
            if (enemy_model) { bindTexture(enemy_texture); objDraw(enemy_model); }
            else draw_cube(target_x[i], 0.0f, target_z[i], 1.2f, 0);
        }
    }
    sceGuDisable(GU_DEPTH_TEST);
    sceGuFinish();
    sceGuSync(0, 0);
    sceGuSwapBuffers();
}

int main(void) {
    int running = 1;
    setup_callbacks();
    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
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
    sceGuFinish(); sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    initAudio();
    load_assets();

    while (running) {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(&pad, 1);
        if (!game_started) {
            pspDebugScreenSetXY(7, 5);
            pspDebugScreenPrintf("RE4 PROTO - PSP\n\n");
            pspDebugScreenPrintf("X: START GAME\n");
            pspDebugScreenPrintf("O: EXIT\n");
            pspDebugScreenPrintf("\nPrototype build - placeholder assets included.\n");
            if (pad.Buttons & PSP_CTRL_CROSS) { reset_game(); sceKernelDelayThread(200000); }
            if (pad.Buttons & PSP_CTRL_CIRCLE) running = 0;
            sceDisplayWaitVblankStart();
        } else if (paused) {
            pspDebugScreenSetXY(8, 7);
            pspDebugScreenPrintf("PAUSED\n\nSTART: RESUME\nO: MAIN MENU\n");
            if (pad.Buttons & PSP_CTRL_CIRCLE) { game_started = 0; paused = 0; sceKernelDelayThread(200000); }
            if (pad.Buttons & PSP_CTRL_START) { paused = 0; sceKernelDelayThread(200000); }
            sceDisplayWaitVblankStart();
        } else {
            update_game();
            render_scene();
            pspDebugScreenSetXY(0, 0);
            pspDebugScreenPrintf("HP:%d  SCORE:%d  AMMO:%d/%d  SELECT:SAVE  START:PAUSE",
                                 hp, score, ammo, reserve_ammo);
            sceDisplayWaitVblankStart();
        }
        if (pad.Buttons & PSP_CTRL_HOME) running = 0;
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
