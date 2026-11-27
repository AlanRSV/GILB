/* audio.h -- GILB: the sound effects' mixer (gilb_sound.c; the MG24 port's src/audio.h mixed 8
   channels to PWM). Stereo; no music yet. */
#ifndef SRC_PWM_AUDIO_H_
#define SRC_PWM_AUDIO_H_
#include "main.h"
#define AUDIO_SAMPLE_RATE           11025
#define MAX_CHANNELS                (8)
#define STEREO_AUDIO                1
#define DMX_DATA_SOUND_OFFSET       0x18
typedef struct
{
    uint16_t lastAudioBufferIdx;
    uint16_t offset;
    uint8_t sfxIdx;
    int8_t volumeLeft;
    int8_t volumeRight;
} soundChannel_t;
void initAudio(void);
void updateSound(void);
void muteSound(void);
extern soundChannel_t soundChannels[MAX_CHANNELS];
void gilb_sound_tick(void);          /* a mix if it is due: call it from anywhere, often */
#endif
