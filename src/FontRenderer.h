#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "font_baker.h"  // C 包装：extern "C" + stb_truetype.h

class Shader;

class FontRenderer
{
public:
    FontRenderer() = default;
    ~FontRenderer();

    FontRenderer(const FontRenderer&) = delete;
    FontRenderer& operator=(const FontRenderer&) = delete;

    // 从 .ttf 文件烘焙 ASCII + 中文标点 + CJK 汉字到 2048x2048 单通道纹理
    bool Init(const std::string& fontPath, float fontSize);

    // 渲染 UTF-8 编码的中英文文本
    void RenderText(Shader& shader, unsigned int vao,
                    const std::string& text,
                    float x, float y, float scale,
                    const glm::vec3& color);

    unsigned int TextureID() const { return m_TextureID; }

private:
    // UTF-8 字符串 → Unicode 码点序列
    std::vector<int> DecodeUTF8(const std::string& s) const;

    unsigned int m_TextureID = 0;
    int m_AtlasWidth = 8192;
    int m_AtlasHeight = 8192;
    float m_FontSize = 16.0f;

    // 三个字符范围的排版数据
    stbtt_packedchar m_AsciiChars[95];           // 32-126
    stbtt_packedchar m_CjkPunctuation[64];       // 0x3000-0x303F
    stbtt_packedchar m_CjkUnified[20992];        // 0x4E00-0x9FFF
};
