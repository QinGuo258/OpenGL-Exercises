# src/ 目录源码文件说明

`src/` 下共 15 个 `.cpp` 文件，各文件功能详解如下。

---

## 1. `main.cpp` — 程序入口 & 渲染主循环

**~2000+ 行，最庞大的文件**，统领全局运行框架。

| 模块 | 功能 |
|------|------|
| **GLFW/GLAD 初始化** | 窗口 1920×1080，OpenGL 4.6 Core Profile |
| **8 个 FBO 创建** | Shadow map (4096²)、Rain depth (1024²)、G-Buffer (RGBA16F×2 + DEPTH24)、SSAO (GL_RED)、SSAO Blur、HDR (RGBA16F)、Bloom ping-pong ×2 |
| **VAO 创建** | 天空盒 unit cube、雨粒子 VBO-less (VAO only)、UI 2D quad、全屏 quad |
| **游戏状态机** | `GameState { MENU, PLAYING }` — 主菜单与游戏中切换 |
| **故事系统** | `StoryState` 6 阶段剧情脚本：游荡→动画→前往城门→防守→结局→完成 |
| **多 Pass 渲染循环** | Shadow Map → G-Buffer → SSAO → SSAO Blur → Main Scene (HDR) → Particles → Skybox → FP Arm → Rain → 2D UI → Post-Processing (Bloom + God Rays + Tone Mapping) |
| **玩家系统** | HP 20（10 颗心）、受伤无敌 0.5s、进食（面包 1.5s 回 4 HP）、死亡重生 |
| **战斗系统** | 左键射线检测（3.5m）、剑/空手伤害、横扫之刃 AOE（3m 范围 + 90° 锥形）、击退 |
| **粒子系统** | `DeathParticle`（敌人死亡烟雾 10 粒子 8 纹理变体）、`SweepParticle`（剑气 8 帧序列帧） |
| **动态时间** | `timeOfDay` 弧度制，24 分钟一昼夜；F2 快速跳转面板（正午/黄昏/午夜/清晨） |
| **天气系统** | R 键开关雨，2s 平滑过渡，`rainIntensity` 驱动着色器效果 |
| **F1 热重载** | 仅 `sky.frag` 可从源码目录热重载 |
| **UI 绘制** | 准星、血条心形、快捷栏（9 格）、物品图标、聊天记录（T 键）、F3 调试覆盖层 |
| **音频** | miniaudio 引擎初始化、所有音效触发（挥剑/受伤/死亡/射箭/钟声） |

---

## 2. `Player.cpp` — 玩家角色控制器

| 功能 | 细节 |
|------|------|
| **圆柱体碰撞** | 半径 0.4m，高度 1.8m |
| **5 状态运动 FSM** | WALK (4.0)、RUN (8.0)、SNEAK_WALK (2.0)、EATING_WALK (1.6)、IDLE (0) |
| **6 步物理更新** | 重力 → 落地检测 → 墙面排斥 → 掉落死亡 (< -50) → 调试打印 → Y 偏移 |
| **跳跃** | 力度 6.0，仅在着地时可用 |
| **移动方向** | 相对摄像机朝向的 WASD 方向解耦 |
| **动画驱动** | 状态机选择动画（idle/walk/sprint/sneak/eat）、UpdateAnimation 调用 |
| **骨骼渲染** | 上传 `finalBonesMatrices[100]` 到着色器 |
| **第一人称** | 支持隐藏头部骨骼 |

---

## 3. `Enemy.cpp` — 敌对生物 AI & 物理

| 功能 | 细节 |
|------|------|
| **两种类型** | `ZOMBIE` (速度 2.5) 和 `SKELETON` (速度 1.8) |
| **Zombie AI** | >15m 闲置 → 1.5–15m 行走追击 → ≤1.5m 撕咬 (HP-2 + 击退 + 0.5s 无敌)，攻击冷却 1.5s |
| **Skeleton AI** | >15m 闲置 → 8–15m 站定射箭 → <8m 后撤风筝 (80% 速度)，射击冷却 4s |
| **双轨动画** | 基底轨（idle/walk）+ 覆盖轨（attack，2 根臂骨覆盖） |
| **物理** | 共享 Player 的落地检测 + 墙面排斥 + 掉落死亡 |
| **受击反馈** | `StunTimer` 1.0s 暂停 AI，前 0.2s 红色闪烁 (`uIsHit`) |
| **骨骼矩阵** | 预填充骨骼矩阵到指定位置，支持共享骨架的不同动画 |
| **诊断日志** | `printf` 输出 AI 状态切换 |

---

## 4. `Arrow.cpp` — 箭矢抛射物

| 功能 | 细节 |
|------|------|
| **抛物线物理** | 重力 -9.8m/s²，初速 25m/s，从玩家眼睛发射 |
| **墙壁碰撞** | `CollisionWorld::Raycast` 检测命中 → 插墙状态 |
| **玩家命中** | 距离 < 1.0m + 高度在身体范围 → HP-2 + 0.5s 无敌 |
| **生命周期** | 10s 后自动销毁，插墙箭矢不移动但继续计时 |
| **音效** | 命中时播放 `Weak_attack1.mp3` |

---

## 5. `CollisionWorld.cpp` — CPU 3D 碰撞检测

| 功能 | 细节 |
|------|------|
| **三角形提取** | 从 Model 的 Mesh 提取世界空间三角形（应用 model matrix 变换） |
| **植物/水排除** | `materialType == 1`（植物）和 `materialType == 4`（水）不生成碰撞三角形 |
| **2D 空间哈希网格** | 单元格大小 5.0，key 为 `int64_t` 编码 (cx, cz)，加速空间查询 |
| **射线检测** | Möller–Trumbore 算法 (`glm::gtx::intersect`)，返回最近命中点和距离 |
| **几何工具** | `ClosestPointOnTriangle2D`、`PointToSegmentDist2D`、`TriangleHeightAtXZ` — 用于角色站立高度计算 |
| **调试线框** | 生成碰撞三角形的绿色线框顶点数据（F6 切换） |

---

## 6. `Model.cpp` — Assimp 模型加载

| 功能 | 细节 |
|------|------|
| **Assimp 导入** | `Triangulate | GenNormals | CalcTangentSpace`，无 FlipUVs |
| **递归节点树遍历** | `ProcessNode` → `ProcessMesh`，建立 Mesh 列表 + 骨骼信息 |
| **材质分类** | 通过 `AI_MATKEY_NAME` 子串匹配自动分类 6 种 materialType |
| **自发光点光源提取** | `materialType == 3` 的顶点聚类（0.5m 半径均值）→ `pointLights` |
| **骨骼/权重提取** | 每顶点 4 根骨骼 ID + 权重 |
| **包围盒** | 全模型 AABB 计算 |
| **日志** | 输出网格数、材质数、顶点/面数、贴图路径 |

---

## 7. `Animation.cpp` — 骨骼动画系统

| 功能 | 细节 |
|------|------|
| **Bone 类** | TRS 关键帧数据存储 (Position/Rotation/Scale)，`InterpolatePosition/Rotation/Scaling` 线性/LERP 插值 |
| **Animation 类** | Assimp 加载单条动画，`LoadAll` 工厂函数从场景提取全部动画到 `map<string, shared_ptr<Animation>>` |
| **Animator 类** | 双轨动画混合：基底轨 (base) + 叠加轨 (overlay)，按骨骼存在性覆盖；`m_NodeGlobalTransforms` 缓存；`GetNodeGlobalTransform(name)` 按名称查询节点全局变换 |
| **头部控制** | `SetHeadRotation` 旋转头部骨骼，`SetHideHead` 第一人称隐藏头部 |
| **循环播放** | `UpdateAnimation` 使用 `fmod(m_CurrentTime, duration)` 无限循环 |
| **骨骼矩阵上传** | `GetFinalBoneMatrices()` 返回 100 个 `mat4` 给着色器 |

---

## 8. `Mesh.cpp` — VAO/VBO/EBO 包装器

| 功能 | 细节 |
|------|------|
| **GPU 缓冲区** | `SetupMesh()` 创建 VAO + VBO + EBO |
| **6 属性顶点布局** | loc=0 Position, loc=1 Normal, loc=2 TexCoords, loc=3 BoneIDs (ivec4), loc=4 Weights (4 floats), loc=5 Color (RGB) |
| **绘制** | `glBindVertexArray` + `glDrawElements` + 纹理绑定 (diffuse/specular/normal/roughness) |
| **移动语义** | move 构造/赋值，自动 `Release()` 旧资源 |
| **材质类型** | `int materialType` 成员，在渲染时控制着色器分支 |

---

## 9. `Shader.cpp` — GLSL 着色器编译 & Uniform 设置

| 功能 | 细节 |
|------|------|
| **编译** | `CompileShader` 返回 0 表示失败（`glDeleteShader` + stderr 日志） |
| **链接** | 构造函数检测 0 后 bail，避免链接损坏的程序 |
| **热重载** | `Reload()` — 从源码目录重读（`SHADER_SRC_DIR` 宏前缀），重新编译，原子替换 GL program；失败时保留旧程序 |
| **Uniform 设置** | `SetMat4`、`SetVec3`、`SetVec2`、`SetFloat`、`SetInt`、`SetBool` — 全部通过 `glGetUniformLocation` + `glUniform*` |
| **移动语义** | move 构造/赋值，析构 `glDeleteProgram` |

---

## 10. `Texture.cpp` — stb_image 纹理加载

| 功能 | 细节 |
|------|------|
| **加载** | stb_image，强制 RGBA 4 通道，Y 轴翻转 (`stbi_set_flip_vertically_on_load(true)`) |
| **参数** | `GL_CLAMP_TO_EDGE`、`GL_NEAREST` 过滤（像素风格一致） |
| **嵌入式纹理** | `LoadFromMemory` 支持 Assimp 嵌入的纹理数据 |
| **移动语义** | move 构造/赋值，析构 `glDeleteTextures` |

---

## 11. `FontRenderer.cpp` — TrueType 字体渲染

| 功能 | 细节 |
|------|------|
| **字体烘焙** | 调用 C 的 `bake_font_atlas`，将 21151 个字符（ASCII 95 + CJK 标点 64 + CJK 统一汉字 20992）烘焙到 8192×8192 `GL_RED` 单通道图集 |
| **UTF-8 解码** | 手动解析多字节序列 → Unicode 码点 |
| **渲染** | `RenderText()` 逐字符生成 quad，从图集提取 UV offset/scale，通过 256 个 `uCharTransforms[]` 统一批次上传，再 instanced 绘制 |
| **着色器状态** | 设置 `uIsFont=true`、`uFontColor`，绘制后恢复 `uIsFont=false` |

---

## 12. `ThirdPersonCamera.cpp` — 三模式摄像机

| 功能 | 细节 |
|------|------|
| **三种模式** | `FirstPerson`（眼睛位置）、`ThirdPersonBack`（身后）、`ThirdPersonFront`（身前），F5 循环切换 |
| **Pitch 限制** | [-89°, 89°]，防止万向节锁 |
| **视角矩阵** | `GetViewMatrix()` 按模式计算 lookAt |
| **FOV 动态** | `SetFOV(float)` 运行时改变视场角（冲刺 45°→55°） |
| **遮挡检测** | 第三人称时射线检测摄像机是否被墙壁遮挡 |
| **完整前向** | `GetFullFrontVector()` 含 pitch + yaw 的 3D 方向向量 |

---

## 13. `Audio.cpp` — miniaudio 音频实现单元

只有 **3 行代码**：

```cpp
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
```

**作用**：隔离 `miniaudio.h` 的 `#define MINIAUDIO_IMPLEMENTATION` 到单独的翻译单元。单个头文件库的 `IMPLEMENTATION` 宏展开大量函数体，单独编译避免每次修改其他文件都重编译音频库。

---

## 14. `Camera.cpp` — 未使用的通用摄像机

实现了完整的轨道摄像机（绕目标点旋转）：`GetPosition()`（球坐标 → 直角坐标）、`GetViewMatrix()`（lookAt）、`ProcessMouseMovement()`（鼠标旋转）、`ProcessMouseScroll()`（滚轮缩放）。**当前项目使用 `ThirdPersonCamera` 替代，此文件已闲置但保留在 CMakeLists 中。**

---

## 15. `FPSCamera.cpp` — 未使用的第一人称摄像机

实现了经典 FPS 自由飞行摄像机：WASD 键盘移动、鼠标朝向、`updateCameraVectors()` 正交基更新 (Front/Right/Up)。**当前项目使用 `ThirdPersonCamera` 的第一人称模式替代，此文件已闲置但保留在 CMakeLists 中。**

---

## 功能依赖关系图

```
main.cpp ──┬── Player.cpp ──────┬── CollisionWorld.cpp
           │                    ├── Model.cpp ── Mesh.cpp ── Texture.cpp
           │                    └── Animation.cpp
           ├── Enemy.cpp ───────┬── CollisionWorld.cpp
           │                    ├── Model.cpp
           │                    └── Animation.cpp
           ├── Arrow.cpp ───────┬── CollisionWorld.cpp
           │                    └── Audio.cpp
           ├── ThirdPersonCamera.cpp
           ├── Shader.cpp
           ├── FontRenderer.cpp
           └── Audio.cpp
```

**未使用**：`Camera.cpp`、`FPSCamera.cpp` — 仅编译但不参与运行时逻辑。
