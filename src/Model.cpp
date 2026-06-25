#include "Model.h"
#include <algorithm>
#include <cstdio>
#include <cctype>

extern FILE* gLogFile;

#define LOG(fmt, ...) do { \
    if (gLogFile) { fprintf(gLogFile, fmt "\n", ##__VA_ARGS__); fflush(gLogFile); } \
} while(0)

Model::Model(const std::string& filePath)
{
    LoadModel(filePath);
}

void Model::Draw(unsigned int shaderProgram) const
{
    for (const Mesh& mesh : m_Meshes)
        mesh.Draw(shaderProgram);
}

void Model::LoadModel(const std::string& filePath)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        LOG("ERROR::ASSIMP: %s", importer.GetErrorString());
        return;
    }

    LOG("=== Model Loaded ===");
    LOG("File: %s", filePath.c_str());
    LOG("Meshes: %u", scene->mNumMeshes);
    LOG("Materials: %u", scene->mNumMaterials);
    LOG("Embedded textures: %u", scene->mNumTextures);

    // 包围盒
    float bbMinX = 1e10f, bbMinY = 1e10f, bbMinZ = 1e10f;
    float bbMaxX = -1e10f, bbMaxY = -1e10f, bbMaxZ = -1e10f;
    unsigned int totalVerts = 0;
    unsigned int totalFaces = 0;

    for (unsigned int mi = 0; mi < scene->mNumMeshes; mi++)
    {
        aiMesh* mesh = scene->mMeshes[mi];
        totalVerts += mesh->mNumVertices;
        totalFaces += mesh->mNumFaces;
        for (unsigned int vi = 0; vi < mesh->mNumVertices; vi++)
        {
            aiVector3D v = mesh->mVertices[vi];
            if (v.x < bbMinX) bbMinX = v.x;
            if (v.y < bbMinY) bbMinY = v.y;
            if (v.z < bbMinZ) bbMinZ = v.z;
            if (v.x > bbMaxX) bbMaxX = v.x;
            if (v.y > bbMaxY) bbMaxY = v.y;
            if (v.z > bbMaxZ) bbMaxZ = v.z;
        }
    }

    LOG("Total Vertices: %u", totalVerts);
    LOG("Total Faces: %u", totalFaces);
    LOG("BBox Min: (%.2f, %.2f, %.2f)", bbMinX, bbMinY, bbMinZ);
    LOG("BBox Max: (%.2f, %.2f, %.2f)", bbMaxX, bbMaxY, bbMaxZ);
    LOG("BBox Size: (%.2f, %.2f, %.2f)", bbMaxX - bbMinX, bbMaxY - bbMinY, bbMaxZ - bbMinZ);

    // 推荐缩放比例（使模型适配屏幕）
    float maxDim = bbMaxX - bbMinX;
    if (bbMaxY - bbMinY > maxDim) maxDim = bbMaxY - bbMinY;
    if (bbMaxZ - bbMinZ > maxDim) maxDim = bbMaxZ - bbMinZ;
    if (maxDim > 0.001f)
        LOG("Suggested scale for ~5 unit size: %.6f", 5.0f / maxDim);
    LOG("Model center: (%.2f, %.2f, %.2f)",
        (bbMinX + bbMaxX) / 2, (bbMinY + bbMaxY) / 2, (bbMinZ + bbMaxZ) / 2);

    if (totalVerts == 0)
    {
        LOG("ERROR: Model has no vertices!");
        return;
    }

    m_Directory = filePath.substr(0, filePath.find_last_of('/'));
    ProcessNode(scene->mRootNode, scene);
    LOG("Meshes processed: %zu", m_Meshes.size());
    LOG("=== Load Complete ===");
}

void Model::ProcessNode(aiNode* node, const aiScene* scene)
{
    LOG("  Node '%s': %u meshes, %u children",
        node->mName.C_Str(), node->mNumMeshes, node->mNumChildren);

    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        m_Meshes.push_back(ProcessMesh(mesh, scene));
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++)
        ProcessNode(node->mChildren[i], scene);
}

Mesh Model::ProcessMesh(aiMesh* mesh, const aiScene* scene)
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;

    LOG("    Mesh '%s': %u verts, %u faces, mat=%d, hasTC=%s",
        mesh->mName.C_Str(), mesh->mNumVertices, mesh->mNumFaces,
        mesh->mMaterialIndex, mesh->mTextureCoords[0] ? "yes" : "no");

    for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        Vertex vertex;
        vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);

        if (mesh->mTextureCoords[0])
            vertex.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        else
            vertex.TexCoords = glm::vec2(0.0f, 0.0f);

        vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }

    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

        std::vector<Texture> diffuseMaps = LoadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse", scene);
        textures.insert(textures.end(),
                        std::make_move_iterator(diffuseMaps.begin()),
                        std::make_move_iterator(diffuseMaps.end()));

        std::vector<Texture> baseColorMaps = LoadMaterialTextures(material, aiTextureType_BASE_COLOR, "texture_diffuse", scene);
        textures.insert(textures.end(),
                        std::make_move_iterator(baseColorMaps.begin()),
                        std::make_move_iterator(baseColorMaps.end()));
    }

    ExtractBoneWeightForVertices(vertices, mesh, scene);

    // 材质名称判定 materialType
    int matType = 0;
    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        aiString matName;
        if (AI_SUCCESS == aiGetMaterialString(material, AI_MATKEY_NAME, &matName))
        {
            std::string nameStr = matName.C_Str();
            std::transform(nameStr.begin(), nameStr.end(), nameStr.begin(), ::tolower);

            if (nameStr.find("leaves") != std::string::npos)
            {
                matType = 2; // 树叶
            }
            else if (
                (nameStr.find("grass") != std::string::npos && nameStr.find("block") == std::string::npos) ||
                nameStr.find("wheat") != std::string::npos ||
                nameStr.find("crop") != std::string::npos ||
                nameStr.find("flower") != std::string::npos ||
                nameStr.find("plant") != std::string::npos ||
                nameStr.find("fern") != std::string::npos ||
                nameStr.find("tall") != std::string::npos ||
                nameStr.find("vine") != std::string::npos
            ) {
                matType = 1; // 杂草/花朵/农作物 (非实体，无物理碰撞，且随风飘动)
            }
            else if (nameStr.find("lava") != std::string::npos ||
                     nameStr.find("torch") != std::string::npos ||
                     nameStr.find("glowstone") != std::string::npos ||
                     nameStr.find("lantern") != std::string::npos)
            {
                matType = 3; // 自发光物体
            }
            else if (nameStr.find("water") != std::string::npos ||
                     nameStr.find("fluid") != std::string::npos ||
                     nameStr.find("ocean") != std::string::npos ||
                     nameStr.find("river") != std::string::npos) {
                matType = 4; // 水面 (Water)
            }

            LOG("      Material '%s' → materialType=%d", matName.C_Str(), matType);
        }
    }

    // 自发光网格 → 提取点光源位置（0.5m 聚类取平均，合并同一方块的顶点）
    if (matType == 3) {
        for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
            glm::vec3 pos(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
            bool isNewLight = true;
            for (auto& light : this->pointLights) {
                if (glm::distance(pos, light) < 0.5f) {
                    light = (light + pos) * 0.5f;
                    isNewLight = false;
                    break;
                }
            }
            if (isNewLight) {
                this->pointLights.push_back(pos);
            }
        }
    }

    Mesh newMesh(vertices, indices, std::move(textures));
    newMesh.materialType = matType;
    return newMesh;
}

void Model::ExtractBoneWeightForVertices(std::vector<Vertex>& vertices, aiMesh* mesh, const aiScene* scene)
{
    LOG("      Mesh '%s' has %u bones", mesh->mName.C_Str(), mesh->mNumBones);
    for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++)
    {
        int boneID = -1;
        std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();

        if (m_BoneInfoMap.find(boneName) == m_BoneInfoMap.end())
        {
            BoneInfo newBoneInfo;
            newBoneInfo.id = m_BoneCounter;
            newBoneInfo.offset = AiMatrixToGlm(mesh->mBones[boneIndex]->mOffsetMatrix);
            m_BoneInfoMap[boneName] = newBoneInfo;
            boneID = m_BoneCounter;
            LOG("        Bone[%d]: '%s' numWeights=%u", boneID, boneName.c_str(),
                mesh->mBones[boneIndex]->mNumWeights);
            m_BoneCounter++;
        }
        else
        {
            boneID = m_BoneInfoMap[boneName].id;
        }

        aiVertexWeight* weights = mesh->mBones[boneIndex]->mWeights;
        unsigned int numWeights = mesh->mBones[boneIndex]->mNumWeights;

        for (unsigned int weightIndex = 0; weightIndex < numWeights; weightIndex++)
        {
            unsigned int vertexId = weights[weightIndex].mVertexId;
            float weight = weights[weightIndex].mWeight;

            for (int i = 0; i < MAX_BONE_INFLUENCE; i++)
            {
                if (vertices[vertexId].m_BoneIDs[i] < 0)
                {
                    vertices[vertexId].m_BoneIDs[i] = boneID;
                    vertices[vertexId].m_Weights[i] = weight;
                    break;
                }
            }
        }
    }
}

std::vector<Texture> Model::LoadMaterialTextures(aiMaterial* mat, aiTextureType type,
                                                  const std::string& typeName,
                                                  const aiScene* scene)
{
    std::vector<Texture> textures;

    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
    {
        aiString str;
        mat->GetTexture(type, i, &str);
        std::string filename = std::string(str.C_Str());

        Texture tex;

        if (filename.size() >= 2 && filename[0] == '*' && std::isdigit(static_cast<unsigned char>(filename[1])))
        {
            int texIndex = std::stoi(filename.substr(1));
            if (texIndex >= 0 && static_cast<unsigned int>(texIndex) < scene->mNumTextures)
            {
                const aiTexture* embeddedTex = scene->mTextures[texIndex];
                LOG("      Tex[%s]: '%s' [embedded, fmt=%s]", typeName.c_str(), filename.c_str(), embeddedTex->achFormatHint);

                if (embeddedTex->mHeight == 0)
                {
                    tex.LoadFromMemory(
                        reinterpret_cast<const unsigned char*>(embeddedTex->pcData),
                        static_cast<int>(embeddedTex->mWidth),
                        filename);
                }
                else
                {
                    tex.LoadFromMemory(
                        reinterpret_cast<const unsigned char*>(embeddedTex->pcData),
                        static_cast<int>(embeddedTex->mWidth * embeddedTex->mHeight * 4),
                        filename);
                }
                tex.SetType(typeName);
            }
            else
            {
                LOG("      Tex[%s]: '%s' [INVALID INDEX %d]", typeName.c_str(), filename.c_str(), texIndex);
            }
        }
        else
        {
            std::string fullPath = m_Directory + "/" + filename;
            LOG("      Tex[%s]: '%s' [external]", typeName.c_str(), fullPath.c_str());
            tex = Texture(fullPath, typeName);
        }

        if (tex.IsLoaded())
            textures.push_back(std::move(tex));
        else
            LOG("      Tex[%s]: '%s' LOAD FAILED", typeName.c_str(), filename.c_str());
    }

    return textures;
}
