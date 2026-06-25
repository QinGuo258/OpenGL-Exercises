
#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <map>
#include "Mesh.h"

struct BoneInfo
{
    int id;
    glm::mat4 offset;
};

inline glm::mat4 AiMatrixToGlm(const aiMatrix4x4& from)
{
    glm::mat4 to;
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
    return to;
}

class Model
{
public:
    Model(const std::string& filePath);
    ~Model() = default;

    void Draw(unsigned int shaderProgram) const;

    const std::vector<Mesh>& GetMeshes() const { return m_Meshes; }
    auto& GetBoneInfoMap() { return m_BoneInfoMap; }
    int& GetBoneCount() { return m_BoneCounter; }

    std::vector<glm::vec3> pointLights; // 自动提取的自发光网格点光源（世界空间）

private:
    void LoadModel(const std::string& filePath);
    void ProcessNode(aiNode* node, const aiScene* scene);
    Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene);
    void ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene);
    std::vector<Texture> LoadMaterialTextures(aiMaterial* mat, aiTextureType type,
                                               const std::string& typeName,
                                               const aiScene* scene);

    std::vector<Mesh> m_Meshes;
    std::string m_Directory;
    std::map<std::string, BoneInfo> m_BoneInfoMap;
    int m_BoneCounter = 0;
};
