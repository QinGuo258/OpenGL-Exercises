# shaders/ 目录着色器文件说明

`shaders/` 下共 24 个 GLSL 文件：11 个顶点着色器（`.vert`）、13 个片段着色器（`.frag`）。全部使用 `#version 460 core`（OpenGL 4.6）。

---

## 目录

- [顶点着色器](#顶点着色器)
  - [model.vert](#modelvert)
  - [shadow.vert](#shadowvert)
  - [shadow_skinned.vert](#shadow_skinnedvert)
  - [g_buffer.vert](#g_buffervert)
  - [g_buffer_skinned.vert](#g_buffer_skinnedvert)
  - [sky.vert](#skyvert)
  - [rain.vert](#rainvert)
  - [ui.vert](#uivert)
  - [fullscreen.vert](#fullscreenvert)
  - [debug.vert](#debugvert)
  - [grid.vert](#gridvert)
- [片段着色器](#片段着色器)
  - [model.frag](#modelfrag)
  - [shadow.frag](#shadowfrag)
  - [g_buffer.frag](#g_bufferfrag)
  - [sky.frag](#skyfrag)
  - [rain.frag](#rainfrag)
  - [ssao.frag](#ssaofrag)
  - [ssao_blur.frag](#ssao_blurfrag)
  - [blur.frag](#blurfrag)
  - [hdr_compose.frag](#hdr_composefrag)
  - [godrays.frag](#godraysfrag)
  - [ui.frag](#uifrag)
  - [debug.frag](#debugfrag)
  - [grid.frag](#gridfrag)
- [渲染管线着色器匹配表](#渲染管线着色器匹配表)
- [Uniform 跨着色器依赖关系](#uniform-跨着色器依赖关系)

---

## 顶点着色器

### `model.vert`

**用途**：主场景中所有静态和骨骼动画模型的顶点处理。

| 输入属性 | 位置 | 类型 |
|----------|------|------|
| `aPos` | loc=0 | `vec3` — 顶点位置 |
| `aNormal` | loc=1 | `vec3` — 顶点法线 |
| `aTexCoords` | loc=2 | `vec2` — 纹理坐标 |
| `boneIds` | loc=3 | `ivec4` — 4 根骨骼 ID |
| `weights` | loc=4 | `vec4` — 4 根骨骼权重 |

| 输出 | 目标 |
|------|------|
| `FragPos` | 世界空间顶点位置 |
| `FragNormal` | 世界空间法线（含 model 矩阵变换） |
| `TexCoords` | 直通 UV 坐标 |
| `FragPosLightSpace` | 光源空间裁剪坐标（阴影贴图采样） |

| Uniform | 用途 |
|---------|------|
| `uModel` / `uView` / `uProjection` | 标准 MVP 变换 |
| `uLightSpaceMatrix` | 光源空间变换（正交投影 × lookAt） |
| `finalBonesMatrices[100]` | 骨骼最终矩阵数组（骨骼动画） |
| `uTime` | 时间（风力动画相位） |
| `uRainIntensity` | 雨量（风速增强） |
| `uMaterialType` | 材质类型（控制风力行为和水面偏移） |

**核心逻辑**：

1. **骨骼蒙皮**：4 骨骼加权混合变换，`boneTransform = Σ finalBonesMatrices[id] × weight`
2. **风力摇摆**（`uMaterialType == 1 或 2`）：
   - 草丛 (type=1)：根部锚定不动（`windWeight = aTexCoords.y`，根部 Y≈0）
   - 树叶 (type=2)：整体抖动（`windWeight = 0.8`）
   - 算法：`offset = sin/cos(worldPos.xz * 2.0 + uTime * windSpeed) * windStrength * windWeight`
   - 雨越大风速越快（`windSpeed = 2.0 + uRainIntensity × 4.0`）
3. **水面整体下沉**（`uMaterialType == 4`）：`worldPos.y -= 0.1`，防止与岸边方块 Z-Fighting
4. 输出 `FragPosLightSpace` 供片段着色器做 PCF 阴影计算

---

### `shadow.vert`

**用途**：静态几何体的阴影贴图深度渲染（Pass 1a + Pass 1b 雨遮挡深度）。

| 输入属性 | 位置 | 类型 |
|----------|------|------|
| `aPos` | loc=0 | `vec3` — 顶点位置 |
| `aNormal` | loc=1 | `vec3` — 法线（用于十字交叉面片检测） |
| `aTexCoords` | loc=2 | `vec2` — 纹理坐标 |

| 输出 | 目标 |
|------|------|
| `TexCoords` | 直通 UV 坐标（片段着色器做 alpha 剔除） |

**核心逻辑**：

1. 通过**法线方向检测十字交叉面片**（草丛/小麦/花）：`abs(aNormal.x) ∈ (0.1, 0.9) && abs(aNormal.z) ∈ (0.1, 0.9)`
2. 对交叉面片施加与 `model.vert` 完全一致的风力摇摆，确保阴影形状与可见几何体匹配
3. 输出 `gl_Position = uLightSpaceMatrix * vec4(worldPos, 1.0)` — 光源空间裁剪坐标

---

### `shadow_skinned.vert`

**用途**：骨骼动画模型（玩家/敌人）的阴影贴图深度渲染。

与 `shadow.vert` 类似，但额外处理骨骼蒙皮：

| 输入属性 | 位置 | 类型 |
|----------|------|------|
| `aPos` | loc=0 | `vec3` — 顶点位置 |
| `aTexCoords` | loc=2 | `vec2` — 纹理坐标 |
| `boneIds` | loc=3 | `ivec4` — 骨骼 ID |
| `weights` | loc=4 | `vec4` — 骨骼权重 |

**核心逻辑**：

1. 与 `model.vert` 相同的 4 骨骼加权混合
2. 最终：`gl_Position = uLightSpaceMatrix * uModel * boneTransform * vec4(aPos, 1.0)`
3. 无风力摇摆（玩家/敌人非植被），但保留 `uTime`/`uRainIntensity` 声明以兼容 C++ 的 `SetFloat` 调用

---

### `g_buffer.vert`

**用途**：静态几何体的 G-Buffer 几何预填充（Pass 1.5），输出视图空间坐标和法线。

| 输入属性 | 位置 | 类型 |
|----------|------|------|
| `aPos` | loc=0 | `vec3` — 顶点位置 |
| `aNormal` | loc=1 | `vec3` — 法线 |
| `aTexCoords` | loc=2 | `vec2` — 纹理坐标 |
| `aColor` | loc=5 | `vec3` — 顶点色（R 通道=f 风力权重） |

| 输出 | 目标 |
|------|------|
| `FragPos` | 视图空间坐标（`viewPos.xyz`） |
| `Normal` | 视图空间法线（`transpose(inverse(uView * uModel)) × aNormal`） |
| `TexCoords` | 直通 UV（片段着色器 alpha 剔除用） |

**核心逻辑**：

1. **植被风摆同步**：通过 `aColor.r > 0.01` 判断植被顶点，施加与 `model.vert` 完全相同的风力偏移
2. G-Buffer 存储的是**视图空间**位置和法线（后续 SSAO 在视图空间计算）

---

### `g_buffer_skinned.vert`

**用途**：骨骼动画模型的 G-Buffer 几何预填充（玩家/敌人/箭矢/手持物品/第一人称手臂）。

| 输入属性 | 位置 | 类型 |
|----------|------|------|
| `aPos` | loc=0 | `vec3` — 顶点位置 |
| `aNormal` | loc=1 | `vec3` — 法线 |
| `aTexCoords` | loc=2 | `vec2` — 纹理坐标 |
| `aBoneIDs` | loc=3 | `ivec4` — 骨骼 ID |
| `aWeights` | loc=4 | `vec4` — 骨骼权重 |

**核心逻辑**：

1. 4 骨骼加权蒙皮
2. 世界空间 → 视图空间变换
3. 法线矩阵：`transpose(inverse(mat3(uView * uModel * boneTransform)))` — 正确处理非均匀缩放
4. 输出视图空间坐标和法线

---

### `sky.vert`

**用途**：天空盒渲染（Pass 3）。使用 LEQUAL 深度测试 + Z=1.0 技巧确保始终在最远。

| 输入属性 | 位置 | 类型 |
|----------|------|------|
| `aPos` | loc=0 | `vec3` — unit cube 顶点坐标 |

| 输出 | 目标 |
|------|------|
| `TexCoords` | 立方体贴图方向向量（`= aPos`） |

**核心逻辑**：

```glsl
gl_Position = (uProjection * uView * vec4(aPos, 1.0)).xyww;
```
`.xyww` 技巧：强制 Z = W，经过透视除法后深度恒为 1.0（最远），确保天空盒始终在所有场景物体之后。

---

### `rain.vert`

**用途**：VBO-less 程序化雨粒子生成（Pass 5）。无需 VBO，全部由 `gl_VertexID` 驱动。

**核心逻辑**：

1. **随机种子生成**：`dropID = gl_VertexID / 2`，`isTop = gl_VertexID % 2`（每两顶点组成一条雨线）
2. **随机参数**（全由 `hash(dropID)` 派生）：
   - 起始位置偏移 `(rx, ry, rz) ∈ [0,1]³`
   - 下落速度 `speed ∈ [10, 25]`
   - 在 40×40×40 米的相机跟随盒子内随机分布
3. **下落与循环**：`localPos += fallDir(0.1, -1.0, 0.1) * uTime * speed`，通过 `mod` 实现循环包裹
4. **雨丝拉伸**：顶部顶点沿风向反向拉伸 `dropLength ∈ [0.2, 0.5]`（雨越大丝越长）
5. 输出 `FragPosRainSpace`：雨的光源空间坐标，用于遮挡剔除（Pass 1b 的 `rainDepthMap`）

| 输出 | 目标 |
|------|------|
| `FragPosRainSpace` | 雨遮挡深度空间坐标 |

---

### `ui.vert`

**用途**：通用 2D UI + 3D 公告板粒子 + TrueType 字体字形渲染。

| 输入属性 | 位置 | 类型 |
|----------|------|------|
| `aPos` | loc=0 | `vec2` — 单位正方形基准顶点 |
| `aTexCoords` | loc=1 | `vec2` — 基准 UV |

| Uniform | 用途 |
|---------|------|
| `uProjection` | 正交投影矩阵（UI 2D）或透视投影（3D 公告板） |
| `uModel` | TRS 模型矩阵（位移+旋转+缩放） |
| `uUvScale` | 精灵图集的 UV 缩放（字体：字形在图集中的缩放） |
| `uUvOffset` | 精灵图集的 UV 偏移（字体：字形在图集中的位置） |

**核心逻辑**：`TexCoords = aTexCoords * uUvScale + uUvOffset` — 统一的 UV 映射，同时支持完整贴图 UI 和碎片式字体图集。

---

### `fullscreen.vert`

**用途**：全屏后处理四边形（SSAO、SSAO 模糊、Bloom、God Rays、HDR 合成）。

| 输入属性 | 位置 | 类型 |
|----------|------|------|
| `aPos` | loc=0 | `vec2` — NDC 坐标 [-1,1]² |
| `aTexCoords` | loc=1 | `vec2` — UV 坐标 [0,1]² |

| 输出 | 目标 |
|------|------|
| `TexCoords` | 直通 UV 坐标 |

**核心逻辑**：零开销直通，`gl_Position = vec4(aPos, 0.0, 1.0)`。NDC 坐标与预定义的 fullscreenVAO 顶点完全匹配。

---

### `debug.vert`

**用途**：碰撞调试线框渲染（F6 切换）。

| 输入属性 | 位置 | 类型 |
|----------|------|------|
| `aPos` | loc=0 | `vec3` — 世界空间线框顶点 |

**核心逻辑**：顶点已在世界空间（由 CollisionWorld 的 `GetDebugLineVertices()` 生成），直接乘 `projection * view`。

---

### `grid.vert`

**用途**：未使用的网格地板渲染。

| 输入属性 | 位置 | 类型 |
|----------|------|------|
| `aPos` | loc=0 | `vec3` |

| 输出 | 目标 |
|------|------|
| `WorldPos` | 世界空间坐标（片段着色器用于程序化网格线） |

**状态**：与 `grid.frag` 配对，当前未在任何渲染 Pass 中使用。

---

## 片段着色器

### `model.frag`

**用途**：所有场景几何体的主光照着色器 — 整个项目最核心、最复杂的着色器（331 行）。

**光照模型**：Blinn-Phong（半角向量）+ 多项扩展。

#### 输出

`OutColor` — `vec4(finalColor, alpha)`

#### Uniform 清单

| Uniform | 类型 | 用途 |
|---------|------|------|
| `uCameraPos` | `vec3` | 摄像机世界坐标（视线计算） |
| `uLightDir` | `vec3` | **光传播方向**（太阳→地面，或月亮→地面） |
| `uLightColor` | `vec3` | 动态光源颜色（日光/日落/月光渐变） |
| `uAmbientColor` | `vec3` | 动态环境光颜色（日夜切换） |
| `uSpecularStrength` | `float` | 镜面高光基础强度 |
| `uShininess` | `float` | 高光锐利度 |
| `uRainIntensity` | `float` | 降雨强度（水坑/潮湿/暗沉） |
| `uMaterialType` | `int` | 材质分类（0–5），控制着色分支 |
| `uDiffuseTexture` | `sampler2D` | 漫反射纹理 |
| `uHasDiffuseTexture` | `bool` | 是否有漫反射纹理 |
| `uIsHit` | `bool` | 受击闪红开关（仅敌人使用） |
| `uTime` | `float` | 运行时间（水面法线动画） |
| `uNightFade` | `float` | 夜色系数（C++ 根据 sunY 计算，0=白天, 1=深夜） |
| `uHorizonColor` | `vec3` | 天空地平线实时颜色（水面反射） |
| `uZenithColor` | `vec3` | 天空天顶实时颜色（水面反射） |
| `uPackedNoiseMap` | `sampler2D` | 打包噪点图（R=大波浪, G=小波纹） |
| `uShadowMap` | `sampler2D` | 4096² 阴影贴图（纹理单元 15） |
| `uLightSpaceMatrix` | `mat4` | 法线偏移阴影重投影矩阵 |
| `uSsaoMap` | `sampler2D` | SSAO 模糊后遮蔽图（纹理单元 13） |
| `uScreenSize` | `vec2` | 屏幕尺寸（SSAO UV 计算） |
| `uPointLights[512]` | `vec3[]` | 自发光点光源位置数组 |
| `uNumPointLights` | `int` | 点光源数量 |

#### 渲染阶段（按执行顺序）

**阶段 0 — Alpha 剔除**

```glsl
if (texColor.a < 0.1) discard;
```

**阶段 1 — 湿润/水坑效果**

- 低频噪声生成水坑掩码（`noise2D(FragPos.xz * 0.3)`）
- `upFactor`：只有朝上面（`normal.y > 0.8`）才能积水
- `puddleMask`：smoothstep 动态阈值，雨大时水坑扩张
- 湿润材质变化：漫反射变暗（`×0.5`）、高光变锐利（→256）、高光强度提升（→1.0）

**阶段 2 — 微表面涟漪扰动**

积水区域的高频噪声扰动法线（0.03 强度），消除浮点条纹

**阶段 3 — 环境光 + SSAO**

```glsl
ambient = uAmbientColor * albedoColor * ssao;
```
SSAO 仅衰减环境光，太阳直射光和点光源不受影响

**阶段 4 — 漫反射 + 镜面反射**

- 镜面高光策略：只有手持道具 (`uMaterialType == 5`) 或**积水表面** (`wetness > 0`) 才产生 specular
- 泥土/草/树叶/发光体始终无高光，呈现纯粹漫反射
- 湿润区域 specular 强度 = `currentSpecStrength × wetness`

**阶段 5 — PCF 软阴影**

`ShadowCalculation()` 函数：
1. **法线偏移**：将世界坐标沿法线外推 0.035 米，消除几何自阴影三角切边
2. 16 抽泊松圆盘采样（`PoissonDisk[16]`）
3. 每次采样用 `rand(gl_FragCoord.xy)` 旋转采样点（消除固定噪声图案）
4. 柔和半径 `filterRadius = 2.5`
5. 动态 bias：`max(0.005 × (1-N·L), 0.0005)`

**阶段 6 — 雨天积水天空反射**

- 计算视线反射向量 → 采样传入的动态天空颜色（`uHorizonColor` / `uZenithColor`）
- 菲涅尔效应（Fresnel Schlick）：`fresnel = 0.02 + 0.98 × (1-cosθ)⁵`
- 夜间反射强度自动压至 2%（`nightDamp`）

**阶段 7 — 点光源循环**

- 遍历 `uPointLights[0..uNumPointLights-1]`
- 距离 > 12m 直接跳过
- 物理距离衰减：`1/(1 + 0.14d + 0.07d²)`
- 边缘消隐：距离 8.0–12.0 时线性淡出（消灭硬边）
- 火光颜色：`(0.7, 0.45, 0.2)`

**阶段 8 — 自发光材质**（`uMaterialType == 3`）

```glsl
emission = texColor.rgb * 1.2;
finalColor = mix(finalColor, emission, 0.9);
```

**阶段 9 — 受击闪红**（`uIsHit == true`）

```glsl
finalColor = mix(finalColor, vec3(1,0,0), 0.3);
```

**阶段 10 — 大气距离雾**

- 密度：`mix(0.0005, 0.005, uRainIntensity)` — 晴天极度通透，雨天才起浓雾
- 颜色：`mix(淡蓝, 灰暗灰, uRainIntensity)`
- 指数平方衰减：`exp(-(dist × density)²)`

**阶段 11 — 曝光色调映射**

```glsl
finalColor = vec3(1.0) - exp(-finalColor * 1.0);
```

**阶段 12 — 水面渲染**（`uMaterialType == 4`，覆盖之前所有计算）

1. **波浪法线**：`calculateWaterNormal()` — 有限差分法从打包噪点图实时计算
   - UV 缩放 0.06（大型连绵波浪）
   - 双层 R 通道采样（不同速度/方向）
   - 偏导数 → 法线扰动（强度 0.25，镜面般温润）
2. **水底色**：`deepWater` / `shallowWater` 混合（夜间转为几乎纯黑）
3. **天空反射**：视线反射采样天空颜色，菲涅尔加权
4. **太阳/月光高光 Glint**：（修复后）`reflect(normalize(uLightDir), waterNorm)`
   - 白天：`shininessFactor=256, glintIntensity=8.0` — 极锐利强光
   - 夜晚：`shininessFactor=64, glintIntensity=0.4` — 柔和微弱
5. **透明度**：菲涅尔驱动 `0.12–0.75`，夜间加厚至 ~0.5

---

### `shadow.frag`

**用途**：阴影贴图片段着色器（静态几何体 + 雨遮挡深度共用）。

| 输入 | 来源 |
|------|------|
| `TexCoords` | `shadow.vert` 输出 |
| `uDiffuseTexture` | 绑定在 layout(binding=0) |

**核心逻辑**：

```glsl
float alpha = texture(uDiffuseTexture, TexCoords).a;
if (alpha < 0.1) discard;
```

无条件执行 alpha 剔除 — 不需要 C++ 传入任何 `uHasDiffuseTexture` 布尔值。草丛/树叶等透明像素不写入阴影贴图，避免假阴影。

---

### `g_buffer.frag`

**用途**：G-Buffer 几何预填充（Pass 1.5 后半段 — 静态几何体）。

| 输出 | 格式 | 内容 |
|------|------|------|
| `gPosition` (loc=0) | RGBA16F | 视图空间坐标 `.xyz` |
| `gNormal` (loc=1) | RGBA16F | 视图空间单位法线 `.xyz` |

**核心逻辑**：

1. Alpha 剔除：在 G-Buffer 阶段执行，透明像素不写入坐标/法线缓冲
2. 输出归一化法线

---

### `sky.frag`

**用途**：程序化天空盒渲染 — 项目第二复杂的着色器（232 行）。

#### 天空渐变

```glsl
skyColor = mix(uHorizonColor, uZenithColor, clamp(viewDir.y, 0, 1));
```
来自 C++ 的动态地平线/天顶颜色（白天→黄昏→深夜平滑过渡）。

#### 太阳光晕

- 双重幂次：64 次宽泛光 + 256 次核心强光
- 颜色：`(1.0, 0.95, 0.8)` — 暖黄白色
- 可见性随太阳高度和降雨衰减

#### 月亮光晕

- 方向 = `-uSunDir`（与太阳相对）
- 更弱更冷：`(0.6, 0.7, 0.9)`
- 64 次幂 × 0.2 强度

#### Minecraft 风格方块日月

- 天球本地坐标系（`uStarMatrix` 旋转）
- **太阳**：`celestialPos.x > 0 && abs(y) < 0.05 && abs(z) < 0.05`
  - 外围边框：奶油色 `(2.55, 2.35, 1.95)`
  - 核心：纯白 `(3.0, 3.0, 3.0)` — 超亮 HDR 值供 Bloom 提取
- **月亮**：`celestialPos.x < 0 && abs(y) < 0.05 && abs(z) < 0.05`
  - 带陨石坑纹理（8×8 网格 hash 噪声）
  - 颜色：`(0.6, 0.65, 0.7)`

#### 程序化星星

- 仅在非日月区域渲染
- `gridPos = floor(celestialPos * 200.0)` — 空间划分抗闪烁
- `starValue > 0.99` 阈值筛选（1% 的天空像素是星星）
- 强度随 `nightFade` 和地平线仰角衰减

#### 体积云（光线步进 Raymarching）

项目最复杂的视觉效果：

| 参数 | 值 | 说明 |
|------|-----|------|
| 云层高度 | 300–550m | 200m 物理厚度 |
| 视线步数 | 16 | 主视觉步进 |
| 光源步数 | 2 | 每步内的自阴影采样 |
| 光源步长 | 40m | 自阴影光线步长 |
| 相位函数 | HG (g=0.5) + (g=-0.1) | 前向散射（银边）+ 微弱后向散射 |

1. **起始点抖动**：Interleaved Gradient Noise 蓝噪点（`getJitter(gl_FragCoord.xy)`），以极低步数消除步进伪影
2. **密度计算**：4 层 FBM 3D 噪声 + 高度遮罩（平底蓬松顶） + 降雨阈值降低（云连成片）
3. **光源步进**：每步沿太阳方向采样 2 次计算自阴影透射率（比尔-朗伯定律）
4. **累积**：Alpha 合成公式，`totalAlpha >= 0.99` 提前终止
5. **地平线消隐**：`smoothstep(0.02, 0.15, viewDir.y)`

#### 空间噪声系统

| 函数 | 维度 | 用途 |
|------|------|------|
| `hash3D(vec3)` | 3D | 基本哈希 |
| `noise3D(vec3)` | 3D | 平滑值噪声 |
| `fbm3D(vec3)` | 4 层 | 分形布朗运动（云密度） |
| `hash(vec3)` | 3D | 星星/陨石坑生成 |
| `hash(vec3 p, float)` | 3D+1 | 云层细节 |
| `getJitter(vec2)` | 2D | IGN 蓝噪点（光线步进去伪影） |

---

### `rain.frag`

**用途**：雨滴片段着色器 — 遮挡剔除 + 半透明着色。

| 输入 | 来源 |
|------|------|
| `FragPosRainSpace` | `rain.vert` 输出的光源空间坐标 |
| `uRainDepthMap` | Pass 1b 生成的雨遮挡深度图（纹理单元 14） |

**核心逻辑**：

1. **遮挡剔除**：将雨滴投影到 `rainDepthMap` 的屏幕空间
   - `currentDepth - 0.005 > mapDepth` → `discard`（雨滴在遮挡物后方）
   - 偏移 0.005 防止精度闪烁
2. **着色**：`vec4(0.8, 0.9, 1.0, 0.2 * uRainIntensity)` — 浅蓝白色，透明度由全局雨量控制

---

### `ssao.frag`

**用途**：SSAO 环境光遮蔽生成（Pass 1.8）。在视图空间使用半球采样估算每个像素的局部遮蔽程度。

| 输入纹理 | 来源 |
|----------|------|
| `gPosition` | G-Buffer Pass（视图空间坐标） |
| `gNormal` | G-Buffer Pass（视图空间法线） |
| `texNoise` | 4×4 RGBA16F 随机旋转向量（GL_REPEAT） |

| 输出 | 格式 |
|------|------|
| `FragColor` | `float` — 遮蔽值（1.0 = 无遮蔽, 0.0 = 全暗） |

| Uniform | 用途 |
|---------|------|
| `samples[64]` | C++ 预生成的 64 个半球采样向量（shader 只用前 16 个） |
| `projection` | 投影矩阵（视图空间 → 屏幕空间变换） |

**核心逻辑**：

1. **TBN 构建**：`randomVec` → Gram-Schmidt 正交化 → `tangent + bitangent + normal`
2. **16 抽采样**：
   - 每个采样点从切线空间旋转到视图空间 → 以 `fragPos` 为中心偏移
   - 投影到屏幕空间查 G-Buffer 深度
   - `sampleDepth >= samplePos.z + bias (0.025)` → 该方向无遮蔽
3. **范围检查**：`smoothstep(0, 1, radius / abs(fragPos.z - sampleDepth))` — 防止远处物体遮蔽近处
4. 最终：`occlusion = 1.0 - occlusion/16`

**采样核**：C++ 中生成 64 个样本（`lerp(0.1, 1.0, scale²)` 偏向原点），着色器只用前 16 个。

---

### `ssao_blur.frag`

**用途**：SSAO 4×4 盒式模糊（Pass 1.9）。消除 SSAO 4×4 噪声贴图的高频伪影。

| 输入 | 来源 |
|------|------|
| `ssaoInput` | Pass 1.8 的 `ssaoColorBuffer` |

**核心逻辑**：16 像素（4×4）均值滤波，输出平滑的 `ssaoColorBufferBlur` 供 `model.frag` 采样。

---

### `blur.frag`

**用途**：亮部提取 + 双通道高斯模糊（Bloom Pass 1+2）。

#### 模式一：亮部提取（`uExtract == true`）

- 亮度 > 1.4 的像素才保留（超亮 HDR 值：太阳核心 3.0、火把、灯笼）
- 使用相对亮度公式：`dot(color, vec3(0.2126, 0.7152, 0.0722))`
- 低于阈值 → 纯黑，写入 `pingpongColorTex[0]`

#### 模式二：高斯模糊（`uExtract == false`）

- 5 抽高斯核：`[0.227, 0.195, 0.122, 0.054, 0.016]`
- 三水平 + 三竖直 = 6 次迭代（3H+3V ping-pong）
- 水平/竖直由 `uHorizontal` 控制
- 半分辨率（960×540）

---

### `hdr_compose.frag`

**用途**：最终色调映射（Pass 6）。将 HDR 场景+光晕合成为 LDR 屏幕输出。

| Uniform | 当前值 | 用途 |
|---------|--------|------|
| `uSceneTex` | hdrFBO 纹理 | 场景原始 HDR 颜色 |
| `uBloomTex` | 模糊后光晕纹理 | 6 次高斯模糊后的 Bloom |
| `uBloomIntensity` | `0.35` | Bloom 混合权重 |
| `uExposure` | `0.5` | 曝光度 |

**5 步管线**：

1. **线性叠加**：`sceneColor + bloomColor × 0.35`
2. **曝光映射**：`1 - exp(-result × 0.5)` — Reinhard 变体
3. **ACES 电影色调映射**：好莱坞标准曲线
   - 参数：`a=2.51, b=0.05, c=2.43, d=0.59, e=0.16`
4. **饱和度提升**：×1.25（仅作用于色度，不改变亮度）
5. **Gamma 2.4**：`pow(result, 1/2.4)`

---

### `godrays.frag`

**用途**：屏幕空间体积光（Crepuscular Rays / God Rays）。在 Bloom 亮部提取和模糊之间执行。

| Uniform | 当前值 | 用途 |
|---------|--------|------|
| `uInputTex` | `pingpongColorTex[0]` | 亮部提取图 |
| `uLightScreenPos` | 动态计算 | 太阳/月光在屏幕上的 2D 位置 [0,1]² |
| `uDensity` | `0.95` | 光线密度/拖尾长度 |
| `uDecay` | `0.92` | 每步指数衰减率 |
| `uWeight` | `0.02` (`godrayWeight`) | 采样权重 |

**核心逻辑**：

1. 60 步径向采样：从当前像素沿指向光源的向量逐步收缩
2. 每步采样亮部图 → 乘以衰减 → 累加
3. 衰减按指数递减：`illuminationDecay *= 0.92`（指数衰减形成光线拖尾）
4. 结果写入 `pingpongColorTex[1]`，随后输入高斯模糊

C++ 端控制：`godrayWeight = 0` 时跳过整个 God Rays Pass（太阳不在屏幕上）。

---

### `ui.frag`

**用途**：通用 UI 片段着色器（2D 面板 + 3D 粒子 + 字体字形）。

| Uniform | 用途 |
|---------|------|
| `uTexture` | 主纹理（精灵图、字体图集、粒子贴图） |
| `uAlpha` | 全局透明度（粒子淡入淡出） |
| `uUseTexture` | `false` 时渲染纯色块（黑屏/菜单背景/聊天面板） |
| `uSolidColor` | 纯色块颜色 |
| `uIsFont` | `true` 时启用字体渲染模式 |
| `uFontColor` | 字体颜色（RGB） |

**三种渲染模式**：

1. **字体模式**（`uIsFont == true`）：`GL_RED` 单通道图集 → R 通道作为 alpha → `vec4(uFontColor, alphaVal)`，alpha < 0.05 丢弃
2. **正常纹理模式**：标准 RGBA 采样，alpha < 0.1 丢弃
3. **纯色模式**（`uUseTexture == false`）：直接输出 `uSolidColor`，用于黑屏覆盖层、菜单背景

---

### `debug.frag`

**用途**：碰撞调试线框着色（F6 切换）。

| Uniform | 用途 |
|---------|------|
| `uColor` | 线框颜色（绿色 `vec3(0,1,0)`） |

直接输出纯色，无光照计算。

---

### `grid.frag`

**用途**：未使用的网格地板着色。

**核心逻辑**：
1. 使用 `fwidth()` 计算屏幕空间导数，实现抗锯齿网格线
2. 每 1 米一条白线
3. 基础灰色地板 + 白色网格线混合

**状态**：与 `grid.vert` 配对，当前未在任何渲染 Pass 中使用。

---

## 渲染管线着色器匹配表

| Pass | 描述 | 顶点着色器 | 片段着色器 | FBO |
|------|------|-----------|-----------|-----|
| 1a | 阴影贴图（静态） | `shadow.vert` | `shadow.frag` | `depthMapFBO` (4096²) |
| 1a | 阴影贴图（骨骼） | `shadow_skinned.vert` | `shadow.frag` | `depthMapFBO` (4096²) |
| 1b | 雨遮挡深度（静态） | `shadow.vert` | `shadow.frag` | `rainDepthFBO` (1024²) |
| 1.5 | G-Buffer（静态） | `g_buffer.vert` | `g_buffer.frag` | `gBuffer` |
| 1.5 | G-Buffer（骨骼） | `g_buffer_skinned.vert` | `g_buffer.frag` | `gBuffer` |
| 1.8 | SSAO 生成 | `fullscreen.vert` | `ssao.frag` | `ssaoFBO` |
| 1.9 | SSAO 模糊 | `fullscreen.vert` | `ssao_blur.frag` | `ssaoBlurFBO` |
| 2 | 主场景渲染 | `model.vert` | `model.frag` | `hdrFBO` |
| 2 | 水面半透明 | `model.vert` | `model.frag` | `hdrFBO` |
| 3 | 天空盒 | `sky.vert` | `sky.frag` | `hdrFBO` |
| 4 | 第一人称手臂 | `g_buffer_skinned.vert`→`model.vert` | `model.frag` | `hdrFBO` |
| 5 | 程序化雨 | `rain.vert` | `rain.frag` | `hdrFBO` |
| 6a | 2D UI | `ui.vert` | `ui.frag` | 默认 FBO |
| 6b | 3D 粒子 | `ui.vert` | `ui.frag` | `hdrFBO` |
| 6c | 字体渲染 | `ui.vert` | `ui.frag` | 默认 FBO |
| — | 调试线框 | `debug.vert` | `debug.frag` | `hdrFBO` |
| — | 亮部提取 | `fullscreen.vert` | `blur.frag` (uExtract) | `pingpongFBO[0]` (960×540) |
| — | God Rays | `fullscreen.vert` | `godrays.frag` | `pingpongFBO[1]` (960×540) |
| — | 高斯模糊 (×6) | `fullscreen.vert` | `blur.frag` (!uExtract) | `pingpongFBO[0/1]` 交替 |
| — | HDR 合成 | `fullscreen.vert` | `hdr_compose.frag` | 默认 FBO |

---

## Uniform 跨着色器依赖关系

### 贯穿多个着色器的核心 Uniform

| Uniform | 使用者 | 设置位置 |
|---------|--------|----------|
| `uTime` | `model.vert/frag`, `shadow.vert`, `shadow_skinned.vert`, `g_buffer.vert`, `rain.vert`, `sky.frag` | `main.cpp` 每帧 `glfwGetTime()` |
| `uRainIntensity` | `model.vert/frag`, `shadow.vert`, `shadow_skinned.vert`, `g_buffer.vert`, `rain.frag`, `sky.frag` | `main.cpp` 平滑插值 |
| `uView` / `uProjection` | `model.vert`, `g_buffer.vert`, `g_buffer_skinned.vert`, `sky.vert`, `rain.vert`, `debug.vert` | `ThirdPersonCamera` |
| `uLightSpaceMatrix` | `model.vert`, `shadow.vert`, `shadow_skinned.vert`, `model.frag` (阴影重投影) | `main.cpp` 光照矩阵计算 |
| `uModel` | `model.vert`, `shadow.vert`, `shadow_skinned.vert`, `g_buffer.vert`, `g_buffer_skinned.vert`, `ui.vert` | 各 Draw() 调用前 |
| `finalBonesMatrices[100]` | `model.vert`, `shadow_skinned.vert`, `g_buffer_skinned.vert` | `Animator::GetFinalBoneMatrices()` |
| `uNightFade` | `model.frag`, `sky.frag` | `main.cpp` 根据 `sunY` 计算 |
| `uHorizonColor` / `uZenithColor` | `model.frag`, `sky.frag` | `main.cpp` 日夜渐变插值 |

### 阴影系统 Uniform 流

```
C++ main.cpp:
  lightSpaceMatrix = ortho(-40,40,-40,40,1,100) * lookAt(lightPos, playerPos, Up)
  ↓
shadow.vert / shadow_skinned.vert:
  uLightSpaceMatrix → gl_Position (写入 shadow map)
  ↓
model.frag:
  uLightSpaceMatrix → 法线偏移重投影 → PCF 采样 uShadowMap
```

### 水面反射 Uniform 流

```
C++ main.cpp:
  uHorizonColor / uZenithColor = 动态日夜天空色
  uLightColor = 当前光源颜色
  uNightFade = 夜色系数
  ↓
model.frag (uMaterialType == 4):
  viewReflect → mix(uHorizonColor, uZenithColor, ...) → skyColor * fresnel
  reflect(normalize(uLightDir), waterNorm) → sun glint
```

### 后处理 FBO 路由

```
hdrFBO (RGBA16F) → blur.frag (亮部提取) → pingpong[0]
    → godrays.frag (径向模糊) → pingpong[1]  ← 仅当 sun 在屏幕上
    → blur.frag (3H+3V 高斯) → pingpong[0/1] 交替
    → hdr_compose.frag → 默认 FBO (屏幕)
```
