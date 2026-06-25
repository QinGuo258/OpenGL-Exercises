#include "Player.h"
#include "CollisionWorld.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>

extern FILE* gLogFile;

#define LOG(fmt, ...) do { \
    if (gLogFile) { fprintf(gLogFile, fmt "\n", ##__VA_ARGS__); fflush(gLogFile); } \
} while(0)

Player::Player(const std::string& modelPath)
    : m_Model(modelPath)
{
    m_Animations = Animation::LoadAll(modelPath, m_Model);

    // 初始化为 idle 动画
    auto it = m_Animations.find("idle");
    if (it != m_Animations.end())
    {
        m_Animator.PlayAnimation(it->second);
        LOG("Player: Initial animation set to 'idle'");
    }
    else if (!m_Animations.empty())
    {
        m_Animator.PlayAnimation(m_Animations.begin()->second);
        LOG("Player: 'idle' not found, using '%s' as default",
            m_Animations.begin()->first.c_str());
    }
    else
    {
        LOG("WARNING: No animations loaded for player model '%s'", modelPath.c_str());
    }
}

void Player::Draw(Shader& shader)
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(Position.x, Position.y + m_VisualYOffset, Position.z));
    model = glm::rotate(model, glm::radians(RotationYaw + 180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    shader.SetMat4("uModel", model);

    // 防止主角受击闪红（uIsHit 只有僵尸使用）
    shader.SetBool("uIsHit", false);

    const auto& matrices = m_Animator.GetFinalBoneMatrices();
    for (size_t i = 0; i < matrices.size(); i++)
    {
        std::string name = "finalBonesMatrices[" + std::to_string(i) + "]";
        shader.SetMat4(name, matrices[i]);
    }

    m_Model.Draw(shader.ID());
}

void Player::UpdatePhysics(float deltaTime, const CollisionWorld& collisionWorld)
{
    // ======================================================================
    // 步骤 1：重力积分 + 预估新位置
    // ======================================================================
    Velocity.y += Gravity * deltaTime;
    Position += Velocity * deltaTime;

    // 水平速度仅作为瞬时冲量（击退等），积分后立即衰减归零
    Velocity.x *= 0.1f;
    Velocity.z *= 0.1f;

    // ======================================================================
    // 步骤 2：垂直地板检测（必须先于水平推斥，确保 Y 坐标精确）
    // ======================================================================
    float floorQueryRadius = glm::max(CollisionRadius, 2.0f);
    auto floorTris = collisionWorld.GetTrianglesNearXZ(Position.x, Position.z, floorQueryRadius);

    float floorY = -FLT_MAX;

    for (const Triangle* tri : floorTris)
    {
        if (tri->normal.y <= 0.1f)
            continue;

        float h = TriangleHeightAtXZ(Position.x, Position.z, *tri);
        if (h < -1e9f)
            continue;

        // 动态向下补偿：如果下落速度极快，加大向上寻找地板的容差范围，防止高速穿模
        float fallCompensation = std::max(0.0f, -Velocity.y * deltaTime);
        float searchTop = Position.y + 0.5f + fallCompensation;
        if (h > searchTop)
            continue;

        if (h > floorY)
            floorY = h;
    }

    if (floorY > -1e9f && Position.y <= floorY)
    {
        Position.y = floorY;
        Velocity.y = 0.0f;
        isGrounded = true;
    }
    else
    {
        isGrounded = false;
    }

    // ======================================================================
    // 步骤 3：水平墙壁推斥（XZ 平面）—— 此时 Y 坐标已精确修正
    // ======================================================================
    {
        float queryRadius = CollisionRadius + 0.1f;
        auto triangles = collisionWorld.GetTrianglesNearXZ(Position.x, Position.z, queryRadius);

        for (const Triangle* tri : triangles)
        {
            // 规则 1a：必须是竖直墙壁
            if (std::fabs(tri->normal.y) >= 0.1f)
                continue;

            // 规则 1b：必须轴对齐（法线平行于 X 或 Z 轴）
            if (std::fabs(tri->normal.x) < 0.9f && std::fabs(tri->normal.z) < 0.9f)
                continue;

            // 规则 2：低矮障碍过滤（台阶/接缝，留给跨步容差）
            if (tri->maxY <= Position.y + 0.60f)
                continue;

            // Y 轴范围：墙壁必须在玩家圆柱体高度范围内
            if (tri->maxY < Position.y || tri->minY > Position.y + CollisionHeight)
                continue;

            // ---- 有限矩形面的最近点 + 平滑推斥（解决拐角弹射） ----
            float closestX, closestZ;

            if (std::fabs(tri->normal.x) > 0.9f)
            {
                // 法线平行于 X 轴 → 墙面沿 Z 轴延伸
                closestX = tri->v0.x;
                closestZ = glm::clamp(Position.z, tri->minZ, tri->maxZ);
            }
            else // abs(normal.z) > 0.9f
            {
                // 法线平行于 Z 轴 → 墙面沿 X 轴延伸
                closestX = glm::clamp(Position.x, tri->minX, tri->maxX);
                closestZ = tri->v0.z;
            }

            float diffX = Position.x - closestX;
            float diffZ = Position.z - closestZ;
            float dist = std::sqrt(diffX * diffX + diffZ * diffZ);

            if (dist >= CollisionRadius)
                continue;

            if (dist > 0.001f)
            {
                // 从最近点指向玩家中心的方向（在拐角处自然斜向，提供圆弧滑出）
                float pushDirX = diffX / dist;
                float pushDirZ = diffZ / dist;
                float push = CollisionRadius - dist;
                Position.x += pushDirX * push;
                Position.z += pushDirZ * push;
            }
            else
            {
                // 圆心完全嵌入墙面 — 沿法线硬推出
                Position.x += tri->normal.x * CollisionRadius;
                Position.z += tri->normal.z * CollisionRadius;
            }
        }
    }

    // ======================================================================
    // 步骤 4：跌落保护
    // ======================================================================
    if (Position.y < -50.0f)
    {
        Position = glm::vec3(0.0f, 50.0f, 0.0f);
        Velocity = glm::vec3(0.0f);
        isGrounded = false;
        printf("[Physics] Fell below world — reset to spawn\n");
    }

    // ======================================================================
    // 步骤 5：调试打印（每 60 帧）
    // ======================================================================
    {
        static int frameCounter = 0;
        if (++frameCounter % 60 == 0)
        {
            printf("[Physics] Pos: %.2f, %.2f, %.2f | FloorY: %.2f | Grounded: %d\n",
                   Position.x, Position.y, Position.z, floorY, isGrounded ? 1 : 0);
        }
    }

    // ======================================================================
    // 步骤 6：视觉 Y 偏移平滑（潜行下降 / 站立恢复）
    // ======================================================================
    float t = glm::clamp(10.0f * deltaTime, 0.0f, 1.0f);
    m_VisualYOffset = glm::mix(m_VisualYOffset, m_TargetYOffset, t);
}


void Player::UpdateAnimation(float deltaTime)
{
    // 始终推进时间，确保 Overlay（攻击）动画也能在待机时播放
    m_Animator.UpdateAnimation(deltaTime);
}

void Player::UpdateMovementState(bool isMoving, bool isMovingForward)
{
    PlayerState targetState;
    float currentSpeed = 0.0f;
    float baseSpeed = 4.0f;

    // 是否真正疾跑：奔跑模式下按下 W 前进 + 不在潜行/进食状态
    bool isActuallySprinting = IsRunningMode && isMoving && isMovingForward && !IsSneaking && !IsEating;

    if (IsSneaking)
    {
        if (isMoving)
        {
            targetState = PlayerState::SNEAK_WALK;
            currentSpeed = baseSpeed * 0.5f;
        }
        else
        {
            targetState = PlayerState::SNEAK_IDLE;
        }
    }
    else if (IsEating && isMoving)
    {
        // 进食时移动速度降至疾跑速度的 20%
        targetState = PlayerState::WALK;
        currentSpeed = baseSpeed * 2.0f * 0.2f;
    }
    else if (isActuallySprinting)
    {
        // 疾跑模式下按 W 前进：跑步动画 + 疾跑速度
        targetState = PlayerState::RUN;
        currentSpeed = baseSpeed * 2.0f;
    }
    else
    {
        if (isMoving)
        {
            // 疾跑模式下按 S/A/D 或组合键（不含 W）：行走速度 + 行走动画
            targetState = PlayerState::WALK;
            currentSpeed = baseSpeed;
        }
        else
        {
            targetState = PlayerState::IDLE;
        }
    }

    MovementSpeed = currentSpeed;

    // 潜行时模型视觉下降
    m_TargetYOffset = IsSneaking ? -0.08f : 0.0f;

    if (m_CurrentState != targetState)
    {
        m_CurrentState = targetState;

        auto play = [this](const std::string& key) {
            auto it = m_Animations.find(key);
            if (it != m_Animations.end())
                m_Animator.PlayAnimation(it->second);
            else
                LOG("WARNING: Animation '%s' not found in m_Animations", key.c_str());
        };

        switch (m_CurrentState)
        {
        case PlayerState::IDLE:        play("idle");        break;
        case PlayerState::WALK:        play("walk");        break;
        case PlayerState::RUN:         play("run");         break;
        case PlayerState::SNEAK_IDLE:  play("sneak_idle");  break;
        case PlayerState::SNEAK_WALK:  play("sneak_walk");  break;
        }
    }
}

void Player::TryAttack()
{
    if (m_Animator.IsOverlayPlaying())
        return;

    std::string animKey = IsSneaking ? "sneak_attack" : "attack";
    auto it = m_Animations.find(animKey);
    if (it != m_Animations.end())
        m_Animator.PlayOverlay(it->second);
    else
        LOG("WARNING: Attack animation '%s' not found", animKey.c_str());
}

void Player::Jump()
{
    if (isGrounded)
    {
        Velocity.y = JumpForce;
        isGrounded = false;
    }
}

glm::mat4 Player::GetModelMatrix() const
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(Position.x, Position.y + m_VisualYOffset, Position.z));
    model = glm::rotate(model, glm::radians(RotationYaw + 180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    return model;
}

glm::mat4 Player::GetBoneTransform(const std::string& name) const
{
    return m_Animator.GetNodeGlobalTransform(name);
}
