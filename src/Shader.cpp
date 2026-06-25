#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
    : m_VertexPath(vertexPath), m_FragmentPath(fragmentPath)
{
    std::string vertexCode = ReadFile(vertexPath);
    std::string fragmentCode = ReadFile(fragmentPath);

    unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, vertexCode);
    unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentCode);

    if (vertexShader == 0 || fragmentShader == 0)
    {
        std::cerr << "ERROR::SHADER::CONSTRUCTOR: Compilation failed, program invalid."
                  << " Vertex=" << vertexPath << " Fragment=" << fragmentPath << std::endl;
        m_ID = 0;
        return;
    }

    m_ID = glCreateProgram();
    glAttachShader(m_ID, vertexShader);
    glAttachShader(m_ID, fragmentShader);
    glLinkProgram(m_ID);

    int success;
    char infoLog[512];
    glGetProgramiv(m_ID, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(m_ID, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

Shader::~Shader()
{
    Release();
}

Shader::Shader(Shader&& other) noexcept
    : m_ID(other.m_ID)
{
    other.m_ID = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept
{
    if (this != &other)
    {
        Release();
        m_ID = other.m_ID;
        other.m_ID = 0;
    }
    return *this;
}

void Shader::Reload()
{
    // 热重载时优先从源码目录读取（而非 CMake POST_BUILD 拷贝的副本）
    std::string vertPath = m_VertexPath;
    std::string fragPath = m_FragmentPath;
#ifdef SHADER_SRC_DIR
    vertPath = std::string(SHADER_SRC_DIR) + std::filesystem::path(m_VertexPath).filename().string();
    fragPath = std::string(SHADER_SRC_DIR) + std::filesystem::path(m_FragmentPath).filename().string();
#endif

    std::string vertexCode = ReadFile(vertPath);
    std::string fragmentCode = ReadFile(fragPath);

    if (vertexCode.empty() || fragmentCode.empty())
    {
        std::cerr << "[Shader] Reload failed: could not read source files, keeping old program." << std::endl;
        return;
    }

    unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, vertexCode);
    unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentCode);

    if (vertexShader == 0 || fragmentShader == 0)
    {
        std::cerr << "[Shader] Reload failed: compilation error, keeping old program." << std::endl;
        return;
    }

    unsigned int newID = glCreateProgram();
    glAttachShader(newID, vertexShader);
    glAttachShader(newID, fragmentShader);
    glLinkProgram(newID);

    int linkSuccess;
    glGetProgramiv(newID, GL_LINK_STATUS, &linkSuccess);
    if (!linkSuccess)
    {
        char infoLog[512];
        glGetProgramInfoLog(newID, 512, nullptr, infoLog);
        std::cerr << "[Shader] Reload failed: linking error:\n" << infoLog << std::endl;
        std::cerr << "[Shader] Keeping old program." << std::endl;
        glDeleteProgram(newID);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return;
    }

    // 新程序编译链接成功 → 删除旧的，替换为新的
    glDeleteProgram(m_ID);
    m_ID = newID;

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    std::cout << "[Shader] Hot reloaded: " << vertPath << " / " << fragPath << std::endl;
}

void Shader::Use() const
{
    glUseProgram(m_ID);
}

void Shader::SetMat4(const std::string& name, const glm::mat4& mat) const
{
    glUniformMatrix4fv(glGetUniformLocation(m_ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

void Shader::SetVec3(const std::string& name, const glm::vec3& value) const
{
    glUniform3fv(glGetUniformLocation(m_ID, name.c_str()), 1, &value[0]);
}

void Shader::SetVec3(const std::string& name, float x, float y, float z) const
{
    glUniform3f(glGetUniformLocation(m_ID, name.c_str()), x, y, z);
}

void Shader::SetVec4(const std::string& name, const glm::vec4& value) const
{
    glUniform4fv(glGetUniformLocation(m_ID, name.c_str()), 1, &value[0]);
}

void Shader::SetVec4(const std::string& name, float x, float y, float z, float w) const
{
    glUniform4f(glGetUniformLocation(m_ID, name.c_str()), x, y, z, w);
}

void Shader::SetVec2(const std::string& name, const glm::vec2& value) const
{
    glUniform2fv(glGetUniformLocation(m_ID, name.c_str()), 1, &value[0]);
}

void Shader::SetVec2(const std::string& name, float x, float y) const
{
    glUniform2f(glGetUniformLocation(m_ID, name.c_str()), x, y);
}

void Shader::SetFloat(const std::string& name, float value) const
{
    glUniform1f(glGetUniformLocation(m_ID, name.c_str()), value);
}

void Shader::SetInt(const std::string& name, int value) const
{
    glUniform1i(glGetUniformLocation(m_ID, name.c_str()), value);
}

void Shader::SetBool(const std::string& name, bool value) const
{
    glUniform1i(glGetUniformLocation(m_ID, name.c_str()), (int)value);
}

void Shader::Release()
{
    if (m_ID != 0)
    {
        glDeleteProgram(m_ID);
        m_ID = 0;
    }
}

std::string Shader::ReadFile(const std::string& path) const
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "ERROR::SHADER::FILE_NOT_FOUND: " << path << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

unsigned int Shader::CompileShader(GLenum type, const std::string& source) const
{
    unsigned int shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    char infoLog[1024];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED (type="
                  << (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") << ")\n"
                  << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}
