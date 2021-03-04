#include "videoCapture.h"
#include <string.h>
#include "log.h"
#include "errCode.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "types.h"
#include <sys/time.h>

// @return 当前时间，单位-ms
int64 get_timestamp(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec/1000;
}

void save_yuv(void* start, int length)
{
    int64 timestamp = get_timestamp();
    char fileName[64] = {0};
    int fd = -1;

    snprintf(fileName, sizeof(fileName), "./test/yuv/yuvFrame%lld.yuv", timestamp);
    printf("%s\n", fileName);
    fd = open(fileName, O_RDWR | O_CREAT, 0666);
    if(fd < 0)
        printf("open:%d %s\n", errno, strerror(errno));
    write(fd, start, length);
    close(fd);
}

void start_video_capture(void)
{
    CameraInfo cam;
    int ret = -1;
    int a = 4;

    memset(&cam, 0, sizeof(cam));
    cam.deviceName = "/dev/video0";
    cam.devFd = -1;           
    cam.resInfo.width = 640;
    cam.resInfo.height = 480;
    cam.deal_frame = save_yuv;
    ret = camera_init(&cam);
    if(ERROR == ret)
    {
        DBGLOG("camera_init error\n");
        camera_close(&cam);
        return;
    }
   
    DBGLOG("camera_init-%s:%d [%d x %d]\n", cam.deviceName, cam.devFd, cam.resInfo.width, cam.resInfo.height);
    ret = camera_capture_start(cam.devFd, cam.buffNum);
    if(ERROR == ret)
    {
        DBGLOG("camera_capture_start failed!\n");
        camera_close(&cam);
        return;
    }

    // 缺编码函数
    while(a--)
    {
        camera_read_frame(&cam);
    }
    

    camera_capture_stop(cam.devFd);
    camera_close(&cam);
}