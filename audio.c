#include "audio.h"

#include <pspaudio.h>
#include <math.h>
#include <stdlib.h>

#define AUDIO_SAMPLES 512
#define PI_F 3.14159265358979323846f

static int audio_channel = -1;
static short sample_buffer[AUDIO_SAMPLES] __attribute__((aligned(64)));

void initAudio(void) {
    audio_channel = sceAudioChReserve(-1, AUDIO_SAMPLES, PSP_AUDIO_FORMAT_MONO);
}

void playSound(int sfx_id) {
    int i;
    float frequency, amplitude;
    if (audio_channel < 0) return;
    switch (sfx_id) {
        case SFX_GUNSHOT: frequency = 115.0f; amplitude = 0.72f; break;
        case SFX_HIT:     frequency = 260.0f; amplitude = 0.42f; break;
        case SFX_RELOAD:  frequency = 520.0f; amplitude = 0.25f; break;
        case SFX_EMPTY:   frequency = 180.0f; amplitude = 0.22f; break;
        case SFX_PICKUP:  frequency = 740.0f; amplitude = 0.28f; break;
        case SFX_HURT:    frequency = 90.0f;  amplitude = 0.38f; break;
        default: return;
    }
    for (i = 0; i < AUDIO_SAMPLES; ++i) {
        float envelope = 1.0f - (float)i / (float)AUDIO_SAMPLES;
        float wave = sinf(2.0f * PI_F * frequency * (float)i / 44100.0f);
        sample_buffer[i] = (short)(wave * envelope * amplitude * 32767.0f);
    }
    sceAudioOutputBlocking(audio_channel, PSP_AUDIO_VOLUME_MAX, sample_buffer);
}

void releaseAudio(void) {
    if (audio_channel >= 0) {
        sceAudioChRelease(audio_channel);
        audio_channel = -1;
    }
}
