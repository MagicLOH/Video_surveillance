#include "freetype.h"
struct FREE_TYPE_CONFIG FreeTypeConfig;
extern int width, height;
/*
画点函数
x,y -- 要绘制的位置
w,h  --图片宽和高
src_buf  --图片颜色起始地址
rgb --要绘制的颜色
*/
void Image_DrawPoint(int x, int y, int w, int h, char *src_buf, int rgb) {
    int oneline_size = w * 3; // 一行字节数
    char *p = src_buf + y * oneline_size + x * 3;
    *(p + 2) = rgb & 0xff;
    *(p + 1) = (rgb >> 8) & 0xff;
    *p = (rgb >> 16) & 0xff;
}
/*
 * LCD 显示矢量字体的位图信息
 * bitmap : 要显示的字体的矢量位图
 * x : 显示的 x 坐标
 * y : 显示的 y 坐标
 */
void LCD_DrawBitmap(FT_Bitmap *bitmap, FT_Int x, FT_Int y, char *src_buf) {
    FT_Int i, j, p, q;
    FT_Int x_max = x + bitmap->width;
    FT_Int y_max = y + bitmap->rows;

    /* 将位图信息循环打印到屏幕上 */
    for (i = x, p = 0; i < x_max; i++, p++) {
        for (j = y, q = 0; j < y_max; j++, q++) {
            if ((i > x_max) || (j > y_max) || (i < 0) || (j < 0))
                continue;
            if (bitmap->buffer[q * bitmap->width + p] != 0) {
                Image_DrawPoint(i, j, width, height, src_buf, 0xFF0033);
            } else {
                // LCD_DrawPoint(i, j,0xFFFFFF);
            }
        }
    }
}
/*
函数功能: 初始化 FreeType 配置
*/
int InitConfig_FreeType(char *font_file) {
    FT_Error error;
    /*1. 初始化 freetype 库*/
    error = FT_Init_FreeType(&FreeTypeConfig.library);
    if (error) {
        printf("freetype 字体库初始化失败.\n");
        return -1;
    }
    /*2. 打开加载的字体文件*/
    error = FT_New_Face(FreeTypeConfig.library, font_file, 0, &FreeTypeConfig.face);
    if (error) {
        printf("矢量字体文件加载失败.\n");
        return -2;
    }
    return 0;
}
/*
函数功能: 释放 FreeType 配置
*/
void FreeType_Config(void) {
    FT_Done_Face(FreeTypeConfig.face);
    FT_Done_FreeType(FreeTypeConfig.library);
}
/*
    函数功能: 在 LCD 屏显示一串文本数据
    函数参数:
    u32 x 坐标位置
    u32 y 坐标位置
    u32 size 字体大小
    wchar_t *text 显示的文本数据
*/
int LCD_DrawText(u32 x, u32 y, u32 size, wchar_t *text, char *src_buf) {
    FT_Error error;
    int i = 0;
    int bbox_height_min = 10000;
    int bbox_height_max = 0;
    /*3. 设置字符的像素的大小为 size*size*/
    error = FT_Set_Pixel_Sizes(FreeTypeConfig.face, size, 0);
    if (error) {
        printf("字符的像素大小设置失败.\n");
        return -1;
    }
    /*4. 设置字体文件的轮廓的插槽*/
    FreeTypeConfig.slot = FreeTypeConfig.face->glyph;
    /* 设置坐标为原点坐标
     * 将 LCD 坐标转换成笛卡尔坐标
     * 单位是 1/64 Point
     */
    FreeTypeConfig.pen.x = x * 64;
    FreeTypeConfig.pen.y = (height - size - y) * 64;
    /*5. 循环的将文字显示出来*/
    for (i = 0; i < wcslen(text); i++) {
        FT_Set_Transform(FreeTypeConfig.face, 0, &FreeTypeConfig.pen); // 设置字体的起始坐标位置
        /*装载字符编码，填充 face 的 glyph slot 成员*/
        error = FT_Load_Char(FreeTypeConfig.face, text[i], FT_LOAD_RENDER);
        if (error) {
            printf("装载字符编码失败.\n");
            return -1;
        }
        /*通过 glyph slot 来获得 glyph*/
        FT_Get_Glyph(FreeTypeConfig.slot, &FreeTypeConfig.glyph);
        /*通过 glyph 来获得 cbox*/
        FT_Glyph_Get_CBox(FreeTypeConfig.glyph, FT_GLYPH_BBOX_TRUNCATE, &FreeTypeConfig.bbox);
        /*获得字体高度的最大值和最小值*/
        if (bbox_height_min > FreeTypeConfig.bbox.yMin)
            bbox_height_min = FreeTypeConfig.bbox.yMin;
        if (bbox_height_max < FreeTypeConfig.bbox.yMax)
            bbox_height_max = FreeTypeConfig.bbox.yMax;
        /*画点，把笛卡尔坐标转换成 LCD 坐标*/
        LCD_DrawBitmap(&FreeTypeConfig.slot->bitmap,
                       FreeTypeConfig.slot->bitmap_left,
                       height - FreeTypeConfig.slot->bitmap_top,
                       src_buf);
        if (FreeTypeConfig.slot->bitmap_left + size * 2 > width) {
            FreeTypeConfig.pen.x = 0;                               // 更新 X 坐标位置
            FreeTypeConfig.pen.y = (height - size - y - size) * 64; // 更新 Y 坐标位置
        } else {
            /* 更新原点坐标位置 */
            FreeTypeConfig.pen.x += FreeTypeConfig.slot->advance.x;
            FreeTypeConfig.pen.y += FreeTypeConfig.slot->advance.y;
        }
    }
    return 0;
}
// 将char类型转化为wchar
// src:  源
// dest: 目标
// locale: 环境变量的值，mbstowcs依赖此值来判断src的编码方式
void CharToWchar(char *src, wchar_t *dest) {
    // 根据环境变量设置locale
    setlocale(LC_CTYPE, "zh_CN.utf8");
    // mbstowcs函数得到转化为需要的宽字符大小: 计算char转为wcchar存放时实际占用空间大小.
    // 也可以直接传入一个大数组代替
    // w_size=mbstowcs(NULL, src, 0) + 1;
    mbstowcs(dest, src, strlen(src) + 1);
}