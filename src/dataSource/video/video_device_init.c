#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <dirent.h>
#include <stdio.h>
#include <fcntl.h>				
#include <asm/types.h>		
#include <linux/videodev2.h>
#include "videoCapture.h"
#include "log.h"
#include "errCode.h"


static void camera_buffer_release(Buffer *buffers, int buffNum);

/***********************************
 *  [in]:       deviceName -- camera node
 *  [return]：  fd -- open success , ERROR -- open failed
 ***********************************/
static int camera_open(char* deviceName)
{
    struct  stat devStat;
    int fd = -1;

    memset(&devStat, 0, sizeof(devStat));
    if(stat(deviceName, &devStat) < 0)
    {
       DBGLOG("get device[%s] info failed: %d, %s\n", deviceName, errno, strerror(errno)); 
       return ERROR;
    }

    if(!S_ISCHR(devStat.st_mode))
    {
        DBGLOG("%s is not char device!\n", deviceName);
        return ERROR;
    }

    fd = open(deviceName, O_RDWR | O_NONBLOCK, 0);
    if(fd < 0)
    {
        DBGLOG("cannot open %s:%d, %s\n", deviceName, errno, strerror(errno));
        return ERROR;
    }

    return fd;
}


static int camera_query_cap(int fd)
{
    struct v4l2_capability cap;

    memset(&cap, 0, sizeof(cap));
    if(ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0)
    {
        DBGLOG("VIDIOC_QUERYCAP error: %d %s\n", errno, strerror(errno));
        return ERROR;
    }

   if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
   {
       DBGLOG("device does not support capture!!!\n");
       return ERROR;
   }

   if(!(cap.capabilities & V4L2_CAP_STREAMING))
   {
       DBGLOG("device does not support streaming!!!\n");
       return ERROR;
   }

    DBGLOG("VIDIOC_QUERYCAP\n");
    DBGLOG("driver:%s\n", cap.driver);
    DBGLOG("card:%s\n", cap.card); 
    DBGLOG("bus info:%s\n", cap.bus_info); 
    DBGLOG("version:%u\n", cap.version);            
    DBGLOG("capabilities:%x\n",cap.capabilities);

    return OK;
}

#if 0
// 接口写出，一般用不到
static int camera_enum_input(int fd)
{
    struct v4l2_input input;

    memset(&input, 0, sizeof(input));
    if(ioctl(fd, VIDIOC_ENUMINPUT, &input) < 0)
    {
        DBGLOG("VIDIOC_ENUMINPUT error: %d %s\n", errno, strerror(errno));
        return ERROR;
    }

    DBGLOG("VIDIOC_ENUMINPUT\n");
    DBGLOG("index:%u\n", input.index);
    DBGLOG("name:%s\n", input.name);
    DBGLOG("type:%u\n", input.type);
    DBGLOG("audioset:%u\n", input.audioset);
    DBGLOG("tuner:%u\n", input.tuner);
    DBGLOG("std:%llu\n", input.std);
    DBGLOG("status:%u\n", input.status);

    return OK;
}

// 接口写出，一般用不到
static int camera_query_std(int fd)
{
    v4l2_std_id std = 0;
    if(ioctl(fd, VIDIOC_QUERYSTD, &std) < 0)
    {
        DBGLOG("VIDIOC_QUERYSTD error:%d %s\n", errno, strerror(errno));
        return ERROR;
    }

    DBGLOG("VIDIOC_QUERYSTD\n");
    switch (std)
    {
        case V4L2_STD_NTSC:
        DBGLOG("V4L2_STD_NTSC\n");
        break;

        case V4L2_STD_PAL:
        DBGLOG("V4L2_STD_PAL\n");
        break;

        default:
        break;
    }

    return OK;
}
#endif

static int camera_set_video_fmt(int fd, ResInfo* resInfo)
{
    struct v4l2_format fmt;

    memset(&fmt, 0, sizeof(fmt));
    fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = resInfo->width;
    fmt.fmt.pix.height      = resInfo->height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;    // yuv422
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;    // 隔行扫描

    if(ioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
    {
        DBGLOG("VIDIOC_S_FMT error:%d %s\n", errno, strerror(errno));
        return ERROR;
    }

    // VIDIOC_S_FMT 有可能会改变分辨率,会限制为最大分辨率
    DBGLOG("VIDIOC_S_FMT:%d x %d\n", fmt.fmt.pix.width, fmt.fmt.pix.height);
    resInfo->width  = fmt.fmt.pix.width;
    resInfo->height = fmt.fmt.pix.height;

    return OK;
}


static int camera_request_buffer(int fd, Buffer *buffers, int *buffNum)
{
    struct v4l2_requestbuffers req;
    struct v4l2_buffer v4l2Buf;
    int i = 0;

    memset(&req, 0, sizeof(req));
    req.count    = 4;   // 内核空间内存，申请4个帧缓冲空间
	req.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory   = V4L2_MEMORY_MMAP;    // 使用mmap

    if(ioctl(fd, VIDIOC_REQBUFS, &req) < 0)
    {
        DBGLOG("VIDIOC_REQBUFS error:%d %s\n", errno, strerror(errno));
        return ERROR;
    }
    else
    {
        DBGLOG("VIDIOC_REQBUFS:count = %d\n", req.count);
        *buffNum = req.count;
    }

    memset(&v4l2Buf, 0, sizeof(v4l2Buf));
	v4l2Buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	v4l2Buf.memory = V4L2_MEMORY_MMAP;

    buffers = calloc(req.count, sizeof(*(buffers)));
	if (!buffers) {
		DBGLOG("calloc failed,Out of memory\n");
		return ERROR;
	}

    for(i = 0; i < req.count; ++i)
    {
        v4l2Buf.index = i;
        if(ioctl(fd, VIDIOC_QUERYBUF, &v4l2Buf) < 0)
        {
            DBGLOG("VIDIOC_QUERYBUF [%d] error: %d %s\n", i, errno, strerror(errno));
            return ERROR;
        }
        buffers[i].length = v4l2Buf.length;
        buffers[i].start  = mmap(NULL, v4l2Buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, v4l2Buf.m.offset);
        if(MAP_FAILED == buffers[i].start)
        {
            DBGLOG("buffer[%d] mmap fafiled!", i);
            if(i > 0)
            {
                camera_buffer_release(buffers, i);
            }
            return ERROR;
        }
    }

    return OK;
}

static void camera_buffer_release(Buffer *buffers, int buffNum)
{
    int i = 0;

    if(NULL == buffers)
    {
        return;
    }

    for(i = 0; i < buffNum; ++i)
    {
        munmap(buffers[i].start, buffers[i].length);
    }

    free(buffers);
    buffers = NULL;
}

int camera_init(CameraInfo* cam)
{
    cam->devFd = camera_open(cam->deviceName);
    if(cam->devFd < 0)
    {
        DBGLOG("open %s failed!!\n", cam->deviceName);
        return ERROR;
    }

    if(ERROR == camera_query_cap(cam->devFd))
    {
        DBGLOG("camera_query_cap failed!\n");
        return ERROR;
    }

    if(ERROR == camera_set_video_fmt(cam->devFd, &(cam->resInfo)))
    {
        DBGLOG("camera_set_video_fmt failed!\n");
        return ERROR;
    }

    if(ERROR == camera_request_buffer(cam->devFd, cam->buffers, &(cam->buffNum)))
    {
        cam->buffNum = 0;
        DBGLOG("camera_set_video_fmt failed!\n");
        return ERROR;
    }

    return OK;
}

void camera_close(CameraInfo* cam)
{
    if(cam->buffNum > 0)
    {
        camera_buffer_release(cam->buffers, cam->buffNum);
        cam->buffNum = 0;
    }
    
    if(cam->devFd > 0)
    {
        close(cam->devFd);
        cam->devFd = -1;
    }
}


int camera_capture_start(int fd, int buffNum)
{
    int i = 0;
    struct v4l2_buffer buf;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    for(i = 0; i < buffNum; ++i)
    {
        buf.index = i;
        if(ioctl(fd, VIDIOC_QBUF, &buf) < 0)
        {
            DBGLOG("VIDIOC_QBUF error:%d %s\n", errno, strerror(errno));
            return ERROR;
        }
    }

    if(ioctl(fd, VIDIOC_STREAMON, &type) < 0)
    {
        DBGLOG("VIDIOC_STREAMON error:%d %s\n", errno, strerror(errno));
        return ERROR;
    }

    return OK;
}

void camera_capture_stop(int fd)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(fd, VIDIOC_STREAMOFF, &type) < 0)
    {
        DBGLOG("VIDIOC_STREAMOFF error:%d %s\n", errno, strerror(errno));
        return ;
    }
}

