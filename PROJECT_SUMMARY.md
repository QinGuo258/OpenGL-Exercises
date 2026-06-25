# 项目总结文档 (Project Summary)

> **OpenGL — 从零实现的 3D 动作沙盒游戏引擎**
>
> 计算机图形学课程展示项目 · 纯 C++20 + OpenGL 4.6 Core Profile 手写实现 · 未使用任何第三方图形引擎

---

## 项目概述 (Project Overview)

本项目是一个**使用现代 C++20 和 OpenGL 4.6 Core Profile 从零实现的 3D 动作沙盒游戏引擎/框架**。项目的核心目标是深入理解实时渲染管线的底层工作原理——每一行着色器代码、每一个矩阵运算、每一段物理逻辑均为手写实现，不依赖 Unity、Unreal Engine 或其他任何现成图形中间件。

### 核心特色

- **高度拟真的 Minecraft 风格图形渲染**：程序化天空盒（含体积云与网格化星空）、动态昼夜循环（含日落过渡与月光阴影）、基于物理的软阴影、HDR + ACES 电影级色调映射、SSAO 环境光遮蔽、屏幕空间上帝光（God Rays / 丁达尔效应）、全屏高斯泛光（Bloom）。
- **数据驱动的游戏玩法系统**：手写运动学角色控制器与 3D 碰撞检测、双轨骨骼动画混合、精准射线战斗判定、僵尸/骷髅双类型 AI、脚本化剧情导演系统（6 阶段线性叙事 + 电影级过场）。
- **完整的中文 UI 体系**：基于 `stb_truetype` 从零烘焙的 CJK TrueType 字体渲染器（含 21,000+ 汉字），像素级对齐的快捷栏、血量 HUD、聊天/提示信息与调试覆盖层。
- **从零手搓理念**：项目中的所有图形特效——包括水面有限差分法线扰动、菲涅尔反射、植被顶点颜色烘焙风摆、VBO-less GPU 粒子雨与深度剔除——均为纯 GLSL Shader 数学推导与实现，无任何成品特效库或图形中间件依赖。

---

## 核心技术栈 (Core Technologies Used)

### 编程语言与图形 API

| 技术 | 版本/说明 |
|------|----------|
| **C++** | ISO C++20 (`std::erase_if`、Designated Initializers、`std::make_unique` 等) |
| **OpenGL** | 4.6 Core Profile（无废弃固定管线，全程可编程管线） |
| **GLSL** | 核心着色器语言，含 24 套顶点/片段着色器 |

### 第三方基础库

| 库 | 用途 | 集成方式 |
|----|------|---------|
| **GLFW 3.x** | 窗口创建、输入处理（鼠标/键盘/滚轮） | 源码编译 (`add_subdirectory`) |
| **GLAD** | OpenGL 4.6 Core 函数加载 | 静态库 |
| **GLM** | 数学库（矩阵/向量/四元数/几何运算） | 纯头文件 (Header-only) |
| **Assimp** | 3D 模型导入（.glb/.fbx/.obj → 网格+骨骼+材质） | 源码编译，禁用测试/文档/工具 |
| **stb_image** | 纹理加载（PNG → RGBA8） | 源码编译 |
| **stb_truetype** | TrueType 字体光栅化（.ttf → 单通道位图图集） | 单头文件，C 编译隔离 |
| **miniaudio** | 音频引擎（MP3 音效播放） | 单头文件，隔离在 `Audio.cpp` 中编译 |

### 构建系统

- **CMake + Ninja + MSVC (cl.exe)**：通过 `CMakePresets.json` 管理 4 组预设配置（x64/x86 × Debug/Release）。
- **POST_BUILD 自动资源复制**：`shaders/`, `models/`, `textures/`, `audio/`, `fonts/` 在构建后自动镜像到可执行文件目录，无需手动管理。
- **`/utf-8` 编译标志**：确保全中文注释和字符串字面量在 MSVC 下正确编译。

### Shader 热重载 (Shader Hot-Reload)

按下 **F1** 键，天空着色器 (`sky.frag`) 从源目录重新读取、重新编译、原子交换 GL Program。编译失败时保留旧程序，不会引发运行时崩溃。这种**零重启-所见即所得**的调试流程极大地加速了着色器开发迭代——传统图形开发中每次修改着色器需重启整个应用程序，而热重载将这一过程缩短为亚秒级，是本项目开发效率的关键支撑。

```cpp
// Shader::Reload() 核心逻辑（main.cpp 中 F1 边缘触发）
// 1. 从 SHADER_SRC_DIR 宏指向的源码目录重新读取 .vert/.frag
// 2. glCreateShader → glShaderSource → glCompileShader
// 3. 编译失败则 glDeleteShader，保留当前程序，输出错误日志
// 4. 成功则 glCreateProgram → glAttachShader → glLinkProgram
// 5. glDeleteProgram(旧程序) → 原子替换为 m_ID = 新程序
```

---

## 图形渲染系统亮点 (Graphics Rendering System Highlights)

> **一切高级特效均为从零手搓，纯 Shader 数学实现。本项目未使用任何第三方图形引擎、特效中间件或成品渲染框架。**

### 1. 光照与阴影 (Lighting & Shadows)

#### 动态昼夜循环与月光阴影

- **天球轨道模型**：`rotateZ(timeOfDay)` 旋转太阳/月亮方向，`timeScale = 0.5`（约 12 秒一个完整昼夜周期）。
- **日落过渡**：`sunY`（太阳高度角）在黄昏区间内平滑插值——`dayLight`（暖色日光）→ `sunsetLight`（橙红晚霞）→ `moonLight`（极暗冷色月光），同时环境光从 `dayAmbient` 过渡到 `nightAmbient`。
- **夜空反向光源**：当 `sunY ≤ 0`（太阳在地平线以下），月光从下方照射——`activeLightDir = sunPosDir`（反向），`ambientColor` 锁定为深蓝夜光。
- **阴影跟随**：`lightPos = player.Position - activeLightDir * 50.0f`——光源始终位于玩家后方 50 米，确保 4096×4096 阴影贴图覆盖角色周围区域。

#### 泊松圆盘采样软阴影 (Poisson Disk Soft Shadows)

- **16 采样点泊松圆盘**：在 `model.frag` 中离线生成 16 个泊松分布采样点（均匀覆盖单位圆），经 `rand(gl_FragCoord.xy)` 随机旋转后，在阴影贴图上采样并计算 PCF（Percentage-Closer Filtering）。
- **动态深度偏移**：`bias = max(0.005 * (1 - N·L), 0.0005)`，根据表面与光照方向自动调整，消除阴影痤疮（Shadow Acne）。
- **柔和度可调**：`filterRadius = 2.5` 控制半影区范围。
- **4096×4096 浮点深度贴图**：使用 `GL_CLAMP_TO_BORDER`（白色边框）防止光照截锥体外区域产生伪影。

#### 动态点光源（火把、灯笼等）

- **自发光网格光源提取**：`Model::ProcessMesh()` 在加载时识别 `materialType==3`（匹配 `lava`/`torch`/`glowstone`/`lantern` 等关键词），将自发光顶点聚类（0.5m 半径平均）为 `Model::pointLights`。
- **手持火把动态光源**：选中快捷栏槽位 1（火把）时，在玩家位置注入实时跟随的点光源——第一人称跟随摄像机，第三人称偏移至右手位置。
- **物理衰减模型**：`attenuation = 1 / (1 + 0.20*d + 0.10*d²)`，有效半径 **12.0m** → 超出范围提前截断（`continue`），避免无效计算。
- **512 个光源数组**：完整上传至 `uPointLights[512]` uniform 数组，在 `model.frag` 中逐像素累加。

---

### 2. 全屏后处理 (Full-Screen Post-Processing)

#### HDR 渲染管线与 ACES 电影级色调映射

完整的 **高动态范围 (High Dynamic Range) 渲染管线**——场景渲染至 `GL_RGBA16F` 浮点帧缓冲（支持 >1.0 亮度值），经由 5 阶段色调映射输出到 LDR 显示器：

```
场景 HDR (RGBA16F) → 混合 Bloom → Reinhard 曝光 → ACES 电影曲线 → 饱和度增强 → Gamma 校正
```

- **Reinhard 曝光**：`color / (color + 1.0)`，曝光参数 `uExposure = 0.5`。
- **ACES Filmic 曲线**：5 参数 `(2.51, 0.05, 2.43, 0.59, 0.16)` 实现胶片级高光滚降（Highlight Roll-off），保留亮部细节。
- **饱和度增强**：`1.25×` 色彩增强，使最终画面更具视觉冲击力。
- **Gamma 2.4**：匹配 sRGB 显示器空间。

#### 双通道高斯泛光 (Dual-Pass Gaussian Bloom)

1. **亮部提取** (Bright-Pass)：阈值 `> 1.0` 的像素保留，其余归零 → 写入半分辨率（960×540）。
2. **6 次高斯模糊** (3 水平 + 3 垂直)：5-tap 高斯核在 `pingpongFBO[0]` 与 `[1]` 之间往复渲染，实现宽范围柔光晕。
3. **HDR 合成**：场景与模糊后的 Bloom 纹理以 `uBloomIntensity = 0.35` 混合，再送入色调映射。

#### 屏幕空间环境光遮蔽 (SSAO)

**三阶段 G-Buffer 管线**：

| 阶段 | 帧缓冲 | 着色器 | 输出 |
|------|--------|--------|------|
| **1.5** G-Buffer 构建 | `gBuffer` (RGBA16F×2 + DEPTH24) | `g_buffer.vert/frag` + `g_buffer_skinned.vert/frag` | 视图空间位置 + 法线 |
| **1.8** SSAO 生成 | `ssaoFBO` (GL_RED) | `ssao.frag` + `fullscreen.vert` | 单通道遮蔽值 |
| **1.9** SSAO 模糊 | `ssaoBlurFBO` (GL_RED) | `ssao_blur.frag` + `fullscreen.vert` | 4×4 均值平滑后遮蔽值 |

- **64 采样核心**：在单位半球内生成，偏置向原点中心（`lerp(0.1, 1.0, scale²)`），着色器使用前 16 个。
- **4×4 旋转噪声贴图**：`GL_REPEAT` 平铺 + TBN 随机旋转，消除高频规则采样条纹。
- **G-Buffer 骨骼动画支持**：`g_buffer_skinned.vert` 对 4 骨骼加权顶点输出视图空间坐标与法线，确保玩家和敌怪的 SSAO 正确。
- **仅影响环境光**：`ambient *= texture(uSsaoMap, gl_FragCoord.xy / uScreenSize).r`——漫反射、镜面高光和点光源不受遮蔽影响，确保物理正确性。

#### 上帝光 / 丁达尔效应 (God Rays — Crepuscular Rays)

屏幕空间后处理特效——在 Bloom 亮部提取之后、高斯模糊之前插入：

1. 将世界空间光源方向 `-activeLightDir` 变换到 NDC 屏幕坐标。
2. 60 次径向采样：从太阳屏幕位置向边缘沿径向步进，指数衰减 (`uDecay = 0.92`)。
3. 输出存入 `pingpongColorTex[1]`，后续高斯模糊从此贴图开始（`blurSourceTex = 1`），使光线同样获得柔化。
4. 太阳位于屏幕外时自动禁用（`lightPosNDC` 超出范围 → `godrayWeight = 0`）。

---

### 3. 水体与环境特效 (Water & Environment)

#### 真实动态水面

**纯 GLSL 数学实现**——所有水面效果均在 `model.frag` 中通过数学推导完成：

- **顶点波浪动画** (`model.vert`)：基于 `sin(uTime + pos.x * 2.0) * 0.05` 的顶点位移，水面随时间起伏。
- **有限差分法线扰动**：从 `uPackedNoiseMap`（纹理单元 12，打包云/水噪点的 RGB 贴图，含各向异性过滤 + Mipmap）采样 5 个相邻像素 → 通过有限差分计算扰动后的法线向量（Central Differences: `dh/dx` 和 `dh/dy`）。
- **菲涅尔反射**：`F = mix(0.04, 1.0, pow(1 - V·N, 5))`——掠射角反射增强、正视透明，使用真实天空渐变颜色 `uHorizonColor`/`uZenithColor` 作为反射源。
- **太阳镜面高光**：`pow(glint, 256) * 10.0`——镜面反射方向与太阳方向对齐时产生极强窄带高光，模拟水面太阳耀斑。
- **视角驱动透明度**：alpha 在 0.4（正视）到 0.9（掠射）之间平滑过渡。
- **双通道渲染**：先渲染所有不透明物体，然后 `glDepthMask(GL_FALSE)` + `GL_BLEND` 渲染水面，保证正确的半透明混合顺序。

#### 植被随风飘动 (Wind-Animation Vegetation)

- **材质自动分类**：`Model::ProcessMesh()` 通过关键字匹配（`grass`/'wheat'/'crop'/'flower'/'plant'/'leaves'` 等）自动识别 3 种植被类型（`materialType == 1` 植物茎干 / `== 2` 树叶）。
- **顶点颜色烘焙**：R 通道存储顶点风权重 → 根部权重为 0（不动），顶部权重为 1（最大摆动）。
- **两种风摆模式**：
  - **植物茎干** (`type 1`)：根部锚定 (`anchor = pos * (1-windWeight)`)，顶部正弦摆动。
  - **树叶** (`type 2`)：整体刚性正弦平移。
- **碰撞世界自动排除**：`materialType == 1` 和 `== 4`（水面）的三角形在 `CollisionWorld` 构建时跳过，植物不产生碰撞。

#### VBO-less GPU 粒子雨与深度剔除防漏雨

- **10,000 个雨滴**：无 VBO 绑定，纯 `gl_VertexID` 推导位置——`glDrawArrays(GL_LINES, 0, 20000)`。
- **40m³ 摄像机跟随盒**：雨滴始终围绕玩家生成，避免全局粒子管理的性能问题。
- **双层遮挡剔除**：
  - **场景深度测试** (`GL_DEPTH_TEST`)：HDR FBO 中的场景深度自动剔除被建筑物/地形遮挡的雨滴。
  - **雨滴遮挡深度贴图** (`rainDepthMap`, 1024×1024)：从光源视角单独渲染静态场景深度，用于剔除屋顶/天花板下方的雨滴——防止"室内漏雨"。
- **平滑强度过渡**：`rainIntensity` 通过 `lerp(targetRain, dt * 0.2)` 在 2 秒内平滑达到目标值（R 键切换）。

#### 网格化像素星空 (Voxelized Hash Stars)

- **基于网格的哈希函数**：将天球划分为均匀网格单元，在每个网格内对坐标取哈希 → 确定性随机决定该网格是否生成星星，消除逐帧闪烁（Anti-Flicker Grid Hash）。
- **亮度调制**：星星亮度随 `nightFade`（夜色系数）平滑变化——白天隐没，夜晚显现。
- **逆天球矩阵**：`starMatrix = inverse(celestialMat)` 使星星随天球旋转，保持与太阳/月亮的相对位置一致。

---

## 角色与动画系统 (Character & Animation System)

### 双轨骨骼混合动画 (Dual-Track Bone Blending)

`Animator` 类支持同时播放两轨动画——**Base 轨道（下半身：行走/奔跑/潜行）** 与 **Overlay 轨道（上半身：攻击/射箭）**：

```
最终骨骼矩阵 = Base 轨道动画 × Overlay 轨道覆盖（仅当 Overlay 中存在该骨骼时）
```

- **骨骼存在性判定覆盖**：Overlay 动画中的骨骼如果在 Base 中存在，则完全覆盖；否则保留 Base 姿态。这允许**上半身攻击动作与下半身行走动作独立混合**——例如骷髅可以一边后退射击一边保持行走循环。
- **头部旋转/隐藏**：Animator 支持独立的头部骨骼旋转 (`SetHeadRotation`) 和隐藏 (`SetHideHead`)——第一人称模式下隐藏玩家头部避免遮挡视野。
- **节点全局变换缓存**：`m_NodeGlobalTransforms` 缓存每帧计算的 Assimp 节点树全局矩阵 → `GetNodeGlobalTransform(name)` 按名称查询，供手持物品骨骼挂载使用。
- **无限循环**：`fmod(m_CurrentTime, duration)` 实现动画的无缝循环。

### 绑定姿态矩阵注入 (Bind Pose Translation Injection)

针对 **Blockbench 导出的 .glb 文件**中动画通道缺少 Translation 关键帧的问题（导出 Bug 导致骨骼平移分量丢失，模型在动画播放时产生位移偏差），在 `Animation::LoadAll` 中实现了自动修复：

- 检测每个骨骼的动画通道是否包含 Position/Translation 关键帧。
- 若缺失，则**注入绑定姿态的平移值**（从 Assimp 节点层次树中提取原始局部变换）作为唯一平移关键帧。
- 此修复避免了手动在 Blockbench 中补充关键帧的繁琐流程，确保模型动画在引擎中的表现与 Blockbench 预览一致。

### 程序化头部追踪 (Procedural Head Tracking)

玩家头部朝向独立于身体旋转，通过四元数插值实现平滑追踪：

- **移动时**：身体朝向摄像机前方方向（带 45° 斜向偏移检测 WASD 组合），头部精确对准摄像机准星。
- **静止时**：头部跟踪摄像机，身体在头部偏离超过 45° 时自动跟随旋转（延迟跟随），产生自然的"转头→转身"动画。
- **Quaternion/Euler 混合**：`glm::quat` 计算目标旋转，`glm::eulerAngles` 输出 Yaw 角度供骨骼矩阵构建。

### 第一/第三人称手持物品骨骼挂载

- **第三人称**：获取玩家模型的右手骨骼 (`"Right Arm_bone"`) 全局变换矩阵 → 物品模型矩阵 = **玩家世界矩阵 × 手部骨骼矩阵 × 握持偏移**，实现物品随手臂动画自然挥动。
- **第一人称**：清空深度缓冲 (`glClear(GL_DEPTH_BUFFER_BIT)`)、`uView = Identity`，物品直接绘制在最前层。支持运行时通过键盘微调握持偏移（Translate/Rotate/Scale 六个自由度）。
- **进食视觉反馈**：手持面包 (slot 3) 时，物品向摄像机拉近并施加高频抖动 + 倾斜旋转 (`sin(glfwGetTime() * 40.0)` × 0.05)，模拟进食动作。

---

## 物理与战斗系统 (Physics & Combat System)

### 手写运动学角色控制器 (Kinematic Character Controller)

`Player::UpdatePhysics(dt, mapCollision)` 实现了完整的 6 阶段物理更新循环：

1. **重力与加速度**：`Velocity.y += -9.8 * dt; Position += Velocity * dt; Velocity.xz *= 0.1`——施加重力后位置积分，然后水平速度阻尼至 10%（每次积分后衰减，而非连续阻尼，产生"无惯性"的精准手感）。
2. **防隧道效应** (Tunneling Prevention)：`fallCompensation = max(0, -Velocity.y * dt)` 添加到地面搜索范围——当垂直速度极高时自动扩展地面检测半径，防止高速下落穿透薄地面。
3. **水平墙壁排斥**：基于 AABB 包围盒 (`radius=0.4, height=1.8`) 与碰撞三角形的轴对齐矩形在 XZ 平面的最近点计算推开向量——`tri->maxY <= Position.y + 0.2f` 排除可跨过的低矮台阶。
4. **跌落致死**：`Position.y < -50` → 重生至安全位置。
5. **调试输出**（每 60 帧 print 一次物理状态）。
6. **视觉 Y 偏移**：潜行时降低 `0.08`，使摄像机位置匹配视觉高度。

### 3D 地形碰撞 (CollisionWorld)

- **三角形提取**：从 `mapModel` 的所有 Mesh（跳过 `materialType==1` 植物和 `==4` 水面）中提取三角形，经 `mapModelMatrix`（Y = -35 偏移）变换至世界空间。
- **2D 空间哈希网格**：cell size 5.0，key = `int64_t(int32 cx, int32 cz)` 打包——射线检测和墙壁排斥查询时仅检测相邻网格，O(1) 空间复杂度查询。
- **Möller–Trumbore 射线-三角形相交**：`glm::intersectRayTriangle` 实现精确三角形级别命中检测。
- **F6 Debug Wireframe**：从碰撞三角形生成绿色线段顶点，通过 `debugShader` + `GL_LINES` 实时叠加渲染。

### 精准准星射线判定与横扫之刃

#### 射线-敌怪 AABB 相交

```
rayOrigin = player.Position + camera.EyeOffset        // 玩家眼睛位置
rayDir    = camera.GetFullFrontVector()                // 含 Pitch 的真实 3D 方向
maxRange  = 3.5m                                        // 近战攻击范围
```

**Slab 算法**：射线与 `enemy.Position + (−0.4, 0, −0.4)` ↔ `(+0.4, 1.8, +0.4)` 的 AABB 包围盒求交，选择距离最近且 < 3.5m 的敌怪作为主目标。

#### 横扫之刃 (Sweeping Edge AOE)

仅当手持剑 (slot 0) 时生效：
- **3m 水平距离** + **90° 前向锥形判定** (`dot > 0.707`)——目标周围 90° 扇形范围内的其他敌怪受到额外 AOE 伤害。
- 主目标 7 点伤害 / AOE 3 点伤害 / 空手 1 点伤害。
- **击退实现**：`Velocity = frontDir * 5.0 + (0, 4.5, 0)`——水平向后 5.0 + 垂直上升 4.5（复合击飞效果）。

### 僵尸/骷髅双类型 AI

| 类型 | 生命 | 速度 | 行为 |
|------|------|------|------|
| **僵尸 (ZOMBIE)** | 20 | 2.5 | >15m 待机 / 1.5–15m 追击 / ≤1.5m 撕咬（HP-2 + 击退 + 0.5s 免疫） |
| **骷髅 (SKELETON)** | 20 | 1.8 | >15m 待机 / 8–15m 站立射箭（4s CD）/ <8m 后退风筝（80% 速度）+ 射箭 |

- **双轨动画**：骷髅的行走 (Base) + 攻击 (Overlay, 仅手臂骨骼) 独立混合。
- **受击闪红**：`uIsHit` uniform → `model.frag` 中 `mix(finalColor, red, 0.3)` 在雾化前混合。
- **死亡粒子**：死亡时在敌怪位置生成 10 个烟雾粒子（随机位移/速度/纹理，0.4s 寿命），广告牌渲染。

### 玩家血量与进食系统

- **20 HP / 10 颗心**：3 种心脏贴图（满/半/空），按剩余血量条件绘制。
- **进食回血**：选中面包 (slot 3) + HP < 20 + 按住右键 1.5 秒 → +4 HP（最多 20）。
- **死亡重生**：HP ≤ 0 → 重生至 `(-20, -7, -13)`，满血，速度清零。

---

## UI 与交互/脚本系统 (UI & Interaction/Scripting)

### 基于 stb_truetype 的中文 TrueType 字体渲染器

`FontRenderer` 类实现了从 .ttf 文件到 OpenGL 位图图集的完整烘焙管线：

- **8192×8192 单通道图集** (`GL_RED`)：在如此巨大的图集中一次性烘焙 21,151 个字符，涵盖：
  - **ASCII** (32–126, 95 字符)：数字、英文字母、基本标点。
  - **CJK 标点** (0x3000–0x303F, 64 字符)：中文全角标点、括号、书名号等。
  - **CJK 统一汉字** (0x4E00–0x9FFF, 20,992 字符)：几乎所有常见汉字。
- **UTF-8 → Unicode 码点解码**：`DecodeUTF8()` 按字节前缀解析 1–4 字节 UTF-8 序列，支持中英文混排。
- **逐字符四边形渲染**：每个字符从图集中提取 UV 区域 (`uUvScale`/`uUvOffset`)，使用 `uiShader` 的字体模式 (`uIsFont=true`) 逐字符绘制，xadvance 自动递进。
- **C/C++ 编译隔离**：`stb_truetype` 通过 C 包装文件 `font_baker.c` 编译，`set_source_files_properties(LANGUAGE C)` 强制 MSVC 以 C 模式编译，避免 C++ 兼容性问题。

### 像素级对齐的 HUD 系统

- **9 格快捷栏**：原版 Minecraft 风格 182×22 像素底框，4× 放大至 728×88，居中底部对齐。动态选中框 (24×24→96×96) 跟随 `activeSlot` 水平移动。
- **10 颗红心血条**：原版 9×9 像素，4× 放大至 36×36。3 层叠加绘制——空心底座 (container) → 满心覆盖 (full) → 半心覆盖 (half)，按剩余血量逐心判定。
- **4 槽位物品图标**：剑 (slot 0) / 火把 (slot 1) / 绿宝石 (slot 2) / 面包 (slot 3)，原版 16×16 像素 4× 放大。垂直翻转补偿 (stb_image 的 Y-flip 默认行为导致上下颠倒)。
- **第一人称准星**：原版 15×15 像素，4× 放大，屏幕居中，仅第一人称模式下显示。
- **所有 HUD 元素使用同一套 UI VAO**（0→1 正方形），通过 `glm::translate/scale` 变换定位，在正交投影 `glm::ortho(0, 1920, 0, 1080)` 下绘制。

### F3 调试信息覆盖层 (Debug Overlay)

按下 **F3** 切换屏幕左上角的实时调试信息：
- 玩家世界坐标 X / Y / Z
- 实时帧率 FPS（每 0.5 秒刷新一次）

### 脚本化剧情导演系统 (Scripted Story Director)

`StoryState` 枚举驱动 6 阶段线性叙事，全程在 `main.cpp` 主循环中以**纯代码驱动的时间序列**实现——无需外部脚本语言或数据文件：

| 阶段 | 触发条件 | 事件 |
|------|---------|------|
| **WANDERING** | 游戏开始 | 30s 后提示回家（坐标: -17, -7, -11）。玩家进入 4m 范围 → CUTSCENE |
| **CUTSCENE** | 接近家门 | 屏幕淡黑 (0–2s) → 5 声钟响（3s 间隔）→ 第 2 声响起时强制深夜 (`timeOfDay=270°`) + 暴雨 → NPC 对话（t=11s, t=16s）→ 屏幕恢复 (6–8s) |
| **GOTO_GATE** | 过场结束 | 玩家前往东门 (X:47–51, Z:-9–0)。到达后清除旧敌怪，生成 2 僵尸 + 2 骷髅 |
| **DEFENDING** | 到达东门 | 等待所有敌怪被消灭 |
| **ENDING** | 敌怪全灭 | 3s 后村民对话 → 6s 后"自由探索"提示 |
| **COMPLETED** | 结局完成 | 无更多剧情事件，完全自由探索 |

### 聊天/消息系统

- **ChatLine 结构体**：文本 + 5 秒生命周期 → 自动淡出（alpha = lifeTime / 5.0）。
- **可视化**：最多同时显示 6 条消息，半透明黑色背景板 + 白色中文文本。
- **永久历史记录**：`chatHistory` 保存最多 100 条消息。
- **T 键历史查看器**：按下 T 切换——显示最近 12 条历史消息，半透明黑色背景面板覆盖聊天区域，鼠标解锁、移动锁定。

### 主菜单

- **游戏启动状态**：`MENU` 状态下显示鼠标，3D 场景（天空旋转）在背景可见。
- **"开始游戏"按钮**：Minecraft 风格灰色面板 + 黑色描边（800×80 px），居中显示金色中文 "开始游戏"。
- **点击判定**：`mouse_button_callback` 捕获鼠标坐标（翻转 Y 轴对齐 1080p 正交坐标系），命中按钮矩形区域 → 切换 `PLAYING` 状态，隐藏鼠标，播放第一句剧情提示。

---

## 性能优化与调试 (Performance & Debugging)

### Uniform Location Caching 消除驱动阻塞

`Shader` 类在构造时缓存所有 uniform 位置的 `GLint` 到 `std::unordered_map<std::string, GLint>`。后续调用 `SetMat4`/`SetVec3`/`SetFloat` 等无需再调用 `glGetUniformLocation`——该调用若在每帧/每绘制调用中执行，会触发 CPU-GPU 同步阻塞（驱动管道刷新），是实时渲染中常见的性能杀手。缓存后每帧仅执行轻量级的 `glUniform*` 调用。

### C++ 端材质名称判定优化 Draw Call

材质类型（0–5）的分类在 `Model::ProcessMesh()` 加载阶段通过 Assimp 材质名称的**子串匹配**一劳永逸完成，结果存储在 `Mesh::materialType` 中。渲染循环中无需在着色器中进行分支判断——C++ 端可直接按类型跳过/选择网格（如 `if (mesh.materialType == 4) continue;` 跳过水面），大幅减少 CPU 端渲染循环开销与 GPU 着色器分支发散。

### 动态 FOV 增强疾跑速度感

```
targetFOV = sprinting ? 55° : 45°
currentFOV = glm::mix(currentFOV, targetFOV, deltaTime * 8.0f)
thirdPersonCamera.SetFOV(currentFOV)  // 所有 12+ Shader 调用位置自动生效
```

疾跑时视野平滑拉伸至 55°，产生速度感增强的视觉反馈。使用 `glm::mix` 进行 8.0 速率的指数平滑，避免突变带来的眩晕感。FOV 变化通过 `ThirdPersonCamera::SetFOV()` 自动同步到所有使用投影矩阵的着色器。

### F1 Shader 热重载

天空着色器 (`sky.frag`) 支持**运行中无重启重新编译**。按下 F1 → `Shader::Reload()` → 从 `SHADER_SRC_DIR` 编译时注入的绝对路径重新读取源文件 → 编译 → 原子替换 GL Program。编译失败时旧程序保留不变，错误信息输出到 stderr。这种调试流程将着色器开发从"编辑→重启→重新定位场景→验证"的分钟级循环缩短为**亚秒级所见即所得**——是项目着色器开发效率的核心保障。

---

## 挑战与心得 (Challenges & Learnings)

### 主要技术挑战

**1. Blockbench 动画导出兼容性**

Assimp 导入 .glb 文件时发现部分骨骼动画通道缺少 Translation 关键帧——这是因为 Blockbench 在导出没有位移变化的骨骼动画时省略了平移通道。这导致动画播放时角色模型产生位置偏移。最终通过在 `Animation::LoadAll` 中检测缺失通道并自动注入绑定姿态平移值解决。这一过程让我深刻理解了 glTF 规范中 TRS 变换的独立性以及不同 DCC 工具对 Bone Animation 的不同导出策略。

**2. 多通道渲染管线的设计与调试**

项目包含 8 个帧缓冲对象 (FBO)、12+ 个不同的着色器程序、在单帧内完成 7 个渲染通道（Shadow → Rain Depth → G-Buffer → SSAO → SSAO Blur → Main Scene → Bloom → God Rays → UI），管线设计极其复杂。最大的挑战在于：
- **Uniform 状态泄漏**：`uIsHit` 和 `uMaterialType` 是 sticky uniform，在一帧内跨越数十个绘制调用。若在敌怪循环后忘记重置 `uIsHit = false`，后续所有地图网格都会变红——这种 Bug 非常隐蔽，因为影响范围可能跨越数百个不相关的 Draw Call。
- **FBO 完整性问题**：G-Buffer 的 `glDrawBuffers` 必须精确指定附件数量和格式，任何不匹配都会导致静默失败（黑屏）。最终形成了一套"每个 FBO 绑定后立即检查 `glCheckFramebufferStatus`"的防御性编程规范。

**3. 数学与物理 Bug 调试**

- **地表穿透 (Tunneling)**：高速下落时，角色在一个物理帧内的位移可能超过地面三角形厚度，直接穿透。解决方案是 `fallCompensation = max(0, -Velocity.y * dt)`——根据速度动态扩展地面搜索框，在高速下落时提前"预见"地面。
- **射线检测精度**：Möller–Trumbore 算法在退化三角形（面积接近零）上产生 NaN 结果，导致射线穿过实体。通过在 `CollisionWorld` 构建时过滤面积过小的三角形解决。
- **头顶遮挡问题**：第三人称摄像机在紧贴墙壁时会穿透墙体看到角色内部。解决方案是从玩家头部向摄像机反向发射 `CollisionWorld::Raycast`，检测遮挡时动态拉近摄像机（`Distance = max(0.5, hitDist - 0.2)`）。

**4. 字体渲染性能**

最初尝试在每帧渲染文字时逐字符调用 `glDrawArrays`，在显示大量中文文本时造成显著的 Draw Call 开销。后来的优化方案是将字体烘焙到 8192×8192 的超大单通道图集中，所有字符共享同一纹理，整个文本字符串仅需一次绑定即可逐字符绘制——将 Draw Call 开销从 O(N) 字节数降低为 O(1)。

### 核心心得

**从零实现的价值**：通过手写每一行着色器代码、每一个矩阵变换、每一个物理计算步骤，我获得了对现代实时渲染管线的深入理解——这些知识在使用 Unity/Unreal 等商业引擎时往往被高度封装的 API 所遮蔽。具体而言：

- 理解了 **延迟着色与正向渲染的本质区别**——G-Buffer 的设计、多渲染目标 (MRT) 的工作方式、以及 SSAO 作为屏幕空间近似的局限性。
- 掌握了 **HDR → 色调映射 → Gamma 校正** 的完整颜色管线，理解了 sRGB/Linear 色彩空间转换为何必须在管线的特定环节进行。
- 认识到 **物理模拟的数值稳定性** 问题——浮点精度、积分方法选择（Euler vs Verlet）、以及离散时间步长如何影响碰撞检测可靠性。
- 领会了 **着色器热重载对图形开发的革命性意义**——亚秒级的迭代反馈使得复杂的数学效果（如水面法线扰动和体积云）的调试变得可行。

**工程实践领悟**：在一个人独立完成从引擎、着色器、物理、UI、音效到游戏系统的完整项目中，最重要的能力不是某个特定领域的深度知识，而是**系统化的问题分解与逐层调试能力**——当屏幕显示不正确时，需要快速判断问题是出在模型加载、矩阵变换、着色器逻辑、FBO 配置还是深度测试设置，这需要建立一套完整的"渲染管线心智模型"。

---

*本文档基于项目源码与开发过程实录撰写，准确反映截至 2026 年 6 月的项目技术状态。*
