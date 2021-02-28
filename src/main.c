#include <stdio.h>
#include "server.h"
#include "log.h"
#include "videoCapture.h"
int main(void)
{
    DBGLOG("begin to run\n");
    start_video_capture();
    start_video_server();
    return 0;
}