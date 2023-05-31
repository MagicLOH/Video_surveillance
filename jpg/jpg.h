#ifndef _JPG_H
#define _JPG_H

#include <stdio.h>
#include <jpeglib.h>
#include <stdlib.h>
#include <string.h>

#define OUTPUT_BUF_SIZE 4096
void yuv_to_rgb(unsigned char *yuv_buffer, unsigned char *rgb_buffer, int iWidth, int iHeight);
int rgb_to_jpeg(int Width, int Height, int size, unsigned char *rgb_buffer, unsigned char *jpg_buffer, int quality); // RGB转JPG
// int LCD_ShowJPEG(unsigned char *jpg_buffer,int size,struct ImageDecodingInfo *image_rgb);//JPG解压缩
#endif
