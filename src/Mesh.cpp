#include "Mesh.h"
#include <iostream>

Mesh::Mesh(std::vector<Vertex> vertices,
           std::vector<unsigned int> indices,
           std::vector<Texture> textures)
    : m_Vertices(std::move(vertices)), m_Indices(std::move(indices)), m_Textures(std::move(textures))
{
    SetupMesh();
}

Mesh::~Mesh()
{
    Release();
}

Mesh::Mesh(Mesh&& other) noexcept
    : m_VAO(other.m_VAO), m_VBO(other.m_VBO), m_EBO(other.m_EBO),
      m_Vertices(std::move(other.m_Vertices)),
      m_Indices(std::move(other.m_Indices)),
      m_Textures(std::move(other.m_Textures)),
      materialType(other.materialType)
{
    other.m_VAO = 0;
    other.m_VBO = 0;
    other.m_EBO = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept
{
    if (this != &other)
    {
        Release();
        m_VAO = other.m_VAO;
        m_VBO = other.m_VBO;
        m_EBO = other.m_EBO;
        m_Vertices = std::move(other.m_Vertices);
        m_Indices = std::move(other.m_Indices);
        m_Textures = std::move(other.m_Textures);
        materialType = other.materialType;
        other.m_VAO = 0;
        other.m_VBO = 0;
        other.m_EBO = 0;
    }
    return *this;
}

void Mesh::Draw(unsigned int shaderProgram) const
{
    bool hasDiffuse = false;
    for (unsigned int i = 0; i < m_Textures.size(); i++)
    {
        if (m_Textures[i].Type() == "texture_diffuse" && !hasDiffuse)
        {
            m_Textures[i].Bind(0);
            glUniform1i(glGetUniformLocation(shaderProgram, "uDiffuseTexture"), 0);
            glUniform1i(glGetUniformLocation(shaderProgram, "uHasDiffuseTexture"), 1);
            hasDiffuse = true;
        }
    }

    if (!hasDiffuse)
        glUniform1i(glGetUniformLocation(shaderProgram, "uHasDiffuseTexture"), 0);

    glUniform1i(glGetUniformLocation(shaderProgram, "uMaterialType"), materialType);

    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_Indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Mesh::SetupMesh()
{
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_Vertices.size() * sizeof(Vertex)),
                 m_Vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_Indices.size() * sizeof(unsigned int)),
                 m_Indices.data(), GL_STATIC_DRAW);

    // location=0: Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Position));
    glEnableVertexAttribArray(0);
    // location=1: Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
    glEnableVertexAttribArray(1);
    // location=2: TexCoords
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
    glEnableVertexAttribArray(2);

    // location=3: BoneIDs
    glVertexAttribIPointer(3, MAX_BONE_INFLUENCE, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, m_BoneIDs));
    glEnableVertexAttribArray(3);
    // location=4: Weights
    glVertexAttribPointer(4, MAX_BONE_INFLUENCE, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, m_Weights));
    glEnableVertexAttribArray(4);

    glBindVertexArray(0);
}

void Mesh::Release()
{
    if (m_EBO != 0) { glDeleteBuffers(1, &m_EBO); m_EBO = 0; }
    if (m_VBO != 0) { glDeleteBuffers(1, &m_VBO); m_VBO = 0; }
    if (m_VAO != 0) { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
}
