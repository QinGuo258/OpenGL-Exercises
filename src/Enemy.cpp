#include "Enemy.h"
#include "CollisionWorld.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>
#include <algorithm>

Enemy::Enemy(EnemyType type, glm::vec3 startPos, Model* model,
             const std::map<std::string, std::shared_ptr<Animation>>& anims)
    : Type(type), Position(startPos), RotationYaw(0.0f)
    , m_Model(model), m_Animations(anims)
{
    Velocity = glm::vec3(0.0f);

    // 骷髅移速稍慢
    if (Type == EnemyType::SKELETON)
        MovementSpeed = 1.8f;

    // 诊断：检查模型是否成功加载了网格
    size_t meshCount = m_Model ? m_Model->GetMeshes().size() : 0;
    const char* typeStr = (Type == EnemyType::SKELETON) ? "Skeleton" : "Zombie";
    printf("[Enemy] %s spawned at (%.1f, %.1f, %.1f) | Model meshes: %zu | Anims: %zu\n",
           typeStr, Position.x, Position.y, Position.z, meshCount, m_Animations.size());

    if (meshCount == 0)
    {
        printf("[Enemy] ERROR: Model has 0 meshes — enemy will be invisible!\n");
    }

    // 打印所有可用动画名称
    printf("[Enemy] Available animations (%zu):\n", m_Animations.size());
    for (const auto& [name, anim] : m_Animations)
        printf("  '%s' (%.1f ticks)\n", name.c_str(), anim->GetDuration());

    auto it = m_Animations.find("idle");
    if (it != m_Animations.end())
    {
        m_Animator.PlayAnimation(it->second);
        m_CurrentAnim = "idle";
        printf("[Enemy] Initial animation: 'idle'\n");
    }
    else if (!m_Animations.empty())
    {
        m_Animator.PlayAnimation(m_Animations.begin()->second);
        m_CurrentAnim = m_Animations.begin()->first;
        printf("[Enemy] 'idle' not found, using '%s'\n", m_CurrentAnim.c_str());
    }
    else
    {
        printf("[Enemy] WARNING: No animations loaded!\n");
    }

    // 骷髅骨骼名诊断：检查 attack 动画能否真的驱动模型
    if (Type == EnemyType::SKELETON)
    {
        auto atkIt = m_Animations.find("attack");
        if (atkIt != m_Animations.end())
        {
            auto& modelBoneMap = m_Model->GetBoneInfoMap();
            int matched = 0, unmatched = 0;
            for (const auto& bone : atkIt->second->GetBones())
            {
                if (modelBoneMap.find(bone.GetName()) != modelBoneMap.end())
                    matched++;
                else
                    unmatched++;
            }
            printf("[Skeleton] Attack anim bones: %zu total, %d matched model, %d unmatched\n",
                   atkIt->second->GetBones().size(), matched, unmatched);
        }
    }
}

void Enemy::PlayAnim(const std::string& animName)
{
    if (m_CurrentAnim == animName)
        return;

    auto it = m_Animations.find(animName);
    if (it != m_Animations.end())
    {
        const char* typeStr = (Type == EnemyType::SKELETON) ? "Skeleton" : "Zombie";
        printf("[%s] Anim switch: '%s' → '%s'\n",
               typeStr, m_CurrentAnim.c_str(), animName.c_str());
        m_Animator.PlayAnimation(it->second);
        m_CurrentAnim = animName;
    }
    else if (!m_Animations.empty())
    {
        if (!m_WarnedAnim)
        {
            const char* typeStr = (Type == EnemyType::SKELETON) ? "Skeleton" : "Zombie";
            printf("[%s] ERROR: Animation '%s' not found! Available:\n",
                   typeStr, animName.c_str());
            for (const auto& [name, _] : m_Animations)
                printf("  '%s'\n", name.c_str());
            m_WarnedAnim = true;
        }
        m_Animator.PlayAnimation(m_Animations.begin()->second);
        m_CurrentAnim = m_Animations.begin()->first;
    }
}

void Enemy::PlayOverlayAnim(const std::string& animName)
{
    auto it = m_Animations.find(animName);
    if (it != m_Animations.end())
    {
        m_Animator.PlayOverlay(it->second);
    }
}

void Enemy::ClearOverlayAnim()
{
    m_Animator.ClearOverlay();
}

void Enemy::TakeDamage(int damage, const glm::vec3& knockback)
{
    Health -= damage;
    Velocity = knockback; // 强制应用击飞速度
    IsGrounded = false;   // 击飞后离地
    StunTimer = 1.0f;     // 赋予 1 秒硬直，期间暂停寻路 AI
}

void Enemy::UpdateAI(float dt, const glm::vec3& playerPos)
{
    // 攻击冷却递减
    if (AttackCooldown > 0.0f) AttackCooldown -= dt;

    // 硬直期间暂停 AI：无法主动移动和转向，完全受物理系统支配
    if (StunTimer > 0.0f)
    {
        StunTimer -= dt;
        m_Animator.UpdateAnimation(dt); // 仅更新动画
        return;
    }

    glm::vec3 dir = playerPos - Position;
    float horizontalDist = glm::sqrt(dir.x * dir.x + dir.z * dir.z);

    if (Type == EnemyType::ZOMBIE)
    {
        // ========== 僵尸 AI（近战追击）==========
        if (horizontalDist > 15.0f)
        {
            PlayAnim("idle");
            Velocity.x = 0.0f;
            Velocity.z = 0.0f;
        }
        else if (horizontalDist > 1.5f && horizontalDist <= 15.0f)
        {
            PlayAnim("walk");
            float invDist = 1.0f / horizontalDist;
            RotationYaw = glm::degrees(std::atan2(dir.x, dir.z));
            Velocity.x = (dir.x * invDist) * MovementSpeed;
            Velocity.z = (dir.z * invDist) * MovementSpeed;

            // 追逐时如果被墙壁阻挡且在地面上，自动跳跃跨过单格障碍
            if (IsBlockedHorizontally && IsGrounded)
            {
                Velocity.y = 5.0f;
                IsGrounded = false;
            }
        }
        else
        {
            PlayAnim("idle");
            Velocity.x = 0.0f;
            Velocity.z = 0.0f;
        }
    }
    else if (Type == EnemyType::SKELETON)
    {
        if (horizontalDist > 15.0f)
        {
            // 15m 外：idle 静止
            PlayAnim("idle");
            Velocity.x = 0.0f; Velocity.z = 0.0f;
            RangedAttackTimer = 0.0f;
        }
        else
        {
            // 15m 内：面朝玩家，播放 walk
            RotationYaw = glm::degrees(std::atan2(dir.x, dir.z));
            PlayAnim("walk");

            if (horizontalDist < 8.0f)
            {
                // 8m 内：远离玩家
                float invDist = 1.0f / horizontalDist;
                Velocity.x = -(dir.x * invDist) * MovementSpeed * 0.8f;
                Velocity.z = -(dir.z * invDist) * MovementSpeed * 0.8f;
            }
            else
            {
                // 8–15m：站定射击
                Velocity.x = 0.0f; Velocity.z = 0.0f;
            }

            // 4 秒射箭冷却
            RangedAttackTimer += dt;
            if (RangedAttackTimer >= 4.0f)
            {
                RangedAttackTimer = 0.0f;

                extern void SpawnEnemyArrow(glm::vec3 spawnPos, glm::vec3 targetPos);
                SpawnEnemyArrow(
                    Position + glm::vec3(0.0f, 1.2f, 0.0f),
                    playerPos  + glm::vec3(0.0f, 1.0f, 0.0f));
            }
        }
    }

    m_Animator.UpdateAnimation(dt);
}

void Enemy::UpdatePhysics(float dt, const CollisionWorld& collisionWorld)
{
    // 每帧重置碰壁标志
    IsBlockedHorizontally = false;

    // 步骤 1：重力积分
    Velocity.y += -9.8f * dt;
    Position += Velocity * dt;

    // 步骤 2：垂直地板检测（与 Player 完全一致）
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
        float fallCompensation = std::max(0.0f, -Velocity.y * dt);
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
        IsGrounded = true;
    }
    else
    {
        IsGrounded = false;
    }

    // 步骤 3：水平墙壁推斥（XZ 平面）—— 此时 Y 坐标已精确修正
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
            if (tri->maxY <= Position.y + 0.2f)
                continue;

            // Y 轴范围：墙壁必须在敌人圆柱体高度范围内
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
                // 从最近点指向敌人中心的方向（在拐角处自然斜向，提供圆弧滑出）
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

            // 碰到了竖直墙壁，标记用于 AI 跳跃判断
            IsBlockedHorizontally = true;
        }
    }

    // 步骤 4：跌落保护
    if (Position.y < -50.0f)
    {
        Position = glm::vec3(0.0f, 50.0f, 0.0f);
        Velocity = glm::vec3(0.0f);
        IsGrounded = false;
        printf("[Enemy] Fell below world — reset to spawn\n");
    }

    // 步骤 5：每 60 帧打印一次位置（包含网格数确认可见性）
    {
        static int frameCounter = 0;
        if (++frameCounter % 60 == 0)
        {
            size_t meshes = m_Model ? m_Model->GetMeshes().size() : 0;
            printf("[Enemy] Pos: %.2f, %.2f, %.2f | Grounded: %d | Blocked: %d | Meshes: %zu | Anims: %zu\n",
                   Position.x, Position.y, Position.z, IsGrounded ? 1 : 0,
                   IsBlockedHorizontally ? 1 : 0,
                   meshes, m_Animations.size());
        }
    }
}

void Enemy::Draw(Shader& shader)
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, Position);
    model = glm::rotate(model, glm::radians(RotationYaw + 180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    shader.SetMat4("uModel", model);

    // 预填所有 100 个骨骼矩阵为 identity，防止其他实体骨骼数据干扰
    for (int i = 0; i < 100; ++i)
    {
        shader.SetMat4("finalBonesMatrices[" + std::to_string(i) + "]", glm::mat4(1.0f));
    }

    // 上传当前动画骨骼矩阵（如果有动画）
    const auto& matrices = m_Animator.GetFinalBoneMatrices();
    for (size_t i = 0; i < matrices.size(); i++)
    {
        std::string name = "finalBonesMatrices[" + std::to_string(i) + "]";
        shader.SetMat4(name, matrices[i]);
    }

    // 确保材质类型为非自发光、非风力，防止僵尸被当成草丛
    shader.SetInt("uMaterialType", 0);

    // 受击闪红：硬直超过 0.8 秒时全身混合红色（仅受击后前 0.2 秒闪红）
    shader.SetBool("uIsHit", StunTimer > 0.8f);

    m_Model->Draw(shader.ID());
}
