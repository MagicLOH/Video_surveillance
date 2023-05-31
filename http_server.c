#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>

#include "camera/camera.h"
#include "freetype/freetype.h"
#include "jpg/jpg.h"

#define errEXIT(msg)        \
    do {                    \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

#define SERVER_PORT (8080)
#define SERVER_IP ("192.168.23.139")

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;  // 静态定义互斥锁
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;     // 静态定义条件变量
struct _CAMERA_INFO camera_info;
int width, height;
static char *image_buffer;  // 传输图像缓冲区
static int image_size;      // 传输图像大小

/**
 * @brief HTTP服务器发送图像函数
 * 
 * @param sockfd 
 * @return int 
 */
int HTTP_SendImage(int sockfd) {
    char buff[1024] =
        "HTTP/1.0 200 OK\r\n"
        "Server: toyz\r\n"
        "Content-Type:multipart/x-mixed-replace;boundary=boundarydonotcross\r\n"
        "\r\n"
        "--boundarydonotcross\r\n";
    /* 建立长连接 */
    ssize_t wr_s = write(sockfd, buff, strlen(buff));
    if (wr_s != strlen(buff)) {
        printf("发送报文失败!\n");
        return -1;
    }
    memset(buff, 0, sizeof(buff));

    /* 申请JPG图片动态缓冲区 */
    char *jpg_buffer = malloc(height * width * 3);
    assert(jpg_buffer != NULL);
    int jpg_size = 0;

    while (1) {
        pthread_mutex_lock(&mutex);       /* 上锁 */
        pthread_cond_wait(&cond, &mutex); /* 等待条件变量产生 */
        /* 获取摄像头采集数据 */
        jpg_size = image_size;
        memcpy(jpg_buffer, image_buffer, jpg_size);
        pthread_mutex_unlock(&mutex); /* 解锁 */

        /* 发送报文头 */
        snprintf(buff, sizeof(buff),
                 "Content-type:image/jpeg\r\n"
                 "Content-Length:%d\r\n"
                 "\r\n",
                 jpg_size);
        wr_s = write(sockfd, buff, strlen(buff));
        if (wr_s != strlen(buff)) {
            printf("发送报文头失败!\n");
            return -2;
        }
        /* 发送实体内容 */
        wr_s = write(sockfd, jpg_buffer, jpg_size);
        if (wr_s != jpg_size) {
            printf("发送采集数据失败!\n");
            return -3;
        }
        /* 发送结尾符 */
        memset(buff, 0, sizeof(buff));
        strcpy(buff, "\r\n--boundarydonotcross\r\n");
        wr_s = write(sockfd, buff, strlen(buff));
        if (wr_s != strlen(buff)) {
            printf("发送结尾符!\n");
            return -4;
        }
    }
    free(jpg_buffer);
    return 0;
}

/**
 * @brief 发送HTTP报文函数
 * 
 * @param sockfd 
 * @param type 
 * @param file_path 
 * @return int 
 */
int HTTP_SendContent(int sockfd, char *type, const char *file_path) {
    int fd = open(file_path, O_RDONLY);
    if (-1 == fd)
        errEXIT("文件打开失败");

    char buff[1024];
    /* 响应报文头 */
    struct stat statbuf;
    fstat(fd, &statbuf);  // 获取文件信息
    snprintf(buff, sizeof(buff),
             "HTTP/1.1 200 OK\r\n"
             "Content-type:%s\r\n"
             "Content-Length:%ld\r\n"
             "\r\n",
             type, statbuf.st_size);
    ssize_t wr_s = write(sockfd, buff, strlen(buff));
    if (wr_s != strlen(buff)) {
        printf("wr_s=%ld响应报文数据失败!\n", wr_s);
        return -1;
    }
    memset(buff, 0, sizeof(buff));

    ssize_t rd_s = 0;
    while (1) {
        rd_s = read(fd, buff, sizeof(buff));
        // printf("读取到文件内容%ldbyte,正在发送给客户端\n", rd_s);
        write(sockfd, buff, rd_s);
        if (rd_s != sizeof(buff))
            break;
    }

    printf("close(fd=%d)\n", fd);
    close(fd);
    return 0;
}

/**
 * @brief HTTP客户端服务线程
 * 
 * @param arg 
 * @return void* 
 */
void *http_thread(void *arg) {
    int c_fd = *(int *)arg;
    free(arg);
    printf("This is [c_fd=%d] pthread!\n", c_fd);

    char rxbuff[1024];
    ssize_t rd_s = read(c_fd, rxbuff, sizeof(rxbuff) - 1);
    if (rd_s <= 0) {
        printf("[c_fd=%d]客户端断开连接...\n", c_fd);
        printf("close(c_fd=%d)\n", c_fd);
        close(c_fd);
        pthread_exit(NULL);
    }
    rxbuff[rd_s] = '\0';
    printf("[c_fd=%d]收到HTTP数据!\n rxbuff:%s\n", c_fd, rxbuff);

    if (strstr(rxbuff, "GET / HTTP/1.1")) {
        HTTP_SendContent(c_fd, "text/html", "./http/image.html");
    } else if (strstr(rxbuff, "GET /favicon.ico HTTP/1.1")) {
        HTTP_SendContent(c_fd, "image/x-icon", "./http/TheLegendOfZelda.ico");
    } else if (strstr(rxbuff, "GET /1.jpg HTTP/1.1")) {
        HTTP_SendImage(c_fd);
    }

    printf("close(c_fd=%d)\n", c_fd);
    close(c_fd);
    pthread_exit(NULL);
}

/**
 * @brief 摄像头捕获线程
 * 
 * @param arg 
 * @return void* 
 */
void *Camera_CaptureThraed(void *arg) {
    int camera_fd = *(int *)arg;
    printf("This is Camera capture thread!\n");
    if (camera_fd < 0) {
        printf("camera_fd出错!\n");
        close(camera_fd);
        pthread_exit(NULL);
    }
    if (InitConfig_FreeType("freetype/simkai.ttf")) {
        printf("打开矢量字库失败\n");
        pthread_exit(NULL);
    }

    struct v4l2_buffer v4l2buf;
    memset(&v4l2buf, 0, sizeof(v4l2buf));
    v4l2buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  // 视频捕获格式
    v4l2buf.memory = V4L2_MEMORY_MMAP;           // 内存映射

    unsigned char *rgb_buffer = malloc(height * width * 3);
    unsigned char *jpeg_buffer = malloc(height * width * 3);
    int jepg_size = 0;
    static time_t sec1, sec2;
    static struct tm result;
    static char time_s[100];
    static wchar_t WaterMarkbuf[200];
    while (1) {
        if (ioctl(camera_fd, VIDIOC_DQBUF, &v4l2buf)) {
            printf("摄像头采集数据失败!\n");
            break;
        }
        // printf("camera_buffer[%d]'s mmap addr=%p\n",
        //        v4l2buf.index, camera_info.camera_buffer[v4l2buf.index]);
        /* YUYV转RGB */
        yuv_to_rgb(camera_info.camera_buffer[v4l2buf.index], rgb_buffer, width, height);
        // 获取秒单位时间
        sec1 = time(NULL);
        if (sec2 != sec1) {
            sec2 = sec1;
            localtime_r(&sec2, &result);
            /*格式化打印*/
            strftime(time_s, sizeof(time_s), "%Y/%m/%d %H:%M:%S", &result);
            CharToWchar(time_s, WaterMarkbuf);
            // printf("时间:%s\n",time_s);
        }
        LCD_DrawText(10, 10, 32, WaterMarkbuf, rgb_buffer);  // 添加时间水印
        /* 压缩jepg格式 */
        jepg_size = rgb_to_jpeg(width, height, width * height * 3, rgb_buffer, jpeg_buffer, 100);

        pthread_mutex_lock(&mutex);                     /* 上锁 */
        image_size = jepg_size;                         // 更新采集图像大小
        memcpy(image_buffer, jpeg_buffer, image_size);  // 拷贝压缩后的jepg缓冲区数据
        pthread_cond_broadcast(&cond);                  // 广播唤醒所有采集线程
        pthread_mutex_unlock(&mutex);                   /* 解锁 */

        /* 将缓冲区添加回采集队列 */
        if (ioctl(camera_fd, VIDIOC_QBUF, &v4l2buf)) {
            printf("缓冲区添加回采集队列失败!\n");
            break;
        }
    }
    close(camera_fd);
    free(image_buffer);
    free(rgb_buffer);
    free(jpeg_buffer);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    memset(&camera_info, 0, sizeof(camera_info));
    int camera_fd = Camera_Init(&camera_info);
    if (camera_fd < 0) {
        printf("[camera_fd=%d]摄像头初始化失败\n", camera_fd);
        return camera_fd;
    }
    printf("设置图像大小:%d*%d\n", camera_info.width, camera_info.height);
    width = camera_info.width;
    height = camera_info.height;
    image_buffer = (char *)malloc(width * height * 3);
    assert(image_buffer != NULL);

    signal(SIGPIPE, SIG_IGN);  // 忽略管道破裂信号
    /* 创建摄像头捕获线程 */
    pthread_t pthid;
    pthread_create(&pthid, NULL, Camera_CaptureThraed, (void *)&camera_fd);
    pthread_detach(pthid);

    /* 开始搭建TCP服务端 */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        errEXIT("创建网络套接字失败");

    /* 允许使用已绑定的端口号 */
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in s_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SERVER_PORT),
        .sin_addr.s_addr = inet_addr(SERVER_IP),
    };
    if (bind(sockfd, (struct sockaddr *)&s_addr, sizeof(s_addr)))
        errEXIT("绑定端口号失败");
    /* 设置最大监听数量 */
    listen(sockfd, 100);
    printf("服务器创建成功,等待客户端连接...\n");

    struct sockaddr_in c_addr;
    socklen_t addrlen = sizeof(c_addr);
    int c_fd;
    while (1) {
        /* 阻塞接收新连接客户端请求 */
        c_fd = accept(sockfd, (struct sockaddr *)&c_addr, &addrlen);
        if (-1 == c_fd)
            errEXIT("客户端连接失败");
        printf("[c_fd=%d]客户端连接成功! IP:%s,Port:%d\n",
               c_fd, inet_ntoa(c_addr.sin_addr), ntohs(c_addr.sin_port));

        int *ptr = malloc(sizeof(int)); /* 保证每次连接的客户端fd地址不同 */
        assert(ptr != NULL);
        printf("动态分配空间成功! ptr=%p\n", ptr);
        *ptr = c_fd;
        /* 创建http传输线程 */
        pthread_create(&pthid, NULL, http_thread, ptr);
        pthread_detach(pthid);
    }
}
