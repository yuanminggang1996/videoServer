#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <alsa/asoundlib.h>

typedef struct
{
    snd_pcm_t *handle;
    snd_pcm_uframes_t frames;
    int buffSize;
    char* readBuffer;
    void (*deal_pcm)(void*,int);
}AudioParam;

int audio_init(AudioParam *audioParam);
int audio_read_pcm(AudioParam *audioParam);
void audio_close(AudioParam *audioParam);
void start_audio_capture(void);
#endif