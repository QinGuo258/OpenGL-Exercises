# 渲染效果调参指南

> **两种调参方式**：
> - **F1 热加载**：修改 `shaders/tuning.json` 中的数值 + 按 F1 → **立即生效，无需编译**
> - **F1 热加载**：修改 `shaders/*.frag` / `shaders/*.vert` GLSL 源码 + 按 F1 → 立即生效
> - **重新编译**：修改 `src/main.cpp` 中未被 `tuning.json` 覆盖的 C++ 参数需重编译

> `tuning.json` 中可调的参数在下方表格中以 ★ 标注。

---

## tuning.json 热重载参数列表

以下参数修改后按 **F1** 即时生效，无需编译：

| 参数名 | 默认值 | 类型 | 说明 |
|--------|--------|------|------|
| `dayLight` | `[1.3, 1.15, 0.95]` | vec3 | 日光颜色 |
| `sunsetLight` | `[1.0, 0.4, 0.1]` | vec3 | 日落光色 |
| `moonLight` | `[0.01, 0.02, 0.04]` | vec3 | 月光颜色 |
| `dayAmbient` | `[0.25, 0.35, 0.50]` | vec3 | 白昼环境光 |
| `nightAmbient` | `[0.02, 0.02, 0.06]` | vec3 | 夜环境光 |
| `dayHorizon` | `[0.55, 0.75, 0.95]` | vec3 | 白天地平线色 |
| `dayZenith` | `[0.15, 0.35, 0.80]` | vec3 | 白天天顶色 |
| `sunsetHorizon` | `[1.0, 0.45, 0.2]` | vec3 | 黄昏地平线 |
| `sunsetZenith` | `[0.3, 0.1, 0.35]` | vec3 | 黄昏天顶 |
| `nightHorizon` | `[0.05, 0.05, 0.12]` | vec3 | 夜景地平线 |
| `nightZenith` | `[0.0, 0.0, 0.02]` | vec3 | 夜景天顶 |
| `bloomIntensity` | `0.35` | float | Bloom 混合强度 |
| `exposure` | `0.5` | float | HDR 曝光度 |
| `godrayWeight` | `0.015` | float | 上帝光整体强度 |
| `godrayDensity` | `1.0` | float | 上帝光拉扯距离 |
| `godrayDecay` | `0.92` | float | 上帝光衰减速度 |
| `ssrMaxDistance` | `15.0` | float | SSR 光线最大距离 |
| `ssrRaySteps` | `40` | int | SSR 步进步数 |
| `ssrRefinementSteps` | `4` | int | SSR 二分精炼次数 |
| `specularStrength` | `0.5` | float | 镜面高光强度 |
| `shininess` | `32.0` | float | 高光锐利度 |
| `defaultFov` | `45.0` | float | 默认 FOV |
| `sprintFov` | `55.0` | float | 疾跑 FOV |

---

## 一、天空与云层

### 天空颜色 ★

> 以下 6 组色值均可通过 `tuning.json` + F1 热重载，无需编译。

| 参数 | tuning.json 键名 | 说明 |
|------|-----------------|------|
| 白天地平线色 | `dayHorizon` | 晴天地平线淡蓝 `[0.55,0.75,0.95]` |
| 白天天顶色 | `dayZenith` | 天顶深蓝 `[0.15,0.35,0.80]` |
| 黄昏地平线色 | `sunsetHorizon` | 橙红 `[1.0,0.45,0.2]` |
| 黄昏天顶色 | `sunsetZenith` | 紫色 `[0.3,0.1,0.35]` |
| 夜景地平线色 | `nightHorizon` | 深灰蓝 `[0.05,0.05,0.12]` |
| 夜景天顶色 | `nightZenith` | 近纯黑 `[0.0,0.0,0.02]` |

### 太阳 / 月亮

| 参数 | 位置 | 说明 |
|------|------|------|
| 方块太阳大小 | `sky.frag:106-107` | `sunOuter = 0.05`, `sunInner = 0.043` — 越大方块越大 |
| 太阳光晕宽泛光 | `sky.frag:79` | `pow(sunDot, 64.0) * 0.3` — 改 0.3 调亮度 |
| 太阳光晕核心光 | `sky.frag:80` | `pow(sunDot, 256.0) * 0.5` — 改 256 调锐度，0.5 调亮度 |
| 月亮光晕 | `sky.frag:92` | `pow(moonDot, 64.0) * 0.2` — 改 0.2 调亮度 |

### 体积云

| 参数 | 位置 | 说明 |
|------|------|------|
| 云层高度 | `sky.frag:154-155` | `cloudMin = 300.0`, `cloudMax = 550.0` (米) |
| 视步数 | `sky.frag:161` | `steps = 16` — 越多云层越精细（更吃性能） |
| 光源步数 | `sky.frag:197` | `lightSteps = 2` — 自阴影精度 |
| 云密度阈值 | `sky.frag:44` | `threshold = mix(0.3, 0.2, uRainIntensity)` — 越小云越密 |
| 风向速度 | `sky.frag:38` | `uvw.x += time * 0.005` — 风吹快慢 |

### 星星

| 参数 | 位置 | 说明 |
|------|------|------|
| 星空密度 | `sky.frag:142` | `starValue > 0.99` — 越小星星越多（0.98 = 2%） |
| 网格分辨率 | `sky.frag:140` | `floor(celestialPos * 200.0)` — 越大星星越小 |

---

## 二、光照与阴影

### 昼夜光照 ★

> 以下 5 组光照参数均可通过 `tuning.json` + F1 热重载，无需编译。

| 参数 | tuning.json 键名 | 说明 |
|------|-----------------|------|
| 日光颜色 | `dayLight` | 暖色日光 `[1.3, 1.15, 0.95]` |
| 日落光色 | `sunsetLight` | 橙红晚霞光 `[1.0, 0.4, 0.1]` |
| 月光颜色 | `moonLight` | 极暗冷月光 `[0.01, 0.02, 0.04]` |
| 白昼环境光 | `dayAmbient` | 阴影区域补光 `[0.25, 0.35, 0.50]` |
| 夜晚环境光 | `nightAmbient` | 夜间很深暗 `[0.02, 0.02, 0.06]` |

| 时间流速 | `main.cpp` | `timeScale` — 昼夜循环速度（仍需重编译） |

### 软阴影

| 参数 | 位置 | 说明 |
|------|------|------|
| 阴影柔和半径 | `model.frag:91` | `filterRadius = 2.5` — 越大阴影越柔和 |
| 法线偏移 | `model.frag:67` | `normal * 0.035` — 消除自阴影条纹 |
| 动态 bias | `model.frag:79` | `max(0.005*(1-N·L), 0.0005)` |

### 点光源 (火把/灯笼/岩浆)

| 参数 | 位置 | 说明 |
|------|------|------|
| 衰减半径 | `model.frag:259` | `maxDist = 12.0` (米) |
| 衰减系数 | `model.frag:266` | `1/(1 + 0.14d + 0.07d²)` |
| 火光颜色 | `model.frag:253` | `fireColor = (0.7, 0.45, 0.2)` |
| 最大光源数 | `model.frag:39` | `#define MAX_POINT_LIGHTS 512` |

---

## 三、水面

### 颜色与底色

| 参数 | 位置 | 说明 |
|------|------|------|
| 深水底色 (蓝色) | `model.frag:328` | `deepWater = mix(vec3(0, 0.06, 0.12), ...)` — B 通道控制蓝度 |
| 浅水底色 | `model.frag:329` | `shallowWater = mix(vec3(0, 0.15, 0.22), ...)` — B 通道同上 |
| 底色混合权重 | `model.frag:355` | `waterBase * 0.3` — 越小底色越弱反射越占主导 |

### 菲涅尔与反射

| 参数 | 位置 | 说明 |
|------|------|------|
| 菲涅尔 F0 基准反射率 | `model.frag:325` | `fresnel = 0.02 + 0.98 * ...` — 改 0.02 (水的 F0) |
| SSR→天空色过渡阈值 | `model.frag:345` | `ssrLum * 2.0` — 越小 SSR 越容易接管 |
| 反射强度补偿 | `model.frag:355` | `* fresnel * 1.0` — 增大此处乘数以整体提亮反射 |

### 波浪

| 参数 | 位置 | 说明 |
|------|------|------|
| 波浪 UV 缩放 | `model.frag:130` | `worldPos.xz * 0.06` — 越小波浪越绵长 |
| 法线扰动强度 | `model.frag:145` | `normalOffset.x * 0.25` — 越大水面越不平 |
| 波浪速度 | `model.frag:120-121` | `time * vec2(0.01, 0.015)` — 系数越大流速越快 |

### 透明度

| 参数 | 位置 | 说明 |
|------|------|------|
| 最小透明度 (垂直看) | `model.frag:358` | `mix(0.45, 0.90, fresnel)` — 改 0.45：越小越透明 |
| 最大透明度 (掠射) | `model.frag:358` | 同上 0.90 — 1.0 = 完全不透明 |

### 太阳/月光高光

| 参数 | 位置 | 说明 |
|------|------|------|
| 白天锐度 | `model.frag:348` | `mix(256.0, 64.0, ...)` — 256.0 越大光斑越集中 |
| 夜晚锐度 | `model.frag:348` | 同上 64.0 |
| 白天强度 | `model.frag:351` | `mix(8.0, 0.4, ...)` — 8.0 越大越亮 |
| 夜晚强度 | `model.frag:351` | 同上 0.4 |

---

## 四、屏幕空间反射 (SSR)

### 热重载参数 ★ (`tuning.json` + F1)

| 参数 | tuning.json 键名 | 说明 |
|------|-----------------|------|
| 光线最大距离 | `ssrMaxDistance` | `15.0` 米 — 调大反射更远；调小仅反射近处 |
| 线步进步数 | `ssrRaySteps` | `40` — 调大命中更精确（略吃性能） |
| 二分精炼次数 | `ssrRefinementSteps` | `4` — 调大交点更精准；0 跳过精炼 |

### Shader 内参数 (F1 热加载)

| 参数 | 位置 | 说明 |
|------|------|------|
| 天空回落亮度 | `ssr.frag:151` | `distAtten = 1.0` — 被遮挡时回落天空的亮度 |
| 地面 (type 0) 反射率 | `model.frag:286` | `ssrWeight = 0.0` — 当前关闭 |
| 手持道具 (type 5) 反射率 | `model.frag:287` | `mix(0.04, 1.0, grazingFresnel) * 0.5` |

### 不可热重载 (需重编译)

| 参数 | 位置 | 说明 |
|------|------|------|
| 输出分辨率 | `main.cpp` SSR FBO | `SSR_WIDTH/HEIGHT = 960/540` |

---

## 五、雨水与天气

### 雨水坑

| 参数 | 位置 | 说明 |
|------|------|------|
| 噪声频率 (水坑大小) | `model.frag:162` | `FragPos.xz * 0.5` — 越小水坑越大 |
| 水坑覆盖率低阈值 | `model.frag:168` | `smoothstep(0.8 - rain*0.3, ...)` — 越低水坑越多 |
| 水坑覆盖率高阈值 | `model.frag:168` | `..., 0.9 - rain*0.3, n)` — 同上 |
| 水坑反射亮度 | `model.frag:245` | `* 1.5` — 放大整体亮度 |
| 坡度衰减 | `model.frag:244` | `slopeDamp = normal.y² * 3.0` — 越大陡坡允许越强反射 |
| 夜间反射压制 | `model.frag:243` | `nightDamp = mix(1.0, 0.05, uNightFade)` |

### 雨滴粒子

| 参数 | 位置 | 说明 |
|------|------|------|
| 雨滴数量 | `main.cpp` rain pass | `glDrawArrays(GL_LINES, 0, 20000)` |
| 雨滴透明度 | `rain.frag:533` | `0.8, 0.9, 1.0, 0.2*uRainIntensity` |
| 跟随盒子 | `rain.vert` | `localPos` 40×40×40m 包围盒 |

### 植被风摆

| 参数 | 位置 | 说明 |
|------|------|------|
| 风速度基值 | `model.vert` | `windSpeed = 2.0 + uRainIntensity * 4.0` |
| 风强度 | `model.vert` | `windStrength = 0.05 + uRainIntensity * 0.05` |

---

## 六、上帝光 (God Rays)

### 热重载参数 ★ (`tuning.json` + F1)

| 参数 | tuning.json 键名 | 说明 |
|------|-----------------|------|
| 整体强度 | `godrayWeight` | `0.015` — 调大光柱更亮；0 完全消失 |
| 光线密度 | `godrayDensity` | `1.0` — 越大光线向内拉伸更长 |
| 衰减速度 | `godrayDecay` | `0.92` — 越接近 1.0 尾越长 |

### Shader 内参数 (F1 热加载)

| 参数 | 位置 | 说明 |
|------|------|------|
| 采样步数 | `godrays.frag:12` | `NUM_SAMPLES = 60` |
| 非太阳像素过滤半径 | `godrays.frag:29` | `smoothstep(0, 0.50, distToSun)` — 越大越纯净 |

---

## 七、SSAO (环境光遮蔽)

| 参数 | 位置 | 说明 |
|------|------|------|
| 采样半径 | `ssao.frag:26` | `radius = 0.5` |
| 采样偏移 | `ssao.frag:27` | `bias = 0.025` |
| 使用采样数 | `ssao.frag:29` | `i < 16` (前 16 个内核) |

---

## 八、HDR / Bloom / 色调映射

### 热重载参数 ★ (`tuning.json` + F1)

| 参数 | tuning.json 键名 | 说明 |
|------|-----------------|------|
| Bloom 混合强度 | `bloomIntensity` | `0.35` — 调大光晕更显眼；调小更清淡 |
| 曝光度 | `exposure` | `0.5` — 调大整体更亮；调小整体变暗 |

### Shader 内参数 (F1 热加载)

| 参数 | 位置 | 说明 |
|------|------|------|
| 亮部提取阈值 | `blur.frag:15` | `brightness > 1.4` — 越低越多像素参与 Bloom |
| 饱和度增强 | `hdr_compose.frag:29` | `saturation = 1.25` |
| Gamma | `hdr_compose.frag:35` | `pow(result, 1.0/2.4)` |

---

## 九、大气雾

| 参数 | 位置 | 说明 |
|------|------|------|
| 晴天雾密度 | `model.frag:285` | `mix(0.0005, 0.005, uRainIntensity)` — 改 0.0005 |
| 雨天雾密度 | `model.frag:285` | 同上 0.005 |
| 晴天雾颜色 | `model.frag:288` | `mix(vec3(0.6,0.8,0.95), ...)` |
| 雨天雾颜色 | `model.frag:288` | 同上 `vec3(0.4,0.45,0.5)` |
