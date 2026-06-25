#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

class Shader
{
public:
    Shader() = default;
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    void Use() const;
    unsigned int ID() const { return m_ID; }
    void Reload();

    void SetMat4(const std::string& name, const glm::mat4& mat) const;
    void SetVec3(const std::string& name, const glm::vec3& value) const;
    void SetVec3(const std::string& name, float x, float y, float z) const;
    void SetVec4(const std::string& name, const glm::vec4& value) const;
    void SetVec4(const std::string& name, float x, float y, float z, float w) const;
    void SetVec2(const std::string& name, const glm::vec2& value) const;
    void SetVec2(const std::string& name, float x, float y) const;
    void SetFloat(const std::string& name, float value) const;
    void SetInt(const std::string& name, int value) const;
    void SetBool(const std::string& name, bool value) const;

private:
    void Release();
    std::string ReadFile(const std::string& path) const;
    unsigned int CompileShader(GLenum type, const std::string& source) const;

    unsigned int m_ID = 0;
    std::string m_VertexPath;
    std::string m_FragmentPath;
};
