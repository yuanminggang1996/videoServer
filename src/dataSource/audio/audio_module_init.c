#include "log.h"
#include "audioCapture.h"
#include "errCode.h"
#include <alsa/asoundlib.h>
#include <sys/types.h>    
#include <sys/stat.h>  

void save_pcm(void* start, int length)
{
    char fileName[64] = {0};
    int fd = -1;

    snprintf(fileName, sizeof(fileName), "./test/pcm/test.pcm");
    printf("%s\n", fileName);
    fd = open(fileName, O_RDWR | O_CREAT | O_APPEND , 0666);
    if(fd < 0)
        printf("open:%d %s\n", errno, strerror(errno));
    write(fd, start, length);
    close(fd);
}


int get_file_size(void)
{
    struct  stat devStat;
    memset(&devStat, 0, sizeof(devStat));
    stat("./test/pcm/test.pcm", &devStat);
    return devStat.st_size;
}

void start_audio_capture(void)
{
    AudioParam audioParam;
    memset(&audioParam, 0, sizeof(audioParam));
    audioParam.deal_pcm = save_pcm;

    if(audio_init(&audioParam) < 0)
    {
        DBGLOG("audio_init error\n");
        return;
    }

    while(get_file_size() < 50*1024)
    {
        if(audio_read_pcm(&audioParam) < 0)
        {
            DBGLOG("audio_read_pcm error!\n");
            return;
        }
    }
    

    audio_close(&audioParam);
}
