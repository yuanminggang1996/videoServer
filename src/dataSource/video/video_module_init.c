#include "videoCapture.h"
#include <string.h>
#include "log.h"
#include "errCode.h"

void start_video_capture(void)
{
    CameraInfo cam;
    int ret = -1;

    memset(&cam, 0, sizeof(cam));
    cam.deviceName = "/dev/video0";
    cam.devFd = -1;           
    cam.resInfo.width = 640;
    cam.resInfo.height = 480;
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

    camera_capture_stop(cam.devFd);
    camera_close(&cam);
}