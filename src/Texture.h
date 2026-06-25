#pragma once

#include <glad/glad.h>
#include <string>

class Texture
{
public:
    Texture() = default;
    Texture(const std::string& filePath, const std::string& type = "texture_diffuse");
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    void Load(const std::string& filePath);
    void LoadFromMemory(const unsigned char* data, int length,
                        const std::string& debugName = "embedded");
    void Bind(unsigned int unit = 0) const;
    void Unbind() const;
    void SetWrapMode(GLenum wrapS, GLenum wrapT);

    unsigned int ID() const { return m_ID; }
    int Width() const { return m_Width; }
    int Height() const { return m_Height; }
    bool IsLoaded() const { return m_Loaded; }
    std::string Type() const { return m_Type; }
    void SetType(const std::string& type) { m_Type = type; }
    std::string Path() const { return m_Path; }

private:
    void Release();
    void UploadToGL(unsigned char* data);

    unsigned int m_ID = 0;
    int m_Width = 0;
    int m_Height = 0;
    int m_Channels = 0;
    bool m_Loaded = false;
    std::string m_Type;
    std::string m_Path;
};
