#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

int bake_font_ranges(
    const unsigned char* fontData, int fontDataSize,
    float fontSize,
    unsigned char* pixels, int atlasW, int atlasH,
    stbtt_packedchar* asciiChars,
    stbtt_packedchar* punctChars,
    stbtt_packedchar* cjkChars)
{
    stbtt_pack_context ctx;
    if (!stbtt_PackBegin(&ctx, pixels, atlasW, atlasH, 0, 1, 0))
        return 0;

    stbtt_pack_range ranges[3];

    // 范围 1：ASCII (32-126, 95 字符)
    ranges[0].font_size = fontSize;
    ranges[0].first_unicode_codepoint_in_range = 32;
    ranges[0].array_of_unicode_codepoints = 0;
    ranges[0].num_chars = 95;
    ranges[0].chardata_for_range = asciiChars;
    ranges[0].h_oversample = 0;
    ranges[0].v_oversample = 0;

    // 范围 2：中文标点 (0x3000 - 0x303F, 64 字符)
    ranges[1].font_size = fontSize;
    ranges[1].first_unicode_codepoint_in_range = 0x3000;
    ranges[1].array_of_unicode_codepoints = 0;
    ranges[1].num_chars = 64;
    ranges[1].chardata_for_range = punctChars;
    ranges[1].h_oversample = 0;
    ranges[1].v_oversample = 0;

    // 范围 3：CJK 统一汉字 (0x4E00 - 0x9FFF, 20992 汉字)
    ranges[2].font_size = fontSize;
    ranges[2].first_unicode_codepoint_in_range = 0x4E00;
    ranges[2].array_of_unicode_codepoints = 0;
    ranges[2].num_chars = 20992;
    ranges[2].chardata_for_range = cjkChars;
    ranges[2].h_oversample = 0;
    ranges[2].v_oversample = 0;

    if (!stbtt_PackFontRanges(&ctx, fontData, 0, ranges, 3))
    {
        stbtt_PackEnd(&ctx);
        return 0;
    }

    stbtt_PackEnd(&ctx);
    return 1;
}
