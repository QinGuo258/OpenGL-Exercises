#pragma once

#include <glm/glm.hpp>
#include <string>
#include <map>
#include <memory>
#include "Shader.h"
#include "Model.h"
#include "Animation.h"

class CollisionWorld;

enum class PlayerState { IDLE, WALK, RUN, SNEAK_IDLE, SNEAK_WALK };

class Player
{
public:
    Player(const std::string& modelPath);
    ~Player() = default;

    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;
    Player(Player&&) = delete;
    Player& operator=(Player&&) = delete;

    void Draw(Shader& shader);
    void UpdatePhysics(float deltaTime, const CollisionWorld& collisionWorld);
    void UpdateAnimation(float deltaTime);
    void UpdateMovementState(bool isMoving, bool isMovingForward = false);
    void TryAttack();
    void Jump();

    static constexpr float CollisionRadius = 0.4f;
    static constexpr float CollisionHeight = 1.8f;

    glm::vec3 Position = glm::vec3(-20.0f, -7.0f, -13.0f);
    float RotationYaw = 0.0f;
    float MovementSpeed = 5.0f;

    glm::vec3 Velocity = glm::vec3(0.0f);
    float Gravity = -19.6f;
    float JumpForce = 6.0f;
    bool isGrounded = false;
    bool IsMoving = false;
    bool IsRunningMode = false;
    bool IsSneaking = false;
    bool IsEating = false;

    Animator& GetAnimator() { return m_Animator; }
    Model& GetModel() { return m_Model; }
    PlayerState GetCurrentState() const { return m_CurrentState; }
    float GetVisualYOffset() const { return m_VisualYOffset; }

    glm::mat4 GetModelMatrix() const;
    glm::mat4 GetBoneTransform(const std::string& name) const;

private:
    Model m_Model;
    std::map<std::string, std::shared_ptr<Animation>> m_Animations;
    Animator m_Animator;
    PlayerState m_CurrentState = PlayerState::IDLE;
    float m_VisualYOffset = 0.0f;
    float m_TargetYOffset = 0.0f;
};
