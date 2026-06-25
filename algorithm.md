# 项目算法清单 — OpenGL 游戏引擎

> 全部 52 项算法原理已补充完毕 ✅

## 快速导航

| # | 章节 | 行号 | 内容 |
|---|------|:---:|------|
| 一 | [光照与着色](#一渲染--光照与着色) | 24 | Blinn-Phong、Shadow Map、PCF 软阴影、SSAO、G-Buffer、点光源 |
| 二 | [后处理管线](#二渲染--后处理管线) | 52 | Bright-Pass、高斯模糊、God Rays、Reinhard/ACES 色调映射、饱和度、Gamma、距离雾 |
| 三 | [天空与大气](#三渲染--天空与大气) | 88 | 天空盒、方块日月、星场、体积云、FBM 3D、HG 相位函数、IGN |
| 四 | [水面](#四渲染--水面) | 327 | 有限差分法线、Fresnel-Schlick、Sun/Moon Glint |
| 五 | [天气系统](#五渲染--天气系统) | 446 | 程序化雨、雨遮挡、湿润水坑、植被风摆 |
| 六 | [GPU 蒙皮与粒子](#六渲染--gpu-蒙皮与动画) | 591 | LBS 蒙皮、billboard 粒子、TrueType 字体 |
| 七 | [碰撞检测与物理](#七碰撞检测与物理) | 719 | 空间哈希、Möller-Trumbore、边函数法、角色控制器、箭矢物理 |
| 八 | [动画系统](#八动画系统) | 958 | TRS 插值、双轨混合、递归骨骼层级、头部控制 |
| 九 | [AI 系统](#九ai-系统) | 1102 | 僵尸 FSM、骷髅 FSM、受击硬直 |
| 十 | [游戏系统](#十游戏系统) | 1187 | 玩家 FSM、Slab 射线、AOE、HP/进食、故事导演、摄像机、消息系统 |
| 十一 | [音频](#十一音频) | 1366 | miniaudio |
| 十二 | [数学与工具函数](#十二数学与工具函数) | 1404 | 矩阵转换、亮度公式、哈希函数族、值噪声 |

---

## 一、渲染 — 光照与着色

### 1.1 Blinn-Phong 光照模型
- **文件**: `shaders/model.frag` (第 144–212 行)
- **说明**: 环境光 + 漫反射 + 镜面高光。镜面反射仅对 `uMaterialType==5`（手持物品）或湿润积水表面启用，材质类型 0/1/2/3 均无镜面反射呈现纯粹漫反射。环境光受 SSAO 遮蔽因子衰减。雨天 `shininess` 升至 256、`specStrength` 升至 1.0。

### 1.2 阴影映射 (Shadow Mapping)
- **文件**: `src/main.cpp` (第 1607–1675 行, FBO 初始化第 950–978 行)、`shaders/shadow.vert/frag`、`shaders/shadow_skinned.vert`
- **说明**: 4096×4096 `GL_DEPTH_COMPONENT`/`GL_FLOAT` 深度帧缓冲。从光源视角渲染场景深度到 Shadow Map，灯空间矩阵 `ortho(-40,40)² × lookAt` 跟随玩家 50m 后。`GL_CLAMP_TO_BORDER` 白色边框防止贴图外区域误判。白天 `activeLightDir = -sunPosDir`，夜晚 `activeLightDir = sunPosDir`。

### 1.3 泊松圆盘软阴影 (Poisson Disk PCF)
- **文件**: `shaders/model.frag` (第 46–98 行 `ShadowCalculation()`)
- **说明**: 16 点泊松圆盘采样 + 每像素随机旋转矩阵 (`rand(gl_FragCoord.xy)` → `mat2`) + `filterRadius=2.5`。两步偏移消除自阴影：法线偏移 (0.035m) + 动态 bias `max(0.005*(1-N·L), 0.0005)`。16 次 PCF 取平均实现柔和半影过渡。

### 1.4 屏幕空间环境光遮蔽 (SSAO)
- **文件**: `shaders/ssao.frag`、`shaders/ssao_blur.frag`、`shaders/g_buffer.vert/frag`、`shaders/g_buffer_skinned.vert`、`src/main.cpp` (第 1520–1580 行)
- **说明**: 三通道系统 — (1) G-Buffer 输出视空间位置+法线 (RGBA16F×2 + DEPTH24)； (2) 16 点半球采样 + TBN 噪声旋转（Gram-Schmidt 正交化）+ 视空间投影回屏幕 + `smoothstep` 范围检查，遮蔽值 = 1 - (遮挡数/16)； (3) 4×4 盒式模糊去噪。SSAO 仅衰减环境光，不触碰漫反射/镜面反射/点光源。64 点内核取前 16，4×4 噪声纹理 `GL_REPEAT` 平铺。

### 1.5 G-Buffer 延迟几何预渲染
- **文件**: `shaders/g_buffer.vert/frag`、`shaders/g_buffer_skinned.vert` + `shaders/g_buffer.frag`、`src/main.cpp` (Pass 1.5)
- **说明**: 将所有可见几何体一次性渲染到 G-Buffer（COLOR0=视空间位置 RGBA16F，COLOR1=视空间法线 RGBA16F，DEPTH24），SSAO 从中读取数据。静态 shader 含植被风摆代码，蒙皮 shader 含 4 骨骼加权混合，两套共用同一个片段着色器。风摆参数必须与主 `model.vert` 同步。

### 1.6 点光源系统 (Point Lights)
- **文件**: `shaders/model.frag` (第 240–266 行)、`src/Model.cpp` (光源提取)
- **说明**: 从 `materialType==3`（火把/岩浆/灯笼/荧石）的网格顶点聚类提取点光源（0.5m 半径平均）。uniform 数组 `uPointLights[512]`。距离衰减 `1/(1 + 0.14d + 0.07d²)`，12m 硬截断 + 8–12m `smoothstep` 线性淡出。火光颜色 `(0.7, 0.45, 0.2)`，总强度 ×0.8。仅对非自发光体生效。

---

## 二、渲染 — 后处理管线

### 2.1 HDR 渲染与亮部提取 (Bright-Pass)
- **文件**: `shaders/blur.frag` (第 11–20 行 `uExtract=true` 分支)、`src/main.cpp` (第 2236–2248 行)
- **说明**: 半分辨率 (960×540) `GL_RGBA16F` FBO。BT.709 亮度公式 `0.2126R + 0.7152G + 0.0722B`，阈值 1.4 以上的像素保留为 Bloom 输入，其余丢弃为黑色。降分辨率使后续模糊像素量降至 1/4。

### 2.2 可分离高斯模糊 (Separable Gaussian Blur)
- **文件**: `shaders/blur.frag` (第 23–37 行 `uExtract=false` 分支)、`src/main.cpp` (第 2290–2314 行)
- **说明**: 5-tap 高斯核 `[0.227, 0.195, 0.122, 0.054, 0.016]` (σ≈1.0)。利用高斯核可分离性（2D = 1D 横 × 1D 纵），6 次迭代 (3 横 + 3 纵) 在 `pingpongFBO[0]` 与 `[1]` 之间乒乓读写。5×5 需 25 次采样降为 10 次/迭代。

### 2.3 上帝光 (God Rays / Crepuscular Rays)
- **文件**: `shaders/godrays.frag`、`src/main.cpp` (第 2250–2288 行)
- **说明**: 60 步屏幕空间径向模糊。太阳 3D 位置 → NDC 投影 → 2D 屏幕坐标。每像素向太阳方向步进采样 Bright-Pass 输出图，每步指数衰减 (`uDecay=0.92`)，`uWeight=0.015`。太阳 NDC.z 越界（屏幕外）时跳过。结果写入 `pingpongColorTex[1]`，后续高斯模糊起点改为 `[1]`。

### 2.4 色调映射 — Reinhard
- **文件**: `shaders/hdr_compose.frag` (第 23 行)
- **说明**: `result = vec3(1.0) - exp(-result × uExposure)`，`uExposure=0.5`。HDR 值压缩到 [0,1]，高亮区域逐渐饱和但不硬截断。\$C \to 0\$ 近似线性，\$C \to \infty\$ 趋近于 1。

### 2.5 色调映射 — ACES Filmic
- **文件**: `shaders/hdr_compose.frag` (第 10–13 行)
- **说明**: Narkowicz 有理函数拟合：`ACES(x) = clamp((x(2.51x+0.05))/(x(2.43x+0.59)+0.16), 0, 1)`。S 形曲线在暗部保持对比度、亮部柔和滚降（filmic toe & shoulder）。与 Reinhard 双阶段串联：`ACES(Reinhard(color))`。

### 2.6 饱和度增强
- **文件**: `shaders/hdr_compose.frag` (第 29–32 行)
- **说明**: 亮度保持法：提取 BT.709 亮度 → `mix(grayColor, result, 1.25)`。色度分量放大 25%，亮度不变，补偿色调映射的颜色衰减。

### 2.7 Gamma 校正
- **文件**: `shaders/hdr_compose.frag` (第 35 行)
- **说明**: `pow(result, vec3(1.0/2.4))`，sRGB 标准 gamma。必须在所有颜色计算之后执行，匹配显示器的非线性 EOTF 响应。

### 2.8 大气距离雾 (Exponential-Squared Distance Fog)
- **文件**: `shaders/model.frag` (第 279–285 行)
- **说明**: `fogFactor = exp(-pow(dist × density, 2.0))`。指数平方使近景几乎不受影响、中远景逐渐起雾、远景完全覆盖。晴天密度 0.0005 雾色淡蓝 `(0.6,0.8,0.95)`，雨天密度 0.005 雾色阴灰 `(0.4,0.45,0.5)`，随 `uRainIntensity` 平滑插值。

---

## 三、渲染 — 天空与大气

### 3.1 程序化天空盒 (Procedural Skybox)

- **文件**: `shaders/sky.frag` (第 63–231 行)、`src/main.cpp` (skybox VAO + 渲染调度)

#### 原理

天空盒是渲染管线中最后一个 3D Pass（在场景之后、UI 之前），使用单位立方体 VAO + `glDepthFunc(GL_LEQUAL)` + `gl_Position.z = 1.0`（NDC 远平面）确保天空永远在所有场景几何之后。天空颜色完全由着色器程序生成，无任何纹理输入，真正零显存占用。

#### 渲染流程

```
Pass 3 — Skybox (模型渲染、粒子之后):
  1. glDepthFunc(GL_LEQUAL) — 只渲染深度为最远处的天空
  2. skyShader.Use()
  3. 绑定单位立方体 VAO (NDC [-1,1]³)
  4. glDrawArrays(GL_TRIANGLES, 0, 36)
  5. 片段着色器输入: TexCoords = 立方体顶点坐标 = 视线方向
```

#### 天空颜色组成

```
skyColor = 基础渐变(horizon → zenith)
         + sunHalo × sunVisibility
         + moonHalo × moonVisibility
         + 方块太阳(if inSunOuter)
         + 方块月亮(if isMoon)
         + 星星(if night && !inSun && !isMoon)
         + 体积云(if viewDir.y > 0.02)
```

#### 基础渐变

```glsl
float blend = clamp(viewDir.y, 0, 1);  // 仰角 → [0, 1]
vec3 skyColor = mix(uHorizonColor, uZenithColor, blend);
```

`uHorizonColor` 和 `uZenithColor` 由 C++ 根据时间实时插值传入，支持 白天→黄昏→夜晚 四个时段的 4 对颜色。

#### 日月光晕

太阳光晕使用双重幂函数模拟米氏散射：`pow(sunDot, 64) × 0.3`（宽泛光）+ `pow(sunDot, 256) × 0.5`（核心强光），颜色为暖黄白 `(1.0, 0.95, 0.8)`。可见性 = `clamp(sunDir.y × 10 + 1, 0, 1)` 使太阳落山时逐渐消散，雨天乘 `(1 - uRainIntensity × 0.9)` 压抑光晕。

月光晕同理但更弱（系数 0.2）、更冷（`0.6, 0.7, 0.9`）。

---

### 3.2 Minecraft 风格方块太阳/月亮

- **文件**: `shaders/sky.frag` (第 100–149 行)

#### 原理

使用天球坐标系定位日月——视线方向经 `uStarMatrix`（天球逆矩阵）变换回天球本地坐标系，再在 X 轴方向做包围盒检测。

#### 太阳：双层包围盒

```
外层包围盒: celestialPos.x > 0 && |celestialPos.y| < 0.05 && |celestialPos.z| < 0.05
内层包围盒:                    |celestialPos.y| < 0.043 && |celestialPos.z| < 0.043

外层 = 奶油色边框 (2.55, 2.35, 1.95)
内层 = 纯白核心 (3.0, 3.0, 3.0)  — 耀眼亮白
```

两层包围盒形成清晰的方块边界 + 奶油色边框装饰，接近 Minecraft 原版视觉。

#### 月亮：程序化陨石坑

```
月亮区域: celestialPos.x < 0 && |y| < 0.05 && |z| < 0.05
月面 UV: (celestialPos.yz / 0.05) × 0.5 + 0.5
网格量化: floor(moonUV × 8.0) → 8×8 网格
陨石坑 hash: hash(vec3(pixelGrid, 0.0))

亮度分档:
  hash > 0.7 → baseColor × 0.7  (暗色陨石坑)
  hash > 0.4 → baseColor × 0.85 (灰色月海)
  否则       → baseColor 0.6,0.65,0.7 (亮色高地)
```

月亮和太阳互斥（通过 `inSunOuter` / `isMoon` 条件和 if-else），同一视线方向不会同时出现两者。

---

### 3.3 程序化星场 (Star Field)

- **文件**: `shaders/sky.frag` (第 137–149 行)

#### 原理

星星仅在非日月区域 + `nightFade > 0` 时渲染。使用**网格 hash 量化**防闪烁——将 3D 天球坐标量化为 200×200×200 的整数格子（`floor(celestialPos × 200.0)`），每个格子 hash 出一个确定值。这保证了相机微动时星星不会因浮点量化而随机闪烁。

```
stars:
  gridPos = floor(celestialPos × 200.0)
  starValue = hash(gridPos)
  if starValue > 0.99:                          // 1% 概率 = 每帧约 800 颗星
    starIntensity = (starValue - 0.99) × 100    // 亮度随机变化
    starIntensity ×= nightFade                  // 夜幕加深
    starIntensity ×= clamp(viewDir.y × 5, 0, 1) // 地平线附近衰减为 0
    skyColor += starIntensity                   // 白色星光叠加
```

---

### 3.4 体积云光线步进 (Volumetric Cloud Raymarching)

- **文件**: `shaders/sky.frag` (第 151–229 行)

#### 原理

真 3D 光线步进体积渲染：从天空视线方向穿过 300–550m 高度的云层（200m 物理厚度），每步采样 3D 噪声密度，累积散射和消光。比简单的 2D 云纹理更真实——云有厚度、自阴影、银边效应。

#### 算法步骤

```
1. 计算视线与云层的交点:
   tMin = 300.0 / viewDir.y   (云底)
   tMax = 550.0 / viewDir.y   (云顶)
   仅当 viewDir.y > 0.02 时执行 (仰角太低跳过)

2. 16 步视步进:
   stepVec = (pMax - pMin) / 16
   currentPos = pMin + stepVec × jitter  // IGN 抖动

   每步:
     a. 归一化高度: h = (y - 300) / 250
     b. 密度 = getCloudDensity(pos, h, time)  // FBM 3D + 高度遮罩
     c. 光步进: 2 步 × 40m 向太阳方向行进
        lightDensity = 路径上的云密度累积
        transmittance = exp(-lightDensity × 0.03)  // 比尔-朗伯定律
     d. 散射光: S = sunColor × transmittance × phaseVal + ambientColor
     e. 消光: extinction = exp(-density × stepSize × 0.005)
     f. 累积: cloudColor += S × (1 - extinction) × (1 - totalAlpha)
        totalAlpha += (1 - extinction) × (1 - totalAlpha)
     g. 提前退出: if totalAlpha >= 0.99 break

3. 地平线消隐: totalAlpha ×= smoothstep(0.02, 0.15, viewDir.y)
4. skyColor = mix(skyColor, cloudColor, totalAlpha)
```

#### 密度生成 (getCloudDensity)

```
density(u, v, w, h, t):
  n = fbm3D(p × 0.001 + windOffset)        // 3D FBM 噪声
  mask = smoothstep(0, 0.1, h)             // 平坦的云底
       × smoothstep(1.0, 0.4, h)           // 逐渐消散的云顶
  threshold = mix(0.3, 0.2, uRainIntensity) // 雨天阈值更低 → 云更浓密
  return smoothstep(threshold, threshold+0.4, n × mask)
```

---

### 3.5 分形布朗运动噪声 (FBM 3D)

- **文件**: `shaders/sky.frag` (第 25–33 行)

#### 原理

FBM (Fractional Brownian Motion) 通过叠加多个不同频率和振幅的噪声层（倍频程 / octaves）来模拟自然界中的不规则纹理。每个后续层频率加倍、振幅减半，实现从大到小的细节覆盖。

#### 公式

$$\text{FBM}(\mathbf{p}) = \sum_{i=0}^{3} \frac{1}{2^{i+1}} \cdot \text{noise}(\mathbf{p} \cdot 2^i + \text{offset})$$

本项目使用 4 个倍频程：振幅 0.5, 0.25, 0.125, 0.0625（等比数列），频率 1×, 2×, 4×, 8×（逐层翻倍）。每次迭代后加空间偏移 `vec3(100.0)` 防止各层噪声在原点对称重叠。

#### 在项目中的应用

| 应用场景 | FBM 参数 | 效果 |
|---------|---------|------|
| 体积云密度 | 4 octaves, scale 0.001 | 蓬松不规则云朵 |
| 风向移动 | `uvw.x += time × 0.005` | 云随时间漂移 |

---

### 3.6 Henyey-Greenstein 相位函数

- **文件**: `shaders/sky.frag` (第 46–48 行)

#### 原理

HG 相位函数是描述光在参与介质（云、雾、大气）中散射方向分布的解析模型。它用单一参数 `g ∈ [-1, 1]` 控制散射的各向异性。

#### 公式

$$\Phi(\cos\theta, g) = \frac{1 - g^2}{(1 + g^2 - 2g\cos\theta)^{1.5}}$$

其中 `θ` 是视线方向与光照方向的夹角。

#### 参数含义

| g 值 | 散射类型 | 视觉效果 |
|------|---------|---------|
| `g = 1` | 纯前向散射 | 光束穿透介质 |
| `g = 0` | 各向同性散射 | 均匀漫散射 |
| `g = -1` | 纯后向散射 | 光线反射回光源 |

#### 本项目使用

```glsl
float phaseVal = mix(HG(cosTheta, 0.5), HG(cosTheta, -0.1), 0.5);
```

混合 50% 强前向散射（`g=0.5`，太阳方向的银边）和 50% 弱后向散射（`g=-0.1`，云底不显死黑），产生真实的云层光照感。

---

### 3.7 Interleaved Gradient Noise (IGN)

- **文件**: `shaders/sky.frag` (第 58–61 行)

#### 原理

IGN 是最廉价的蓝噪点生成器之一。蓝噪点的特点是高频分量远大于低频分量——分散均匀、不产生视觉上的"团块"。在体积云光线步进中，用 IGN 抖动起始采样位置来消除低步数产生的带状伪影（banding）。

#### 公式

```glsl
float getJitter(vec2 fragCoord) {
    return fract(52.9829189 × fract(dot(fragCoord, vec2(0.06711056, 0.00583715))));
}
```

- 外层的 `fract(dot(...))` 将像素坐标映射到一个伪随机值
- 内层的 `fract(...)` 是经典的 `sin(dot(...))` 变种，但使用常量 52.9829189 替代 `sin` 计算
- 输出在 [0, 1] 均匀分布且相邻像素差异大（蓝噪特性）

#### 性能优势

对比 `sin(dot(...))` 的随机哈希，IGN 不需要超越函数（`sin`），仅需乘法 + 点积 + `fract`，在 GPU 上快约 3–5 倍。

---

## 四、渲染 — 水面

### 4.1 有限差分法水面法线计算

- **文件**: `shaders/model.frag` (第 111–141 行 `calculateWaterNormal()`)、`src/main.cpp`（`uPackedNoiseMap` 绑定到纹理单元 12）

#### 原理

水面的视觉核心在于**动态法线扰动**——如果水面法线始终等于几何法线（朝上），反射将是完美的镜面平面，缺乏水的波动感。本项目从预烘焙的打包噪点贴图 `uPackedNoiseMap` 采样高度场，再用有限差分法实时计算法线的倾斜偏导数。

#### 噪声贴图结构

`uPackedNoiseMap` 是一张预生成的 RGBA 纹理：
- **R 通道** = 大波浪（最平滑的长波振荡）
- **G 通道** = 小波纹（细碎短波）

本项目**两层均采样 R 通道**（舍弃了 G 通道的细碎噪点），因为大波浪提供的平滑过渡更接近"镜面一般的温润倒影"。

#### 算法

```
1. 计算 UV (世界空间 XZ → 纹理坐标):
   uv = worldPos.xz × 0.06           // 拉伸 8 倍，使波浪极其庞大连绵

2. 两层波高采样:
   wave1 = texture(noiseMap, uv × 0.3 + time × (0.01, 0.015)).r
   wave2 = texture(noiseMap, uv × 0.7 - time × (0.015, 0.01)).r × 0.4
   height = wave1 + wave2

3. 有限差分求偏导数 (步长 eps = 0.03):
   hL = getWaterHeight(uv - (eps, 0), time)
   hR = getWaterHeight(uv + (eps, 0), time)
   hD = getWaterHeight(uv - (0, eps), time)
   hU = getWaterHeight(uv + (0, eps), time)

4. 法线向量:
   normalOffset = vec3(hL - hR,  2×eps,  hD - hU)
   waterNormal = normalize(vec3(normalOffset.x × 0.25,  1.0,  normalOffset.z × 0.25))
```

#### 参数调优逻辑

| 参数 | 值 | 设计意图 |
|------|-----|---------|
| UV 缩放 (0.06) | 8 倍于常规值 0.5 | 消除"锡纸折皱"感，形成大气磅礴的长浪 |
| 步长 (0.03) | 极微小 | 差分精度足够捕捉细微波浪 |
| 扰动强度 (0.25) | 远低于曾经的 0.6 | 反射只有平缓而悠扬的波动，像镜面般温润 |
| 两层波速 (0.01/0.015 vs 0.01/0.015) | 极慢 | 水流迟缓，看起来是湖面而非急流 |

---

### 4.2 Fresnel-Schlick 近似 (水面反射)

- **文件**: `shaders/model.frag` (第 297–321 行)

#### 原理

水面的物理反射率不是恒定值——随观察角度的变化，反射强度会发生剧烈改变。垂直看水面时反射极弱（透视清澈水底），视线越接近水平面反射越强（看到天空倒影）。Fresnel 效应描述了这一现象。

#### 公式 (Schlick 近似)

$$F(\theta) = F_0 + (1 - F_0) \cdot (1 - \cos\theta)^5$$

其中 `F₀ = 0.02`（水的基底反射率），`θ` = 视线与法线的夹角。

```
cosTheta = max(dot(viewDir, waterNormal), 0.0)
fresnel = 0.02 + 0.98 × pow(1 - cosTheta, 5.0)
```

#### 反射色来源

水面**不采样真实环境贴图**，而是直接使用 C++ 传入的昼夜动态天空色作为反射源：

```
viewReflect = reflect(-viewDir, waterNormal)
skyColor = mix(uHorizonColor, uZenithColor, clamp(viewReflect.y, 0, 1))
```

视线越平，越反射地平线色（白天淡蓝 `0.55,0.75,0.95`），往上看则反射天顶色（白天深蓝 `0.15,0.35,0.80`）。这保证了深夜的天空倒影自动变暗（C++ 侧 `uHorizonColor` 在夜晚已降为 `0.05,0.05,0.12`）。

#### 夜间抑制

水面在夜晚容易发出"荧光"感——即使天空很暗，Fresnel 反射仍可能使水面偏亮。为防止这一点，项目根据 `uNightFade` 动态调节：

| 参数 | 白天 | 夜晚 | 平滑过渡 |
|------|------|------|---------|
| 水底色 | 青蓝 `(0,0.35,0.5)` / `(0,0.12,0.25)` | 几乎黑 `(0.005,0.01,0.02)` / `(0.001,0.003,0.008)` | `mix(dayColor, nightColor, uNightFade)` |

---

### 4.3 太阳/月光水面闪烁 (Sun/Moon Glint)

- **文件**: `shaders/model.frag` (第 314–318 行)

#### 原理

水面闪烁是光源在水面上的镜面反射——当视线恰好与光源的反射方向对齐时，水面出现一道亮光（常见于清晨和黄昏的湖面）。该效果基于 Phong 镜面反射模型，但专门针对水面法线和光源方向。

#### 公式

```
反射向量: reflectDir = reflect(-normalize(uLightDir), waterNormal)
镜面因子: specFactor = pow(max(dot(viewDir, reflectDir), 0.0), shininessFactor)
闪烁颜色: waterGlint = uLightColor × specFactor × glintIntensity
```

#### 日夜差异化

太阳高光锐利而明亮（小光源角直径），月光高光柔和而暗弱（由散射光形成）：

| 参数 | 白天 (uNightFade=0) | 夜晚 (uNightFade=1) | 原理 |
|------|---------------------|---------------------|------|
| `shininessFactor` | 256 | 64 | 白天太阳点光源需要极锐利反射，夜晚月亮面光源需宽泛柔和 |
| `glintIntensity` | 8.0 | 0.4 | 太阳亮度远超月亮，反射强度差约 20 倍 |
| 过渡方式 | — | — | `mix(dayVal, nightVal, uNightFade)` |

---

## 五、渲染 — 天气系统

### 5.1 VBO-less 程序化雨滴 (Procedural Rain)

- **文件**: `shaders/rain.vert/frag`、`src/main.cpp` (第 2177–2235 行)

#### 原理

传统粒子系统需要为每个雨滴维护 CPU 侧的 VBO 数据（位置、速度、生命周期）。本项目采用**零顶点缓冲**方案——直接利用 OpenGL 的 `gl_VertexID` 生成雨滴几何体，所有数据在着色器中程序化计算。10000 条雨丝 = 20000 个 GL_LINES 顶点，显存占用为零。

#### 顶点着色器算法

```
每条雨丝的 dropID = gl_VertexID / 2  (0 ~ 9999)
每个顶点的 isTop   = gl_VertexID % 2  (0=底端, 1=顶端)

1. 为每滴雨生成唯一随机种子:
   rx = hash(dropID × 1.123)           // X 偏移 [0, 1]
   ry = hash(dropID × 2.345)           // Y 偏移 [0, 1]
   rz = hash(dropID × 3.456)           // Z 偏移 [0, 1]
   speed = mix(10, 25, hash(dropID × 4.567))  // 下落速度 10–25 m/s

2. 40m³ 相机跟随包围盒:
   localPos = (rx×40-20,  ry×40-20,  rz×40-20)

3. 时间驱动下落 + 风向偏移:
   fallDir = normalize(0.1, -1.0, 0.1)  // 略微倾斜
   localPos += fallDir × time × speed

4. Modulo 循环回绕 (掉出底部 → 回到顶部):
   localPos.y = mod(localPos.y + 20, 40) - 20
   (X/Z 同理, 防止相机移动时雨滴断层)

5. 雨丝拉伸:
   if (isTop == 1):
     localPos -= fallDir × dropLength    // 向上拉伸 0.2–0.5m
```

#### 片段着色器

颜色为浅蓝白 `(0.8, 0.9, 1.0)`，透明度 = `0.2 × uRainIntensity`（雨小时接近透明，雨大时不透明）。

---

### 5.2 雨滴遮挡剔除 (Rain Occlusion)

- **文件**: `shaders/rain.frag` (第 7–21 行)、`src/main.cpp` (第 1667–1675 行)

#### 原理

如果把一整盒雨滴均匀画在场景上，屋顶下方的雨滴仍然可见——这显然违背物理（人在屋檐下不应看到雨）。解决方案：借用 Shadow Map 硬件，额外渲染一张 1024×1024 的**雨深度图**，然后在雨的片段着色器中比较深度来 `discard` 被遮挡的雨丝。

#### 流程

```
Pass 1b — 雨遮挡深度图 (rainDepthMap, 1024×1024):
  1. 使用与 Shadow Map 相同的 shadowShader
  2. 仅渲染地图静态几何（跳过玩家/敌人/水面）
  3. 写入深度: gl_FragDepth = gl_FragCoord.z

Pass 5 — 雨滴渲染:
  FragPosRainSpace = uRainSpaceMatrix × worldPos  (顶点着色器传出的光源空间坐标)
  片段着色器:
    projCoords = FragPosRainSpace.xyz / FragPosRainSpace.w  // 透视除法
    projCoords = projCoords × 0.5 + 0.5                     // [0,1] 映射
    mapDepth = texture(uRainDepthMap, projCoords.xy).r
    if (currentDepth - 0.005 > mapDepth)  // 雨在房顶"后面"
        discard                           // 丢弃该雨丝片段！
```

偏置值 0.005 是必需的——防止因深度浮点精度导致雨丝在房顶表面闪烁。

---

### 5.3 雨湿润度与水坑系统 (Wetness & Puddles)

- **文件**: `shaders/model.frag` (第 155–185 行)

#### 原理

下雨不只是头顶增加的雨滴粒子——地面应该逐渐变潮湿、形成水坑（puddles），反射天空光。这个系统在 `model.frag` 中作用于所有受光表面，通过噪声生成水坑形状、动态调整材质属性。

#### 算法

```
1. 水坑噪声:
   n = noise2D(FragPos.xz × 0.3)       // 低频值噪声 → 水坑形状
   upFactor = clamp((normal.y - 0.8) × 5, 0, 1)  // 只有朝上的面才积水

2. 动态阈值 (雨越大水坑越多):
   puddleMask = smoothstep(0.6 - rain×0.3, 0.7 - rain×0.3, n)
   // 雨小时 threshold ≈ 0.6 → 只有少量噪声峰值形成水坑
   // 雨大时 threshold ≈ 0.3 → 一半以上面积形成水坑

3. 综合湿润度:
   wetness = clamp(rainIntensity × 0.3 + puddleMask × rainIntensity, 0, 1)
           × upFactor

4. 湿润引起的材质变化:
   albedoColor ×= mix(1.0, 0.5, wetness)        // 泥土吸水变深 50%
   shininess = mix(default, 256.0, wetness)      // 光滑积水
   specStrength = mix(default, 1.0, wetness)      // 满反射

5. 微表面涟漪扰动:
   if (wetness > 0.01):
     microNoise = noise2D(FragPos.xz × 15 + time × 2)
     normal.x += (microNoise - 0.5) × 0.03 × wetness
     normal.z += (microNoise - 0.5) × 0.03 × wetness
```

微表面涟漪是高频的（空间频率 15.0，时间频率 2.0），模拟雨滴砸在水坑上的微小波纹。扰动幅度仅 ±0.015，不会造成明显的法线变形，但足以打散浮点精度的阶梯条纹。

---

### 5.4 植被风力摇摆 (Wind Animation)

- **文件**: `shaders/model.vert` (第 39–51 行)、`shaders/g_buffer.vert` (第 21–27 行)

#### 原理

草地和树叶在风中的摆动是自然环境感的关键组成部分。在顶点着色器中做风摆——控制每个顶点的世界空间偏移量。

#### 算法

```
if (uMaterialType == 1 || uMaterialType == 2):  // 只对草和树叶启用
  windSpeed = 2.0 + uRainIntensity × 4.0         // 晴天 2.0, 暴雨 6.0
  windStrength = 0.05 + uRainIntensity × 0.05    // 晴天 0.05, 暴雨 0.10

  windWeight = (type==1) ? texCoords.y : 0.8
  // 草丛: 根部不摇 (y≈0 → weight=0)，顶部摆动最大
  // 树叶: 整体均匀摆动 (weight=0.8)

  worldPos.x += sin(worldPos.x×2 + time×windSpeed) × windStrength × windWeight
  worldPos.z += cos(worldPos.z×2 + time×windSpeed) × windStrength × windWeight
```

`sin` 和 `cos` 使用不同的空间轴（X 和 Z），防止同一方向上两个频率耦合形成肉眼可见的周期性条纹。雨天加速风以加重氛围。

#### 两个 Pass 的同步

G-Buffer 的 `g_buffer.vert` 包含完全相同的风摆代码（参数硬编码一致），否则 SSAO 采样位置和最终渲染位置不一致，出现错位遮蔽。

---

## 六、渲染 — GPU 蒙皮与粒子

### 6.1 GPU 线性混合蒙皮 (Linear Blend Skinning)

- **文件**: `shaders/model.vert` (第 25–31 行)

#### 原理

LBS 是现代实时渲染中骨骼动画的标准方法。每个顶点受最多 4 根骨骼影响，每根骨骼有自己的权重和变换矩阵。GPU 在顶点着色器中实时混合这些矩阵来计算最终的顶点位置。

#### 算法

```
boneTransform = finalBonesMatrices[boneIds[0]] × weights[0]
              + finalBonesMatrices[boneIds[1]] × weights[1]
              + finalBonesMatrices[boneIds[2]] × weights[2]
              + finalBonesMatrices[boneIds[3]] × weights[3]

worldPos = uModel × boneTransform × vec4(aPos, 1.0)
```

#### 数据流

```
CPU 侧 (Animation.cpp):
  Assimp 关键帧插值 → 每根骨骼的 Local Transform
  DFS 递归骨骼树 → 每根骨骼的 Global Transform
  FinalBoneMatrices[id] = Global × Offset
  → 上传到 uniform finalBonesMatrices[100]

GPU 侧 (model.vert):
  读取 boneIds[4] (int) + weights[4] (float)
  加权混合矩阵
  逐顶点执行世界变换
```

#### 法线变换

骨骼变换后的法线需经逆转置矩阵校正：

```glsl
FragNormal = mat3(transpose(inverse(modelBone))) × aNormal
```

否则非均匀缩放（如角色的某些关节缩放）会导致法线方向错误，产生奇怪的明暗。

---

### 6.2 Billboard 粒子系统

- **文件**: `src/main.cpp` (Particle 结构体 + 渲染循环约第 2100–2150 行)

#### 原理

粒子效果（死亡烟雾、横扫剑气）使用 billboard 技术——总是面对相机的 2D 方块，用 `glDepthMask(FALSE)` 禁用深度写入，防止粒子方块裁切场景几何。

#### 死亡烟雾 (DeathParticle)

```
触发条件: 敌人 Health ≤ 0
生成: 10 粒子/敌人
  位置: 敌人 Position + (±0.4, 0~1.5, ±0.4) 随机偏移
  速度: 随机散开方向
  纹理: generic_0–7.png (8 变体随机选取)
  寿命: 0.4s

每帧更新:
  life -= dt
  scale = 0.8 × (1.2 - life/0.4)  // 从 0.96 增长到 0.16
  alpha = life/0.4                 // 从 1.0 淡出到 0.0
```

#### 横扫剑气 (SweepParticle)

```
触发条件: 持剑攻击命中敌人
生成: 1 粒子
  位置: 相机前方 1.5m 处 (眼睛高度)
  纹理: sweep_0–7.png (8 帧精灵表)
  寿命: 0.15s

每帧更新:
  progress = 1 - life/0.15
  frameIndex = floor(progress × 8)  // 播放精灵表动画
  使用 uUvScale/uUvOffset 切分精灵表帧
```

---

### 6.3 TrueType 字体烘焙与渲染

- **文件**: `src/font_baker.c`、`src/FontRenderer.cpp`、`shaders/ui.frag` (第 17–21 行 `uIsFont` 分支)

#### 原理

中英文混杂游戏需要同时支持 ASCII 字符和 CJK 汉字（6000+ 常用字）。预烘焙方案：在启动时使用 `stb_truetype.h` 将所有需要的字符栅格化到一张超大纹理图中，运行时仅需从图集中查找 UV 坐标绘制。

#### 烘焙流程 (font_baker.c)

```
stbtt_PackFontRanges() × 3 次:
  Range 1: ASCII 32–126 (95 字符)
  Range 2: CJK 标点 0x3000–0x303F (64 字符)
  Range 3: CJK 统一汉字 0x4E00–0x9FFF (20992 字符)

总计: 21151 字符 → 8192×8192 GL_RED 单通道图集
```

#### 运行时渲染 (FontRenderer.cpp)

```
RenderText(shader, vao, "你好 World"):
  1. UTF-8 解码 → 码点列表: [0x4F60, 0x597D, 0x20, W, o, r, l, d]
  2. 逐字符:
     a. 查找 stbtt_packedchar 中的 UV 坐标 + 大小信息
     b. 计算屏幕空间 quad (x, y, w, h)
     c. shader.SetVec2("uUvOffset" + "uUvScale")  设置单个字的 UV
     d. glDrawArrays(GL_TRIANGLES, 0, 6)
  3. 设置 uIsFont=true → ui.frag 进入字体模式
     alphaVal = texture(uTexture, TexCoords).r      // Red 通道 = 字形
     if (alphaVal < 0.05) discard                    // 过滤空白
     FragColor = vec4(uFontColor, alphaVal)          // 染色输出
```

`GL_RED` 格式使 8192² 纹理仅占 64MB 而非 RGBA 的 256MB。

---

## 七、碰撞检测与物理

### 7.1 2D 空间哈希网格 (Spatial Hash Grid)

- **文件**: `src/CollisionWorld.cpp` (第 236–261 行 `BuildGrid()`)

#### 原理

在 XZ 平面上将三角形按照 5m×5m 的格子分桶。查询时只需检测查询范围内覆盖的格子——将 \$O(N)\$ 的遍历降到 \$O(k)\$（k 为范围内的三角形数，远小于 N）。

#### 数据结构

```
Grid: unordered_map<int64, vector<int>>
Key:  int64 = (uint64(uint32(cx)) << 32) | uint32(cz)

cx = floor(tri.minX / 5.0)、floor(tri.maxX / 5.0) 之间的每个格子
cz = floor(tri.minZ / 5.0)、floor(tri.maxZ / 5.0) 之间的每个格子
→ 同一三角形可能插入多个格子（跨格处理）
```

#### 查询

```
GetTrianglesNearXZ(x, z, radius):
  计算覆盖的格子范围: cxMin..cxMax, czMin..czMax
  thread_local unordered_set<int> visited 去重
  遍历每个格子 → 取出三角形索引 → 返回 const Triangle*
```

`thread_local visited` 避免了每次查询的堆分配开销。

---

### 7.2 Möller–Trumbore 射线-三角形求交

- **文件**: `src/CollisionWorld.cpp` (第 324–376 行 `Raycast()`)

#### 原理

MT 算法是射线-三角形求交的黄金标准——将射线方向代入三角形的重心坐标公式，一次求解得到三个值：(t, u, v) = 交点距离 + 重心坐标。它免去了先求平面交点再判断是否在三角形内的两步骤开销。

#### 算法

本项目使用 `glm::intersectRayTriangle(start, dir, v0, v1, v2, bary, distance)`，内部实现即 MT 算法：

```
射线: P(t) = start + t × dir
三角形: P(u,v) = v0 + u×(v1-v0) + v×(v2-v0)

联立: start + t×dir = v0 + u×e1 + v×e2
→ 3×3 线性方程组
→ Cramer 法则求解 (t, u, v)

命中条件: t > 0 && u ≥ 0 && v ≥ 0 && u + v ≤ 1
```

#### 性能优化

```
1. 射线 XZ 包围盒 → 计算覆盖的网格范围 (cxMin..cxMax, czMin..czMax)
2. thread_local unordered_set 去重
3. 遍历每个候选三角形 → MT 检测
4. 只取最近命中点 (distance < closestDist)
```

---

### 7.3 边函数法点-三角形测试 (Edge Function)

- **文件**: `src/CollisionWorld.cpp` (第 88–109 行 `PointInTriangleXZ()`)

#### 原理

给定三角形的三条边，点 p 在三角形内部的条件是 p 在三边同侧（顺时针或逆时针）。边函数计算 2D 叉积的符号：

```
edge(p, e1, e2) = (px - e1x)(e2z - e1z) - (pz - e1z)(e2x - e1x)

e0 = edge(p, A, B)  // 对边 AB
e1 = edge(p, B, C)  // 对边 BC
e2 = edge(p, C, A)  // 对边 CA

条件: (e0 ≥ -ε && e1 ≥ -ε && e2 ≥ -ε)  或  (e0 ≤ ε && e1 ≤ ε && e2 ≤ ε)
```

允许 `ε = 1e-4`（0.1mm 浮点容差）使点恰好落在三角形边缘时也判为命中，防止站在两三角形缝隙处漏判。

---

### 7.4 三角形高度插值 (TriangleHeightAtXZ)

- **文件**: `src/CollisionWorld.cpp` (第 114–127 行)

#### 原理

给定世界空间的 XZ 坐标，求解该点在地图三角形平面上的精确 Y 值。这是地板检测的核心——角色必须站在三角形表面上，Y 坐标由平面方程求解得到。

#### 公式

平面方程: $N \cdot (P - P_0) = 0$

展开:

$$N_x(x - v_{0x}) + N_y(y - v_{0y}) + N_z(z - v_{0z}) = 0$$

解出 y:

$$y = v_{0y} - \frac{N_x(x - v_{0x}) + N_z(z - v_{0z})}{N_y}$$

前置条件：
- `N.y > 0.1`（只有朝上的面才能作为地板）
- 点 (x,z) 必须先通过 `PointInTriangleXZ()` 验证在三角形投影内

---

### 7.5 三角形 XZ 投影最近点 (ClosestPointOnTriangle2D)

- **文件**: `src/CollisionWorld.cpp` (第 47–83 行)

#### 原理

求解点 p 到三角形 XZ 投影的最近距离及对应的最近点。用于角色行走时精确计算到墙壁的距离。

#### 算法

```
1. 重心坐标判断法:
   求解 v0 + u×v1 + w×v2 = p (在 XZ 平面)
   如果 u≥0 && w≥0 && u+w≤1 → p 在三角形内, 最近点即 p 自身

2. 点在外面 → 取三条边上最近的点:
   cpAB = ClosestPointOnSegment2D(p, A, B)
   cpBC = ClosestPointOnSegment2D(p, B, C)
   cpCA = ClosestPointOnSegment2D(p, C, A)
   返回距离最小的那个
```

---

### 7.6 点到线段 XZ 投影最近点

- **文件**: `src/CollisionWorld.cpp` (第 24–42 行)

#### 原理

$$\text{closest} = A + \text{clamp}\left(\frac{(p - A) \cdot (B - A)}{|B - A|^2},\ 0,\ 1\right) \cdot (B - A)$$

夹紧参数 `t ∈ [0, 1]` 确保最近点落在 AB 线段上而不是其延长线上。

---

### 7.7 圆柱体角色控制器 (Cylinder Character Controller)

- **文件**: `src/Player.cpp` (第 56–200 行 `UpdatePhysics()`)、`src/Enemy.cpp` (第 217–349 行)

#### 原理

角色物理用圆柱体近似（半径 0.4m，高度 1.8m）。每帧执行 6 个步骤：

#### 步骤详述

```
步骤 1: 重力积分 + 水平冲量阻尼
  Vy += -9.8 × dt                          // 恒定重力加速度
  Position += Velocity × dt                 // 欧拉积分
  Vx *= 0.1; Vz *= 0.1                     // 水平冲量快速衰减（非累积速度）

步骤 2: 地板检测 (Y 轴)
  floorQueryRadius = max(0.4, 2.0)         // 至少 2m 范围找地板
  GetTrianglesNearXZ → 过滤 N.y > 0.1 的三角
  fallCompensation = max(0, -Vy × dt)      // 高速下落时向上扩大搜索范围
  searchTop = Pos.y + 0.5 + fallCompensation
  floorY = max(TriangleHeightAtXZ(x, z, tri)) // 最高地板面
  
  if Pos.y ≤ floorY:
    Pos.y = floorY; Vy = 0; isGrounded = true

步骤 3: 水平墙壁推斥 (XZ 平面)
  queryRadius = 0.4 + 0.1                  // 检查角色边缘 0.1m 外
  过滤条件:
    - |N.y| < 0.1                           // 竖直墙壁
    - |N.x| > 0.9 或 |N.z| > 0.9           // 轴对齐 (Minecraft 方块墙)
    - tri.maxY > Pos.y + 0.6               // 排除低矮台阶 (跨步容差)
    - tri.maxY ≥ Pos.y && tri.minY ≤ Pos.y + 1.8  // 在圆柱高度内
  
  最近矩形面点:
    法线平行 X → closestX = tri.v0.x, closestZ = clamp(Pos.z, tri.minZ, tri.maxZ)
    法线平行 Z → closestX = clamp(Pos.x, tri.minX, tri.maxX), closestZ = tri.v0.z
  
  推斥力: 沿最近点→圆心方向, 距离 = (半径 - dist) + 圆弧滑出处理

步骤 4: 跌落保护
  if Pos.y < -50: reset to (0, 50, 0)

步骤 5: 调试打印 (每 60 帧)

步骤 6: 视觉 Y 偏移平滑
  m_VisualYOffset = mix(current, target, 10×dt)
  潜行: target = -0.08m, 站立: target = 0.0m
```

---

### 7.8 抛物线箭矢物理

- **文件**: `src/Arrow.cpp`

#### 原理

箭矢遵循理想抛物线运动：水平匀速 + 垂直重力加速。使用 `CollisionWorld::Raycast` 检测每一帧的位移路径。

#### 算法

```
每帧 UpdatePhysics(dt):
  if (IsStuck) return                          // 已插墙, 只倒计时

  1. 玩家命中检测:
     distXZ < 1.0m && Pos.y ∈ [player.y, player.y+1.8] && playerImmunity≤0
     → HP -= 2, immunityTimer = 0.5s, Life = 0

  2. 重力积分:
     Vy += -9.8 × dt
     stepVec = Velocity × dt
     dir = normalize(stepVec)

  3. 碰撞检测:
     Raycast(Position, dir, stepDist, hitDist)
     if hit → Position += dir × hitDist; IsStuck = true; 播放音效
     else   → Position += stepVec; 更新 Yaw/Pitch 指向运动方向

  4. 生命周期: Life -= dt; 超过 10s 自动销毁
```

飞行姿态 (`Yaw`, `Pitch`) 实时根据速度方向计算（`atan2(-dir.x, -dir.z)` 和 `asin(dir.y)`），使箭在飞行中始终指向其运动轨迹。

---

## 八、动画系统

### 8.1 TRS 关键帧插值 (Position / Rotation / Scale)

- **文件**: `src/Animation.cpp` (第 53–155 行 `Bone::Update()`)

#### 原理

从 Assimp 导入的骨骼动画存储为离散的关键帧序列，运行时需要在这些关键帧之间平滑插值。TRS 分别代表平移 (Translation)、旋转 (Rotation)、缩放 (Scale) 三种变换，每种使用不同的插值策略。

#### 插值方法

$$
\text{平移}: \quad P(t) = \text{lerp}(P_i, P_{i+1}, \alpha)
$$

$$
\text{旋转}: \quad Q(t) = \text{slerp}(Q_i, Q_{i+1}, \alpha),\quad \alpha = \frac{t - t_i}{t_{i+1} - t_i}
$$

$$
\text{缩放}: \quad S(t) = \text{lerp}(S_i, S_{i+1}, \alpha)
$$

旋转使用**球面线性插值 (slerp)** 而非线性插值——slerp 保证四元数在单位球面上的最短弧上匀速插值，无速度突变。

#### 回退机制

如果 Assimp 导出的动画缺少某些通道（例如只有旋转没有平移），系统回退到绑定姿态：
- 无 `mPositionKeys` → 使用 `m_DefaultPosition`（从 aiNode 分解而来）
- 无 `mScalingKeys` → 使用 `m_DefaultScale`

这防止了关节因缺数据而脱落归零（`(0,0,0)`）。

#### 最终局部变换

`LocalTransform = Translation × Rotation × Scale`（TRS 顺序，符合大多数建模软件约定）

---

### 8.2 双轨动画混合 (Base + Overlay)

- **文件**: `src/Animation.cpp` (第 433–535 行 `Animator::UpdateAnimation()` + `CalculateBoneTransform()`)

#### 原理

一段动画很难同时驱动移动和攻击——走路时上半身和下半身都在动，但攻击时我们希望腿继续走、手执行攻击动作。双轨混合：Base 轨道驱动全身（行走 idle/walk/run），Overlay 轨道仅覆盖特定骨骼（手臂攻击）。

#### 混合逻辑

```
对每个骨骼节点:
  1. 从 Base 动画获取骨骼的 Local Transform
     如果该骨骼还不在 appliedBones 中 → appliedBones.insert(name)

  2. 从 Overlay 动画查找同名骨骼:
     if Overlay 中存在该骨骼:
       overlayTransform = Overlay.Bone.Update(overlayTime)
       // 矩阵列注入: 防止手臂脱臼
       overlayTransform[3] = node->transformation[3]  // 平移列从绑定姿态恢复
       finalNodeTransform = overlayTransform           // Overlay 接管此骨骼
```

#### Overlay 生命周期

```
PlayOverlay(anim):
  m_OverlayAnimation = anim
  m_OverlayTime = 0.0

每帧 UpdateAnimation:
  m_OverlayTime += dt × TPS
  if m_OverlayTime >= Duration:
    m_OverlayAnimation = nullptr  // 播放完毕, 自动回到纯 Base
```

---

### 8.3 递归骨骼层级变换 (Recursive Bone Hierarchy)

- **文件**: `src/Animation.cpp` (第 466–535 行 `CalculateBoneTransform()`)

#### 原理

骨骼动画的层级本质：子骨骼的全局位置 = 父骨骼的全局位置 × 子骨骼的局部变换。从根节点开始 DFS 遍历整个骨架树。

#### 算法

```
DFS(node, parentGlobal):
  1. 计算本节点的局部变换: baseTransform = Bone.Update(time)
  2. 如果有 Overlay: 尝试用 Overlay 的骨骼覆盖 localTransform[3] (平移列注入)
  3. 如果是 Head 节点: 叠加 Yaw + Pitch 旋转; if m_HideHead: scale = 0.0001
  4. globalTransform = parentGlobal × finalNodeTransform
  5. 缓存: m_NodeGlobalTransforms[name] = globalTransform
  6. 如果有对应的 BoneInfo: FinalBoneMatrices[id] = globalTransform × offset
  7. 递归处理所有子节点
```

`offset` 矩阵是 Assimp 提供的逆绑定姿态矩阵，将网格顶点从模型空间变换到骨骼局部空间。`Global × Offset` 得到的是骨骼动画后的模型空间顶点偏移。

---

### 8.4 头部旋转与隐藏

- **文件**: `src/Animation.cpp` (第 460–517 行 `SetHeadRotation()` + `CalculateBoneTransform()` 中 isHead 分支)

#### 原理

第一人称视角需要隐藏头部（防止模型头发遮挡视线），且头部需要跟随鼠标 Pitch/Yaw 旋转（使角色的视线方向与玩家的视角方向一致）。通过字符串匹配找到 Head 节点并叠加额外旋转。

```
if nodeName contains "Head" or "head":
  finalTransform = rotate(finalTransform, m_HeadYaw,   Y轴)
  finalTransform = rotate(finalTransform, clamp(m_HeadPitch, -89.9, 89.9), X轴)

  if m_HideHead:
    finalTransform = scale(finalTransform, vec3(0.0001))  // 几乎不可见
```

---

### 8.5 动画名标准化

- **文件**: `src/Animation.cpp` (第 163–340 行 `LoadAll()`)

#### 问题

Blender 导出的 FBX 动画名可能带有骨架前缀（如 `Armature|walk`）或自动编号后缀（如 `idle.001`）。不同骨架的同一个动画必须映射到同一个键名才能正确查找。

#### 标准化步骤

```
原始名: "Armature|Walk.001"

1. 剥离骨架前缀:   split('|') → "Walk.001"
2. 剥离 .NNN 后缀:  rfind('.') → 检测是否为纯数字 → "Walk"
3. 转小写:         → "walk"
```

Key 冲突时覆盖并打日志警告。

---

## 九、AI 系统

### 9.1 僵尸 AI 有限状态机

- **文件**: `src/Enemy.cpp` (第 141–170 行 `UpdateAI()` 中 ZOMBIE 分支)

#### 原理

僵尸 AI 是一维距离驱动的三态 FSM：

```
                 > 15m
  ┌──── idle ──────────────────────────────┐
  │       │                                │
  │   ≤15m│                                │ >15m
  │       ▼                                │
  └──→ walk (速度 2.5, 追击玩家) ──────────┘
  │       │
  │  ≤1.5m│
  │       ▼
  └──→ 咬击 (HP-2 + 击退, 1.5s CD)

附加: 如果被墙壁阻挡(IsBlockedHorizontally) && 在地面 → Vy = 5.0 跳跃
```

#### 攻击判定

僵尸的咬击在 `main.cpp` 的 processInput 循环中检测：距离 ≤ 1.5m + `AttackCooldown ≤ 0` → `HP -= 2` + 击退 + `playerImmunityTimer = 0.5s` + `AttackCooldown = 1.5s`。

---

### 9.2 骷髅 AI 有限状态机

- **文件**: `src/Enemy.cpp` (第 172–212 行 `UpdateAI()` 中 SKELETON 分支)

#### 原理

骷髅是远程 AI，行为更复杂——不能无限逼近玩家，需要在射击距离内保持优势位置。

```
                 > 15m
  ┌──── idle ───────────────────────────────┐
  │       │                                 │
  │   ≤15m│                                 │ >15m
  │       ▼                                 │
  │   facePlayer + play walk                │
  │       │                                 │
  │  < 8m │           8–15m                 │
  │       ▼            ▼                    │
  │  后退风筝       站立射击                │
  │  (80% 移速)    (4s cooldown)            │
  │  + 射击        SpawnEnemyArrow()        │
  └─────────────────────────────────────────┘
```

骷髅的双轨动画通过 Overlay 实现：Base 是 walk，Overlay 是 attack（仅手臂拉弓动画），实现边走边射或站立射击。

---

### 9.3 受击硬直与击退

- **文件**: `src/Enemy.cpp` (第 117–123 行 `TakeDamage()`)

#### 原理

敌人受到攻击后进入短暂硬直状态，停止 AI 寻路但继续受物理系统控制（击飞轨迹是纯物理的抛物线）。

```
TakeDamage(damage, knockback):
  Health -= damage
  Velocity = knockback         // 强制覆盖速度 (击飞冲量)
  IsGrounded = false            // 离地进入抛射状态
  StunTimer = 1.0               // 1 秒硬直

UpdateAI (每帧):
  if StunTimer > 0:
    StunTimer -= dt
    只更新动画 (m_Animator.UpdateAnimation)
    return                     // 跳过所有 AI 逻辑和移动
```

击退向量（来自 `main.cpp` 的攻击判定）：`frontDir × 5.0 + (0, 4.5, 0)`（水平 5m/s + 垂直 4.5m/s 上抛）。

---

## 十、游戏系统

### 10.1 玩家 5 状态有限状态机

- **文件**: `src/Player.cpp` (第 209–281 行 `UpdateMovementState()`)

#### 状态转换规则

```
            Sneak + Moving → SNEAK_WALK (2.0)
  WALK ─────────────────────────────────────
  (4.0)   Eating + Moving → WALK (1.6)
   │      Ctrl+W + Forward → RUN (8.0)
   │      S/A/D without W  → WALK (4.0)
   │      停止移动          → IDLE (0.0)
   ▼                                           Sneak + Stop → SNEAK_IDLE (0.0)
```

核心约束：疾跑模式（`IsRunningMode=true`）只在**按住 W** 时生效。若按 S/A/D 或停止移动，即使 `IsRunningMode=true` 也不进入 Run 状态。

#### FOV 联动

```
疾跑激活 → targetFOV = 55° (广角增强速度感)
否则      → targetFOV = 45° (默认)
currentFOV = mix(currentFOV, targetFOV, dt × 8.0)  // 8.0 速度平滑过渡
```

---

### 10.2 战斗射线检测 (Slab AABB Raycasting)

- **文件**: `src/main.cpp` (processInput 中 Left Mouse 逻辑)

#### 原理

从玩家眼睛位置沿相机前方方向发射射线，用 Slab（Slabs method）算法检测 AABB 相交——将 3D 射线分别向 X、Y、Z 三轴投影，求三个区间的交集。

#### 算法

```
射线: P(t) = playerPos + cameraEyeOffset + t × cameraFront
最大范围: 3.5m

对每个敌人:
  AABB: [Pos.x-0.4, Pos.x+0.4] × [Pos.y, Pos.y+1.8] × [Pos.z-0.4, Pos.z+0.4]

  Slab 算法:
    对每个轴 (X, Y, Z):
      t1 = (min - origin) / dir
      t2 = (max - origin) / dir
      tNear = max(tNear, min(t1, t2))
      tFar  = min(tFar,  max(t1, t2))

    if tNear ≤ tFar && tFar > 0:
      命中! 取 tNear 最小的敌人
```

---

### 10.3 横扫之刃 AOE (Sweeping Edge)

- **文件**: `src/main.cpp` (processInput 中攻击逻辑)

#### 原理

仅在持剑（hotbar slot 0）时触发——对主目标身后的其他敌人造成 AOE 伤害。

```
触发条件:
  - 武器类型 = 剑
  - 主目标命中

AOE 判定:
  对每个其他敌人:
    horizontalDist < 3.0m && dot(frontDir, toEnemy) > 0.707  // 90° 锥形

伤害:
  主目标: 7  (剑) 或 1 (空手)
  AOE:   3  (仅剑)
```

---

### 10.4 生命值与进食系统

- **文件**: `src/main.cpp` (HP 逻辑 + eatingTimer + 热键槽 3)

#### 生命值

```
maxHP = 20 → 显示 10 颗心 (满/半/空)
心纹理: textures/UI/heart/full.png, half.png, empty.png
伤害: 僵尸咬伤-2, 箭矢-2, 免疫期 0.5s
死亡: HP ≤ 0 → 重生 (-20, -7, -13), 全 HP 恢复
```

#### 进食

```
条件: 热键槽 3 (面包) 被选中 && HP < 20
触发: 按住右鼠标键 1.5s
效果: HP += 4 (上限 20)
动画: 物品摇动 + 倾斜 + 拉向相机
中断: 松开右键 → eatingTimer 重置
```

---

### 10.5 故事导演 (6 阶段线性脚本)

- **文件**: `src/main.cpp` (StoryState 枚举 + 主循环中的 StoryDirector)

#### 阶段流程

```
WANDERING ──(30s 提示 + 玩家靠近家 4m)──→ CUTSCENE
CUTSCENE ──(黑屏+钟声+强制夜晚+雨+对话)──→ GOTO_GATE
GOTO_GATE ──(玩家走到东门 X:47-51, Z:-9~0)──→ DEFENDING
DEFENDING ──(消灭所有敌人 enemies.empty())──→ ENDING
ENDING ──(3s 延迟+对话+6s 延迟)──→ COMPLETED
COMPLETED (自由探索, 无更多事件)
```

#### 过场效果

```
黑屏: blackScreenAlpha 从 0 → 1 (2s 淡入)
钟声: ma_engine_play_sound("audio/Bell_use1.mp3") × 5 次, 3s 间隔
时间: timeOfDay = 4.71 (270° = 午夜)
天气: rain on + 强制目标雨量
对话: ChatLine 系统, t=11s 和 t=16s 各一条
屏幕淡出: blackScreenAlpha 从 1 → 0 (6–8s)
```

---

### 10.6 摄像机模式切换

- **文件**: `src/ThirdPersonCamera.cpp`、`src/main.cpp` (F5 键处理)

#### 三种模式

| 模式 | 视角位置 | 特点 |
|------|---------|------|
| FirstPerson | 玩家眼睛 `Pos + (0, 1.75, 0)` | 隐藏头部, FP 手臂渲染在单独 Pass |
| ThirdPersonBack | 玩家背后 `Distance=5.0m` | 标准越肩视角 |
| ThirdPersonFront | 玩家前方 | 面朝玩家的正面视角 |

#### 参数

```
F5 循环切换: FirstPerson → ThirdPersonBack → ThirdPersonFront → FirstPerson...
Pitch 限制: [-89°, 89°] (防止万向锁)
FOV: 动态 45° → 55° (疾跑时)
遮挡检测: 第三人称时相机与玩家之间有物体会拉近
```

---

### 10.7 聊天/消息系统

- **文件**: `src/main.cpp` (ChatLine 结构体 + chatLog/chatHistory 容器)

#### 数据结构

```
ChatLine { std::string text; float lifeTime; }

chatLog:     最多 6 条可见, lifeTime 初始 5.0s
chatHistory: 最多 100 条, 永久保存

显示:
  chatLog → 底部左侧逐行叠加, alpha = lifeTime / 5.0 (线性淡出)
  T 键 → 打开半透明黑色历史面板, 显示最近 12 条 chatHistory
```

---

## 十一、音频

### 11.1 miniaudio 音频引擎

- **文件**: `src/Audio.cpp`（`#define MINIAUDIO_IMPLEMENTATION`）、`src/miniaudio.h`（单头文件库）

#### 原理

miniaudio 是一个单头文件 C 音频库，零外部依赖。本项目的音频需求极简——只需要播放 mp3 音效文件——因此使用 miniaudio 的高层 API `ma_engine_play_sound()` 即可。

#### API

```cpp
ma_engine audioEngine;
ma_engine_init(NULL, &audioEngine);
ma_engine_play_sound(&audioEngine, "audio/Sweep_attack1.mp3", NULL);
```

#### 编译隔离

`miniaudio.h` 的 `MINIAUDIO_IMPLEMENTATION` 单独放在 `Audio.cpp` 中，避免 `main.cpp` 每次修改都重编译这个大型库（miniaudio 单文件约 70k 行）。

#### 音效映射

| 事件 | 文件 |
|------|------|
| 挥剑 | `audio/Sweep_attack1.mp3` |
| 空手挥击 | `audio/Weak_attack1.mp3` |
| 僵尸受伤 | `audio/Zombie_hurt{1,2}.mp3` |
| 僵尸死亡 | `audio/Zombie_death.mp3` |
| 骷髅受伤 | `audio/Skeleton_hurt1.mp3` |
| 骷髅死亡 | `audio/Skeleton_death.mp3` |
| 箭击中 | `audio/Arrow_hit1.mp3` |
| 射箭 | `audio/Bow_shoot.mp3` |
| 故事钟声 | `audio/Bell_use1.mp3` |

---

## 十二、数学与工具函数

### 12.1 Assimp 矩阵转换

- **文件**: `src/Model.h` (第 19–27 行 `AiMatrixToGlm()`)

#### 原理

Assimp 使用行优先矩阵存储（`a1..a4, b1..b4, c1..c4, d1..d4`），GLM 使用列优先矩阵。转换需要手动做 4×4 转置。

```
Assimp:                           GLM:
  row 0: a1, a2, a3, a4            col 0: a1, b1, c1, d1
  row 1: b1, b2, b3, b4    →       col 1: a2, b2, c2, d2
  row 2: c1, c2, c3, c4            col 2: a3, b3, c3, d3
  row 3: d1, d2, d3, d4            col 3: a4, b4, c4, d4
```

这等价于：

$$to[i][j] = from[j][i]$$

（将 Assimp 的 `[行][列]` 访问转换为 GLM 的 `[列][行]` 存储）

---

### 12.2 颜色空间与亮度计算 (BT.709 Luma)

- **文件**: `shaders/blur.frag`、`shaders/hdr_compose.frag`、`shaders/model.frag`

#### 公式

$$Y = 0.2126R + 0.7152G + 0.0722B$$

BT.709 是 HDTV 的标准亮度系数。绿色权重 (0.7152) 远大于红蓝，因为人眼视网膜中 60% 以上的视锥细胞对绿光敏感。

#### 在本项目中的应用

| 位置 | 用途 |
|------|------|
| `blur.frag` Bright-Pass | 判断像素是否"足够亮"参与 Bloom |
| `hdr_compose.frag` 饱和度 | 提取亮度用于色度缩放 |
| `model.frag` | 间接使用（作为色调映射前的亮度参考） |

---

### 12.3 伪随机哈希函数族

- **文件**: `shaders/model.frag` `rand()` + `hash2D()`、`shaders/sky.frag` `hash3D()` + `hash()`、`shaders/rain.vert` `hash()`

#### 1D Hash

```glsl
float hash(float n) { return fract(sin(n) × 43758.5453123); }
```

经典的 `sin` 哈希——利用正弦函数的高频振荡性和大常数乘法来破碎输入，`fract` 提取小数部分作为 0–1 伪随机值。适用于雨滴随机属性生成。

#### 2D Hash

```glsl
float hash2D(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) × 43758.5453); }
```

将 2D 坐标投影到 1D 方向（通过 `dot`），然后做 1D hash。用于 PCF 软阴影的随机旋转角。

#### 3D Hash

```glsl
float hash3D(vec3 p) {
    p = fract(p × vec3(12.9898, 78.233, 37.719));
    p += dot(p, p.yxz + 34.19);
    return fract(p.x × p.y × p.z);
}
```

破碎输入 → 自馈扰动 → 三元素乘积分枝，用于体积云密度噪声的基底。

---

### 12.4 2D 值噪声 (Value Noise + Smoothstep 插值)

- **文件**: `shaders/model.frag` (第 101–108 行 `noise2D()`)

#### 原理

经典 Perlin 风格的栅格噪声：将空间划分为整数格子，每个格子顶点生成伪随机值，点 p 的值 = 四个顶点值的双线性插值（使用 smoothstep 作为 fade 曲线）。

#### 算法

```
1. 栅格化: i = floor(p), f = fract(p)
2. Smoothstep fade: u = f²(3 - 2f)  // 消除栅格接缝
3. 双线性插值:
   n00 = hash(i + (0,0))
   n10 = hash(i + (1,0))
   n01 = hash(i + (0,1))
   n11 = hash(i + (1,1))
   
   nx0 = mix(n00, n10, u.x)
   nx1 = mix(n01, n11, u.x)
   result = mix(nx0, nx1, u.y)
```

用 smoothstep 替代线性 fade 是关键——线性 fade 在格子边界处导数不连续，会产生肉眼可见的菱形栅格。smoothstep 保证 C¹ 连续性，消除人工痕迹。

#### 应用

地面水坑形状生成：`noise2D(FragPos.xz × 0.3)`，频率 0.3 产生大块的不规则水坑轮廓。

---

### 12.5 3D 值噪声 (Value Noise + Smoothstep 插值)

- **文件**: `shaders/sky.frag` (第 17–23 行 `noise3D()`)

#### 原理

2D 值噪声的三维推广——三线性插值 8 个栅格顶点：

```
n000 = hash3D(i + (0,0,0))      n100 = hash3D(i + (1,0,0))
n010 = hash3D(i + (0,1,0))      n110 = hash3D(i + (1,1,0))
n001 = hash3D(i + (0,0,1))      n101 = hash3D(i + (1,0,1))
n011 = hash3D(i + (0,1,1))      n111 = hash3D(i + (1,1,1))

→ X 轴 4 次插值 → Y 轴 2 次插值 → Z 轴 1 次插值
```

作为 FBM 3D 的基础噪声函数，驱动体积云的密度场。
