#include "Arrow.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include "miniaudio.h"

extern ma_engine audioEngine;
extern int playerHP;
extern float playerImmunityTimer;

Arrow::Arrow(glm::vec3 startPos, glm::vec3 velocity)
    : Position(startPos), Velocity(velocity)
{
}

void Arrow::UpdatePhysics(float dt, const CollisionWorld& collisionWorld,
                          const glm::vec3& playerPos)
{
    Life -= dt;
    if (IsStuck) return;

    // 0. 检测是否击中玩家（飞行中，距离 < 1.0m 且高度在身体范围内）
    float distToPlayer = glm::distance(
        glm::vec2(Position.x, Position.z),
        glm::vec2(playerPos.x, playerPos.z));
    if (distToPlayer < 1.0f &&
        Position.y > playerPos.y && Position.y < playerPos.y + 1.8f &&
        playerImmunityTimer <= 0.0f)
    {
        playerHP -= 2;
        playerImmunityTimer = 0.5f;
        ma_engine_play_sound(&audioEngine, "audio/Weak_attack1.mp3", NULL);
        Life = 0.0f; // 箭矢销毁
        return;
    }

    // 1. 重力下坠
    Velocity.y += -9.8f * dt;
    glm::vec3 stepVec = Velocity * dt;
    float stepDist = glm::length(stepVec);
    if (stepDist < 0.0001f) return;
    glm::vec3 dir = stepVec / stepDist;

    // 2. 射线检测：箭矢是否撞墙
    float hitDist = 0.0f;
    bool hit = collisionWorld.Raycast(Position, dir, stepDist, hitDist);

    if (hit)
    {
        // 插在墙上
        Position += dir * hitDist;
        IsStuck = true;
        ma_engine_play_sound(&audioEngine, "audio/Arrow_hit1.mp3", NULL);
    }
    else
    {
        // 继续飞行
        Position += stepVec;

        // 实时更新飞行姿态（指向运动方向，模型面朝 -Z）
        Yaw   = std::atan2(-dir.x, -dir.z);
        Pitch = std::asin(dir.y);
    }
}

void Arrow::Draw(Shader& shader, Model* model)
{
    glm::mat4 modelMat = glm::mat4(1.0f);
    modelMat = glm::translate(modelMat, Position);

    // 旋转以对齐飞行/插墙方向
    modelMat = glm::rotate(modelMat, Yaw,   glm::vec3(0.0f, 1.0f, 0.0f));
    modelMat = glm::rotate(modelMat, Pitch, glm::vec3(1.0f, 0.0f, 0.0f));

    modelMat = glm::scale(modelMat, glm::vec3(0.8f));

    shader.SetMat4("uModel", modelMat);

    // 清除骨骼形变影响（箭是静态模型）
    for (int i = 0; i < 100; ++i)
    {
        shader.SetMat4("finalBonesMatrices[" + std::to_string(i) + "]", glm::mat4(1.0f));
    }
    shader.SetInt("uMaterialType", 0);

    model->Draw(shader.ID());
}
