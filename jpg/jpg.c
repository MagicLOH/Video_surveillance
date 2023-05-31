#include "jpg.h"

/*YUYV转RGB888*/
void yuv_to_rgb(unsigned char *yuv_buffer, unsigned char *rgb_buffer, int iWidth, int iHeight) {
    int x;
    int z = 0;
    unsigned char *ptr = rgb_buffer;
    unsigned char *yuyv = yuv_buffer;
    for (x = 0; x < iWidth * iHeight; x++) {
        int r, g, b;
        int y, u, v;
        if (!z)
            y = yuyv[0] << 8;
        else
            y = yuyv[2] << 8;
        u = yuyv[1] - 128;
        v = yuyv[3] - 128;
        b = (y + (359 * v)) >> 8;
        g = (y - (88 * u) - (183 * v)) >> 8;
        r = (y + (454 * u)) >> 8;
        *(ptr++) = (b > 255) ? 255 : ((b < 0) ? 0 : b);
        *(ptr++) = (g > 255) ? 255 : ((g < 0) ? 0 : g);
        *(ptr++) = (r > 255) ? 255 : ((r < 0) ? 0 : r);
        if (z++) {
            z = 0;
            yuyv += 4;
        }
    }
}
typedef struct
{
    struct jpeg_destination_mgr pub; /* public fields */
    JOCTET *buffer;                  /* start of buffer */
    unsigned char *outbuffer;
    int outbuffer_size;
    unsigned char *outbuffer_cursor;
    int *written;

} mjpg_destination_mgr;
typedef mjpg_destination_mgr *mjpg_dest_ptr;
/******************************************************************************
函数功能: 初始化输出的目的地
******************************************************************************/
METHODDEF(void)
init_destination(j_compress_ptr cinfo) {
    mjpg_dest_ptr dest = (mjpg_dest_ptr)cinfo->dest;

    /* Allocate the output buffer --- it will be released when done with image */
    dest->buffer = (JOCTET *)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_IMAGE, OUTPUT_BUF_SIZE * sizeof(JOCTET));

    *(dest->written) = 0;

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

/******************************************************************************
函数功能: 当jpeg缓冲区填满时调用
******************************************************************************/
METHODDEF(boolean)
empty_output_buffer(j_compress_ptr cinfo) {
    mjpg_dest_ptr dest = (mjpg_dest_ptr)cinfo->dest;

    memcpy(dest->outbuffer_cursor, dest->buffer, OUTPUT_BUF_SIZE);
    dest->outbuffer_cursor += OUTPUT_BUF_SIZE;
    *(dest->written) += OUTPUT_BUF_SIZE;

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

    return TRUE;
}

/******************************************************************************
函数功能:在写入所有数据之后，由jpeg_finish_compress调用。通常需要刷新缓冲区。
******************************************************************************/
METHODDEF(void)
term_destination(j_compress_ptr cinfo) {
    mjpg_dest_ptr dest = (mjpg_dest_ptr)cinfo->dest;
    size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

    /* Write any data remaining in the buffer */
    memcpy(dest->outbuffer_cursor, dest->buffer, datacount);
    dest->outbuffer_cursor += datacount;
    *(dest->written) += datacount;
}

/******************************************************************************
功能描述：初始化输出流
函数参数:
    j_compress_ptr cinfo  :保存JPG图像压缩信息的结构体地址
    unsigned char *buffer :存放压缩之后的JPG图片的缓冲区首地址
    int size              :源图像字节总大小
    int *written          :存放压缩之后的JPG图像字节大小
******************************************************************************/
GLOBAL(void)
dest_buffer(j_compress_ptr cinfo, unsigned char *buffer, int size, int *written) {
    mjpg_dest_ptr dest;

    if (cinfo->dest == NULL) {
        cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(mjpg_destination_mgr));
    }

    dest = (mjpg_dest_ptr)cinfo->dest;
    dest->pub.init_destination = init_destination;
    dest->pub.empty_output_buffer = empty_output_buffer;
    dest->pub.term_destination = term_destination;
    dest->outbuffer = buffer;
    dest->outbuffer_size = size;
    dest->outbuffer_cursor = buffer;
    dest->written = written;
}
/************************************************
功能描述：将RGB格式的数据转为JPG格式。
函数参数:
  int Width    源图像宽度
  int Height   源图像高度
  int size   	 源图像字节总大小
  unsigned char *rgb_buffer :存放YUV源图像数据缓冲区的首地址
  unsigned char *jpg_buffer :存放转换之后的JPG格式数据缓冲区首地址
  int quality               :jpg图像的压缩质量(值越大质量越好,图片就越清晰,占用的内存也就越大)
              一般取值范围是: 10 ~ 100 。 填10图片就有些模糊了，一般的JPG图片都是质量都是80。
返回值：压缩之后的JPG图像大小
**************************************************************/
int rgb_to_jpeg(int Width, int Height, int size, unsigned char *rgb_buffer, unsigned char *jpg_buffer, int quality) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    static int written;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    /* 原版jpeglib库的标准输出初始化函数,只能填文件指针: jpeg_stdio_dest (&cinfo, file); */
    /* 修改之后的标准输出初始化函数,将输出指向内存空间*/
    dest_buffer(&cinfo, jpg_buffer, size, &written);
    cinfo.image_width = Width;
    cinfo.image_height = Height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    /*2. YUV转RGB格式*/

    while (cinfo.next_scanline < Height) {

        /*3.进行JPG图像压缩(一行一行压缩)*/
        row_pointer[0] = &rgb_buffer[cinfo.next_scanline * Width * 3];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    /*4. 释放压缩时占用的内存空间*/
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    /*5. 返回压缩之后JPG图片大小*/
    return (written);
}

#include <setjmp.h>
struct my_error_mgr {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};
typedef struct my_error_mgr *my_error_ptr;
/*错误处理函数*/
METHODDEF(void)
my_error_exit(j_common_ptr cinfo) {
    my_error_ptr myerr = (my_error_ptr)cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(myerr->setjmp_buffer, 1);
}
#if 0
/*

//JPEG图像解码 编译时加-ljpeg
//图片结构体参数(需要自行定义)
struct ImageDecodingInfo
{
	unsigned int  Width;   //宽度
	unsigned int  Height;	//高度
	unsigned char *rgb;     //RGB数据
	
};

*/
int LCD_ShowJPEG(unsigned char *jpg_buffer,int size,struct ImageDecodingInfo *image_rgb)
{
    struct jpeg_decompress_struct cinfo; //存放图像的数据
    struct my_error_mgr jerr; //存放错误信息
    unsigned char  *buffer;
    unsigned int i,j;
    unsigned int  color;
    static int written;

    /*init jpeg压缩对象错误处理程序*/
    //cinfo.err = jpeg_std_error(&jerr); //初始化标准错误，用来存放错误信息
    cinfo.err = jpeg_std_error(&jerr.pub);//自定义错误信息处理
    jerr.pub.error_exit = my_error_exit;//指向自行设定的接口函数
     if (setjmp(jerr.setjmp_buffer))
     {
     //在正常情况下，setjmp将返回0，而如果程序出现错误，即调用my_error_exit
     //然后程序将再次跳转于此，同时setjmp将返回在my_error_exit中由longjmp第二个参数设定的值1
     //并执行以下代码
      jpeg_destroy_decompress(&cinfo);
      return -1;
     }
    jpeg_create_decompress(&cinfo);    //创建解压缩结构信息
    jpeg_mem_src(&cinfo, (unsigned char *)jpg_buffer, size);
    //jpeg_stdio_src(&cinfo, infile);
    /*读jpeg头*/
    jpeg_read_header(&cinfo, TRUE);
    /*开始解压*/
    jpeg_start_decompress(&cinfo);
#if 0
    printf("JPEG图片高度: %d\n",cinfo.output_height);
    printf("JPEG图片宽度: %d\n",cinfo.output_width);
    printf("JPEG图片颜色位数（字节单位）: %d\n",cinfo.output_components);
#endif
    image_rgb->Height=cinfo.output_height;
    image_rgb->Width=cinfo.output_width;

    unsigned char *rgb_data=image_rgb->rgb;
    /*为一条扫描线上的像素点分配存储空间，一行一行的解码*/
    int row_stride = cinfo.output_width * cinfo.output_components;
    buffer = (unsigned char *)malloc(row_stride);  
 	//将图片内容显示到framebuffer上,cinfo.output_scanline表示当前行的位置，读取数据是会自动增加
    i=0;
    while(cinfo.output_scanline < cinfo.output_height)
    {
        //读取一行的数据    
        jpeg_read_scanlines(&cinfo,&buffer,1);
        memcpy(rgb_data + i * cinfo.output_width * 3, buffer, row_stride);
        i++;
    }    
    /*完成解压,摧毁解压对象*/
    jpeg_finish_decompress(&cinfo); //结束解压
    jpeg_destroy_decompress(&cinfo); //释放结构体占用的空间
    /*释放内存缓冲区*/
    free(buffer);
    return 0;      
}

#endif
#if 0
int read_JPEG_file(const char *jpegData, char *rgbdata, int size)
{
    struct jpeg_error_mgr jerr;
    struct jpeg_decompress_struct cinfo;
    cinfo.err = jpeg_std_error(&jerr);
    // 1.创建解码对象并初始化
    jpeg_create_decompress(&cinfo);
    // 2.装备解码的数据
    // jpeg_stdio_src(&cinfo,infile);
    jpeg_mem_src(&cinfo, (unsigned char *)jpegData, size);
    // 3.获取jpeg图片文件的参数
    (void)jpeg_read_header(&cinfo, TRUE);
    // Step 4:set parameters for decompression
    // 5.开始解码
    (void)jpeg_start_decompress(&cinfo);
    // 6.申请存储一行数据的内存空间
    int row_stride = cinfo.output_width * cinfo.output_components;
    unsigned char *buffer = malloc(row_stride);
    int i = 0;
    while (cinfo.output_scanline < cinfo.output_height)
    {
        (void)jpeg_read_scanlines(&cinfo, &buffer, 1);
        memcpy(rgbdata + i * 640 * 3, buffer, row_stride);
        i++;
    }
    // 7.解码完成
    (void)jpeg_finish_decompress(&cinfo);
    // 8.释放解码对象
    jpeg_destroy_decompress(&cinfo);
    return 1;
}
#endif
