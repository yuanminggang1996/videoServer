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


static int camera_ioctl(int fd, int request, void *arg)
{
    int ret = -1;
    do
    {
        ret = ioctl(fd, request, arg);
    } while (ret < 0 && EINTR == errno);

    return ret;
}

/***********************************
 *  @param  deviceName -- camera node
 *  @return  fd -- open success , ERROR -- open failed
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

    // O_NONBLOCK,需阻塞方式打开，使用非阻塞会在取缓冲帧时一直返回EAGAIN
    fd = open(deviceName, O_RDWR, 0);
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
    if(camera_ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0)
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
    if(camera_ioctl(fd, VIDIOC_ENUMINPUT, &input) < 0)
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
    if(camera_ioctl(fd, VIDIOC_QUERYSTD, &std) < 0)
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

    if(camera_ioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
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


static int camera_request_buffer(CameraInfo* cam)
{
    struct v4l2_requestbuffers req;
    struct v4l2_buffer v4l2Buf;
    int i = 0;

    memset(&req, 0, sizeof(req));
    req.count    = 4;   // 内核空间内存，申请4个帧缓冲空间
	req.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory   = V4L2_MEMORY_MMAP;    // 使用mmap

    if(camera_ioctl(cam->devFd, VIDIOC_REQBUFS, &req) < 0)
    {
        DBGLOG("VIDIOC_REQBUFS error:%d %s\n", errno, strerror(errno));
        return ERROR;
    }
    else
    {
        DBGLOG("VIDIOC_REQBUFS:count = %d\n", req.count);
        cam->buffNum = req.count;
    }

    memset(&v4l2Buf, 0, sizeof(v4l2Buf));
	v4l2Buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	v4l2Buf.memory = V4L2_MEMORY_MMAP;

    cam->buffers = calloc(req.count, sizeof(*(cam->buffers)));
	if (!cam->buffers) {
		DBGLOG("calloc failed,Out of memory\n");
		return ERROR;
	}

    for(i = 0; i < req.count; ++i)
    {
        v4l2Buf.index = i;
        if(camera_ioctl(cam->devFd, VIDIOC_QUERYBUF, &v4l2Buf) < 0)
        {
            DBGLOG("VIDIOC_QUERYBUF [%d] error: %d %s\n", i, errno, strerror(errno));
            return ERROR;
        }
        cam->buffers[i].length = v4l2Buf.length;
        cam->buffers[i].start  = mmap(NULL, v4l2Buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, cam->devFd, v4l2Buf.m.offset);
        if(MAP_FAILED == cam->buffers[i].start)
        {
            DBGLOG("buffer[%d] mmap fafiled!", i);
            if(i > 0)
            {
                camera_buffer_release(cam->buffers, i);
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

    if(ERROR == camera_request_buffer(cam))
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
        if(camera_ioctl(fd, VIDIOC_QBUF, &buf) < 0)
        {
            DBGLOG("VIDIOC_QBUF error:%d %s\n", errno, strerror(errno));
            return ERROR;
        }
    }

    if(camera_ioctl(fd, VIDIOC_STREAMON, &type) < 0)
    {
        DBGLOG("VIDIOC_STREAMON error:%d %s\n", errno, strerror(errno));
        return ERROR;
    }

    return OK;
}

void camera_capture_stop(int fd)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(camera_ioctl(fd, VIDIOC_STREAMOFF, &type) < 0)
    {
        DBGLOG("VIDIOC_STREAMOFF error:%d %s\n", errno, strerror(errno));
        return ;
    }
}


int camera_read_frame(CameraInfo* cam)
{
    struct v4l2_buffer buf;

    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if(camera_ioctl(cam->devFd, VIDIOC_DQBUF, &buf) < 0)
    {
        switch (errno)
        {
        case EAGAIN:
            DBGLOG("camera_read_frame EAGAIN\n");
            return OK;
            break;
        case EIO:
            /* 忽略 */
        default:
            return ERROR;
            break;
        }
    }

    cam->deal_frame(cam->buffers[buf.index].start, buf.length);

    if(camera_ioctl(cam->devFd, VIDIOC_QBUF, &buf) < 0)
    {
        DBGLOG("camera_read_frame VIDIOC_QBUF error:%d %s\n", errno, strerror(errno));
        return ERROR;
    }

    return OK;
}