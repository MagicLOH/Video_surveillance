#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "camera.h"

/**
 * @brief 摄像头初始化
 *
 * @param [out]camera_info
 * @return int
 */
int Camera_Init(struct _CAMERA_INFO *camera_info) {
    int i = 0;
    /*1.打开摄像头设备*/
    int fd = open(CAMERA_DEV, 2);
    if (fd == -1) {
        perror("摄像头设备打开失败");
        return -1;
    }
    /*2.设置视频捕获格式*/
    struct v4l2_format v4l2fmt;
    memset(&v4l2fmt, 0, sizeof(v4l2fmt));
    v4l2fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  // 视频捕获格式
    v4l2fmt.fmt.pix.width = 640;
    v4l2fmt.fmt.pix.height = 480;
    v4l2fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;  // YUYV 格式
    if (ioctl(fd, VIDIOC_S_FMT, &v4l2fmt)) {
        perror("设置摄像头格式失败");
        return -2;
    }
    camera_info->width = v4l2fmt.fmt.pix.width;
    camera_info->height = v4l2fmt.fmt.pix.height;
    /*帧率*/
    struct v4l2_streamparm v4l2stream;
    memset(&v4l2stream, 0, sizeof(v4l2stream));
    v4l2stream.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  // 视频捕获格式
    if (ioctl(fd, VIDIOC_G_PARM, &v4l2stream)) {
        perror("获取帧率失败");
        return -3;
    }
    printf("帧率:%d fps\n",
           v4l2stream.parm.capture.timeperframe.denominator / v4l2stream.parm.capture.timeperframe.numerator);

    /*3.申请缓冲区*/
    struct v4l2_requestbuffers v4l2reqbuf;
    memset(&v4l2reqbuf, 0, sizeof(v4l2reqbuf));
    v4l2reqbuf.count = 4;                           // 缓冲个数
    v4l2reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  // 视频捕获格式
    v4l2reqbuf.memory = V4L2_MEMORY_MMAP;           // 内存映射
    if (ioctl(fd, VIDIOC_REQBUFS, &v4l2reqbuf)) {
        perror("申请缓冲区失败");
        return -4;
    }
    /*4.将缓冲区与进程空间关联*/
    struct v4l2_buffer v4l2buf;
    memset(&v4l2buf, 0, sizeof(v4l2buf));
    for (i = 0; i < v4l2reqbuf.count; i++) {
        v4l2buf.index = i;
        v4l2buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  // 视频捕获格式
        v4l2buf.memory = V4L2_MEMORY_MMAP;           // 内存映射
        if (ioctl(fd, VIDIOC_QUERYBUF, &v4l2buf)) {
            perror("映射到进程空间失败");
            return -5;
        }
        camera_info->camera_buffer[i] = mmap(NULL, v4l2buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                                             fd, v4l2buf.m.offset);
        if (camera_info->camera_buffer[i] == (void *)-1) {
            perror("内存映射失败");
            return -6;
        }
        printf("camera_buffer[%d]=%p\n", i, camera_info->camera_buffer[i]);
    }
    /*5.将缓冲区添加到采集队列*/
    memset(&v4l2buf, 0, sizeof(v4l2buf));
    for (i = 0; i < v4l2reqbuf.count; i++) {
        v4l2buf.index = i;
        v4l2buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  // 视频捕获格式
        v4l2buf.memory = V4L2_MEMORY_MMAP;           // 内存映射
        if (ioctl(fd, VIDIOC_QBUF, &v4l2buf)) {
            perror("添加到采集队列失败");
            return -7;
        }
    }
    /*7.启动摄像头*/
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  // 视频捕获格式
    if (ioctl(fd, VIDIOC_STREAMON, &type)) {
        perror("启动摄像头设备失败");
        return -8;
    }
    return fd;
}
