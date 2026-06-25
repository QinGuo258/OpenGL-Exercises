#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Texture.h"
#include <iostream>

static const bool s_FlipInit = (stbi_set_flip_vertically_on_load(true), true);

Texture::Texture(const std::string& filePath, const std::string& type)
    : m_Type(type), m_Path(filePath)
{
    Load(filePath);
}

Texture::~Texture()
{
    Release();
}

Texture::Texture(Texture&& other) noexcept
    : m_ID(other.m_ID), m_Width(other.m_Width), m_Height(other.m_Height),
      m_Channels(other.m_Channels), m_Loaded(other.m_Loaded),
      m_Type(std::move(other.m_Type)), m_Path(std::move(other.m_Path))
{
    other.m_ID = 0;
    other.m_Loaded = false;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other)
    {
        Release();
        m_ID = other.m_ID;
        m_Width = other.m_Width;
        m_Height = other.m_Height;
        m_Channels = other.m_Channels;
        m_Loaded = other.m_Loaded;
        m_Type = std::move(other.m_Type);
        m_Path = std::move(other.m_Path);
        other.m_ID = 0;
        other.m_Loaded = false;
    }
    return *this;
}

void Texture::Load(const std::string& filePath)
{
    Release();

    unsigned char* data = stbi_load(filePath.c_str(), &m_Width, &m_Height, &m_Channels, 4);
    if (!data)
    {
        std::cerr << "ERROR::TEXTURE::FAILED_TO_LOAD: " << filePath << std::endl;
        return;
    }

    UploadToGL(data);
    stbi_image_free(data);
    m_Loaded = true;
}

void Texture::LoadFromMemory(const unsigned char* data, int length,
                              const std::string& debugName)
{
    Release();

    unsigned char* img = stbi_load_from_memory(data, length, &m_Width, &m_Height, &m_Channels, 4);
    if (!img)
    {
        std::cerr << "ERROR::TEXTURE::FAILED_TO_LOAD_FROM_MEMORY: " << debugName << std::endl;
        return;
    }

    UploadToGL(img);
    stbi_image_free(img);
    m_Loaded = true;
    m_Path = debugName;
}

void Texture::UploadToGL(unsigned char* data)
{
    glGenTextures(1, &m_ID);
    glBindTexture(GL_TEXTURE_2D, m_ID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
}

void Texture::Bind(unsigned int unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_ID);
}

void Texture::Unbind() const
{
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::SetWrapMode(GLenum wrapS, GLenum wrapT)
{
    glBindTexture(GL_TEXTURE_2D, m_ID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::Release()
{
    if (m_ID != 0)
    {
        glDeleteTextures(1, &m_ID);
        m_ID = 0;
    }
    m_Loaded = false;
}
