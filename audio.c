#include "audio.h"

#include <pspaudio.h>
#include <math.h>
#include <string.h>

#define AUDIO_SAMPLES 512
#define AUDIO_VOICES 4
#define PI_F 3.14159265358979323846f

static int g_channels[AUDIO_VOICES];
static short g_voice_buffers[AUDIO_VOICES][AUDIO_SAMPLES] __attribute__((aligned(64)));
static short g_clips[NUM_SFX][AUDIO_SAMPLES] __attribute__((aligned(64)));
static int g_next_voice;
static int g_ready;

static void generate_clips(void) {
    static const float frequencies[NUM_SFX] = {
        115.0f, 260.0f, 520.0f, 180.0f, 740.0f, 90.0f, 330.0f, 62.0f
    };
    static const float amplitudes[NUM_SFX] = {
        0.78f, 0.42f, 0.25f, 0.22f, 0.28f, 0.38f, 0.24f, 0.48f
    };
    int s, i;
    for (s = 0; s < NUM_SFX; ++s) {
        unsigned int noise = 0x13579BDFu + (unsigned int)(s * 7919);
        for (i = 0; i < AUDIO_SAMPLES; ++i) {
            float t = (float)i / 44100.0f;
            float envelope = 1.0f - (float)i / (float)AUDIO_SAMPLES;
            float wave = sinf(2.0f * PI_F * frequencies[s] * t);
            if (s == SFX_GUNSHOT || s == SFX_HIT || s == SFX_HURT || s == SFX_BOSS) {
                float random;
                noise = noise * 1664525u + 1013904223u;
                random = ((float)((noise >> 16) & 0x7FFFu) / 16384.0f) - 1.0f;
                wave = wave * 0.58f + random * (s == SFX_GUNSHOT ? 0.42f : 0.18f);
            }
            g_clips[s][i] = (short)(wave * envelope * amplitudes[s] * 32767.0f);
        }
    }
}

void initAudio(void) {
    int i;
    if (g_ready) return;
    for (i = 0; i < AUDIO_VOICES; ++i) {
        g_channels[i] = sceAudioChReserve(-1, AUDIO_SAMPLES, PSP_AUDIO_FORMAT_MONO);
        if (g_channels[i] < 0) g_channels[i] = -1;
    }
    generate_clips();
    g_next_voice = 0;
    g_ready = 1;
}

void playSound(int sfx_id) {
    int attempt;
    if (!g_ready || sfx_id < 0 || sfx_id >= NUM_SFX) return;

    for (attempt = 0; attempt < AUDIO_VOICES; ++attempt) {
        int voice = g_next_voice;
        g_next_voice = (g_next_voice + 1) % AUDIO_VOICES;
        if (g_channels[voice] >= 0) {
            memcpy(g_voice_buffers[voice], g_clips[sfx_id], sizeof(g_voice_buffers[voice]));
            /* Non-blocking output keeps sound effects out of the game-frame critical path. */
            sceAudioOutput(g_channels[voice], PSP_AUDIO_VOLUME_MAX, g_voice_buffers[voice]);
            return;
        }
    }
}

void releaseAudio(void) {
    int i;
    for (i = 0; i < AUDIO_VOICES; ++i) {
        if (g_channels[i] >= 0) {
            sceAudioChRelease(g_channels[i]);
            g_channels[i] = -1;
        }
    }
    g_ready = 0;
}
