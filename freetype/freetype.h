#ifndef _FREETYPE_H
#define _FREETYPE_H

#include <ft2build.h>
#include <locale.h>
#include <math.h>
#include <wchar.h>

#include FT_FREETYPE_H
#include FT_STROKER_H
typedef unsigned int u32;
/*定义一个结构体存放矢量字体的配置*/
struct FREE_TYPE_CONFIG {
    FT_Library library;
    FT_Face face;
    FT_GlyphSlot slot;
    FT_Vector pen; /* untransformed origin  */
    FT_Error error;
    FT_BBox bbox;
    FT_Glyph glyph;
};

int InitConfig_FreeType(char *font_file);                               // 初始化freetype
void FreeType_Config(void);                                             // 释放freetype
int LCD_DrawText(u32 x, u32 y, u32 size, wchar_t *text, char *src_buf); // 字符串显示
void CharToWchar(char *src, wchar_t *dest);                             // char转wchar
#endif
