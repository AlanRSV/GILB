/* audio.h -- GILB: no sound yet (the MG24 port's src/audio.h mixed 8 channels to PWM). */
#ifndef SRC_PWM_AUDIO_H_
#define SRC_PWM_AUDIO_H_
#include "main.h"
#define AUDIO_SAMPLE_RATE           11025
#define MAX_CHANNELS                (8)
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
#endif
