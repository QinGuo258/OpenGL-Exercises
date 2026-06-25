================================================================================
                              OpenGL 项目 README
================================================================================

项目简介
--------
  一个基于 OpenGL 4.6 Core Profile 的 3D 游戏项目，使用 C++20 编写。包含：
  - 第一人称 / 第三人称视角切换（3 种模式）
  - 动态天空盒（昼夜循环、体积云、星空、日月、晨昏渐变）
  - 骨骼动画系统（玩家 + 敌人，双轨道混合，头部朝向控制）
  - CPU 端碰撞检测（三角形精确射线（Möller–Trumbore）+ AABB，2D 空间哈希加速）
  - 粒子系统（死亡烟雾 8 纹理变体、挥剑扫击 8 帧精灵表）
  - SSAO 屏幕空间环境光遮蔽（16 采样半球 + 4×4 盒式模糊）
  - HDR + Bloom 泛光（亮度阈值提取 + 6 次高斯模糊 + 合成）
  - ACES 色调映射（Reinhard → ACES → 饱和度增强 → Gamma 2.4）
  - 软阴影（4096×4096 深度图 → Poisson Disk 16 采样 + 随机旋转）
  - 体积光 / God Rays（屏幕空间径向模糊，60 步采样）
  - 过程化降雨系统（10000 雨滴 VBO-less 渲染 + 雨水遮挡剔除深度图）
  - TrueType 字体渲染（中文 + 英文，8192×8192 GL_RED 图集）
  - 3D 音效（miniaudio，MP3 解码 + 空间定位）
  - 脚本化故事演出系统（6 阶段线性叙事）
  - 完整战斗系统（近战射线检测 + 横扫 AOE + 远程箭矢）
  - 昼夜动态光源（太阳/月光自动切换，随高度平滑过渡）

================================================================================
1. 编译环境
================================================================================

  [操作系统]
    Windows 10 / 11（仅支持 Windows，CMakePresets.json 限定 Windows）

  [编译器]
    Microsoft Visual C++ (MSVC / cl.exe)
    - 需要 Visual Studio 2022（或更高版本，含 MSVC v143 工具链）
    - 需要安装 "使用 C++ 的桌面开发" 工作负载
    - 编译选项: /utf-8（源码含中文注释）、/std:c++20

  [构建工具]
    CMake 3.16 或更高版本（CMakePresets.json 使用 schema version 3）
    Ninja（CMakePresets.json 默认生成器）
    - Ninja 可通过 Visual Studio Installer 安装（勾选 "CMake 的 C++ Ninja 生成器"），
      或从 https://ninja-build.org 下载并放入 PATH

  [图形 API]
    OpenGL 4.6 Core Profile
    - 需要支持 OpenGL 4.6 的显卡及驱动（NVIDIA GTX 900 系列 / AMD GCN 第4代 及以上）
    - 8 个 FBO 用于延迟渲染管线

================================================================================
2. 依赖库
================================================================================

  项目使用随源码分发的第三方库，放置于 external/ 目录。

  [2.1] GLFW 3.4.0 — 窗口创建与输入处理
    路径：   external/glfw/
    用途：   创建 OpenGL 上下文、窗口管理、键盘鼠标输入
    构建：   从源码编译（CMake add_subdirectory），禁用示例/测试/文档
    官网：   https://www.glfw.org/

  [2.2] GLAD — OpenGL 加载器
    路径：   external/glad/
    用途：   运行时加载 OpenGL 4.6 Core Profile 函数指针
    构建：   静态库（glad.c）
    生成器： https://glad.dav1d.de/
            （GL 4.6 Core，Language: C/C++，Profile: Core）

  [2.3] GLM — OpenGL Mathematics（纯头文件）
    路径：   external/glm/
    用途：   向量/矩阵/四元数数学运算（含 gtx/intersect.hpp 射线-三角形相交）
    构建：   仅头文件库（INTERFACE 目标），无需编译
    官网：   https://github.com/g-truc/glm

  [2.4] Assimp — Open Asset Import Library
    路径：   external/assimp/
    用途：   加载 3D 模型文件（FBX、OBJ、glTF 等），提取骨骼信息、材质名称、
            自发光点光源（聚类顶点）
    构建：   从源码编译（CMake add_subdirectory）
    配置：   启用所有导入器（ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=ON）
            关闭测试、示例、文档、工具、安装目标
    注意：   使用 MSVC 静态 CRT（/MTd / /MT），与项目动态 CRT（/MDd / /MD）
            可能产生链接冲突，见"已知问题"章节
    官网：   https://github.com/assimp/assimp

  [2.5] stb_image — 图像加载库（单头文件）
    路径：   external/stb/
    用途：   加载 PNG / JPG / BMP / TGA 等纹理文件（RGBA 输出，Y 翻转）
    构建：   仅头文件库（INTERFACE 目标），无需编译
    官网：   https://github.com/nothings/stb

  [2.6] stb_truetype — TrueType 字体光栅化（单头文件）
    路径：   src/stb_truetype.h
    用途：   将 .ttf 字体烘焙为 GPU 纹理图集（8192×8192，GL_RED 单通道）
    构建：   通过 src/font_baker.c 编译（C 模式，非 C++），
            烘焙 3 个 Unicode 区间共 21151 个字符：
            - ASCII (32–126, 95 字符)
            - CJK 标点 (0x3000–0x303F, 64 字符)
            - CJK 统一汉字 (0x4E00–0x9FFF, 20992 字符)

  [2.7] miniaudio — 音频引擎（单头文件）
    路径：   src/miniaudio.h（声明头文件）
            src/Audio.cpp（实现单元，定义 MINIAUDIO_IMPLEMENTATION）
    用途：   3D 音效播放（MP3 解码 + 空间定位），通过 ma_engine 管理
    构建：   与项目一同编译，Audio.cpp 独立编译单元
    官网：   https://miniaud.io/

  [库文件汇总]
    external/
    ├── glad/        GLAD     OpenGL 加载器（静态库）
    ├── glfw/        GLFW     窗口管理（静态库，从源码编译）
    ├── glm/         GLM      数学库（纯头文件）
    ├── assimp/      Assimp   模型导入（静态库，从源码编译）
    └── stb/         stb      图像加载（纯头文件）

================================================================================
3. 获取源码与依赖
================================================================================

  [方法一：如果项目含 .gitmodules]
    git clone --recurse-submodules <仓库地址>
    cd OpenGL

  [方法二：如果 external/ 目录已包含完整源码（当前状态）]
    无需额外操作，所有第三方库已随项目分发。

================================================================================
4. 编译步骤
================================================================================

  [4.1] 打开命令行
    打开 "x64 Native Tools Command Prompt for VS 2022"
    （或 "Developer Command Prompt for VS 2022" 后执行 vcvars64.bat）
    确保 cmake、ninja、cl.exe 均可在命令行调用。

    验证方法：
      cmake --version
      ninja --version
      cl   （应显示 MSVC 版本信息）

  [4.2] 配置 CMake（以 x64 Debug 为例）
    cd "D:\Microsoft Visual Studio Projects\Cpp\OpenGL"
    cmake --preset x64-debug

    可用预设（preset）：
      x64-debug      — 64 位调试版（/MDd 动态调试 CRT）
      x64-release    — 64 位发布版（/MD 动态发布 CRT）
      x86-debug      — 32 位调试版
      x86-release    — 32 位发布版

    推荐使用 x64-debug 进行开发调试。

  [4.3] 编译
    cmake --build out/build/x64-debug

    编译产物：
      out/build/x64-debug/OpenGL/OpenGL.exe
      （同目录下包含自动复制的 shaders/、models/、textures/、audio/、fonts/）

  [4.4] 运行
    方式一（命令行）：
      .\out\build\x64-debug\OpenGL\OpenGL.exe

    方式二（文件资源管理器）：
      直接双击 out/build/x64-debug/OpenGL/OpenGL.exe

    窗口参数：1920×1080，标题 "OpenGL"，默认光标隐藏（主菜单除外）。

================================================================================
5. 已知问题与解决方法
================================================================================

  [5.1] Assimp CRT 不匹配（链接错误）
    症状：   出现 operator new / operator delete 等未解析符号错误
    原因：   Assimp 默认使用静态 CRT（/MT），项目使用动态 CRT（/MD）
    解决：
      rm -rf out/build/x64-debug/CMakeCache.txt out/build/x64-debug/CMakeFiles
      cmake --preset x64-debug
      cmake --build out/build/x64-debug

  [5.2] MSVC 编译中文注释乱码
    已在 OpenGL/CMakeLists.txt 中添加 /utf-8 编译标志，确保源码中的
    中文注释以 UTF-8 编码正确编译。如仍有乱码，检查编辑器保存编码是否为 UTF-8。

  [5.3] 运行时缺少资源文件
    CMake POST_BUILD 脚本会自动将以下目录复制到 exe 所在目录：
      shaders/   — GLSL 着色器（*.vert / *.frag，共 14+ 个文件）
      models/    — 3D 模型文件（FBX 等格式）
      textures/  — 纹理图片 + UI 素材 + 粒子精灵表
      audio/     — 音效文件（MP3）
      fonts/     — 字体文件（TTF）

    如果运行时报错找不到文件，检查这些目录是否存在于源码根目录，
    或手动将其复制到 exe 所在目录。

  [5.4] CMake 配置失败（找不到 Ninja）
    症状：   CMake 报错 "Could not find Ninja"
    解决：   安装 Ninja 并确保在 PATH 中，或修改 CMakePresets.json
            将 "generator" 改为 "Visual Studio 17 2022"

  [5.5] OpenGL 4.6 不支持
    症状：   启动时报错或渲染异常
    解决：   检查显卡驱动是否为最新版本；部分集成显卡不支持 OpenGL 4.6，
            需要独立显卡（NVIDIA / AMD）

================================================================================
6. 项目目录结构
================================================================================

  OpenGL/
  ├── CMakeLists.txt             顶层 CMake（第三方库配置）
  ├── CMakePresets.json           构建预设配置（4 个预设）
  ├── README.txt                  本文件
  ├── shaders/                    GLSL 着色器（*.vert / *.frag）
  ├── models/                     3D 模型文件
  ├── textures/                   纹理图片 + UI 素材 + 粒子纹理
  ├── audio/                      音效文件（MP3）
  ├── fonts/                      字体文件（TTF）
  ├── external/                   第三方依赖库（见第 2 节）
  ├── src/                        游戏源代码（见第 7 节）
  ├── OpenGL/                     CMake 可执行目标
  │   ├── CMakeLists.txt          源文件列表 + POST_BUILD 复制命令
  │   ├── OpenGL.h                VS 向导样板（不参与编译）
  │   └── OpenGL.cpp              VS 向导样板（不参与编译）
  └── out/                        构建输出目录（自动生成，不纳入版本控制）

================================================================================
7. 源代码文件说明
================================================================================

  src/
  ├── main.cpp                    入口点与渲染主循环
  │                               - GLFW/GLAD 初始化
  │                               - 8 FBO 创建、Shader 实例化
  │                               - Player / Enemy / Camera / CollisionWorld 管理
  │                               - 完整多通道渲染循环（阴影→G-Buffer→SSAO→主场景→后处理）
  │                               - 故事演出状态机（6 阶段）
  │                               - 聊天/消息系统、玩家 HP/心形 UI、进食系统
  ├── Shader.h / Shader.cpp       GLSL 着色器封装
  │                               - 编译 + 链接 + 统一变量设置
  │                               - 热重载（Reload），全部着色器 F1 一键重载
  ├── Model.h / Model.cpp         Assimp 模型加载
  │                               - 递归节点树 → Mesh 列表 + 骨骼信息
  │                               - 材质分类（名称匹配 → materialType 0–5）
  │                               - 自发光点光源提取（顶点聚类）
  ├── Mesh.h / Mesh.cpp           顶点缓冲封装（VAO/VBO/EBO）
  │                               - 骨骼影响顶点布局（loc 0–5, 含 BoneIDs + Weights）
  ├── Texture.h / Texture.cpp     stb_image 纹理加载
  ├── Player.h / Player.cpp       玩家控制器
  │                               - 圆柱体碰撞（半径 0.4，高度 1.8）
  │                               - 5 状态 FSM + 动画字典 + 运动物理
  ├── Enemy.h / Enemy.cpp         敌人 AI
  │                               - 类型：ZOMBIE（僵尸）/ SKELETON（骷髅）
  │                               - 各自 AI 分支（追击/射击/后退风筝）
  │                               - 蒙皮网格渲染 + 骨骼动画
  ├── Arrow.h / Arrow.cpp         箭矢投射物
  │                               - 抛物线弹道（重力 + 碰撞检测）
  │                               - 10 秒生命周期，卡墙状态处理
  ├── CollisionWorld.h / CollisionWorld.cpp  碰撞检测世界
  │                               - 三角形提取（跳过植物 + 水面）
  │                               - 2D 空间哈希网格（单元大小 5.0）
  │                               - 射线检测（Möller–Trumbore）
  ├── ThirdPersonCamera.h / ThirdPersonCamera.cpp  多模式相机
  │                               - 第一人称 / 第三人称后方 / 第三人称前方
  │                               - 动态 FOV（疾跑 45°→55° 平滑过渡）
  ├── Animation.h / Animation.cpp 骨骼动画系统
  │                               - Bone 关键帧插值（TRS）
  │                               - Animator 双轨道混合（基础 + 叠加）
  │                               - 全局变换缓存 + 按名称查询节点
  ├── FontRenderer.h / FontRenderer.cpp  TrueType 字体渲染
  │                               - UTF-8 → 码点解码
  │                               - 逐字符四边形生成（图集 UV 偏移/缩放）
  ├── font_baker.h / font_baker.c 字体图集烘焙（C 编译）
  ├── stb_truetype.h              TrueType 光栅化（单头文件，stb 风格）
  ├── miniaudio.h                 音频引擎（单头文件声明）
  ├── Audio.cpp                   音频引擎实现单元（MINIAUDIO_IMPLEMENTATION）
  ├── Camera.h / Camera.cpp       基类相机（未使用，保留）
  └── FPSCamera.h / FPSCamera.cpp FPS 相机（未使用，保留）

================================================================================
8. 运行时操作说明
================================================================================

  ┌──────────┬──────────────────────────────────────────────────────┐
  │ 按键     │ 功能                                                 │
  ├──────────┼──────────────────────────────────────────────────────┤
  │ WASD     │ 移动玩家（W 前进，S 后退，A/D 左右平移）             │
  │ 鼠标     │ 旋转视角（俯仰 -89° ~ +89°）                         │
  │ 空格     │ 跳跃（力 6.0）                                       │
  │ L-Ctrl   │ 切换奔跑 / 步行模式（持续切换，非按住）              │
  │          │ 奔跑仅在按住 W 时激活，S/A/D 保持步行速度            │
  │ L-Shift  │ 潜行（按住，速度 2.0）                               │
  │ 左键     │ 攻击（近战射线检测，范围 3.5m）                      │
  │          │ 空手伤害 1，剑伤害 7 + 横扫 AOE                      │
  │ 右键     │ 射箭（25m/s，从眼部发射）/ 手持面包时按住吃饭（HP<20）│
  │ 滚轮     │ 循环切换快捷栏（0–8）                                │
  │ 1–9      │ 直接选择快捷栏格位                                    │
  │ F1       │ 热重载全部着色器（一键重载全部 14 个）              │
  │ F2       │ 快捷时间选择面板（正午 / 黄昏 / 午夜 / 清晨）        │
  │ F3       │ 调试信息叠加（XYZ 坐标 + FPS，左上角）               │
  │ F5       │ 循环切换相机模式（第一人称/第三人称后/第三人称前）   │
  │ F6       │ 碰撞调试线框（绿色线框显示碰撞三角形）               │
  │ R        │ 切换降雨（平滑过渡 2 秒）                            │
  │ T        │ 聊天历史记录面板（显示最近 12 条消息，锁定鼠标）     │
  │ Esc      │ 退出程序                                              │
  └──────────┴──────────────────────────────────────────────────────┘

  主菜单：点击 "开始游戏" 按钮进入游戏。

  玩家 HP：20（10 颗心），受伤闪烁免疫 0.5 秒，死亡后重生至 (-20,-7,-13)。

================================================================================
9. 技术要点
================================================================================

  [渲染管线]
    G-Buffer 延迟几何 → SSAO 环境光遮蔽 → 主场景光照 → 天空盒 →
    第一人称手臂 → 粒子/降雨 → 2D UI → 后处理（Bloom + God Rays + 色调映射）

  [阴影]
    4096×4096 深度图（GL_DEPTH_COMPONENT/GL_FLOAT）
    Poisson Disk 16 采样软阴影 + 随机旋转抗噪
    动态 bias: max(0.005 × (1 − N·L), 0.0005)

  [SSAO]
    16 采样半球遮挡估算（64 样本核取前 16 + 4×4 噪声旋转纹理）
    4×4 盒式模糊降噪

  [昼夜系统]
    天体旋转矩阵（rotateZ(timeOfDay)），太阳/月亮高度自动切换光源
    白天暖光 (1.3, 1.15, 0.95)，日落橙红光 (1.0, 0.4, 0.1)
    夜间极暗月光 (0.01, 0.02, 0.04)，环境光锁定为深蓝夜光

  [水面]
    顶点波浪位移（sin 函数驱动）+ 噪声法线扰动
    Fresnel 天空反射 + 太阳闪光（pow 256，×10）
    半透明混合（alpha 0.4–0.9，视角驱动）
    双通道渲染：不透明物体先，水面后（glDepthMask(FALSE)）

  [植物]
    顶点风动摇摆（根锚定，sin/cos 驱动）
    materialType==1 不参与碰撞检测

  [体积光 God Rays]
    屏幕空间径向模糊（60 步采样），从太阳屏幕坐标向外衰减
    密度 0.95，衰减 0.92，仅太阳在屏幕上时启用

  [雾效]
    指数平方距离雾，密度受降雨强度影响
    晴天淡蓝 (0.6, 0.8, 0.95)，雨天浓灰 (0.4, 0.45, 0.5)

  [灯光]
    512 点光源数组（uniform array），12m 裁切距离
    火光源颜色 (0.7, 0.45, 0.2)，衰减 1/(1 + 0.20d + 0.10d²)
    快捷栏 1 号位添加手持动态光源

  [色调映射]
    Reinhard 曝光（0.5）→ ACES 电影级曲线 → 饱和度增强（1.25×）→ Gamma 2.4

  [泛光 Bloom]
    亮度阈值提取（>1.0）→ God Rays（可选）→ 双通高斯模糊（3H + 3V，5 采样核）→ 合成

  [材质系统]
    6 种 materialType，按材质名称自动分类：
    Type 0 - 默认（石头/木头/泥土，无镜面高光）
    Type 1 - 植物（风摇，无碰撞）
    Type 2 - 树叶（风摇，无镜面）
    Type 3 - 自发光（无视阴影/环境光，恒定发光）
    Type 4 - 水面（波浪 + 反射 + 半透明）
    Type 5 - 手持物品（保留镜面高光，手动设置）

  [音效]
    近战挥击 / 弓箭发射 / 命中 / 敌人受伤 / 死亡 / 故事钟声
    共 8+ MP3 音效文件，通过 miniaudio 引擎播放

================================================================================
10. 开发建议
================================================================================

  - F1 一键热重载全部 14 个着色器。修改任意 .vert/.frag 文件后按 F1 即可
    生效，无需重启程序。
  - 添加新源文件后需在 OpenGL/CMakeLists.txt 中注册并重新配置 CMake。
  - .c 文件需 set_source_files_properties(... PROPERTIES LANGUAGE C)。
  - 新增资源目录需在 CMakeLists.txt 添加 POST_BUILD copy_directory 命令。
  - 绘制顺序中的 uIsHit / uMaterialType 统一变量有"粘性"，切换绘制目标后
    必须手动重置，否则会导致画面异常。

================================================================================
                          Copyright (c) 2025–2026
================================================================================
