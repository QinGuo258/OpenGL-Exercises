#pragma once

#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include "Model.h"

struct KeyPosition
{
    glm::vec3 position;
    float timeStamp;
};

struct KeyRotation
{
    glm::quat rotation;
    float timeStamp;
};

struct KeyScale
{
    glm::vec3 scale;
    float timeStamp;
};

class Bone
{
public:
    Bone() = default;
    Bone(const std::string& name, int id, const aiNodeAnim* channel,
         const glm::vec3& defaultPosition = glm::vec3(0.0f),
         const glm::vec3& defaultScale = glm::vec3(1.0f));

    glm::mat4 Update(float animationTime);
    glm::mat4 GetLocalTransform() const { return m_LocalTransform; }

    std::string GetName() const { return m_Name; }
    int GetBoneID() const { return m_ID; }

private:
    int GetPositionIndex(float animationTime) const;
    int GetRotationIndex(float animationTime) const;
    int GetScaleIndex(float animationTime) const;

    float ScaleFactor(float lastTime, float nextTime, float currentTime) const;
    glm::mat4 InterpolatePosition(float animationTime);
    glm::mat4 InterpolateRotation(float animationTime);
    glm::mat4 InterpolateScale(float animationTime);

    std::string m_Name;
    int m_ID = -1;
    std::vector<KeyPosition> m_Positions;
    std::vector<KeyRotation> m_Rotations;
    std::vector<KeyScale> m_Scales;
    glm::mat4 m_LocalTransform = glm::mat4(1.0f);
    glm::vec3 m_DefaultPosition = glm::vec3(0.0f);
    glm::vec3 m_DefaultScale = glm::vec3(1.0f);
};

struct AssimpNodeData
{
    glm::mat4 transformation;
    std::string name;
    std::vector<AssimpNodeData> children;
};

class Animation
{
public:
    Animation(const std::string& filePath, Model& model);
    Animation(const aiAnimation* anim, const aiNode* rootNode, Model& model);

    static std::map<std::string, std::shared_ptr<Animation>> LoadAll(
        const std::string& filePath, Model& model);

    Bone* FindBone(const std::string& name);
    const std::vector<Bone>& GetBones() const { return m_Bones; }

    float GetDuration() const { return m_Duration; }
    float GetTicksPerSecond() const { return m_TicksPerSecond; }
    const AssimpNodeData& GetRootNode() const { return m_RootNode; }
    const std::map<std::string, BoneInfo>& GetBoneInfoMap() const { return m_ModelRef.GetBoneInfoMap(); }

private:
    void Init(const aiAnimation* anim, const aiNode* rootNode);
    void ReadMissingBones(const aiAnimation* animation, Model& model);
    void ReadHierarchyData(AssimpNodeData& dest, const aiNode* src);

    Model& m_ModelRef;
    std::vector<Bone> m_Bones;
    AssimpNodeData m_RootNode;
    float m_Duration = 0.0f;
    float m_TicksPerSecond = 25.0f;
};

class Animator
{
public:
    Animator() = default;
    Animator(std::shared_ptr<Animation> animation);

    void UpdateAnimation(float dt);
    void PlayAnimation(std::shared_ptr<Animation> pAnimation);
    void PlayOverlay(std::shared_ptr<Animation> anim);
    void ClearOverlay() { m_OverlayAnimation = nullptr; }
    bool IsOverlayPlaying() const { return m_OverlayAnimation != nullptr; }
    const std::vector<glm::mat4>& GetFinalBoneMatrices() const { return m_FinalBoneMatrices; }

    float GetCurrentTime() const { return m_CurrentTime; }
    void SetCurrentTime(float time) { m_CurrentTime = time; }

    void SetHeadRotation(float pitch, float yaw);
    void SetHideHead(bool hide) { m_HideHead = hide; }

    glm::mat4 GetNodeGlobalTransform(const std::string& name) const
    {
        auto it = m_NodeGlobalTransforms.find(name);
        return (it != m_NodeGlobalTransforms.end()) ? it->second : glm::mat4(1.0f);
    }

private:
    void CalculateBoneTransform(const AssimpNodeData* node, const glm::mat4& parentTransform,
                                std::unordered_set<std::string>& appliedBones);
    void ResizeBoneMatrices();

    std::shared_ptr<Animation> m_Animation;
    std::shared_ptr<Animation> m_OverlayAnimation;
    float m_CurrentTime = 0.0f;
    float m_OverlayTime = 0.0f;
    float m_HeadPitch = 0.0f;
    float m_HeadYaw = 0.0f;
    bool m_HideHead = false;
    std::vector<glm::mat4> m_FinalBoneMatrices;
    std::unordered_map<std::string, glm::mat4> m_NodeGlobalTransforms;
};
