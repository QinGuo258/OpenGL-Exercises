#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
#include <algorithm>
#include <memory>
#include "ThirdPersonCamera.h"
#include "Player.h"
#include "Enemy.h"
#include "Arrow.h"
#include "CollisionWorld.h"
#include "Shader.h"
#include "Texture.h"
#include "FontRenderer.h"
#include "miniaudio.h"
#include <cstdlib>
#include <random>

const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;

// 全局日志文件（Model.cpp 也需要用）
FILE* gLogFile = nullptr;

// 全局音频引擎（miniaudio）
ma_engine audioEngine;

// 死亡烟雾粒子系统
struct DeathParticle {
    glm::vec3 Position;
    glm::vec3 Velocity;
    float Life;  // 寿命：0.4秒
    int TexIdx;  // 随机贴图索引 0-7
};
std::vector<DeathParticle> deathParticles;

// 横扫之刃剑气粒子系统（8 帧序列帧动画）
struct SweepParticle {
    glm::vec3 Position;
    float Life;    // 剩余寿命
    float MaxLife; // 总寿命：0.15 秒
};
std::vector<SweepParticle> sweepParticles;

// 箭矢容器（全局，供 SpawnEnemyArrow 访问）
std::vector<Arrow> arrows;

// 玩家生命值系统
int playerHP = 20;                   // 满血 20（10 颗心）
float playerImmunityTimer = 0.0f;    // 受伤无敌时间

// 进食与回血系统
bool isEating = false;               // 玩家当前是否正在吃东西
float eatingTimer = 0.0f;            // 吃东西的累积时间

ThirdPersonCamera thirdPersonCamera(SCR_WIDTH, SCR_HEIGHT);
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// 动态时间系统：太阳轨道路径
float timeOfDay = 0.0f;   // 当前时间（弧度），0 = 正午（太阳在最高点/正东）
float timeScale  = 0.004363323f;  // 时间流速：2π/1440 ≈ 24 分钟度过一整天

// 游戏状态：主菜单 / 游戏中
enum class GameState { MENU, PLAYING };
GameState currentGameState = GameState::MENU;

// 动态天气系统：降雨强度控制
bool isRaining = false;
float rainIntensity = 0.0f;

// 第一人称手臂动画控制（全局指针，main() 中初始化）
Animator* gFpAnimator = nullptr;
std::shared_ptr<Animation> gFpAttackAnim = nullptr;
bool gFpIsAttacking = false;

int activeSlot = 0;  // 快捷栏当前选中槽位 (0-8)

// F3 调试屏幕
bool showDebugScreen = false;
bool f3KeyPressed = false;

// 中文 TrueType 字体渲染器
FontRenderer fontRenderer;

// 左下角聊天 / 系统提示栏
struct ChatLine {
    std::string text;
    float lifeTime = 5.0f;
};
std::vector<ChatLine> chatLog;

// T 键聊天历史查看
std::vector<std::string> chatHistory; // 永久保存的历史记录 (最大100条)
bool isChatHistoryOpen = false;
bool tKeyPressed = false;

// F2 时间选择面板
bool isTimeSelectOpen = false;
bool f2KeyPressed = false;

void AddChatMessage(const std::string& message) {
    chatLog.push_back({ message, 5.0f });
    if (chatLog.size() > 6) chatLog.erase(chatLog.begin());

    // 同步塞入永久历史记录
    chatHistory.push_back(message);
    if (chatHistory.size() > 100) {
        chatHistory.erase(chatHistory.begin());
    }
}

// 第一人称手持物品的握持偏移参数
float gFpItemTranslateX = 1.35f;
float gFpItemTranslateY = -1.45f;
float gFpItemTranslateZ = -2.4f;
float gFpItemRotateX = 0.0f;
float gFpItemRotateY = -55.0f;
float gFpItemRotateZ = -15.0f;
float gFpItemScale = 1.7f;

float lerp(float a, float b, float f) { return a + f * (b - a); }

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    thirdPersonCamera.SetScreenSize(static_cast<float>(width), static_cast<float>(height));
}

// 前向声明：射线与 AABB 相交检测 (Slab 算法)
bool RayAABBIntersect(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                      const glm::vec3& boxMin, const glm::vec3& boxMax, float& tOut);

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    // 菜单状态或历史聊天框/时间选择面板打开时锁定视角
    if (currentGameState == GameState::MENU || isChatHistoryOpen || isTimeSelectOpen) return;

    if (firstMouse)
    {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos) - lastX;
    float yoffset = lastY - static_cast<float>(ypos);
    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);

    thirdPersonCamera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (yoffset > 0) activeSlot--;
    else if (yoffset < 0) activeSlot++;
    if (activeSlot < 0) activeSlot = 8;
    if (activeSlot > 8) activeSlot = 0;
}

void processInput(GLFWwindow* window, Player& player, std::vector<Enemy>& enemies)
{
    // T 键边缘触发：切换聊天历史框（不受 isChatHistoryOpen 锁定影响，保证能关闭）
    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
        if (!tKeyPressed) {
            isChatHistoryOpen = !isChatHistoryOpen;
            tKeyPressed = true;

            // 切换鼠标显示状态；若打开则关闭时间选择面板（互斥）
            if (isChatHistoryOpen) {
                if (isTimeSelectOpen) {
                    isTimeSelectOpen = false;
                    f2KeyPressed = true; // 防止松开 F2 时再次触发
                }
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true; // 重置首帧标志，防止鼠标跳变累积
            }
        }
    } else {
        tKeyPressed = false;
    }

    // F2 键边缘触发：切换时间选择面板
    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS) {
        if (!f2KeyPressed) {
            isTimeSelectOpen = !isTimeSelectOpen;
            f2KeyPressed = true;

            if (isTimeSelectOpen) {
                // 如果聊天历史开着就关掉（互斥）
                if (isChatHistoryOpen) {
                    isChatHistoryOpen = false;
                    tKeyPressed = true; // 防止松开 T 时再次触发
                }
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            }
        }
    } else {
        f2KeyPressed = false;
    }

    // 时间选择面板打开时，ESC 关闭面板（而非退出程序）
    {
        static bool escWasPressedInTimeSelect = false;
        if (isTimeSelectOpen && glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            if (!escWasPressedInTimeSelect) {
                isTimeSelectOpen = false;
                escWasPressedInTimeSelect = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstMouse = true;
            }
        } else {
            escWasPressedInTimeSelect = false;
        }
    }

    // 历史聊天框/时间选择面板/菜单打开时锁定玩家移动
    if (isChatHistoryOpen || isTimeSelectOpen || currentGameState == GameState::MENU) return;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    glm::vec3 front = thirdPersonCamera.GetFrontVector();
    glm::vec3 right = thirdPersonCamera.GetRightVector();

    glm::vec3 moveDir(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) moveDir += front;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) moveDir -= front;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) moveDir -= right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) moveDir += right;

    if (glm::length(moveDir) > 0.0f)
    {
        moveDir = glm::normalize(moveDir);
        player.Position.x += moveDir.x * player.MovementSpeed * deltaTime;
        player.Position.z += moveDir.z * player.MovementSpeed * deltaTime;
        player.IsMoving = true;
    }
    else
    {
        player.IsMoving = false;
    }

    static bool spaceWasPressed = false;
    bool spacePressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (spacePressed && !spaceWasPressed)
        player.Jump();
    spaceWasPressed = spacePressed;

    static bool f5WasPressed = false;
    bool f5Pressed = glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS;
    if (f5Pressed && !f5WasPressed)
        thirdPersonCamera.ToggleMode();
    f5WasPressed = f5Pressed;

    // 鼠标左键攻击（边沿触发）
    static bool mouseWasPressed = false;
    bool mousePressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (mousePressed && !mouseWasPressed)
    {
        if (thirdPersonCamera.CurrentMode == CameraMode::FirstPerson &&
            gFpAnimator && gFpAttackAnim && !gFpIsAttacking)
        {
            gFpAnimator->PlayAnimation(gFpAttackAnim);
            gFpIsAttacking = true;
        }
        else
        {
            player.TryAttack();
        }

        // ==== 战斗结算判定 ====
        // 射线始终从玩家眼睛发出，不受第三人称视角距离影响
        glm::vec3 rayOrigin = player.Position + thirdPersonCamera.EyeOffset;
        glm::vec3 rayDir = thirdPersonCamera.GetFullFrontVector(); // 包含俯仰角的真实 3D 准星方向
        float maxHitDistance = 3.5f;

        float closestHitDist = 9999.0f;
        Enemy* primaryEnemy = nullptr;

        // 1. 准星单体判定 (Raycast)
        for (auto& enemy : enemies)
        {
            // 构建僵尸的 AABB 包围盒 (中心点向四周伸展，高 1.8，半径 0.4)
            glm::vec3 boxMin = enemy.Position + glm::vec3(-0.4f, 0.0f, -0.4f);
            glm::vec3 boxMax = enemy.Position + glm::vec3(0.4f, 1.8f, 0.4f);

            float hitDist;
            if (RayAABBIntersect(rayOrigin, rayDir, boxMin, boxMax, hitDist))
            {
                // 找到距离准星最近的那个敌人
                if (hitDist < maxHitDistance && hitDist < closestHitDist)
                {
                    closestHitDist = hitDist;
                    primaryEnemy = &enemy;
                }
            }
        }

        // 2. 结算伤害与击退
        if (primaryEnemy != nullptr)
        {
            bool isHoldingSword = (activeSlot == 0); // 假定 slot 0 是剑
            int primaryDamage = isHoldingSword ? 7 : 1;

            // 击退方向为摄像机的水平朝向
            glm::vec3 kbDir = thirdPersonCamera.GetFrontVector();
            // 极其强烈的击飞反馈：水平向后推 6.0，并给一个 4.5 的上升滞空力
            glm::vec3 knockback = kbDir * 5.0f + glm::vec3(0.0f, 4.5f, 0.0f);

            // 1. 播放武器挥击音效 + 剑气粒子特效
            if (isHoldingSword)
            {
                ma_engine_play_sound(&audioEngine, "audio/Sweep_attack1.mp3", NULL);

                SweepParticle sweep;
                glm::vec3 spawnPos = thirdPersonCamera.GetPosition()
                                   + thirdPersonCamera.GetFullFrontVector() * 1.5f;
                spawnPos.y -= 0.2f;
                sweep.Position = spawnPos;
                sweep.Life = 0.15f;
                sweep.MaxLife = 0.15f;
                sweepParticles.push_back(sweep);
            }
            else
            {
                ma_engine_play_sound(&audioEngine, "audio/Weak_attack1.mp3", NULL);
            }

            // 2. 执行伤害结算
            primaryEnemy->TakeDamage(primaryDamage, knockback);

            // 3. 播放受击或死亡音效 + 死亡粒子特效
            bool isSkeleton = (primaryEnemy->Type == EnemyType::SKELETON);
            if (primaryEnemy->Health <= 0)
            {
                ma_engine_play_sound(&audioEngine,
                    isSkeleton ? "audio/Skeleton_death.mp3" : "audio/Zombie_death.mp3", NULL);

                // 瞬间生成 10 个烟雾粒子
                for (int p = 0; p < 10; ++p)
                {
                    DeathParticle part;
                    float rx = ((rand() % 100) / 100.0f - 0.5f) * 0.8f;
                    float ry = ((rand() % 100) / 100.0f) * 1.5f;
                    float rz = ((rand() % 100) / 100.0f - 0.5f) * 0.8f;
                    part.Position = primaryEnemy->Position + glm::vec3(rx, ry, rz);
                    float vx = ((rand() % 100) / 100.0f - 0.5f) * 0.5f;
                    float vy = ((rand() % 100) / 100.0f) * 1.5f + 0.5f;
                    float vz = ((rand() % 100) / 100.0f - 0.5f) * 0.5f;
                    part.Velocity = glm::vec3(vx, vy, vz);
                    part.TexIdx = rand() % 8;
                    part.Life = 0.4f;
                    deathParticles.push_back(part);
                }
            }
            else
            {
                if (isSkeleton)
                {
                    ma_engine_play_sound(&audioEngine, "audio/Skeleton_hurt1.mp3", NULL);
                }
                else
                {
                    ma_engine_play_sound(&audioEngine,
                        (rand() % 2 == 0) ? "audio/Zombie_hurt1.mp3" : "audio/Zombie_hurt2.mp3", NULL);
                }
            }

            // 4. 横扫之刃判定 (仅手持剑触发)
            if (isHoldingSword)
            {
                for (auto& enemy : enemies)
                {
                    // 排除已经受到主伤害的目标
                    if (&enemy == primaryEnemy) continue;

                    // 计算水平距离
                    glm::vec3 diff = enemy.Position - player.Position;
                    diff.y = 0.0f;
                    float dist2D = glm::length(diff);

                    if (dist2D < 3.0f) // 横扫范围略小 (3 米)
                    {
                        glm::vec3 dirToEnemy = diff / dist2D; // 归一化
                        // 计算与玩家正前方的夹角点积
                        float dotProd = glm::dot(kbDir, dirToEnemy);

                        // 0.707 约等于正前方左右 45 度 (总 90 度扇形)
                        if (dotProd > 0.707f)
                        {
                            // 横扫造成 3 点伤害，享受相同的击飞效果！
                            enemy.TakeDamage(3, knockback);

                            // 横扫命中播放受击或死亡音效
                            bool sweepIsSkeleton = (enemy.Type == EnemyType::SKELETON);
                            if (enemy.Health <= 0)
                            {
                                ma_engine_play_sound(&audioEngine,
                                    sweepIsSkeleton ? "audio/Skeleton_death.mp3" : "audio/Zombie_death.mp3", NULL);

                                // 死亡烟雾粒子
                                for (int p = 0; p < 10; ++p)
                                {
                                    DeathParticle part;
                                    float rx = ((rand() % 100) / 100.0f - 0.5f) * 0.8f;
                                    float ry = ((rand() % 100) / 100.0f) * 1.5f;
                                    float rz = ((rand() % 100) / 100.0f - 0.5f) * 0.8f;
                                    part.Position = enemy.Position + glm::vec3(rx, ry, rz);
                                    float vx = ((rand() % 100) / 100.0f - 0.5f) * 0.5f;
                                    float vy = ((rand() % 100) / 100.0f) * 1.5f + 0.5f;
                                    float vz = ((rand() % 100) / 100.0f - 0.5f) * 0.5f;
                                    part.Velocity = glm::vec3(vx, vy, vz);
                                    part.Life = 0.4f;
                                    deathParticles.push_back(part);
                                }
                            }
                            else
                            {
                                if (sweepIsSkeleton)
                                {
                                    ma_engine_play_sound(&audioEngine, "audio/Skeleton_hurt1.mp3", NULL);
                                }
                                else
                                {
                                    ma_engine_play_sound(&audioEngine,
                                        (rand() % 2 == 0) ? "audio/Zombie_hurt1.mp3" : "audio/Zombie_hurt2.mp3", NULL);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    mouseWasPressed = mousePressed;

    // Ctrl 切换奔跑模式（边沿触发，单次按下翻转）
    static bool ctrlWasPressed = false;
    bool ctrlPressed = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                       glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    if (ctrlPressed && !ctrlWasPressed)
        player.IsRunningMode = !player.IsRunningMode;
    ctrlWasPressed = ctrlPressed;

    // Shift 潜行（按住持续）
    player.IsSneaking = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

    // 进食系统：手持面包（slot 3）时长按右键进食
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS &&
        activeSlot == 3 && playerHP < 20)
    {
        isEating = true;
    }
    else
    {
        isEating = false;
        eatingTimer = 0.0f;
    }

    // 数字键 1-9 切换快捷栏槽位（边沿触发）
    static bool key1Was = false, key2Was = false, key3Was = false;
    static bool key4Was = false, key5Was = false, key6Was = false;
    static bool key7Was = false, key8Was = false, key9Was = false;
    bool key1 = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    bool key2 = glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;
    bool key3 = glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS;
    bool key4 = glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS;
    bool key5 = glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS;
    bool key6 = glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS;
    bool key7 = glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS;
    bool key8 = glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS;
    bool key9 = glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS;
    if (key1 && !key1Was) activeSlot = 0;
    if (key2 && !key2Was) activeSlot = 1;
    if (key3 && !key3Was) activeSlot = 2;
    if (key4 && !key4Was) activeSlot = 3;
    if (key5 && !key5Was) activeSlot = 4;
    if (key6 && !key6Was) activeSlot = 5;
    if (key7 && !key7Was) activeSlot = 6;
    if (key8 && !key8Was) activeSlot = 7;
    if (key9 && !key9Was) activeSlot = 8;
    key1Was = key1; key2Was = key2; key3Was = key3;
    key4Was = key4; key5Was = key5; key6Was = key6;
    key7Was = key7; key8Was = key8; key9Was = key9;

    // R 键切换降雨天气（边缘触发）
    static bool rWasPressed = false;
    bool rPressed = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    if (rPressed && !rWasPressed)
        isRaining = !isRaining;
    rWasPressed = rPressed;

    // F3 键切换调试屏幕（边缘触发）
    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS) {
        if (!f3KeyPressed) {
            showDebugScreen = !showDebugScreen;
            f3KeyPressed = true;
            printf("[F3] Debug screen: %s\n", showDebugScreen ? "ON" : "OFF");
        }
    } else {
        f3KeyPressed = false;
    }

    thirdPersonCamera.SetSneakOffset(player.GetVisualYOffset());
    thirdPersonCamera.TargetPosition = player.Position;
    thirdPersonCamera.UpdateCameraPosition();
}

// 计算射线(起点, 归一化方向)与 3D AABB 盒(min, max)是否相交，并返回相交距离
bool RayAABBIntersect(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                      const glm::vec3& boxMin, const glm::vec3& boxMax, float& tOut)
{
    glm::vec3 invDir = 1.0f / rayDir;
    glm::vec3 t0 = (boxMin - rayOrigin) * invDir;
    glm::vec3 t1 = (boxMax - rayOrigin) * invDir;
    glm::vec3 tmin = glm::min(t0, t1);
    glm::vec3 tmax = glm::max(t0, t1);
    float tnear = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
    float tfar  = glm::min(glm::min(tmax.x, tmax.y), tmax.z);
    if (tnear > tfar || tfar < 0.0f) return false;
    tOut = tnear;
    return true;
}

// 骷髅射箭函数（供 Enemy.cpp 调用）
void SpawnEnemyArrow(glm::vec3 spawnPos, glm::vec3 targetPos)
{
    glm::vec3 dir = glm::normalize(targetPos - spawnPos);
    dir.y += 0.15f; // 抬高补偿重力下坠
    dir = glm::normalize(dir);
    arrows.emplace_back(spawnPos, dir * 22.0f);
    ma_engine_play_sound(&audioEngine, "audio/Bow_shoot.mp3", NULL);
}

// 鼠标点击回调：处理主菜单按钮点击
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (currentGameState == GameState::MENU && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        // 翻转 Y 轴，对齐 1080p 正交投影坐标系
        float mouseX = (float)xpos;
        float mouseY = 1080.0f - (float)ypos;

        // 判定是否点击了屏幕中央的 "开始游戏" 按钮 (X: 560~1360, Y: 500~580)
        if (mouseX >= 560.0f && mouseX <= 1360.0f && mouseY >= 500.0f && mouseY <= 580.0f)
        {
            // 1. 进入游戏状态
            currentGameState = GameState::PLAYING;
            // 2. 隐藏并锁定鼠标，重置首帧防止跳变
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
            // 3. 触发开局第一声播报
            AddChatMessage("<玩家> 又是新的一天，先去村子里逛逛吧。");
            // 4. 播放点击音效
            ma_engine_play_sound(&audioEngine, "audio/Weak_attack1.mp3", NULL);
        }
    }

    // 时间选择面板按钮点击（仅在时间选择面板打开时处理）
    if (isTimeSelectOpen && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        // 翻转 Y 轴，对齐 1080p 正交投影坐标系
        float mouseX = (float)xpos;
        float mouseY = 1080.0f - (float)ypos;

        // 四个按钮的尺寸：280×70
        const float btnW = 280.0f;
        const float btnH = 70.0f;

        // 按钮位置 (top-left corner):
        // 正午 (top):    centered at (960, 615) → pos (820, 580)
        // 午夜 (bottom): centered at (960, 435) → pos (820, 400)
        // 清晨 (left):   centered at (770, 505) → pos (630, 470)
        // 黄昏 (right):  centered at (1150,505) → pos (1010,470)

        float targetTime = -1.0f; // 哨兵值，表示未命中任何按钮
        const char* label = "";

        // 正午
        if (mouseX >= 820.0f && mouseX <= 1100.0f && mouseY >= 580.0f && mouseY <= 650.0f) {
            targetTime = 1.57080f;       // 90°
            label = "正午";
        }
        // 午夜
        else if (mouseX >= 820.0f && mouseX <= 1100.0f && mouseY >= 340.0f && mouseY <= 410.0f) {
            targetTime = 4.71238f;   // 270°
            label = "午夜";
        }
        // 清晨
        else if (mouseX >= 630.0f && mouseX <= 910.0f && mouseY >= 470.0f && mouseY <= 540.0f) {
            targetTime = 0.17444f;   // 10° 
            label = "清晨";
        }
        // 黄昏
        else if (mouseX >= 1010.0f && mouseX <= 1290.0f && mouseY >= 470.0f && mouseY <= 540.0f) {
            targetTime = 2.96705f;   // 170°
            label = "黄昏";
        }

        // 命中按钮：应用时间、关闭面板、恢复游戏
        if (targetTime >= 0.0f) {
            timeOfDay = targetTime;
            isTimeSelectOpen = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;

            // 系统提示
            char buf[128];
            snprintf(buf, sizeof(buf), "<系统> 时间已切换至 %s", label);
            AddChatMessage(buf);

            ma_engine_play_sound(&audioEngine, "audio/Weak_attack1.mp3", NULL);
        }
    }
}

int main()
{
    gLogFile = fopen("debug.log", "w");

    if (!glfwInit())
    {
        if (gLogFile) fprintf(gLogFile, "ERROR: glfwInit failed\n");
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "OpenGL - Pavilion", nullptr, nullptr);
    if (!window)
    {
        if (gLogFile) fprintf(gLogFile, "ERROR: glfwCreateWindow failed\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // 主菜单显示鼠标

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        if (gLogFile) fprintf(gLogFile, "ERROR: glad init failed\n");
        return -1;
    }

    // 初始化音频引擎
    if (ma_engine_init(NULL, &audioEngine) != MA_SUCCESS)
    {
        printf("Failed to initialize audio engine.\n");
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    Player player("models/player.glb");

    // 敌怪系统：加载僵尸模型和动画
    Model zombieModel("models/zombie.glb");
    auto zombieAnims = Animation::LoadAll("models/zombie.glb", zombieModel);
    std::vector<Enemy> enemies;
    enemies.emplace_back(EnemyType::ZOMBIE, glm::vec3(100.0f, 40.0f, 40.0f), &zombieModel, zombieAnims);
    enemies.emplace_back(EnemyType::ZOMBIE, glm::vec3(95.0f, 40.0f, 40.0f), &zombieModel, zombieAnims);

    // 骷髅敌怪
    Model skeletonModel("models/skeleton.glb");
    auto skeletonAnims = Animation::LoadAll("models/skeleton.glb", skeletonModel);
    enemies.emplace_back(EnemyType::SKELETON, glm::vec3(98.0f, 40.0f, 43.0f), &skeletonModel, skeletonAnims);
    enemies.emplace_back(EnemyType::SKELETON, glm::vec3(92.0f, 40.0f, 45.0f), &skeletonModel, skeletonAnims);
    // 给所有骷髅的射击冷却加随机偏移，避免同时射箭
    for (auto& enemy : enemies)
    {
        if (enemy.Type == EnemyType::SKELETON)
            enemy.RangedAttackTimer = (float)(rand() % 2000) / 1000.0f; // 0~2s 随机偏移
    }

    // 第一人称手臂模型与动画
    Model armModel("models/arm.glb");
    Animator fpAnimator;
    {
        auto animMap = Animation::LoadAll("models/arm.glb", armModel);
        for (auto& [name, anim] : animMap)
        {
            std::string lower = name;
            for (char& c : lower) c = (char)tolower(c);
            if (lower == "attack")
            {
                gFpAttackAnim = anim;
                break;
            }
        }
        // 将攻击动画设为基础动画以初始化骨骼矩阵数组
        // 攻击动画 t=0 的帧即为手臂自然待机位
        if (gFpAttackAnim)
            fpAnimator.PlayAnimation(gFpAttackAnim);
    }
    gFpAnimator = &fpAnimator;

    Shader shader("shaders/model.vert", "shaders/model.frag");

    // 静态地图模型
    Model mapModel("models/map.glb");
    printf("[Light System] Extracted %zu unique point lights.\n", mapModel.pointLights.size());

    // 快捷栏物品模型 (9 槽位，槽位 4-8 为空)
    std::vector<std::unique_ptr<Model>> hotbarModels(9);
    hotbarModels[0] = std::make_unique<Model>("models/sword.glb");
    hotbarModels[1] = std::make_unique<Model>("models/torch.glb");
    hotbarModels[2] = std::make_unique<Model>("models/emerald.glb");
    hotbarModels[3] = std::make_unique<Model>("models/bread.glb");

    // 箭矢系统
    Model arrowModel("models/arrow.glb");

    // 快捷栏 2D 物品图标纹理 (9 槽位，槽位 4-8 为空)
    std::vector<std::unique_ptr<Texture>> hotbarIcons(9);

    // 静态地图碰撞世界（使用与渲染完全一致的模型矩阵）
    glm::mat4 mapModelMatrix = glm::mat4(1.0f);
    mapModelMatrix = glm::translate(mapModelMatrix, glm::vec3(0.0f, -35.0f, 0.0f));
    CollisionWorld mapCollision(mapModel, mapModelMatrix);

    // Shadow 着色器：深度渲染（静态网格 + 骨骼动画网格）
    Shader shadowShader("shaders/shadow.vert", "shaders/shadow.frag");
    Shader shadowSkinnedShader("shaders/shadow_skinned.vert", "shaders/shadow.frag");

    // G-Buffer 几何预渲染着色器（SSAO 第一阶段：提取视图空间坐标和法线）
    Shader gBufferShader("shaders/g_buffer.vert", "shaders/g_buffer.frag");
    Shader gBufferSkinnedShader("shaders/g_buffer_skinned.vert", "shaders/g_buffer.frag");

    // SSAO 着色器（SSAO 第二阶段：半球采样 + 遮蔽计算）
    Shader ssaoShader("shaders/fullscreen.vert", "shaders/ssao.frag");

    // SSAO 模糊着色器（4×4 均值模糊，消除高频噪点）
    Shader ssaoBlurShader("shaders/fullscreen.vert", "shaders/ssao_blur.frag");

    // 碰撞三角形 Debug Draw：提取线段顶点 → GPU VAO
    Shader debugShader("shaders/debug.vert", "shaders/debug.frag");

    // 程序化天空盒着色器 + 单位立方体 VAO
    Shader skyShader("shaders/sky.vert", "shaders/sky.frag");

    // 程序化雨滴着色器（VBO-less 粒子渲染）
    Shader rainShader("shaders/rain.vert", "shaders/rain.frag");

    // 2D UI 着色器（正交投影，屏幕最上层）
    Shader uiShader("shaders/ui.vert", "shaders/ui.frag");

    // HDR + Bloom 后处理着色器
    Shader blurShader("shaders/fullscreen.vert", "shaders/blur.frag");
    Shader hdrComposeShader("shaders/fullscreen.vert", "shaders/hdr_compose.frag");
    Shader godraysShader("shaders/fullscreen.vert", "shaders/godrays.frag");

    // 全部着色器指针列表（F1 一键热重载）
    std::vector<Shader*> allShaders = {
        &shader, &shadowShader, &shadowSkinnedShader,
        &gBufferShader, &gBufferSkinnedShader,
        &ssaoShader, &ssaoBlurShader,
        &debugShader, &skyShader, &rainShader, &uiShader,
        &blurShader, &hdrComposeShader, &godraysShader
    };

    float skyVertices[] = {
        // 位置 (立方体 1x1x1，36 顶点)
        -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f,
    };
    unsigned int skyVAO = 0, skyVBO = 0;
    glGenVertexArrays(1, &skyVAO);
    glBindVertexArray(skyVAO);
    glGenBuffers(1, &skyVBO);
    glBindBuffer(GL_ARRAY_BUFFER, skyVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyVertices), skyVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    std::vector<float> lineVerts = mapCollision.GetDebugLineVertices();
    unsigned int debugVAO = 0, debugVBO = 0;
    if (!lineVerts.empty())
    {
        glGenVertexArrays(1, &debugVAO);
        glBindVertexArray(debugVAO);
        glGenBuffers(1, &debugVBO);
        glBindBuffer(GL_ARRAY_BUFFER, debugVBO);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(lineVerts.size() * sizeof(float)),
                     lineVerts.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    // ======================================================================
    // 2D UI 纹理加载
    // ======================================================================
    Texture hotbarTex("textures/UI/hotbar/hotbar.png");
    Texture selectorTex("textures/UI/hotbar/hotbar_selection.png");
    Texture crosshairTex("textures/UI/crosshair.png");

    // 加载前 4 个槽位的 2D 物品图标
    hotbarIcons[0] = std::make_unique<Texture>("textures/item/diamond_sword.png");
    hotbarIcons[1] = std::make_unique<Texture>("textures/item/torch.png");
    hotbarIcons[2] = std::make_unique<Texture>("textures/item/emerald.png");
    hotbarIcons[3] = std::make_unique<Texture>("textures/item/bread.png");

    // 中文 TrueType 字体渲染器初始化（烘焙 21000+ 字符到 8192x8192 贴图）
    if (!fontRenderer.Init("fonts/font.ttf", 32.0f))
        printf("[Font] WARNING: Font renderer init failed, text will not display!\n");
    else
        printf("[Font] Font renderer initialized OK, atlas %dx%d\n", 8192, 8192);

    // 红心血条贴图
    Texture heartEmpty("textures/UI/heart/container.png");
    Texture heartFull("textures/UI/heart/full.png");
    Texture heartHalf("textures/UI/heart/half.png");

    // 水面噪点打包贴图（RGB通道含高度图+频率噪点，GL_REPEAT + Mipmap + 各向异性）
    Texture packedNoiseTex("textures/cloud-water.png");
    packedNoiseTex.SetWrapMode(GL_REPEAT, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, packedNoiseTex.ID());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    float maxAniso = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
    if (maxAniso > 0.0f)
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, maxAniso);

    // 死亡烟雾粒子贴图（8 张变体，随机选用）
    std::vector<Texture> smokeTexs;
    for (int i = 0; i < 8; ++i)
        smokeTexs.emplace_back(("textures/particle/generic_" + std::to_string(i) + ".png").c_str());

    // 横扫之刃剑气序列帧贴图（8 帧）
    std::vector<std::unique_ptr<Texture>> sweepTextures(8);
    for (int i = 0; i < 8; ++i)
    {
        std::string path = "textures/particle/sweep_" + std::to_string(i) + ".png";
        sweepTextures[i] = std::make_unique<Texture>(path);
    }

    // ======================================================================
    // 2D UI 网格：0→1 正方形 VAO，用于正交投影 UI 渲染
    // ======================================================================
    float uiVertices[] = {
        // 位置          // 纹理坐标
        0.0f, 1.0f,      0.0f, 0.0f, // 翻转 Y，匹配 stb_image 导致的上下颠倒
        1.0f, 0.0f,      1.0f, 1.0f,
        0.0f, 0.0f,      0.0f, 1.0f,

        0.0f, 1.0f,      0.0f, 0.0f,
        1.0f, 1.0f,      1.0f, 0.0f,
        1.0f, 0.0f,      1.0f, 1.0f
    };
    unsigned int uiVAO, uiVBO;
    glGenVertexArrays(1, &uiVAO);
    glGenBuffers(1, &uiVBO);
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(uiVertices), uiVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    // 空 VAO（VBO-less 渲染雨滴使用）
    unsigned int rainVAO = 0;
    glGenVertexArrays(1, &rainVAO);

    // 全屏四边形 VAO（后处理 Bloom / HDR 合成使用）
    float fullscreenVertices[] = {
        // position (NDC)      // texCoords
        -1.0f,  1.0f,          0.0f, 1.0f,
        -1.0f, -1.0f,          0.0f, 0.0f,
         1.0f, -1.0f,          1.0f, 0.0f,
        -1.0f,  1.0f,          0.0f, 1.0f,
         1.0f, -1.0f,          1.0f, 0.0f,
         1.0f,  1.0f,          1.0f, 1.0f,
    };
    unsigned int fullscreenVAO, fullscreenVBO;
    glGenVertexArrays(1, &fullscreenVAO);
    glGenBuffers(1, &fullscreenVBO);
    glBindVertexArray(fullscreenVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fullscreenVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(fullscreenVertices), fullscreenVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    int debugLineCount = static_cast<int>(lineVerts.size()) / 3; // 3 floats per vertex

    // ======================================================================
    // Rain Depth FBO 初始化（雨滴遮挡深度贴图）
    // ======================================================================
    const unsigned int RAIN_DEPTH_WIDTH = 1024, RAIN_DEPTH_HEIGHT = 1024;
    unsigned int rainDepthFBO, rainDepthMap;

    glGenTextures(1, &rainDepthMap);
    glBindTexture(GL_TEXTURE_2D, rainDepthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 RAIN_DEPTH_WIDTH, RAIN_DEPTH_HEIGHT, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float rainBorderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, rainBorderColor);

    glGenFramebuffers(1, &rainDepthFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, rainDepthFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rainDepthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(gLogFile ? gLogFile : stderr, "ERROR: Rain Depth FBO is not complete!\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ======================================================================
    // Shadow FBO 初始化（深度帧缓冲，用于阴影映射）
    // ======================================================================
    const unsigned int SHADOW_WIDTH = 4096, SHADOW_HEIGHT = 4096;
    unsigned int depthMapFBO, depthMap;

    // 生成深度纹理
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 SHADOW_WIDTH, SHADOW_HEIGHT, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // 生成 FBO，附加深度纹理
    glGenFramebuffers(1, &depthMapFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    // 完整性检查
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(gLogFile ? gLogFile : stderr, "ERROR: Shadow FBO is not complete!\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ======================================================================
    // HDR 画布 FBO：场景渲染目标（高精度浮点颜色 + 深度）
    // ======================================================================
    unsigned int hdrFBO, hdrColorTex, rboDepth;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

    glGenTextures(1, &hdrColorTex);
    glBindTexture(GL_TEXTURE_2D, hdrColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0,
                 GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdrColorTex, 0);

    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        fprintf(gLogFile ? gLogFile : stderr, "ERROR: HDR FBO is not complete!\n");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ======================================================================
    // G-Buffer：SSAO 几何预渲染 (视图空间位置 + 法线，均为 RGBA16F)
    // ======================================================================
    unsigned int gBuffer;
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

    // 1. 视图空间位置纹理 (必须是 16F 高精度浮点，因为坐标会超出 1.0)
    unsigned int gPosition;
    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1920, 1080, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPosition, 0);

    // 2. 视图空间法线纹理 (同样使用 16F)
    unsigned int gNormal;
    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1920, 1080, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);

    // 告诉 OpenGL 我们要同时渲染到两个颜色附件
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    // 3. 添加深度缓冲 (复用一张新的 RenderBuffer 即可)
    unsigned int gDepthRBO;
    glGenRenderbuffers(1, &gDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, gDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 1920, 1080);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gDepthRBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        fprintf(gLogFile ? gLogFile : stderr, "ERROR: G-Buffer Framebuffer not complete!\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ======================================================================
    // SSAO 采样核心 + 噪声纹理 + 遮蔽 FBO
    // ======================================================================
    std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
    std::default_random_engine generator;

    // 64 个半球采样点 (带靠近原点偏置)
    std::vector<glm::vec3> ssaoKernel;
    ssaoKernel.reserve(64);
    for (unsigned int i = 0; i < 64; ++i) {
        glm::vec3 sample(
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator)
        );
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        // 让采样点更靠近原点，使阴影根部更深
        float scale = (float)i / 64.0f;
        scale = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        ssaoKernel.push_back(sample);
    }

    // 4×4 随机旋转噪声纹理 (平铺以覆盖全屏)
    std::vector<glm::vec3> ssaoNoise;
    ssaoNoise.reserve(16);
    for (unsigned int i = 0; i < 16; i++) {
        glm::vec3 noise(
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator) * 2.0f - 1.0f,
            0.0f);
        ssaoNoise.push_back(noise);
    }
    unsigned int noiseTexture;
    glGenTextures(1, &noiseTexture);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // SSAO FBO：单通道红色遮蔽纹理
    unsigned int ssaoFBO, ssaoColorBuffer;
    glGenFramebuffers(1, &ssaoFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    glGenTextures(1, &ssaoColorBuffer);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1920, 1080, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBuffer, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        fprintf(gLogFile ? gLogFile : stderr, "ERROR: SSAO FBO not complete!\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // SSAO 模糊 FBO：二次渲染目标，消除 4×4 噪声平铺产生的高频伪影
    unsigned int ssaoBlurFBO, ssaoColorBufferBlur;
    glGenFramebuffers(1, &ssaoBlurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    glGenTextures(1, &ssaoColorBufferBlur);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1920, 1080, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBufferBlur, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 上传采样核心到 SSAO 着色器 (一次性，不会变)
    ssaoShader.Use();
    for (unsigned int i = 0; i < 64; ++i)
        ssaoShader.SetVec3("samples[" + std::to_string(i) + "]", ssaoKernel[i]);

    // ======================================================================
    // Bloom 高斯模糊双通道 FBO（半分辨率：960×540，软光晕性能优化）
    // ======================================================================
    const unsigned int BLOOM_WIDTH = SCR_WIDTH / 2, BLOOM_HEIGHT = SCR_HEIGHT / 2;
    unsigned int pingpongFBO[2], pingpongColorTex[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorTex);
    for (unsigned int i = 0; i < 2; ++i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, BLOOM_WIDTH, BLOOM_HEIGHT, 0,
                     GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorTex[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            fprintf(gLogFile ? gLogFile : stderr,
                    "ERROR: Bloom Ping-Pong FBO[%u] is not complete!\n", i);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // FPS 计时器
    float fpsTimer = 0.0f;
    int frameCount = 0;
    int currentFPS = 0;
    // 动态 FOV（疾跑时拉伸增强速度感）
    float currentFOV = 45.0f;
    // 是否在疾跑模式下按 W 向前移动（影响 FOV 与速度的独立判定）
    bool  isMovingForward = false;

    // ================= 主线剧情状态机 (Story System) =================
    enum class StoryState {
        WANDERING,  // 阶段1：逛村子
        CUTSCENE,   // 阶段2：黑屏过场与打钟
        GOTO_GATE,  // 阶段3：前往东门
        DEFENDING,  // 阶段4：击杀怪物
        ENDING,     // 结局播报
        COMPLETED   // 自由探索
    };
    StoryState storyState = StoryState::WANDERING;
    float storyTimer = 0.0f;
    int storyStep = 0;
    int bellCount = 0;
    float blackScreenAlpha = 0.0f;

    // AddChatMessage 推迟到玩家点击"开始游戏"按钮时触发

    while (!glfwWindowShouldClose(window))
    {
        // 绝对渲染锁：第一人称下，玩家的世界模型（身体）绝对不允许出现在任何缓冲区中！
        bool renderPlayerBody = (thirdPersonCamera.CurrentMode != CameraMode::FirstPerson);

        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // FPS 计算：每 0.5 秒更新一次
        fpsTimer += deltaTime;
        frameCount++;
        if (fpsTimer >= 0.5f) {
            currentFPS = static_cast<int>(frameCount / fpsTimer);
            frameCount = 0;
            fpsTimer = 0.0f;
        }

        // 动态时间推进
        timeOfDay += deltaTime * timeScale;

        // 游戏中才推进剧情和物理，主菜单只旋转天球
        if (currentGameState == GameState::PLAYING)
        {
        // ================= 剧情导演系统 (Story Director) =================
        if (storyState == StoryState::WANDERING) {
            storyTimer += deltaTime;
            if (storyStep == 0 && storyTimer >= 30.0f) {
                AddChatMessage("<玩家> 逛的差不多了，回家歇息吧。(家坐标: -17, -7, -11)");
                storyStep = 1;
            }
            if (storyStep == 1) {
                // 家门坐标: -17, -7, -11
                float distToHome = glm::distance(player.Position, glm::vec3(-17.0f, -7.0f, -11.0f));
                if (distToHome < 4.0f) {
                    storyState = StoryState::CUTSCENE;
                    storyTimer = 0.0f;
                    storyStep = 0;
                    bellCount = 0;
                }
            }
        }
        else if (storyState == StoryState::CUTSCENE) {
            storyTimer += deltaTime;
            // 1. 屏幕逐渐变黑 (0 到 3 秒)
            if (storyTimer < 3.0f) {
                blackScreenAlpha = glm::clamp(storyTimer / 2.0f, 0.0f, 1.0f);
            }
            // 2. 打钟 5 下，间隔 3 秒 (发生在 t=3, 6, 9, 12, 15)
            if (storyTimer >= 3.0f + bellCount * 3.0f && bellCount < 5) {
                ma_engine_play_sound(&audioEngine, "audio/Bell_use1.mp3", NULL);
                bellCount++;

                // 在第二声钟声敲响时 (bellCount 刚变成 2, t≈6s)
                if (bellCount == 2) {
                    // 环境突变：深夜、暴雨
                    timeOfDay = 4.71f; // 270度，深夜
                    isRaining = true;
                    AddChatMessage("<村民> 怪物入侵村子了！大家准备好，按照预定计划展开防御！");
                }
            }
            // 3. 屏幕恢复 (6 秒之后开始淡出)
            if (storyTimer > 6.0f && storyTimer < 9.0f) {
                blackScreenAlpha = glm::clamp(1.0f - (storyTimer - 6.0f) / 2.0f, 0.0f, 1.0f);
            }
            // 4. 后续台词
            if (storyStep == 0 && storyTimer >= 11.0f) {
                AddChatMessage("<玩家> 按照计划，我应该去村子东门抵御怪物。");
                storyStep = 1;
            }
            if (storyStep == 1 && storyTimer >= 16.0f) {
                AddChatMessage("<玩家> 准备好就出发吧。(提示：东门坐标 X:49 Y:-9 Z:-4)");
                storyState = StoryState::GOTO_GATE;
                storyTimer = 0.0f;
            }
        }
        else if (storyState == StoryState::GOTO_GATE) {
            // 东门坐标范围判定：X在 47~51 之间，Z在 -9~0 之间
            if (player.Position.x >= 47.0f && player.Position.x <= 51.0f &&
                player.Position.z >= -9.0f && player.Position.z <= 0.0f) {

                // 清理之前瞎逛时的野怪，确保防守战纯粹
                enemies.clear();

                // 在东门外生成怪物大军
                enemies.emplace_back(EnemyType::ZOMBIE, glm::vec3(60.0f, -9.0f, -7.0f), &zombieModel, zombieAnims);
                enemies.emplace_back(EnemyType::ZOMBIE, glm::vec3(60.0f, -9.0f, -5.0f), &zombieModel, zombieAnims);
                enemies.emplace_back(EnemyType::ZOMBIE, glm::vec3(60.0f, -9.0f, -3.0f), &zombieModel, zombieAnims);
                enemies.emplace_back(EnemyType::ZOMBIE, glm::vec3(60.0f, -9.0f, -9.0f), &zombieModel, zombieAnims);
                enemies.emplace_back(EnemyType::ZOMBIE, glm::vec3(60.0f, -9.0f, -1.0f), &zombieModel, zombieAnims);
                enemies.emplace_back(EnemyType::ZOMBIE, glm::vec3(62.0f, -9.0f, -9.0f), &zombieModel, zombieAnims);
                enemies.emplace_back(EnemyType::ZOMBIE, glm::vec3(62.0f, -9.0f, -7.0f), &zombieModel, zombieAnims); 
                enemies.emplace_back(EnemyType::ZOMBIE, glm::vec3(62.0f, -9.0f, -5.0f), &zombieModel, zombieAnims);
                enemies.emplace_back(EnemyType::ZOMBIE, glm::vec3(62.0f, -9.0f, -3.0f), &zombieModel, zombieAnims);
                enemies.emplace_back(EnemyType::ZOMBIE, glm::vec3(62.0f, -9.0f, -1.0f), &zombieModel, zombieAnims);
                enemies.emplace_back(EnemyType::SKELETON, glm::vec3(65.0f, -9.0f, -5.0f), &skeletonModel, skeletonAnims);
                enemies.emplace_back(EnemyType::SKELETON, glm::vec3(65.0f, -9.0f, -3.0f), &skeletonModel, skeletonAnims);
                enemies.emplace_back(EnemyType::SKELETON, glm::vec3(68.0f, -9.0f, -1.0f), &skeletonModel, skeletonAnims);
                enemies.emplace_back(EnemyType::SKELETON, glm::vec3(68.0f, -9.0f, -4.0f), &skeletonModel, skeletonAnims);
                enemies.emplace_back(EnemyType::SKELETON, glm::vec3(68.0f, -9.0f, -7.0f), &skeletonModel, skeletonAnims);

                storyState = StoryState::DEFENDING;
            }
        }
        else if (storyState == StoryState::DEFENDING) {
            // 怪物清理完毕
            if (enemies.empty()) {
                AddChatMessage("<玩家> 这边搞定了，其他地方怎么样！");
                storyState = StoryState::ENDING;
                storyTimer = 0.0f;
                storyStep = 0;
            }
        }
        else if (storyState == StoryState::ENDING) {
            storyTimer += deltaTime;
            if (storyStep == 0 && storyTimer >= 3.0f) {
                AddChatMessage("<村民> 都解决了！大家可以去歇息了！");
                storyStep = 1;
            }
            if (storyStep == 1 && storyTimer >= 6.0f) {
                AddChatMessage("<系统提示> 任务流程完结，现在可以自由探索。");
                storyState = StoryState::COMPLETED;
            }
        }

        // 降雨强度平滑过渡（约 2 秒内完成）
        float targetRain = isRaining ? 1.0f : 0.0f;
        rainIntensity += (targetRain - rainIntensity) * deltaTime * 0.2f;

        // 玩家无敌时间递减
        if (playerImmunityTimer > 0.0f) playerImmunityTimer -= deltaTime;

        // 进食状态机：长按右键 1.5 秒回复 4 点血（2 颗心）
        if (isEating)
        {
            eatingTimer += deltaTime;
            if (eatingTimer >= 1.5f)
            {
                playerHP = std::min(playerHP + 4, 20);
                eatingTimer = 0.0f;
            }
        }

        // 玩家死亡重生
        if (playerHP <= 0)
        {
            playerHP = 20;
            player.Position = glm::vec3(-20.0f, -7.0f, -13.0f);
            player.Velocity = glm::vec3(0.0f);
            player.isGrounded = false;
        }

        processInput(window, player, enemies);
        player.UpdatePhysics(deltaTime, mapCollision);

        // 敌怪更新：移除死亡实体并推进 AI / 物理
        std::erase_if(enemies, [](const Enemy& e) { return e.Health <= 0; });
        for (auto& enemy : enemies)
        {
            enemy.UpdateAI(deltaTime, player.Position);
            enemy.UpdatePhysics(deltaTime, mapCollision);

            // 僵尸撕咬判定
            float dist = glm::distance(enemy.Position, player.Position);
            if (dist <= 1.5f && enemy.AttackCooldown <= 0.0f && playerImmunityTimer <= 0.0f)
            {
                enemy.AttackCooldown = 1.5f;

                // 玩家扣血并获得无敌时间
                playerHP -= 2;
                playerImmunityTimer = 0.5f;

                // 击退：沿僵尸指向玩家的水平方向
                glm::vec3 pushDir = player.Position - enemy.Position;
                pushDir.y = 0.0f;
                if (glm::length(pushDir) > 0.001f)
                {
                    pushDir = glm::normalize(pushDir);
                }
                else
                {
                    pushDir = thirdPersonCamera.GetFrontVector();
                }
                player.Velocity = pushDir * 5.0f + glm::vec3(0.0f, 3.5f, 0.0f);
                player.isGrounded = false;

                // 播放受伤音效
                ma_engine_play_sound(&audioEngine, "audio/Weak_attack1.mp3", NULL);
            }
        }

        // 死亡烟雾粒子生命周期更新
        for (auto& part : deathParticles)
        {
            part.Life -= deltaTime;
            part.Position += part.Velocity * deltaTime;
        }
        std::erase_if(deathParticles, [](const DeathParticle& p) { return p.Life <= 0.0f; });

        // 剑气粒子寿命更新
        for (auto& part : sweepParticles)
        {
            part.Life -= deltaTime;
        }
        std::erase_if(sweepParticles, [](const SweepParticle& p) { return p.Life <= 0.0f; });

        // 箭矢物理更新
        for (auto& arrow : arrows)
        {
            arrow.UpdatePhysics(deltaTime, mapCollision, player.Position);
        }
        std::erase_if(arrows, [](const Arrow& a) { return a.Life <= 0.0f; });

        // 聊天栏 / 系统提示生命周期：过期消息自动消失
        for (auto& l : chatLog) l.lifeTime -= deltaTime;
        std::erase_if(chatLog, [](const ChatLine& l) { return l.lifeTime <= 0.0f; });

        // ---- 摄像机遮挡处理：第三人称时检测墙壁阻挡，拉近摄像机 ----
        if (thirdPersonCamera.CurrentMode != CameraMode::FirstPerson)
        {
            glm::vec3 eyeTarget = player.Position
                                + thirdPersonCamera.EyeOffset
                                + glm::vec3(0.0f, player.GetVisualYOffset(), 0.0f);

            glm::vec3 fullFront = thirdPersonCamera.GetFullFrontVector();

            glm::vec3 rayDir;
            if (thirdPersonCamera.CurrentMode == CameraMode::ThirdPersonBack)
                rayDir = -fullFront;
            else // ThirdPersonFront
                rayDir = fullFront;

            const float targetDist = 5.0f;
            float hitDist;
            if (mapCollision.Raycast(eyeTarget, rayDir, targetDist, hitDist))
            {
                // 有遮挡：拉近到墙体前 0.2m（至少保留 0.5m）
                thirdPersonCamera.Distance = glm::max(0.5f, hitDist - 0.2f);
            }
            else
            {
                thirdPersonCamera.Distance = targetDist;
            }

            thirdPersonCamera.UpdateCameraPosition();
        }

        {
            bool w = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
            bool s = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
            bool a = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
            bool d = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
            bool isMoving = w || s || a || d;
            isMovingForward = w;

            // 进食状态同步到 Player（影响移动速度）
            player.IsEating = isEating;

            // 动画状态机切换（传入 w 用于判断是否"向前移动"以触发疾跑）
            player.UpdateMovementState(isMoving, w);

            // 身体/头部转向计算
            float mappedCamYaw = -thirdPersonCamera.Yaw;

            float visualBodyYaw = player.RotationYaw + 180.0f;
            float targetVisualHeadYaw = 0.0f;

            if (isMoving)
            {
                float bodyOffset = 0.0f;

                if ((w && a) || (s && d) || (a && !w && !s))
                    bodyOffset = 45.0f;
                else if ((w && d) || (s && a) || (d && !w && !s))
                    bodyOffset = -45.0f;

                visualBodyYaw = mappedCamYaw - 90.0f + bodyOffset;
                targetVisualHeadYaw = mappedCamYaw - 90.0f;
            }
            else
            {
                targetVisualHeadYaw = mappedCamYaw - 90.0f;

                float deltaYaw = targetVisualHeadYaw - visualBodyYaw;
                while (deltaYaw > 180.0f) deltaYaw -= 360.0f;
                while (deltaYaw < -180.0f) deltaYaw += 360.0f;

                if (deltaYaw > 45.0f)
                    visualBodyYaw += (deltaYaw - 45.0f);
                if (deltaYaw < -45.0f)
                    visualBodyYaw += (deltaYaw + 45.0f);
            }

            float finalRelativeYaw = targetVisualHeadYaw - visualBodyYaw;
            while (finalRelativeYaw > 180.0f) finalRelativeYaw -= 360.0f;
            while (finalRelativeYaw < -180.0f) finalRelativeYaw += 360.0f;

            player.RotationYaw = visualBodyYaw - 180.0f;

            player.GetAnimator().SetHeadRotation(thirdPersonCamera.Pitch, finalRelativeYaw);
            player.GetAnimator().SetHideHead(thirdPersonCamera.CurrentMode == CameraMode::FirstPerson);
        }

        player.UpdateAnimation(deltaTime);

        // 第一人称手臂动画更新
        static float fpAttackElapsed = 0.0f;
        if (gFpIsAttacking && gFpAttackAnim)
        {
            fpAttackElapsed += deltaTime;
            float animDurationSec = gFpAttackAnim->GetDuration() / gFpAttackAnim->GetTicksPerSecond();
            if (fpAttackElapsed >= animDurationSec)
            {
                // 攻击动画播放完毕，回归绑定位
                gFpIsAttacking = false;
                fpAttackElapsed = 0.0f;
                fpAnimator.SetCurrentTime(0.0f);
                fpAnimator.UpdateAnimation(0.0f);
            }
            else
            {
                fpAnimator.UpdateAnimation(deltaTime);
            }
        }
        else
        {
            // 无动画时维持 t=0 的待机姿势
            fpAnimator.SetCurrentTime(0.0f);
            fpAnimator.UpdateAnimation(0.0f);
        }

        } // if (currentGameState == GameState::PLAYING)

        // ======================================================================
        // 动态 FOV：疾跑模式下按 W 前进时视角拉伸增强速度感
        // ======================================================================
        {
            bool isActuallySprinting = player.IsRunningMode && isMovingForward && !player.IsSneaking;
            float targetFOV = isActuallySprinting ? 55.0f : 45.0f;
            currentFOV = glm::mix(currentFOV, targetFOV, deltaTime * 8.0f);
            thirdPersonCamera.SetFOV(currentFOV);
        }

        // ======================================================================
        // 光源空间矩阵（Light Space Matrix）—— 太阳视角
        // ======================================================================
        // 1. 创建天球矩阵（统一管理日月星辰的旋转，无倾斜，经过正头顶）
        glm::mat4 celestialMat = glm::mat4(1.0f);
        celestialMat = glm::rotate(celestialMat, timeOfDay, glm::vec3(0.0f, 0.0f, 1.0f));

        // 2. 指向太阳的方向向量（天球矩阵的本地 X 轴）
        glm::vec3 sunPosDir = glm::normalize(glm::vec3(celestialMat * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)));

        // 3. 光照方向（从太阳射向地面，即 sunPosDir 的反方向）
        // 4. 提取太阳高度角用于颜色计算
        float sunY = sunPosDir.y;

        // 动态主光源：白天为太阳，夜晚自动切换为月亮
        glm::vec3 activeLightDir;
        glm::vec3 currentLightColor;
        glm::vec3 currentAmbientColor;

        glm::vec3 dayLight    = glm::vec3(1.3f, 1.15f, 0.95f);   // 温暖的阳光色
        glm::vec3 sunsetLight = glm::vec3(1.0f, 0.4f, 0.1f);   // 橘红色黄昏
        glm::vec3 moonLight   = glm::vec3(0.01f, 0.02f, 0.04f);  // 冷蓝色月光
        glm::vec3 dayAmbient  = glm::vec3(0.25f, 0.35f, 0.50f); // 清透的冷蓝色阴影
        glm::vec3 nightAmbient = glm::vec3(0.02f, 0.02f, 0.06f); // 深蓝色夜晚

        if (sunY > 0.0f)
        {
            // ==========================================
            // 1. 白天状态：主光源是太阳
            // ==========================================
            activeLightDir = -sunPosDir; // 太阳光线向下射向地面

            float blendFactor = glm::clamp(sunY * 5.0f, 0.0f, 1.0f);
            currentLightColor  = glm::mix(sunsetLight, dayLight, blendFactor);
            currentAmbientColor = glm::mix(nightAmbient, dayAmbient, blendFactor);
        }
        else
        {
            // ==========================================
            // 2. 黑夜状态：主光源自动切换为月亮
            // ==========================================
            activeLightDir = sunPosDir; // 月亮在太阳正对面，月光向下 = -(-sunPosDir) = sunPosDir

            float moonY = -sunY;
            float blendFactor = glm::clamp(moonY * 5.0f, 0.0f, 1.0f);

            currentLightColor  = glm::mix(glm::vec3(0.0f), moonLight, blendFactor);
            currentAmbientColor = nightAmbient;
        }

        // 天空盒颜色：与光照同步插值
        glm::vec3 dayHorizon    = glm::vec3(0.55f, 0.75f, 0.95f);  // 地平线浅青/浅蓝
        glm::vec3 dayZenith     = glm::vec3(0.15f, 0.35f, 0.80f);   // 天顶深邃蓝
        glm::vec3 sunsetHorizon = glm::vec3(1.0f, 0.45f, 0.2f);      // 橙红
        glm::vec3 sunsetZenith  = glm::vec3(0.3f, 0.1f, 0.35f);      // 紫
        glm::vec3 nightHorizon  = glm::vec3(0.05f, 0.05f, 0.12f);    // 深灰蓝
        glm::vec3 nightZenith   = glm::vec3(0.0f, 0.0f, 0.02f);      // 近黑

        glm::vec3 currentHorizonColor;
        glm::vec3 currentZenithColor;

        if (sunY > 0.0f)
        {
            float blendFactor = glm::clamp(sunY * 5.0f, 0.0f, 1.0f);
            currentHorizonColor = glm::mix(sunsetHorizon, dayHorizon, blendFactor);
            currentZenithColor  = glm::mix(sunsetZenith,  dayZenith,  blendFactor);
        }
        else
        {
            currentHorizonColor = nightHorizon;
            currentZenithColor  = nightZenith;
        }

        // 光源位置：玩家位置沿光线反方向延伸 50 米
        glm::vec3 lightPos = player.Position - activeLightDir * 50.0f;

        // 光源视图矩阵：太阳摄像机盯着玩家
        glm::mat4 lightView = glm::lookAt(lightPos, player.Position, glm::vec3(0.0f, 1.0f, 0.0f));

        // 正交投影矩阵（平行光无透视）
        float near_plane = 1.0f, far_plane = 100.0f;
        glm::mat4 lightProjection = glm::ortho(-40.0f, 40.0f, -40.0f, 40.0f, near_plane, far_plane);

        // 光源空间变换矩阵
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        // ======================================================================
        // Pass 1：从光源视角渲染场景深度到 Shadow Map
        // ======================================================================
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);

        // 1a. 渲染静态地图深度（使用静态阴影着色器，跳过水面）
        shadowShader.Use();
        shadowShader.SetMat4("uLightSpaceMatrix", lightSpaceMatrix);
        shadowShader.SetMat4("uModel", mapModelMatrix);
        for (const auto& mesh : mapModel.GetMeshes())
        {
            if (mesh.materialType == 4) continue;
            mesh.Draw(shadowShader.ID());
        }

        // 1b. 渲染动态角色深度（使用骨骼动画阴影着色器）
        // 【渲染锁】第一人称下玩家身体禁止写入 Shadow Map，消除幽灵阴影
        shadowSkinnedShader.Use();
        shadowSkinnedShader.SetMat4("uLightSpaceMatrix", lightSpaceMatrix);

        if (renderPlayerBody)
        {
            player.Draw(shadowSkinnedShader);
        }

        // 1c. 渲染敌怪深度（使用骨骼动画阴影着色器）
        for (auto& enemy : enemies)
        {
            enemy.Draw(shadowSkinnedShader);
        }

        // ======================================================================
        // Pass 1b：渲染雨滴遮挡深度图（防漏雨机制）
        // ======================================================================
        glm::mat4 rainSpaceMatrix = glm::mat4(1.0f);
        if (rainIntensity > 0.01f) {
            glViewport(0, 0, RAIN_DEPTH_WIDTH, RAIN_DEPTH_HEIGHT);
            glBindFramebuffer(GL_FRAMEBUFFER, rainDepthFBO);
            glClear(GL_DEPTH_BUFFER_BIT);

            // 构建雨滴相机矩阵 (严格匹配 rain.vert 中的 fallDir: vec3(0.1, -1.0, 0.1))
            glm::vec3 rainDir = glm::normalize(glm::vec3(0.1f, -1.0f, 0.1f));
            glm::vec3 rainCamPos = thirdPersonCamera.GetPosition() - rainDir * 40.0f;
            // 关键修复：rainDir 几乎垂直向下，up(0,1,0) 会退化。改用 up(0,0,-1) 避免矩阵奇异
            glm::mat4 rainView = glm::lookAt(rainCamPos, thirdPersonCamera.GetPosition(), glm::vec3(0.0f, 0.0f, -1.0f));
            glm::mat4 rainProjection = glm::ortho(-30.0f, 30.0f, -30.0f, 30.0f, 1.0f, 80.0f);
            rainSpaceMatrix = rainProjection * rainView;

            // 借用现成的 shadowShader 渲染地图深度（只画地图，不画玩家，防止雨滴在玩家头顶上空消失；跳过水面）
            shadowShader.Use();
            shadowShader.SetMat4("uLightSpaceMatrix", rainSpaceMatrix);
            shadowShader.SetMat4("uModel", mapModelMatrix);
            for (const auto& mesh : mapModel.GetMeshes())
            {
                if (mesh.materialType == 4) continue;
                mesh.Draw(shadowShader.ID());
            }
        }

        // ======================================================================
        // Pass 1.5: G-Buffer 几何预渲染 (视图空间位置 + 法线，供 SSAO 使用)
        // ======================================================================
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
        glViewport(0, 0, 1920, 1080);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 1. 画地图 (静态几何体，跳过水面)
        gBufferShader.Use();
        gBufferShader.SetMat4("uProjection", thirdPersonCamera.GetProjectionMatrix());
        gBufferShader.SetMat4("uView", thirdPersonCamera.GetViewMatrix());
        gBufferShader.SetMat4("uModel", mapModelMatrix);
        gBufferShader.SetFloat("uTime", (float)glfwGetTime());
        gBufferShader.SetFloat("uRainIntensity", rainIntensity);
        for (const auto& mesh : mapModel.GetMeshes())
        {
            if (mesh.materialType == 4) continue;
            mesh.Draw(gBufferShader.ID());
        }

        // 2. 画玩家、敌怪、手持武器 (骨骼动画几何体)
        gBufferSkinnedShader.Use();
        gBufferSkinnedShader.SetMat4("uProjection", thirdPersonCamera.GetProjectionMatrix());
        gBufferSkinnedShader.SetMat4("uView", thirdPersonCamera.GetViewMatrix());

        // 2a. 玩家 (第一人称下跳过，防止身体写入 G-Buffer 产生幽灵 SSAO 遮蔽)
        if (renderPlayerBody)
        {
            player.Draw(gBufferSkinnedShader);
        }

        // 2b. 敌怪
        for (auto& enemy : enemies)
        {
            enemy.Draw(gBufferSkinnedShader);
        }

        // 2c. 箭矢 (静态几何体，用 gBufferShader)
        gBufferShader.Use();
        gBufferShader.SetMat4("uProjection", thirdPersonCamera.GetProjectionMatrix());
        gBufferShader.SetMat4("uView", thirdPersonCamera.GetViewMatrix());
        for (auto& arrow : arrows)
        {
            arrow.Draw(gBufferShader, &arrowModel);
        }

        // 2d. 第三人称手持物品 (骨骼挂载到 Right Arm_bone)
        if (thirdPersonCamera.CurrentMode != CameraMode::FirstPerson && hotbarModels[activeSlot] != nullptr)
        {
            glm::mat4 playerMat = player.GetModelMatrix();
            glm::mat4 handBoneMat = player.GetBoneTransform("Right Arm_bone");
            glm::mat4 itemOffset = glm::mat4(1.0f);
            itemOffset = glm::translate(itemOffset, glm::vec3(0.55f, 0.45f, -0.2f));
            itemOffset = glm::rotate(itemOffset, glm::radians(-85.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            itemOffset = glm::scale(itemOffset, glm::vec3(1.0f));
            glm::mat4 itemModelMat = playerMat * handBoneMat * itemOffset;

            // 清除骨骼矩阵，防止静态道具被骨骼形变干扰
            for (int i = 0; i < 100; ++i)
                gBufferSkinnedShader.SetMat4("finalBonesMatrices[" + std::to_string(i) + "]", glm::mat4(1.0f));
            gBufferSkinnedShader.Use();
            gBufferSkinnedShader.SetMat4("uProjection", thirdPersonCamera.GetProjectionMatrix());
            gBufferSkinnedShader.SetMat4("uView", thirdPersonCamera.GetViewMatrix());
            gBufferSkinnedShader.SetMat4("uModel", itemModelMat);
            hotbarModels[activeSlot]->Draw(gBufferSkinnedShader.ID());
        }

        // 2e.【移除】第一人称手臂/手持物品已从 G-Buffer 中彻底删除。
        //     它们只允许在 Pass 4（主场景深度缓冲清空后）绘制，
        //     否则会在 SSAO 中产生巨大的漂浮环境遮蔽污染。

        // ======================================================================
        // Pass 1.8: SSAO 遮蔽计算 (读取 G-Buffer → 半球采样 → 输出遮蔽掩码)
        // ======================================================================
        glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
        glViewport(0, 0, 1920, 1080);
        glClear(GL_COLOR_BUFFER_BIT);

        ssaoShader.Use();
        ssaoShader.SetMat4("projection", thirdPersonCamera.GetProjectionMatrix());

        ssaoShader.SetInt("gPosition", 0);
        ssaoShader.SetInt("gNormal", 1);
        ssaoShader.SetInt("texNoise", 2);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gPosition);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gNormal);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, noiseTexture);

        glBindVertexArray(fullscreenVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // ======================================================================
        // Pass 1.9: SSAO 模糊 (4×4 均值，消除高频随机噪点)
        // ======================================================================
        glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
        glClear(GL_COLOR_BUFFER_BIT);
        ssaoBlurShader.Use();
        ssaoBlurShader.SetInt("ssaoInput", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);

        glBindVertexArray(fullscreenVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // ======================================================================
        // Pass 2：渲染场景到 HDR 高精度浮点画布（供后处理 Bloom + 色调映射使用）
        // ======================================================================
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.Use();

        shader.SetMat4("uView", thirdPersonCamera.GetViewMatrix());
        shader.SetMat4("uProjection", thirdPersonCamera.GetProjectionMatrix());
        shader.SetVec3("uCameraPos", thirdPersonCamera.GetPosition());
        shader.SetVec3("uLightDir", activeLightDir.x, activeLightDir.y, activeLightDir.z);
        shader.SetVec3("uLightColor", currentLightColor.x, currentLightColor.y, currentLightColor.z);
        shader.SetVec3("uAmbientColor", currentAmbientColor.x, currentAmbientColor.y, currentAmbientColor.z);
        shader.SetFloat("uSpecularStrength", 0.5f);
        shader.SetFloat("uShininess", 32.0f);
        shader.SetFloat("uRainIntensity", rainIntensity);
        shader.SetFloat("uTime", (float)glfwGetTime());

        // 夜色系数：黄昏前后平滑过渡，太阳高度角在 0.15 到 -0.05 之间线性插值
        float nightFadeValue = glm::clamp(1.0f - (sunY + 0.05f) * 5.0f, 0.0f, 1.0f);
        shader.SetFloat("uNightFade", nightFadeValue);

        // 传递真实天空渐变色给水面反射（随昼夜/黄昏自动渐变）
        shader.SetVec3("uHorizonColor", currentHorizonColor);
        shader.SetVec3("uZenithColor", currentZenithColor);

        // 阴影贴图：绑定到纹理单元 15，传入光源空间矩阵
        shader.SetMat4("uLightSpaceMatrix", lightSpaceMatrix);
        shader.SetInt("uShadowMap", 15);
        glActiveTexture(GL_TEXTURE15);
        glBindTexture(GL_TEXTURE_2D, depthMap);

        // SSAO 遮蔽掩码：绑定模糊后的遮罩到纹理单元 13，传入屏幕分辨率
        shader.SetInt("uSsaoMap", 13);
        shader.SetVec2("uScreenSize", glm::vec2(1920.0f, 1080.0f));
        glActiveTexture(GL_TEXTURE13);
        glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);

        // 水面噪点打包贴图：绑定到纹理单元 12，用于有限差分法线计算
        shader.SetInt("uPackedNoiseMap", 12);
        glActiveTexture(GL_TEXTURE12);
        glBindTexture(GL_TEXTURE_2D, packedNoiseTex.ID());

        // 点光源：将自发光网格顶点转为世界坐标传入着色器（留一个空位给手持火把）
        {
            // 1. 动态手持火把光源：当玩家选中 Slot 1 时，在玩家位置追加移动点光源
            bool holdingTorch = (activeSlot == 1);
            int numLights = std::min((int)mapModel.pointLights.size(), 511);

            if (holdingTorch) {
                glm::vec3 torchWorldPos;
                if (thirdPersonCamera.CurrentMode == CameraMode::FirstPerson) {
                    // 第一人称：光源跟随相机（眼睛位置）
                    torchWorldPos = thirdPersonCamera.GetPosition();
                } else {
                    // 第三人称：在玩家身体中心偏右前方，模拟右手持火把
                    glm::vec3 rightDir = thirdPersonCamera.GetRightVector();
                    glm::vec3 frontDir = thirdPersonCamera.GetFrontVector();
                    torchWorldPos = player.Position + glm::vec3(0.0f, 1.2f, 0.0f)
                                  + rightDir * 0.4f + frontDir * 0.2f;
                }
                shader.SetVec3("uPointLights[" + std::to_string(numLights) + "]", torchWorldPos);
                numLights++;
            }

            // 2. 传给 Shader 最新的光源总数
            shader.SetInt("uNumPointLights", numLights);

            // 3. 照常循环绘制地图中的静态光源 (注意循环上限使用原始的静态光源数量，不要覆盖我们刚追加的手持光源)
            int staticLightsCount = std::min((int)mapModel.pointLights.size(), 511);
            for (int i = 0; i < staticLightsCount; ++i) {
                glm::vec3 worldLightPos = glm::vec3(mapModelMatrix * glm::vec4(mapModel.pointLights[i], 1.0f));
                worldLightPos.y += 0.3f;
                shader.SetVec3("uPointLights[" + std::to_string(i) + "]", worldLightPos);
            }
        }

        // 渲染静态地图 — 第一遍：不透明网格（跳过水面 materialType=4）
        shader.SetBool("uIsHit", false);
        shader.SetMat4("uModel", mapModelMatrix);
        for (const auto& mesh : mapModel.GetMeshes())
        {
            if (mesh.materialType == 4) continue;
            shader.SetInt("uMaterialType", mesh.materialType);
            mesh.Draw(shader.ID());
        }

        // 绘制所有敌怪（在所有模式下均可见）
        for (auto& enemy : enemies)
        {
            enemy.Draw(shader);
        }

        // 敌怪循环后强制重置状态（防止最后一个僵尸的 uIsHit 泄漏）
        shader.SetBool("uIsHit", false);
        shader.SetInt("uMaterialType", 0);

        // 绘制所有箭矢
        shader.SetBool("uIsHit", false);
        for (auto& arrow : arrows)
        {
            arrow.Draw(shader, &arrowModel);
        }

        // 仅在第三人称下绘制玩家世界模型
        if (renderPlayerBody)
        {
            shader.SetBool("uIsHit", false);
            shader.SetInt("uMaterialType", 0);
            player.Draw(shader);

            // 第三人称手持物品渲染：将物品挂载在右手骨骼上
            if (hotbarModels[activeSlot] != nullptr)
            {
                // 1. 获取玩家模型矩阵和右手关节的全局变换矩阵
                //    骨骼名 "Right Arm_bone" 通过 debug.log 中的骨骼层级树确认
                glm::mat4 playerMat = player.GetModelMatrix();
                glm::mat4 handBoneMat = player.GetBoneTransform("Right Arm_bone");

                // 2. 清除骨骼动画矩阵，防止静态道具被骨骼形变干扰
                for (int i = 0; i < 100; ++i)
                {
                    shader.SetMat4("finalBonesMatrices[" + std::to_string(i) + "]", glm::mat4(1.0f));
                }
                // 手持金属/高光道具材质 (5: 保留高光，防止风摆)
                shader.SetInt("uMaterialType", 5);
                shader.SetBool("uIsHit", false);

                // 3. 握持偏移：可用键盘 I/J/K/L/U/O/T/G/Y/H 实时微调
                glm::mat4 itemOffset = glm::mat4(1.0f);
                itemOffset = glm::translate(itemOffset, glm::vec3(0.55f, 0.45f, -0.2f));
                itemOffset = glm::rotate(itemOffset, glm::radians(-85.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                itemOffset = glm::scale(itemOffset, glm::vec3(1.0f));

                // 4. 矩阵级联：玩家世界矩阵 * 手部骨骼矩阵 * 握持偏移
                glm::mat4 itemModelMat = playerMat * handBoneMat * itemOffset;
                shader.SetMat4("uModel", itemModelMat);

                // 5. 绘制物品
                hotbarModels[activeSlot]->Draw(shader.ID());
            }
        }

        // Debug Draw：碰撞三角形线框（绿色，F6 切换）
        static bool showDebugWireframe = false;
        {
            static bool f6WasPressed = false;
            bool f6Pressed = glfwGetKey(window, GLFW_KEY_F6) == GLFW_PRESS;
            if (f6Pressed && !f6WasPressed)
                showDebugWireframe = !showDebugWireframe;
            f6WasPressed = f6Pressed;
        }
        if (showDebugWireframe && debugLineCount > 0)
        {
            debugShader.Use();
            debugShader.SetMat4("view", thirdPersonCamera.GetViewMatrix());
            debugShader.SetMat4("projection", thirdPersonCamera.GetProjectionMatrix());
            debugShader.SetVec3("uColor", 0.0f, 1.0f, 0.0f); // 绿色
            glBindVertexArray(debugVAO);
            glDrawArrays(GL_LINES, 0, debugLineCount);
            glBindVertexArray(0);
        }

        // ======================================================================
        // 水面半透明渲染（在所有不透明物体之后，关闭深度写入以正确混合）
        // ======================================================================
        {
            shader.Use();
            shader.SetMat4("uModel", mapModelMatrix);
            shader.SetBool("uIsHit", false);
            shader.SetInt("uMaterialType", 4); // 水面材质

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);

            for (const auto& mesh : mapModel.GetMeshes())
            {
                if (mesh.materialType != 4) continue;
                shader.SetInt("uMaterialType", mesh.materialType);
                mesh.Draw(shader.ID());
            }

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);

            // 重置材质类型，防止泄漏给后续 Pass（第一人称手臂等）
            shader.SetInt("uMaterialType", 0);
        }

        // F1 一键热重载全部着色器（边缘触发）
        {
            static bool f1WasPressed = false;
            bool f1Pressed = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
            if (f1Pressed && !f1WasPressed)
            {
                int reloaded = 0;
                for (auto* shaderPtr : allShaders)
                {
                    shaderPtr->Reload();
                    ++reloaded;
                }
                printf("[Shader] Hot-reloaded %d/%zu shaders\n", reloaded, allShaders.size());
            }
            f1WasPressed = f1Pressed;
        }

        // 天球逆矩阵：用于将 viewDir 变换回天球本地坐标系（星星随之旋转）
        glm::mat4 starMatrix = glm::inverse(celestialMat);

        // ======================================================================
        // 死亡烟雾粒子 + 剑气粒子（3D 广告牌，主场景之后、天空盒之前）
        // ======================================================================
        if (!deathParticles.empty() || !sweepParticles.empty())
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);

            uiShader.Use();
            uiShader.SetMat4("uProjection", thirdPersonCamera.GetProjectionMatrix());
            uiShader.SetInt("uTexture", 0);
            uiShader.SetVec2("uUvScale", glm::vec2(1.0f));
            uiShader.SetVec2("uUvOffset", glm::vec2(0.0f));

            glm::mat4 view = thirdPersonCamera.GetViewMatrix();

            // --- 死亡烟雾粒子 ---
            for (auto& part : deathParticles)
            {
                glActiveTexture(GL_TEXTURE0);
                smokeTexs[part.TexIdx].Bind(0);

                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, part.Position);
                model[0][0] = view[0][0]; model[0][1] = view[1][0]; model[0][2] = view[2][0];
                model[1][0] = view[0][1]; model[1][1] = view[1][1]; model[1][2] = view[2][1];
                model[2][0] = view[0][2]; model[2][1] = view[1][2]; model[2][2] = view[2][2];
                float pScale = 0.8f * (1.2f - part.Life / 0.4f);
                model = glm::scale(model, glm::vec3(pScale, pScale, 1.0f));
                model = glm::translate(model, glm::vec3(-0.5f, -0.5f, 0.0f));
                glm::mat4 viewModel = view * model;
                uiShader.SetMat4("uModel", viewModel);
                uiShader.SetFloat("uAlpha", part.Life / 0.4f);

                glBindVertexArray(uiVAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }

            // --- 剑气序列帧粒子 ---
            for (const auto& part : sweepParticles)
            {
                // 1. 根据当前寿命进度计算该画第几帧 (0-7)
                float progress = 1.0f - (part.Life / part.MaxLife);
                int frame = glm::clamp(static_cast<int>(progress * 8.0f), 0, 7);

                // 2. 绑定当前帧贴图
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, sweepTextures[frame]->ID());

                // 3. 构建广告牌模型矩阵
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, part.Position);
                model[0] = glm::vec4(view[0][0], view[1][0], view[2][0], 0.0f);
                model[1] = glm::vec4(view[0][1], view[1][1], view[2][1], 0.0f);
                model[2] = glm::vec4(view[0][2], view[1][2], view[2][2], 0.0f);
                model = glm::scale(model, glm::vec3(1.8f, -1.8f, 1.0f));
                model = glm::translate(model, glm::vec3(-0.5f, -0.5f, 0.0f));
                glm::mat4 viewModel = view * model;
                uiShader.SetMat4("uModel", viewModel);
                uiShader.SetFloat("uAlpha", 1.0f);

                glBindVertexArray(uiVAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }

            glBindVertexArray(0);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }

        // ======================================================================
        // Pass 5: 程序化雨滴（天空盒之前绘制！此时 hdrFBO 仍有完整场景深度，
        //        硬件深度测试可自动剔除被墙壁遮挡的雨滴；雨滴深度图负责屋顶遮挡）
        // ======================================================================
        if (rainIntensity > 0.01f) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_DEPTH_TEST);     // 关键：利用场景深度剔除墙后雨滴
            glDepthFunc(GL_LESS);
            glDepthMask(GL_FALSE);       // 不写深度，防止雨丝互相遮挡出现黑边

            rainShader.Use();
            rainShader.SetMat4("uView", thirdPersonCamera.GetViewMatrix());
            rainShader.SetMat4("uProjection", thirdPersonCamera.GetProjectionMatrix());
            rainShader.SetVec3("uCameraPos", thirdPersonCamera.GetPosition());
            rainShader.SetFloat("uTime", (float)glfwGetTime());
            rainShader.SetFloat("uRainIntensity", rainIntensity);
            rainShader.SetMat4("uRainSpaceMatrix", rainSpaceMatrix);
            rainShader.SetInt("uRainDepthMap", 14);
            glActiveTexture(GL_TEXTURE14);
            glBindTexture(GL_TEXTURE_2D, rainDepthMap);

            glBindVertexArray(rainVAO);
            glDrawArrays(GL_LINES, 0, 20000);
            glBindVertexArray(0);

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }

        // ======================================================================
        // 程序化天空盒：永远跟随玩家，最后绘制（Z=1.0 最远处）
        // ======================================================================
        glDepthFunc(GL_LEQUAL);
        skyShader.Use();
        skyShader.SetFloat("uTime", (float)glfwGetTime());
        // 移除视图矩阵的平移分量，让天空永远以玩家为中心
        glm::mat4 skyView = glm::mat4(glm::mat3(thirdPersonCamera.GetViewMatrix()));
        skyShader.SetMat4("uProjection", thirdPersonCamera.GetProjectionMatrix());
        skyShader.SetMat4("uView", skyView);
        skyShader.SetMat4("uStarMatrix", starMatrix);
        skyShader.SetVec3("uHorizonColor", currentHorizonColor.x, currentHorizonColor.y, currentHorizonColor.z);
        skyShader.SetVec3("uZenithColor",  currentZenithColor.x,  currentZenithColor.y,  currentZenithColor.z);
        skyShader.SetVec3("uSunDir", sunPosDir.x, sunPosDir.y, sunPosDir.z);
        skyShader.SetFloat("uRainIntensity", rainIntensity);
        glBindVertexArray(skyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        // ======================================================================
        // 第一人称手臂渲染（清空深度，始终在最前面）
        // ======================================================================
        if (thirdPersonCamera.CurrentMode == CameraMode::FirstPerson)
        {
            glClear(GL_DEPTH_BUFFER_BIT);

            shader.Use();
            shader.SetMat4("uView", glm::mat4(1.0f));
            shader.SetMat4("uProjection", thirdPersonCamera.GetProjectionMatrix());
            shader.SetBool("uIsHit", false);

            if (hotbarModels[activeSlot] == nullptr)
            {
                // --- 空手状态：只渲染手臂 ---
                shader.SetInt("uMaterialType", 0); // 手臂皮肤：默认材质
                // 先清空所有骨骼矩阵为 identity，防止切换快捷栏时
                // 上一帧手持物品状态残留的 100 个 identity 与当前
                // 动画骨骼数量不一致导致 armModel 渲染异常
                for (int i = 0; i < 100; ++i)
                {
                    shader.SetMat4("finalBonesMatrices[" + std::to_string(i) + "]", glm::mat4(1.0f));
                }
                const auto& transforms = fpAnimator.GetFinalBoneMatrices();
                for (size_t i = 0; i < transforms.size(); ++i)
                {
                    shader.SetMat4("finalBonesMatrices[" + std::to_string(i) + "]", transforms[i]);
                }

                glm::mat4 armMat = glm::mat4(1.0f);
                armMat = glm::translate(armMat, glm::vec3(0.5f, -1.8f, -1.3f));
                armMat = glm::scale(armMat, glm::vec3(0.8f));
                shader.SetMat4("uModel", armMat);

                armModel.Draw(shader.ID());
            }
            else
            {
                // --- 手持物品状态：隐藏手臂，只渲染道具 ---
                shader.SetInt("uMaterialType", 5); // 手持金属/高光道具
                for (int i = 0; i < 100; ++i)
                {
                    shader.SetMat4("finalBonesMatrices[" + std::to_string(i) + "]", glm::mat4(1.0f));
                }

                glm::mat4 itemMat = glm::mat4(1.0f);
                itemMat = glm::translate(itemMat, glm::vec3(gFpItemTranslateX, gFpItemTranslateY, gFpItemTranslateZ));
                itemMat = glm::rotate(itemMat, glm::radians(gFpItemRotateX), glm::vec3(1.0f, 0.0f, 0.0f));
                itemMat = glm::rotate(itemMat, glm::radians(gFpItemRotateY), glm::vec3(0.0f, 1.0f, 0.0f));
                itemMat = glm::rotate(itemMat, glm::radians(gFpItemRotateZ), glm::vec3(0.0f, 0.0f, 1.0f));
                itemMat = glm::scale(itemMat, glm::vec3(gFpItemScale));

                // 进食抖动视觉反馈：面包拉近+高频抖动+倾斜
                if (isEating)
                {
                    itemMat = glm::translate(itemMat, glm::vec3(-0.4f, 0.3f, 0.5f));
                    float eatShakeX = sin(glfwGetTime() * 40.0f) * 0.05f + ((rand() % 100) / 100.0f - 0.5f) * 0.02f;
                    float eatShakeY = cos(glfwGetTime() * 45.0f) * 0.05f + ((rand() % 100) / 100.0f - 0.5f) * 0.02f;
                    float eatShakeZ = sin(glfwGetTime() * 35.0f) * 0.05f;
                    itemMat = glm::translate(itemMat, glm::vec3(eatShakeX, eatShakeY, eatShakeZ));
                    itemMat = glm::rotate(itemMat, glm::radians(15.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                }

                shader.SetMat4("uModel", itemMat);

                hotbarModels[activeSlot]->Draw(shader.ID());
            }
        }

        // ======================================================================
        // 程序化雨滴绘制（VBO-less GL_LINES）
        // ======================================================================
        // Bloom Pass 1 — 亮部提取（半分辨率，提升性能 + 松软光晕感）
        // ======================================================================
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[0]);
        glViewport(0, 0, BLOOM_WIDTH, BLOOM_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT);

        blurShader.Use();
        blurShader.SetBool("uExtract", true);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdrColorTex);
        blurShader.SetInt("uTexture", 0);
        glBindVertexArray(fullscreenVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // ======================================================================
        // Pass 新增: 计算上帝光 (God Rays)
        // ======================================================================
        // 1. 将 3D 太阳坐标转换为 2D 屏幕坐标 (NDC 坐标)
        // 消除玩家实际物理位移，仅保留相机旋转（复用上方的 skyView）
        glm::mat4 godrayProj = thirdPersonCamera.GetProjectionMatrix();
        // 将太阳放在视锥体绝对安全距离（50m，小于透视矩阵 far_plane=100），保证不触发 Z 裁剪
        glm::vec4 lightPosClip = godrayProj * skyView * glm::vec4(-activeLightDir * 50.0f, 1.0f);
        glm::vec3 lightPosNDC = glm::vec3(lightPosClip) / lightPosClip.w;
        // 将 NDC 坐标 (-1 到 1) 映射到纹理坐标 (0 到 1)
        glm::vec2 lightScreenPos = glm::vec2(lightPosNDC.x * 0.5f + 0.5f, lightPosNDC.y * 0.5f + 0.5f);

        // 只有光源跑到玩家视野正后方（NDC Z 越界）时才淡出上帝光
        float godrayWeight = (lightPosNDC.z > 1.0f || lightPosNDC.z < -1.0f) ? 0.0f : 0.015f;

        // 如果玩家没有回头，执行径向模糊
        if (godrayWeight > 0.0f) {
            // 我们复用 pingpongFBO[1] 来存放生成的光柱图
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[1]);
            glViewport(0, 0, BLOOM_WIDTH, BLOOM_HEIGHT);
            glClear(GL_COLOR_BUFFER_BIT);

            godraysShader.Use();
            godraysShader.SetVec2("uLightScreenPos", lightScreenPos);
            godraysShader.SetFloat("uDensity", 0.95f); // 拉扯距离
            godraysShader.SetFloat("uDecay", 0.92f);   // 衰减速度
            godraysShader.SetFloat("uWeight", godrayWeight);

            godraysShader.SetInt("uInputTex", 0);
            glActiveTexture(GL_TEXTURE0);
            // 绑定刚提取出的高亮部分 (包含太阳)
            glBindTexture(GL_TEXTURE_2D, pingpongColorTex[0]);

            glBindVertexArray(fullscreenVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            // 【极其重要】：因为我们把算好的上帝光存在了 pingpongColorTex[1] 里
            // 为了让它也经历一点点高斯模糊变得更柔和，我们将把双通道模糊循环的起点改为 1
        }

        // ======================================================================
        // Bloom Pass 2 — 双通道高斯模糊 Ping-Pong（6 次：3 横 3 纵）
        // ======================================================================
        blurShader.SetBool("uExtract", false);
        bool horizontal = true;
        int blurSourceTex = 0; // 默认从 pingpongColorTex[0] 开始（无上帝光时）

        // 如果上帝光已生成（存在 pingpongColorTex[1]），高斯模糊从 [1] 开始
        if (godrayWeight > 0.0f) {
            horizontal = false;   // 第一轮写入 FBO[0]，避免读写冲突
            blurSourceTex = 1;    // 从上帝光贴图开始模糊
        }

        for (unsigned int i = 0; i < 6; ++i)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            blurShader.SetBool("uHorizontal", horizontal);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D,
                          (i == 0) ? pingpongColorTex[blurSourceTex] : pingpongColorTex[!horizontal]);
            blurShader.SetInt("uTexture", 0);
            glBindVertexArray(fullscreenVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            horizontal = !horizontal;
        }

        // ======================================================================
        // Bloom Pass 3 — 电影级色调映射与画面合成
        // ======================================================================
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        hdrComposeShader.Use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdrColorTex);
        hdrComposeShader.SetInt("uSceneTex", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorTex[0]);
        hdrComposeShader.SetInt("uBloomTex", 1);
        hdrComposeShader.SetFloat("uBloomIntensity", 0.35f);
        hdrComposeShader.SetFloat("uExposure", 0.5f);
        glBindVertexArray(fullscreenVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // ======================================================================
        // 2D UI 渲染（正交投影，永远贴在最上层）
        // ======================================================================
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_DEPTH_TEST);

            uiShader.Use();
            glm::mat4 orthoProj = glm::ortho(0.0f, 1920.0f, 0.0f, 1080.0f);
            uiShader.SetMat4("uProjection", orthoProj);
            uiShader.SetInt("uTexture", 0);
            glActiveTexture(GL_TEXTURE0);

            glBindVertexArray(uiVAO);

            // 重置 UV 变换、Alpha、纹理/字体模式，确保原有 UI 元素正常显示
            uiShader.SetBool("uUseTexture", true);
            uiShader.SetBool("uIsFont", false);
            uiShader.SetFloat("uAlpha", 1.0f);
            uiShader.SetVec2("uUvScale", glm::vec2(1.0f));
            uiShader.SetVec2("uUvOffset", glm::vec2(0.0f));

            // 0. 剧情黑屏过场特效（覆盖全屏，但不遮挡快捷栏和聊天）
            if (blackScreenAlpha > 0.0f) {
                uiShader.SetBool("uUseTexture", false);
                uiShader.SetVec4("uSolidColor", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
                uiShader.SetFloat("uAlpha", blackScreenAlpha);

                glm::mat4 blackMat = glm::mat4(1.0f);
                blackMat = glm::scale(blackMat, glm::vec3(1920.0f, 1080.0f, 1.0f));
                uiShader.SetMat4("uModel", blackMat);

                glDrawArrays(GL_TRIANGLES, 0, 6);

                // 恢复 UI 默认状态
                uiShader.SetBool("uUseTexture", true);
                uiShader.SetBool("uIsFont", false);
                uiShader.SetFloat("uAlpha", 1.0f);
                uiShader.SetVec2("uUvScale", glm::vec2(1.0f));
                uiShader.SetVec2("uUvOffset", glm::vec2(0.0f));
            }

            // 主菜单渲染（MENU 状态下显示，覆盖在游戏世界上方）
            if (currentGameState == GameState::MENU) {
                // 1. 渲染全屏半透明黑色遮罩，把背景的 3D 场景调暗 (45% 透明度)
                uiShader.SetBool("uUseTexture", false);
                uiShader.SetVec4("uSolidColor", glm::vec4(0.0f, 0.0f, 0.0f, 0.45f));
                uiShader.SetFloat("uAlpha", 1.0f);

                glm::mat4 bgModel = glm::mat4(1.0f);
                bgModel = glm::scale(bgModel, glm::vec3(1920.0f, 1080.0f, 1.0f));
                uiShader.SetMat4("uModel", bgModel);
                uiShader.SetVec2("uUvScale", glm::vec2(1.0f));
                uiShader.SetVec2("uUvOffset", glm::vec2(0.0f));
                glDrawArrays(GL_TRIANGLES, 0, 6);

                // 2. 绘制 "开始游戏" 按钮底框 (Minecraft 经典灰色按钮 + 黑色描边)
                // 按钮大小为 800x80，中心在 X: 560, Y: 500
                float btnX = 560.0f;
                float btnY = 500.0f;
                float btnWidth = 800.0f;
                float btnHeight = 80.0f;

                // A. 先画外围黑色描边框 (稍微大 4 像素)
                uiShader.SetVec4("uSolidColor", glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));
                glm::mat4 btnBorder = glm::mat4(1.0f);
                btnBorder = glm::translate(btnBorder, glm::vec3(btnX - 4.0f, btnY - 4.0f, 0.0f));
                btnBorder = glm::scale(btnBorder, glm::vec3(btnWidth + 8.0f, btnHeight + 8.0f, 1.0f));
                uiShader.SetMat4("uModel", btnBorder);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                // B. 画内侧灰色面板
                uiShader.SetVec4("uSolidColor", glm::vec4(0.58f, 0.58f, 0.58f, 1.0f));
                glm::mat4 btnPanel = glm::mat4(1.0f);
                btnPanel = glm::translate(btnPanel, glm::vec3(btnX, btnY, 0.0f));
                btnPanel = glm::scale(btnPanel, glm::vec3(btnWidth, btnHeight, 1.0f));
                uiShader.SetMat4("uModel", btnPanel);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                // 3. 绘制按钮中央的文字 "开始游戏"
                uiShader.SetBool("uUseTexture", true);
                uiShader.SetBool("uIsFont", true);
                uiShader.SetVec3("uFontColor", glm::vec3(1.0f, 0.95f, 0.8f)); // 淡淡的金色字体
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, fontRenderer.TextureID());
                // scale 3.0，4 个 CJK 字约 384px 宽，按钮中心 960，文字从 768 开始居中
                fontRenderer.RenderText(uiShader, uiVAO,
                    "开始游戏", btnX + 208.0f, btnY + 25.0f, 3.0f, glm::vec3(1.0f, 0.95f, 0.8f));

                // 4. 重置状态，防止影响后续渲染
                uiShader.SetBool("uIsFont", false);
                uiShader.SetVec3("uFontColor", glm::vec3(1.0f));
                uiShader.SetBool("uUseTexture", true);
                uiShader.SetFloat("uAlpha", 1.0f);
                uiShader.SetVec2("uUvScale", glm::vec2(1.0f));
                uiShader.SetVec2("uUvOffset", glm::vec2(0.0f));
                glActiveTexture(GL_TEXTURE0);
            }

            // 游戏中才渲染快捷栏等 HUD 元素
            if (currentGameState == GameState::PLAYING)
            {

            // 1. 渲染 9 格快捷栏底框（原版 182x22 放大 4 倍 = 728x88）
            float scaleFactor = 4.0f;
            float hbWidth = 182.0f * scaleFactor;
            float hbHeight = 22.0f * scaleFactor;
            float hbX = (1920.0f - hbWidth) / 2.0f;
            float hbY = 15.0f;

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(hbX, hbY, 0.0f));
            // 中心轴上下翻转 180°
            model = glm::translate(model, glm::vec3(hbWidth * 0.5f, hbHeight * 0.5f, 0.0f));
            model = glm::scale(model, glm::vec3(1.0f, -1.0f, 1.0f));
            model = glm::translate(model, glm::vec3(-hbWidth * 0.5f, -hbHeight * 0.5f, 0.0f));
            model = glm::scale(model, glm::vec3(hbWidth, hbHeight, 1.0f));
            uiShader.SetMat4("uModel", model);
            hotbarTex.Bind(0);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            // 2. 渲染动态选中框（原版 24x24 放大 4 倍 = 96x96）
            float selSize = 24.0f * scaleFactor;
            float selX = (hbX - 4.0f) + activeSlot * 20.0f * scaleFactor;
            float selY = hbY - 4.0f;

            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(selX, selY, 0.0f));
            // 中心轴上下翻转 180°
            model = glm::translate(model, glm::vec3(selSize * 0.5f, selSize * 0.5f, 0.0f));
            model = glm::scale(model, glm::vec3(1.0f, -1.0f, 1.0f));
            model = glm::translate(model, glm::vec3(-selSize * 0.5f, -selSize * 0.5f, 0.0f));
            model = glm::scale(model, glm::vec3(selSize, selSize, 1.0f));
            uiShader.SetMat4("uModel", model);
            selectorTex.Bind(0);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            // 3. 渲染 10 颗红心血条（原版 9×9 放大 4 倍 = 36×36，间距 32px）
            float heartSize = 9.0f * scaleFactor;
            float heartX = hbX;
            float heartY = hbY + hbHeight + 10.0f;
            float heartStride = 8.0f * scaleFactor;

            for (int i = 0; i < 10; ++i)
            {
                glm::mat4 heartMat = glm::mat4(1.0f);
                heartMat = glm::translate(heartMat, glm::vec3(heartX + i * heartStride, heartY, 0.0f));
                // 中心轴垂直翻转 180°
                heartMat = glm::translate(heartMat, glm::vec3(heartSize * 0.5f, heartSize * 0.5f, 0.0f));
                heartMat = glm::scale(heartMat, glm::vec3(1.0f, -1.0f, 1.0f));
                heartMat = glm::translate(heartMat, glm::vec3(-heartSize * 0.5f, -heartSize * 0.5f, 0.0f));
                heartMat = glm::scale(heartMat, glm::vec3(heartSize, heartSize, 1.0f));
                uiShader.SetMat4("uModel", heartMat);

                // 先画空心底座
                heartEmpty.Bind(0);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                // 根据剩余血量覆盖实心或半心
                int hpLeft = playerHP - (i * 2);
                if (hpLeft >= 2)
                {
                    heartFull.Bind(0);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
                else if (hpLeft == 1)
                {
                    heartHalf.Bind(0);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
            }

            // 4. 循环绘制物品图标（原版 16x16 像素）
            float iconSize = 16.0f * scaleFactor;
            float iconBaseX = hbX + 3.0f * scaleFactor;
            float iconBaseY = hbY + 3.0f * scaleFactor;
            float iconStride = 20.0f * scaleFactor;

            for (int i = 0; i < 9; ++i)
            {
                if (hotbarIcons[i] != nullptr)
                {
                    float currentIconX = iconBaseX + i * iconStride;

                    model = glm::mat4(1.0f);
                    model = glm::translate(model, glm::vec3(currentIconX, iconBaseY, 0.0f));
                    model = glm::translate(model, glm::vec3(iconSize * 0.5f, iconSize * 0.5f, 0.0f));
                    model = glm::scale(model, glm::vec3(1.0f, -1.0f, 1.0f)); // 垂直镜像
                    model = glm::translate(model, glm::vec3(-iconSize * 0.5f, -iconSize * 0.5f, 0.0f));
                    model = glm::scale(model, glm::vec3(iconSize, iconSize, 1.0f));
                    uiShader.SetMat4("uModel", model);
                    hotbarIcons[i]->Bind(0);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
            }

            // 4. 渲染屏幕中心准星（原版 15x15 放大 4 倍，仅第一人称显示）
            if (thirdPersonCamera.CurrentMode == CameraMode::FirstPerson)
            {
                float chSize = 15.0f * scaleFactor;
                float chX = (1920.0f - chSize) / 2.0f;
                float chY = (1080.0f - chSize) / 2.0f;

                model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(chX, chY, 0.0f));
                model = glm::scale(model, glm::vec3(chSize, chSize, 1.0f));
                uiShader.SetMat4("uModel", model);
                crosshairTex.Bind(0);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }

            // 5. 左下角聊天栏 / 系统提示渲染
            {
                float startX = 30.0f;
                float startY = 150.0f;
                float lineHeight = 25.0f;

                if (isChatHistoryOpen) {
                    // ==========================================
                    // 分支 A：历史查看模式 (不淡出，画最近 12 条记录)
                    // ==========================================
                    size_t numHistoryLines = std::min(chatHistory.size(), size_t(12));
                    size_t startIdx = chatHistory.size() - numHistoryLines;

                    // 1. 画一个覆盖整个历史区域的巨大半透明黑色背景板
                    uiShader.SetBool("uUseTexture", false);
                    uiShader.SetVec4("uSolidColor", glm::vec4(0.0f, 0.0f, 0.0f, 0.55f));
                    uiShader.SetFloat("uAlpha", 1.0f);

                    glm::mat4 bgModel = glm::mat4(1.0f);
                    bgModel = glm::translate(bgModel, glm::vec3(startX - 5.0f, startY - 5.0f, 0.0f));
                    bgModel = glm::scale(bgModel, glm::vec3(600.0f, numHistoryLines * lineHeight + 10.0f, 1.0f));
                    uiShader.SetMat4("uModel", bgModel);
                    uiShader.SetVec2("uUvScale", glm::vec2(1.0f));
                    uiShader.SetVec2("uUvOffset", glm::vec2(0.0f));
                    glDrawArrays(GL_TRIANGLES, 0, 6);

                    // 2. 依次渲染不透明的历史文字
                    uiShader.SetBool("uUseTexture", true);
                    uiShader.SetBool("uIsFont", true);
                    uiShader.SetFloat("uAlpha", 1.0f);
                    uiShader.SetVec3("uFontColor", glm::vec3(1.0f));
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, fontRenderer.TextureID());

                    for (size_t i = 0; i < numHistoryLines; ++i) {
                        float currentY = startY + i * lineHeight;
                        fontRenderer.RenderText(uiShader, uiVAO,
                            chatHistory[startIdx + i], startX, currentY,
                            0.7f, glm::vec3(1.0f));
                    }

                    // 恢复默认 UI 渲染状态
                    uiShader.SetBool("uUseTexture", true);
                    uiShader.SetBool("uIsFont", false);
                    uiShader.SetFloat("uAlpha", 1.0f);
                    uiShader.SetVec2("uUvScale", glm::vec2(1.0f));
                    uiShader.SetVec2("uUvOffset", glm::vec2(0.0f));
                    glActiveTexture(GL_TEXTURE0);
                }
                else if (!chatLog.empty()) {
                    // ==========================================
                    // 分支 B：平时模式 (原有的 chatLog 自动淡出逻辑)
                    // ==========================================
                    float chatY = 130.0f;
                    const float fontSize = 0.9f;

                    for (auto& line : chatLog)
                    {
                        float alphaFade = line.lifeTime / 5.0f;
                        if (alphaFade < 0.05f) continue;

                        // 估算文本像素宽度（中文 ~18px，英文 ~10px，取混合估算）
                        float textWidth = line.text.size() * 10.0f * fontSize;
                        float bgW = textWidth + 24.0f;
                        float bgH = 28.0f;

                        // 半透明黑色背景板
                        uiShader.SetBool("uUseTexture", false);
                        uiShader.SetVec4("uSolidColor", glm::vec4(0.0f, 0.0f, 0.0f, 0.45f));
                        uiShader.SetFloat("uAlpha", alphaFade);

                        glm::mat4 bgModel = glm::mat4(1.0f);
                        bgModel = glm::translate(bgModel, glm::vec3(12.0f, chatY, 0.0f));
                        bgModel = glm::scale(bgModel, glm::vec3(bgW, bgH, 1.0f));
                        uiShader.SetMat4("uModel", bgModel);
                        uiShader.SetVec2("uUvScale", glm::vec2(1.0f));
                        uiShader.SetVec2("uUvOffset", glm::vec2(0.0f));
                        glDrawArrays(GL_TRIANGLES, 0, 6);

                        // 白色中文文字
                        uiShader.SetBool("uUseTexture", true);
                        uiShader.SetBool("uIsFont", true);
                        uiShader.SetVec3("uFontColor", glm::vec3(1.0f));
                        uiShader.SetFloat("uAlpha", alphaFade);
                        // 绑定字体纹理到单元 0
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, fontRenderer.TextureID());

                        fontRenderer.RenderText(uiShader, uiVAO,
                                                line.text, 20.0f, chatY + 17.0f,
                                                fontSize, glm::vec3(1.0f));

                        chatY += 30.0f;
                    }

                    // 恢复默认 UI 渲染状态
                    uiShader.SetBool("uUseTexture", true);
                    uiShader.SetBool("uIsFont", false);
                    uiShader.SetFloat("uAlpha", 1.0f);
                    uiShader.SetVec2("uUvScale", glm::vec2(1.0f));
                    uiShader.SetVec2("uUvOffset", glm::vec2(0.0f));
                    glActiveTexture(GL_TEXTURE0);
                }
            }

            // ==========================================
            // F2 时间选择面板（十字形四按钮）
            // ==========================================
            if (isTimeSelectOpen) {
                // 1. 全屏半透明黑色遮罩
                uiShader.SetBool("uUseTexture", false);
                uiShader.SetVec4("uSolidColor", glm::vec4(0.0f, 0.0f, 0.0f, 0.45f));
                uiShader.SetFloat("uAlpha", 1.0f);

                glm::mat4 overlayModel = glm::mat4(1.0f);
                overlayModel = glm::scale(overlayModel, glm::vec3(1920.0f, 1080.0f, 1.0f));
                uiShader.SetMat4("uModel", overlayModel);
                uiShader.SetVec2("uUvScale", glm::vec2(1.0f));
                uiShader.SetVec2("uUvOffset", glm::vec2(0.0f));
                glDrawArrays(GL_TRIANGLES, 0, 6);

                // 2. 绘制四个时间选择按钮
                const float btnW = 280.0f;
                const float btnH = 70.0f;
                const float fontSize = 2.5f;
                const glm::vec3 textColor(1.0f, 0.95f, 0.8f); // 淡金色

                struct TimeButton {
                    float x, y;       // top-left corner
                    const char* label;
                };

                TimeButton buttons[] = {
                    { 820.0f, 580.0f, "正午" },   // top:    center (960, 615)
                    { 820.0f, 340.0f, "午夜" },   // bottom: center (960, 375)
                    { 630.0f, 470.0f, "清晨" },   // left:   center (770, 505)
                    { 1010.0f, 470.0f, "黄昏" },  // right:  center (1150,505)
                };

                for (const auto& btn : buttons) {
                    // A. 外围黑色描边
                    uiShader.SetBool("uUseTexture", false);
                    uiShader.SetVec4("uSolidColor", glm::vec4(0.15f, 0.15f, 0.15f, 1.0f));
                    glm::mat4 borderModel = glm::mat4(1.0f);
                    borderModel = glm::translate(borderModel, glm::vec3(btn.x - 3.0f, btn.y - 3.0f, 0.0f));
                    borderModel = glm::scale(borderModel, glm::vec3(btnW + 6.0f, btnH + 6.0f, 1.0f));
                    uiShader.SetMat4("uModel", borderModel);
                    glDrawArrays(GL_TRIANGLES, 0, 6);

                    // B. 内侧灰色面板
                    uiShader.SetVec4("uSolidColor", glm::vec4(0.45f, 0.45f, 0.45f, 1.0f));
                    glm::mat4 panelModel = glm::mat4(1.0f);
                    panelModel = glm::translate(panelModel, glm::vec3(btn.x, btn.y, 0.0f));
                    panelModel = glm::scale(panelModel, glm::vec3(btnW, btnH, 1.0f));
                    uiShader.SetMat4("uModel", panelModel);
                    glDrawArrays(GL_TRIANGLES, 0, 6);

                    // C. 按钮文字（居中，2个CJK字约180px宽 @ scale 2.5）
                    uiShader.SetBool("uUseTexture", true);
                    uiShader.SetBool("uIsFont", true);
                    uiShader.SetVec3("uFontColor", textColor);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, fontRenderer.TextureID());
                    fontRenderer.RenderText(uiShader, uiVAO,
                        btn.label, btn.x + 50.0f, btn.y + 18.0f,
                        fontSize, textColor);
                }

                // 3. 绘制提示文字（屏幕上方）
                {
                    uiShader.SetBool("uIsFont", true);
                    uiShader.SetVec3("uFontColor", glm::vec3(0.8f, 0.8f, 0.8f));
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, fontRenderer.TextureID());
                    fontRenderer.RenderText(uiShader, uiVAO,
                        "选择时间", 860.0f, 720.0f, 2.0f, glm::vec3(0.8f, 0.8f, 0.8f));
                }

                // 4. 恢复默认 UI 渲染状态
                uiShader.SetBool("uUseTexture", true);
                uiShader.SetBool("uIsFont", false);
                uiShader.SetFloat("uAlpha", 1.0f);
                uiShader.SetVec2("uUvScale", glm::vec2(1.0f));
                uiShader.SetVec2("uUvOffset", glm::vec2(0.0f));
                uiShader.SetVec3("uFontColor", glm::vec3(1.0f));
                glActiveTexture(GL_TEXTURE0);
            }

            // F3 调试屏幕：使用 TrueType 字体渲染玩家坐标和 FPS
            if (showDebugScreen)
            {
                // 确保字体渲染状态正确
                uiShader.SetBool("uUseTexture", true);
                uiShader.SetBool("uIsFont", true);
                uiShader.SetFloat("uAlpha", 1.0f);

                int px = static_cast<int>(std::round(player.Position.x));
                int py = static_cast<int>(std::round(player.Position.y));
                int pz = static_cast<int>(std::round(player.Position.z));

                std::string coordStr = "XYZ: " + std::to_string(px) + " "
                                     + std::to_string(py) + " "
                                     + std::to_string(pz);

                // 绑定字体纹理
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, fontRenderer.TextureID());
                fontRenderer.RenderText(uiShader, uiVAO, coordStr, 20.0f, 1040.0f, 1.2f, glm::vec3(1.0f));

                std::string fpsStr = "FPS: " + std::to_string(currentFPS);
                fontRenderer.RenderText(uiShader, uiVAO, fpsStr, 20.0f, 1010.0f, 1.2f, glm::vec3(1.0f));

                // 恢复普通 UI 状态
                glActiveTexture(GL_TEXTURE0);
                uiShader.SetBool("uIsFont", false);
                uiShader.SetVec2("uUvScale", glm::vec2(1.0f));
                uiShader.SetVec2("uUvOffset", glm::vec2(0.0f));
            }

            } // if (currentGameState == GameState::PLAYING)

            glBindVertexArray(0);
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    if (gLogFile) fclose(gLogFile);
    ma_engine_uninit(&audioEngine);
    glfwTerminate();
    return 0;
}
