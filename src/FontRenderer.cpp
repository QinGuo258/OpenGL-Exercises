#include "FontRenderer.h"
#include "Shader.h"

#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <vector>
#include <cstdio>
#include <iostream>

FontRenderer::~FontRenderer()
{
    if (m_TextureID != 0)
        glDeleteTextures(1, &m_TextureID);
}

bool FontRenderer::Init(const std::string& fontPath, float fontSize)
{
    m_FontSize = fontSize;

    // 1. 多路径自动搜寻字体文件
    std::string possiblePaths[] = { fontPath, "fonts/font.ttf", "textures/font.ttf", "font.ttf" };
    FILE* f = nullptr;
    std::string finalPath;
    for (const auto& path : possiblePaths)
    {
        f = fopen(path.c_str(), "rb");
        if (f)
        {
            fclose(f);
            finalPath = path;
            break;
        }
    }
    if (finalPath.empty())
    {
        printf("[Font] ERROR: Failed to locate font.ttf in any directory!\n");
        return false;
    }

    std::ifstream file(finalPath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "ERROR::FONT_RENDERER::FAILED_TO_OPEN: " << finalPath << std::endl;
        return false;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> fontBuffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(fontBuffer.data()), size))
    {
        std::cerr << "ERROR::FONT_RENDERER::FAILED_TO_READ: " << finalPath << std::endl;
        return false;
    }

    // 2. 分配 2048x2048 单通道像素缓冲区
    std::vector<unsigned char> pixels(m_AtlasWidth * m_AtlasHeight, 0);

    // 3. 调用 C 包装函数烘焙字体（纯 C 编译，避免 MSVC C++ 兼容问题）
    if (!bake_font_ranges(fontBuffer.data(), (int)fontBuffer.size(),
                          fontSize,
                          pixels.data(), m_AtlasWidth, m_AtlasHeight,
                          m_AsciiChars, m_CjkPunctuation, m_CjkUnified))
    {
        std::cerr << "ERROR::FONT_RENDERER::BAKE_FAILED" << std::endl;
        return false;
    }

    // 4. 上传到 OpenGL 纹理（GL_RED 单通道，线性过滤）
    glGenTextures(1, &m_TextureID);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                 m_AtlasWidth, m_AtlasHeight, 0,
                 GL_RED, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    std::cout << "[Font] OK: baked from " << finalPath << " at " << fontSize
              << "px into " << m_AtlasWidth << "x" << m_AtlasHeight
              << " atlas (GL_RED)" << std::endl;

    return true;
}

std::vector<int> FontRenderer::DecodeUTF8(const std::string& s) const
{
    std::vector<int> codepoints;
    for (size_t i = 0; i < s.size(); )
    {
        unsigned char c = static_cast<unsigned char>(s[i]);
        int cp = 0;
        int extra = 0;

        if (c < 0x80) {
            cp = c;
            extra = 0;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            extra = 1;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            extra = 2;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            extra = 3;
        } else {
            i++;
            continue;
        }

        if (i + extra >= s.size()) break;

        bool valid = true;
        for (int j = 0; j < extra; ++j)
        {
            unsigned char follow = static_cast<unsigned char>(s[i + 1 + j]);
            if ((follow & 0xC0) != 0x80) { valid = false; break; }
            cp = (cp << 6) | (follow & 0x3F);
        }

        if (valid) {
            codepoints.push_back(cp);
            i += 1 + extra;
        } else {
            i++;
        }
    }
    return codepoints;
}

void FontRenderer::RenderText(Shader& shader, unsigned int vao,
                              const std::string& text,
                              float x, float y, float scale,
                              const glm::vec3& color)
{
    if (text.empty()) return;

    // 设置字体着色器状态（不覆盖 uAlpha，由调用方控制透明度）
    shader.Use();
    shader.SetBool("uUseTexture", true);
    shader.SetBool("uIsFont", true);
    shader.SetVec3("uFontColor", color);
    shader.SetInt("uTexture", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
    glBindVertexArray(vao);

    float atlasW = static_cast<float>(m_AtlasWidth);
    float atlasH = static_cast<float>(m_AtlasHeight);

    std::vector<int> codepoints = DecodeUTF8(text);

    for (int cp : codepoints)
    {
        stbtt_packedchar* ch = nullptr;

        // 查找该码点属于哪个 Range
        if (cp >= 32 && cp <= 126)
        {
            ch = &m_AsciiChars[cp - 32];
        }
        else if (cp >= 0x3000 && cp <= 0x303F)
        {
            ch = &m_CjkPunctuation[cp - 0x3000];
        }
        else if (cp >= 0x4E00 && cp <= 0x9FFF)
        {
            ch = &m_CjkUnified[cp - 0x4E00];
        }

        if (ch == nullptr || ch->xadvance == 0.0f)
            continue;

        // 计算 UV 坐标（2048x2048 归一化）
        float u0 = ch->x0 / atlasW;
        float v0 = ch->y0 / atlasH;
        float u1 = ch->x1 / atlasW;
        float v1 = ch->y1 / atlasH;

        float w = (ch->x1 - ch->x0) * scale;
        float h = (ch->y1 - ch->y0) * scale;

        // 基线对齐：yoff 为字符左上角到基线的偏移
        float x_pos = x + ch->xoff * scale;
        float y_pos = y - (h + ch->yoff * scale);

        // 设置 UV 变换
        shader.SetVec2("uUvScale", glm::vec2(u1 - u0, v1 - v0));
        shader.SetVec2("uUvOffset", glm::vec2(u0, v0));

        // 绘制四边形
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(x_pos, y_pos, 0.0f));
        model = glm::scale(model, glm::vec3(w, h, 1.0f));
        shader.SetMat4("uModel", model);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        // 递进 X 坐标
        x += ch->xadvance * scale;
    }

    // 恢复 UI 着色器默认状态（保持 VAO 绑定，由调用方管理）
    shader.SetBool("uIsFont", false);
    shader.SetVec2("uUvScale", glm::vec2(1.0f));
    shader.SetVec2("uUvOffset", glm::vec2(0.0f));
}
