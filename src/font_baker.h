#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stb_truetype.h"

// C-only: 烘焙字体到像素缓冲区，返回成功/失败
int bake_font_ranges(
    const unsigned char* fontData, int fontDataSize,
    float fontSize,
    unsigned char* pixels, int atlasW, int atlasH,
    stbtt_packedchar* asciiChars,    // [95]  ASCII 32-126
    stbtt_packedchar* punctChars,    // [64]  中文标点 0x3000-0x303F
    stbtt_packedchar* cjkChars       // [20992] CJK 0x4E00-0x9FFF
);

#ifdef __cplusplus
}
#endif
