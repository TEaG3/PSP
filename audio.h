#ifndef AUDIO_H
#define AUDIO_H

#define SFX_GUNSHOT 0
#define SFX_HIT     1
#define SFX_RELOAD  2
#define SFX_EMPTY   3
#define SFX_PICKUP  4
#define SFX_HURT    5
#define SFX_STAGE   6
#define SFX_BOSS    7
#define NUM_SFX     8

void initAudio(void);
void playSound(int sfx_id);
void releaseAudio(void);

#endif
