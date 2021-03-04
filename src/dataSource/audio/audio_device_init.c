#include <alsa/asoundlib.h>
#include "log.h"
#include "audioCapture.h"
#include "errCode.h"

// 输入参数先不增加，采样率，采样长度，通道数，交错模式，周期都先写死
int audio_init(AudioParam *audioParam)
{ 
    snd_pcm_hw_params_t *params;
    unsigned int sampleRate = 8000;
    snd_pcm_uframes_t frames = 1024;
    int retValue = -1;

    // 摄像头只有麦克，只实现录音功能
    retValue = snd_pcm_open(&(audioParam->handle), "default", SND_PCM_STREAM_CAPTURE, 0);
    if(retValue < 0)
    {
        DBGLOG("can not open pcm device:%s\n", snd_strerror(retValue));
        return ERROR;
    }

    snd_pcm_hw_params_alloca(&params);

    // 使用默认参数
    snd_pcm_hw_params_any(audioParam->handle, params);
    snd_pcm_hw_params_set_access(audioParam->handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);    // 交错访问
    snd_pcm_hw_params_set_format(audioParam->handle, params, SND_PCM_FORMAT_S16_LE);    // 有符号16bit，小端 2bytes
    snd_pcm_hw_params_set_channels(audioParam->handle, params, 1);  // 单声道
    snd_pcm_hw_params_set_rate_near(audioParam->handle, params, &sampleRate, 0);    // 采样率8k
    snd_pcm_hw_params_set_period_size_near(audioParam->handle, params, &frames, 0);

    retValue = snd_pcm_hw_params(audioParam->handle, params);
    if(retValue < 0)
    {
        DBGLOG("set hw params error:%s\n", snd_strerror(retValue));
        return ERROR;
    }

    snd_pcm_hw_params_get_period_size(params, &(audioParam->frames), 0);
    audioParam->buffSize = audioParam->frames * 2; // 16bit = 2bytes / sample , 1 channel
    return OK;
}

int audio_read_pcm(AudioParam *audioParam)
{
    int retValue = -1;

    if(NULL == audioParam || NULL == audioParam->handle || audioParam->frames <= 0)
    {
        DBGLOG("audio_read_pcm input error!\n");
        return ERROR;
    }

    
    audioParam->readBuffer = (char*)malloc(audioParam->buffSize);
    if(NULL == audioParam->readBuffer)
    {
        DBGLOG("audio_read_pcm malloc error!\n");
        return ERROR;
    }
    
    retValue = snd_pcm_readi(audioParam->handle, audioParam->readBuffer, audioParam->frames);
    if(-EPIPE == retValue)
    {
        DBGLOG("overrun occurred!\n");
        snd_pcm_prepare(audioParam->handle);
    }
    else if(retValue < 0)
    {
        DBGLOG("read error: %s\n", snd_strerror(retValue));
        return ERROR;
    }
    else if(retValue != (int)audioParam->frames)
    {
        DBGLOG("read %d less than frames[%d]\n", retValue, (int)audioParam->frames);
        return ERROR;
    }
    else
    {
        audioParam->deal_pcm(audioParam->readBuffer, audioParam->buffSize);
    }
    return OK;
}

void audio_close(AudioParam *audioParam)
{
    if(NULL != audioParam->handle)
    {
        snd_pcm_drain(audioParam->handle);
        snd_pcm_close(audioParam->handle);
    }

    if(NULL != audioParam->readBuffer)
    {
        free(audioParam->readBuffer);
        audioParam->readBuffer = NULL;
    }
}