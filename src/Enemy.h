#pragma once
#include <glm/glm.hpp>
#include <string>
#include <map>
#include <memory>
#include "Model.h"
#include "Animation.h"
#include "Shader.h"

class CollisionWorld;

enum class EnemyType { ZOMBIE, SKELETON };

class Enemy {
public:
    EnemyType Type;
    glm::vec3 Position;
    glm::vec3 Velocity;
    float RotationYaw;
    float MovementSpeed = 2.5f;
    int Health = 20;
    bool IsGrounded = false;
    bool IsBlockedHorizontally = false;
    float StunTimer = 0.0f;          // 受击硬直倒计时
    float AttackCooldown = 0.0f;      // 攻击冷却
    float RangedAttackTimer = 0.0f;   // 骷髅射击倒计时
    float AttackAnimTimer = 0.0f;      // 攻击动画硬直计时（骷髅）

    Enemy(EnemyType type, glm::vec3 startPos, Model* model,
          const std::map<std::string, std::shared_ptr<Animation>>& anims);

    void UpdateAI(float dt, const glm::vec3& playerPos);
    void UpdatePhysics(float dt, const CollisionWorld& collisionWorld);
    void Draw(Shader& shader);
    void TakeDamage(int damage, const glm::vec3& knockback);

private:
    Model* m_Model;
    Animator m_Animator;
    std::map<std::string, std::shared_ptr<Animation>> m_Animations;
    std::string m_CurrentAnim;

    void PlayAnim(const std::string& animName);
    void PlayOverlayAnim(const std::string& animName);
    void ClearOverlayAnim();
    bool m_WarnedAnim = false; // 每实例独立动画丢失警告

    static constexpr float CollisionRadius = 0.4f;
    static constexpr float CollisionHeight = 1.8f;
};
