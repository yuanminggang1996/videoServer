#ifndef VIDEO_CAPTURE_H
#define VIDEO_CAPTURE_H

// error
#define ERR_GET_INFO 1
#define ERR_

typedef struct resolution
{
    unsigned int width;
    unsigned int height;
}ResInfo;

typedef struct buffer {
	void        *start;
	unsigned int length;
}Buffer;

typedef struct v4l2Camera
{
    char    *deviceName;
    int     devFd;         
    ResInfo resInfo;    // 视频采集分辨率，由摄像头决定，可压缩
    int     buffNum;    // 申请的帧缓冲个数
    Buffer  *buffers;   // 帧缓冲地址   
}CameraInfo;

int camera_init(CameraInfo* cam);
void camera_close(CameraInfo* cam);
int camera_capture_start(int fd, int buffNum);
void camera_capture_stop(int fd);
void start_video_capture(void);
#endif 