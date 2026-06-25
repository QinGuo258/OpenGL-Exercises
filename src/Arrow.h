#pragma once
#include <glm/glm.hpp>
#include "Model.h"
#include "Shader.h"
#include "CollisionWorld.h"

class Arrow {
public:
    glm::vec3 Position;
    glm::vec3 Velocity;
    bool IsStuck = false;
    float Life = 10.0f; // 存在 10 秒后消失

    // 插墙/飞行时的姿态角（弧度）
    float Yaw = 0.0f;
    float Pitch = 0.0f;

    Arrow(glm::vec3 startPos, glm::vec3 velocity);

    void UpdatePhysics(float dt, const CollisionWorld& collisionWorld,
                       const glm::vec3& playerPos);
    void Draw(Shader& shader, Model* model);
};
