#include "Animation.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>
#include <cmath>
#include <cctype>

extern FILE* gLogFile;

#define LOG(fmt, ...) do { \
    if (gLogFile) { fprintf(gLogFile, fmt "\n", ##__VA_ARGS__); fflush(gLogFile); } \
} while(0)

// ============================================================================
// Bone
// ============================================================================

Bone::Bone(const std::string& name, int id, const aiNodeAnim* channel,
           const glm::vec3& defaultPosition, const glm::vec3& defaultScale)
    : m_Name(name), m_ID(id), m_DefaultPosition(defaultPosition), m_DefaultScale(defaultScale)
{
    for (unsigned int i = 0; i < channel->mNumPositionKeys; i++)
    {
        KeyPosition key;
        key.position = glm::vec3(channel->mPositionKeys[i].mValue.x,
                                 channel->mPositionKeys[i].mValue.y,
                                 channel->mPositionKeys[i].mValue.z);
        key.timeStamp = static_cast<float>(channel->mPositionKeys[i].mTime);
        m_Positions.push_back(key);
    }

    for (unsigned int i = 0; i < channel->mNumRotationKeys; i++)
    {
        KeyRotation key;
        key.rotation = glm::quat(channel->mRotationKeys[i].mValue.w,
                                 channel->mRotationKeys[i].mValue.x,
                                 channel->mRotationKeys[i].mValue.y,
                                 channel->mRotationKeys[i].mValue.z);
        key.timeStamp = static_cast<float>(channel->mRotationKeys[i].mTime);
        m_Rotations.push_back(key);
    }

    for (unsigned int i = 0; i < channel->mNumScalingKeys; i++)
    {
        KeyScale key;
        key.scale = glm::vec3(channel->mScalingKeys[i].mValue.x,
                              channel->mScalingKeys[i].mValue.y,
                              channel->mScalingKeys[i].mValue.z);
        key.timeStamp = static_cast<float>(channel->mScalingKeys[i].mTime);
        m_Scales.push_back(key);
    }
}

glm::mat4 Bone::Update(float animationTime)
{
    glm::mat4 translation = InterpolatePosition(animationTime);
    glm::mat4 rotation = InterpolateRotation(animationTime);
    glm::mat4 scale = InterpolateScale(animationTime);
    m_LocalTransform = translation * rotation * scale;
    return m_LocalTransform;
}

int Bone::GetPositionIndex(float animationTime) const
{
    for (int i = 0; i < static_cast<int>(m_Positions.size()) - 1; i++)
    {
        if (animationTime < m_Positions[i + 1].timeStamp)
            return i;
    }
    return 0;
}

int Bone::GetRotationIndex(float animationTime) const
{
    for (int i = 0; i < static_cast<int>(m_Rotations.size()) - 1; i++)
    {
        if (animationTime < m_Rotations[i + 1].timeStamp)
            return i;
    }
    return 0;
}

int Bone::GetScaleIndex(float animationTime) const
{
    for (int i = 0; i < static_cast<int>(m_Scales.size()) - 1; i++)
    {
        if (animationTime < m_Scales[i + 1].timeStamp)
            return i;
    }
    return 0;
}

float Bone::ScaleFactor(float lastTime, float nextTime, float currentTime) const
{
    float diff = nextTime - lastTime;
    if (diff < 0.0001f) return 0.0f;
    return (currentTime - lastTime) / diff;
}

glm::mat4 Bone::InterpolatePosition(float animationTime)
{
    // 动画没有位置关键帧 → 回退到绑定姿态的默认平移（防止关节脱落归零）
    if (m_Positions.empty())
        return glm::translate(glm::mat4(1.0f), m_DefaultPosition);

    if (m_Positions.size() == 1)
        return glm::translate(glm::mat4(1.0f), m_Positions[0].position);

    int idx = GetPositionIndex(animationTime);
    int nextIdx = idx + 1;
    float factor = ScaleFactor(m_Positions[idx].timeStamp,
                               m_Positions[nextIdx].timeStamp, animationTime);
    glm::vec3 result = glm::mix(m_Positions[idx].position,
                                m_Positions[nextIdx].position, factor);
    return glm::translate(glm::mat4(1.0f), result);
}

glm::mat4 Bone::InterpolateRotation(float animationTime)
{
    // 动画没有旋转关键帧 → 返回单位旋转（防御性编程）
    if (m_Rotations.empty())
        return glm::mat4(1.0f);

    if (m_Rotations.size() == 1)
    {
        auto rot = glm::normalize(m_Rotations[0].rotation);
        return glm::mat4_cast(rot);
    }

    int idx = GetRotationIndex(animationTime);
    int nextIdx = idx + 1;
    float factor = ScaleFactor(m_Rotations[idx].timeStamp,
                               m_Rotations[nextIdx].timeStamp, animationTime);
    glm::quat result = glm::slerp(m_Rotations[idx].rotation,
                                   m_Rotations[nextIdx].rotation, factor);
    result = glm::normalize(result);
    return glm::mat4_cast(result);
}

glm::mat4 Bone::InterpolateScale(float animationTime)
{
    // 动画没有缩放关键帧 → 回退到绑定姿态的默认缩放（防止模型坍塌）
    if (m_Scales.empty())
        return glm::scale(glm::mat4(1.0f), m_DefaultScale);

    if (m_Scales.size() == 1)
        return glm::scale(glm::mat4(1.0f), m_Scales[0].scale);

    int idx = GetScaleIndex(animationTime);
    int nextIdx = idx + 1;
    float factor = ScaleFactor(m_Scales[idx].timeStamp,
                               m_Scales[nextIdx].timeStamp, animationTime);
    glm::vec3 result = glm::mix(m_Scales[idx].scale,
                                m_Scales[nextIdx].scale, factor);
    return glm::scale(glm::mat4(1.0f), result);
}

static void LogHierarchy(const AssimpNodeData& node, int depth);

// ============================================================================
// Animation
// ============================================================================

Animation::Animation(const std::string& filePath, Model& model)
    : m_ModelRef(model)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath,
        aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace);

    if (!scene || !scene->mRootNode)
    {
        LOG("ERROR::ANIMATION: Failed to load %s", filePath.c_str());
        return;
    }

    const aiAnimation* animation = nullptr;
    for (unsigned int i = 0; i < scene->mNumAnimations; i++)
    {
        std::string animName = scene->mAnimations[i]->mName.C_Str();
        // 剥离骨架前缀（| 之前的部分），取 | 之后的动画名
        std::string shortName = animName;
        size_t pipePos = animName.find('|');
        if (pipePos != std::string::npos)
            shortName = animName.substr(pipePos + 1);
        if (shortName == "walk")
        {
            animation = scene->mAnimations[i];
            LOG("Animation: Found 'walk' animation (full name: '%s')", animName.c_str());
            break;
        }
    }
    if (!animation && scene->mNumAnimations > 0)
    {
        animation = scene->mAnimations[0];
        LOG("Animation: Using first animation: '%s'", animation->mName.C_Str());
    }
    if (!animation)
    {
        LOG("WARNING: No animations found in %s", filePath.c_str());
        return;
    }

    Init(animation, scene->mRootNode);
}

Animation::Animation(const aiAnimation* anim, const aiNode* rootNode, Model& model)
    : m_ModelRef(model)
{
    Init(anim, rootNode);
}

void Animation::Init(const aiAnimation* anim, const aiNode* rootNode)
{
    if (!anim)
    {
        LOG("ERROR::ANIMATION: null aiAnimation pointer");
        return;
    }

    m_Duration = static_cast<float>(anim->mDuration);
    m_TicksPerSecond = static_cast<float>(anim->mTicksPerSecond);
    if (m_TicksPerSecond < 0.001f)
        m_TicksPerSecond = 25.0f;

    LOG("Animation: Duration=%.2f ticks, TPS=%.1f, Channels=%u",
        m_Duration, m_TicksPerSecond, anim->mNumChannels);

    ReadMissingBones(anim, m_ModelRef);

    // 递归查找 aiNode 的辅助 lambda
    auto FindAiNode = [](const aiNode* node, const std::string& name, auto&& self) -> const aiNode*
    {
        if (!node) return nullptr;
        if (name == node->mName.C_Str()) return node;
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            const aiNode* found = self(node->mChildren[i], name, self);
            if (found) return found;
        }
        return nullptr;
    };

    for (unsigned int i = 0; i < anim->mNumChannels; i++)
    {
        aiNodeAnim* channel = anim->mChannels[i];
        std::string boneName = channel->mNodeName.C_Str();

        // 从 aiNode 树中提取该骨骼在绑定姿态下的默认平移和缩放
        // 这样即使动画只有旋转关键帧（mNumPositionKeys==0），手臂关节也不会脱落归零
        glm::vec3 defaultPos(0.0f);
        glm::vec3 defaultScale(1.0f);
        if (rootNode)
        {
            const aiNode* node = FindAiNode(rootNode, boneName, FindAiNode);
            if (node)
            {
                aiVector3D aiPos, aiScale;
                aiQuaternion aiRot;
                node->mTransformation.Decompose(aiScale, aiRot, aiPos);
                defaultPos = glm::vec3(aiPos.x, aiPos.y, aiPos.z);
                defaultScale = glm::vec3(aiScale.x, aiScale.y, aiScale.z);
            }
        }

        auto& boneInfoMap = m_ModelRef.GetBoneInfoMap();
        int boneID = boneInfoMap[boneName].id;
        m_Bones.emplace_back(boneName, boneID, channel, defaultPos, defaultScale);
    }
    LOG("Animation: %zu bones loaded", m_Bones.size());

    if (rootNode)
        ReadHierarchyData(m_RootNode, rootNode);
    LOG("Animation node hierarchy:");
    LogHierarchy(m_RootNode, 0);
}

std::map<std::string, std::shared_ptr<Animation>> Animation::LoadAll(
    const std::string& filePath, Model& model)
{
    std::map<std::string, std::shared_ptr<Animation>> result;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath,
        aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace);

    if (!scene || !scene->mRootNode)
    {
        LOG("ERROR::ANIMATION::LoadAll: Failed to load %s", filePath.c_str());
        return result;
    }

    LOG("Animation::LoadAll: Found %u animation(s) in '%s'",
        scene->mNumAnimations, filePath.c_str());

    for (unsigned int i = 0; i < scene->mNumAnimations; i++)
    {
        const aiAnimation* aiAnim = scene->mAnimations[i];
        std::string rawName = aiAnim->mName.C_Str();

        // 剥离骨架前缀（| 之前的部分），取 | 之后的动画名
        std::string shortName = rawName;
        size_t pipePos = rawName.find('|');
        if (pipePos != std::string::npos)
            shortName = rawName.substr(pipePos + 1);

        // 剥离 Blender 自动添加的 .NNN 数字后缀（如 "idle.001" → "idle"）
        {
            size_t lastDot = shortName.rfind('.');
            if (lastDot != std::string::npos && lastDot + 1 < shortName.size())
            {
                bool allDigits = true;
                for (size_t j = lastDot + 1; j < shortName.size(); j++)
                {
                    if (!std::isdigit(static_cast<unsigned char>(shortName[j])))
                    {
                        allDigits = false;
                        break;
                    }
                }
                if (allDigits)
                    shortName = shortName.substr(0, lastDot);
            }
        }

        // 转为小写
        std::string key;
        for (char c : shortName)
            key += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        LOG("  Animation[%u]: raw='%s' → key='%s'", i, rawName.c_str(), key.c_str());

        auto anim = std::make_shared<Animation>(aiAnim, scene->mRootNode, model);

        if (result.find(key) != result.end())
        {
            LOG("  ⚠ DUPLICATE KEY: '%s' already exists, overwriting (raw='%s')",
                key.c_str(), rawName.c_str());
        }
        result[key] = anim;
    }

    LOG("Animation::LoadAll: %zu animation(s) loaded", result.size());
    return result;
}

Bone* Animation::FindBone(const std::string& name)
{
    for (auto& bone : m_Bones)
    {
        if (bone.GetName() == name)
            return &bone;
    }
    return nullptr;
}

void Animation::ReadMissingBones(const aiAnimation* animation, Model& model)
{
    auto& boneInfoMap = model.GetBoneInfoMap();
    int& boneCount = model.GetBoneCount();

    for (unsigned int i = 0; i < animation->mNumChannels; i++)
    {
        std::string boneName = animation->mChannels[i]->mNodeName.C_Str();
        if (boneInfoMap.find(boneName) == boneInfoMap.end())
        {
            BoneInfo info;
            info.id = boneCount;
            info.offset = glm::mat4(1.0f);
            boneInfoMap[boneName] = info;
            boneCount++;
            LOG("Animation: Added missing bone '%s' (id=%d)", boneName.c_str(), info.id);
        }
    }
}

static void LogHierarchy(const AssimpNodeData& node, int depth)
{
    std::string indent(depth * 2, ' ');
    LOG("%sNode: '%s' children=%zu", indent.c_str(), node.name.c_str(), node.children.size());
    for (const auto& child : node.children)
        LogHierarchy(child, depth + 1);
}

void Animation::ReadHierarchyData(AssimpNodeData& dest, const aiNode* src)
{
    dest.name = src->mName.C_Str();
    dest.transformation = AiMatrixToGlm(src->mTransformation);
    dest.children.resize(src->mNumChildren);
    for (unsigned int i = 0; i < src->mNumChildren; i++)
        ReadHierarchyData(dest.children[i], src->mChildren[i]);
}

// ============================================================================
// Animator
// ============================================================================

Animator::Animator(std::shared_ptr<Animation> animation)
    : m_Animation(animation)
{
    ResizeBoneMatrices();
}

void Animator::PlayAnimation(std::shared_ptr<Animation> pAnimation)
{
    if (m_Animation != pAnimation)
    {
        m_Animation = pAnimation;
        m_CurrentTime = 0.0f;
        ResizeBoneMatrices();
    }
}

void Animator::PlayOverlay(std::shared_ptr<Animation> anim)
{
    m_OverlayAnimation = anim;
    m_OverlayTime = 0.0f;
}

void Animator::ResizeBoneMatrices()
{
    if (!m_Animation) return;

    int maxBones = 100;
    auto& boneInfoMap = m_Animation->GetBoneInfoMap();
    for (auto& pair : boneInfoMap)
    {
        if (pair.second.id >= maxBones)
            maxBones = pair.second.id + 1;
    }
    m_FinalBoneMatrices.resize(maxBones, glm::mat4(1.0f));
}

void Animator::UpdateAnimation(float dt)
{
    if (!m_Animation || m_Animation->GetDuration() <= 0.0f)
        return;

    float tps = m_Animation->GetTicksPerSecond();
    m_CurrentTime += dt * tps;
    float duration = m_Animation->GetDuration();
    m_CurrentTime = std::fmod(m_CurrentTime, duration);

    m_NodeGlobalTransforms.clear();

    std::unordered_set<std::string> appliedBones;
    CalculateBoneTransform(&m_Animation->GetRootNode(), glm::mat4(1.0f), appliedBones);

    // 推进覆盖层（Overlay）时间；播放完毕后自动清除，将控制权还给 Base 轨道
    if (m_OverlayAnimation)
    {
        m_OverlayTime += dt * m_OverlayAnimation->GetTicksPerSecond();
        if (m_OverlayTime >= m_OverlayAnimation->GetDuration())
        {
            m_OverlayAnimation = nullptr;
            m_OverlayTime = 0.0f;
        }
    }
}

void Animator::SetHeadRotation(float pitch, float yaw)
{
    m_HeadPitch = pitch;
    m_HeadYaw = yaw;
}

void Animator::CalculateBoneTransform(const AssimpNodeData* node, const glm::mat4& parentTransform,
                                      std::unordered_set<std::string>& appliedBones)
{
    // 兜底：绑定姿态的默认局部变换
    glm::mat4 baseTransform = node->transformation;

    // 1. 获取 Base 轨道（下半身/全身的正确位置与旋转）
    if (appliedBones.find(node->name) == appliedBones.end())
    {
        Bone* bone = m_Animation->FindBone(node->name);
        if (bone)
        {
            baseTransform = bone->Update(m_CurrentTime);
            appliedBones.insert(node->name);
        }
    }

    std::string nodeName = node->name;
    glm::mat4 finalNodeTransform = baseTransform;

    // 2. 获取 Overlay 轨道（仅接管手臂的拉弓旋转）
    if (m_OverlayAnimation)
    {
        Bone* overlayBone = m_OverlayAnimation->FindBone(nodeName);
        if (overlayBone)
        {
            overlayBone->Update(m_OverlayTime);
            glm::mat4 overlayTransform = overlayBone->GetLocalTransform();

            // ============【终极修复：矩阵列注入】============
            // 提取模型文件里最原始的、静止的肩膀坐标，塞给 Overlay 矩阵！
            // overlayTransform 的 [0][1][2] 列 = 旋转/缩放，[3] 列 = 平移
            // 这样 Overlay 就只能影响手臂的旋转(R)和缩放(S)，绝对无法导致位置(T)脱臼！
            overlayTransform[3] = node->transformation[3];
            // =================================================

            finalNodeTransform = overlayTransform;
        }
    }

    bool isHead = (nodeName.find("Head") != std::string::npos ||
                   nodeName.find("head") != std::string::npos);

    if (isHead)
    {
        float clampedPitch = glm::clamp(m_HeadPitch, -89.9f, 89.9f);

        finalNodeTransform = glm::rotate(finalNodeTransform, glm::radians(m_HeadYaw),   glm::vec3(0.0f, 1.0f, 0.0f));
        finalNodeTransform = glm::rotate(finalNodeTransform, glm::radians(clampedPitch), glm::vec3(1.0f, 0.0f, 0.0f));

        if (m_HideHead)
            finalNodeTransform = glm::scale(finalNodeTransform, glm::vec3(0.0001f));
    }

    glm::mat4 globalTransform = parentTransform * finalNodeTransform;

    m_NodeGlobalTransforms[node->name] = globalTransform;

    auto& boneInfoMap = m_Animation->GetBoneInfoMap();
    auto it = boneInfoMap.find(node->name);
    if (it != boneInfoMap.end())
    {
        int index = it->second.id;
        if (index < static_cast<int>(m_FinalBoneMatrices.size()))
            m_FinalBoneMatrices[index] = globalTransform * it->second.offset;
    }

    for (const auto& child : node->children)
        CalculateBoneTransform(&child, globalTransform, appliedBones);
}
