#ifndef _CAMERA_H
#define _CAMERA_H

#include <linux/videodev2.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>

#define CAMERA_DEV "/dev/video0"
struct _CAMERA_INFO {
    int width;                        // 图像宽
    int height;                       // 图像高
    unsigned char *camera_buffer[4];  // 缓冲区
};
int Camera_Init(struct _CAMERA_INFO *camera_info);  // 摄像头初始化

#endif
