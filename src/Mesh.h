#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include "Texture.h"

#define MAX_BONE_INFLUENCE 4

struct Vertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    int m_BoneIDs[MAX_BONE_INFLUENCE] = {-1, -1, -1, -1};
    float m_Weights[MAX_BONE_INFLUENCE] = {0.0f, 0.0f, 0.0f, 0.0f};
};

class Mesh
{
public:
    Mesh(std::vector<Vertex> vertices,
         std::vector<unsigned int> indices,
         std::vector<Texture> textures);
    ~Mesh();

    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void Draw(unsigned int shaderProgram) const;

    int materialType = 0; // 0:默认, 1:草丛, 2:树叶, 3:自发光

    const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
    const std::vector<unsigned int>& GetIndices() const { return m_Indices; }

private:
    void SetupMesh();
    void Release();

    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;
    unsigned int m_EBO = 0;

    std::vector<Vertex> m_Vertices;
    std::vector<unsigned int> m_Indices;
    std::vector<Texture> m_Textures;
};
