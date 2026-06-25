#version 460 core
in vec3 FragPos;
in vec3 FragNormal;
in vec2 TexCoords;
in vec4 FragPosLightSpace;

out vec4 OutColor;

uniform vec3 uCameraPos;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;
uniform float uSpecularStrength;
uniform float uShininess;
uniform float uRainIntensity;
uniform int uMaterialType;

uniform sampler2D uDiffuseTexture;
uniform bool uHasDiffuseTexture;
uniform bool uIsHit;

uniform float uTime; // 水面法线滚动需要时间驱动
uniform float uNightFade; // 夜色系数：黄昏提前平滑过渡（C++ 根据 sunY 计算传入）
uniform vec3 uHorizonColor; // 天空地平线颜色（实时渐变）
uniform vec3 uZenithColor; // 天空天顶颜色（实时渐变）

uniform sampler2D uPackedNoiseMap; // 水面高度/噪点打包贴图 (R=大波浪, G=小波纹)

uniform sampler2D uShadowMap;
uniform mat4 uLightSpaceMatrix; // 光源空间变换矩阵（法线偏移阴影重投影）

// SSAO 遮蔽掩码（模糊后）
uniform sampler2D uSsaoMap;
uniform vec2 uScreenSize;

// 点光源系统（从自发光网格顶点自动提取）
#define MAX_POINT_LIGHTS 512
uniform vec3 uPointLights[MAX_POINT_LIGHTS];
uniform int uNumPointLights;

// 基于屏幕像素坐标生成随机角度的哈希函数
float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

// 16 采样泊松圆盘 (Poisson Disk)
const vec2 poissonDisk[16] = vec2[](
   vec2( -0.94201624, -0.39906216 ), vec2( 0.94558609, -0.76890725 ),
   vec2( -0.094184101, -0.92938870 ), vec2( 0.34495938, 0.29387760 ),
   vec2( -0.91588581, 0.45771432 ), vec2( -0.81544232, -0.87912464 ),
   vec2( -0.38277543, 0.27676845 ), vec2( 0.97484398, 0.75648379 ),
   vec2( 0.44323325, -0.97511554 ), vec2( 0.53742981, -0.47373420 ),
   vec2( -0.26496911, -0.41893023 ), vec2( 0.79197514, 0.19090188 ),
   vec2( -0.24188840, 0.99706507 ), vec2( -0.81409955, 0.91437590 ),
   vec2( 0.19984126, 0.78641367 ), vec2( 0.14383161, -0.14100790 )
);

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    // =================【核心修复：法线偏移阴影 (Normal Offset Bias)】=================
    // 将世界空间坐标沿法线方向往外轻推 0.035 米，消除几何自阴影三角切边
    // 再使用 uLightSpaceMatrix 将偏移后的安全坐标重新投影到光源空间
    vec3 offsetWorldPos = FragPos + normal * 0.035;
    vec4 offsetLightSpace = uLightSpaceMatrix * vec4(offsetWorldPos, 1.0);

    vec3 projCoords = offsetLightSpace.xyz / offsetLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;

    // 动态 bias（保留作为二级保护）
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);

    // 1. 根据当前屏幕像素坐标，生成一个 0 到 2π 的随机角度
    float randomAngle = rand(gl_FragCoord.xy) * 6.28318530718;
    float s = sin(randomAngle);
    float c = cos(randomAngle);
    mat2 rot = mat2(c, -s, s, c);

    // 2. 控制阴影的柔和半径 (数值越大阴影边缘越模糊)
    float filterRadius = 2.5;

    // 3. 执行 16 次泊松圆盘采样
    for (int i = 0; i < 16; ++i) {
        // 旋转采样点，并将圆盘放大到我们想要的柔和半径
        vec2 offset = rot * poissonDisk[i] * texelSize * filterRadius;
        float pcfDepth = texture(uShadowMap, projCoords.xy + offset).r;
        shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
    }

    shadow /= 16.0;
    return shadow;
}

// --- 2D 噪声函数（用于地面水坑形状）---
float hash2D(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}
float noise2D(vec2 p) {
    vec2 i = floor(p); vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash2D(i + vec2(0.0, 0.0)), hash2D(i + vec2(1.0, 0.0)), u.x),
               mix(hash2D(i + vec2(0.0, 1.0)), hash2D(i + vec2(1.0, 1.0)), u.x), u.y);
}

// --- 水面高度采样（从打包噪点图的 R 通道提取波浪高度）---
float getWaterHeight(vec2 uv, float time) {
    // 彻底抛弃 G 通道那层细碎多刺的噪点，双层均采用 R 通道（最平滑的大波浪）
    // 并且大幅度调慢移动速度，让水流看起来温润、迟缓
    float wave1 = texture(uPackedNoiseMap, uv * 0.3 + time * vec2(0.01, 0.015)).r;
    float wave2 = texture(uPackedNoiseMap, uv * 0.7 - time * vec2(0.015, 0.01)).r * 0.4; // 第二层只占 40% 的微弱细节

    return wave1 + wave2;
}

// --- 有限差分法实时计算水面法线 ---
vec3 calculateWaterNormal(vec3 baseNormal, vec3 worldPos, float time) {
    // 【核心微调一】：将 UV 平铺缩放从 0.5 暴跌到 0.06！
    // 这意味着把水纹贴图拉伸了 8 倍多，小碎波会变成极其庞大、连绵的大波浪
    vec2 uv = worldPos.xz * 0.06;

    float eps = 0.03; // 微调求导步长

    // 对相邻的坐标进行高度采样
    float hL = getWaterHeight(uv - vec2(eps, 0.0), time);
    float hR = getWaterHeight(uv + vec2(eps, 0.0), time);
    float hD = getWaterHeight(uv - vec2(0.0, eps), time);
    float hU = getWaterHeight(uv + vec2(0.0, eps), time);

    // 利用偏导数计算法线倾斜向量 (差分越大，法线越倾斜)
    vec3 normalOffset = vec3(hL - hR, 2.0 * eps, hD - hU);

    // 【核心微调二】：将法线扰动强度从 0.6 降到极低的 0.12！
    // 这会让反射的光滑过渡极其平缓，彻底消除"锡纸折皱感"，形成镜面一般的温润倒影
    return normalize(vec3(normalOffset.x * 0.25, 1.0, normalOffset.z * 0.25));
}

void main()
{
    // 纹理采样（提前，ambient 需要用到）
    vec4 texColor = uHasDiffuseTexture ? texture(uDiffuseTexture, TexCoords) : vec4(0.8, 0.8, 0.8, 1.0);
    if (texColor.a < 0.1)
        discard;

    vec3 normal = normalize(FragNormal);

    vec3 lightDir = normalize(-uLightDir);
    vec3 viewDir = normalize(uCameraPos - FragPos);

    // --- 湿润系数与水坑效果 ---
    // 1. 利用世界坐标 XZ 生成低频噪声水坑
    float n = noise2D(FragPos.xz * 0.3); // 缩放因子控制水坑大小

    // 2. 只有向上的面才能积水 (法线 Y 大于 0.8)
    float upFactor = clamp((normal.y - 0.8) * 5.0, 0.0, 1.0);

    // 3. 随着雨变大，水坑面积扩张 (smoothstep 阈值动态降低)
    float puddleMask = smoothstep(0.6 - uRainIntensity * 0.3, 0.7 - uRainIntensity * 0.3, n);

    // 4. 综合湿润度：整体潮湿底色 + 明显的水坑
    float wetness = clamp(uRainIntensity * 0.3 + puddleMask * uRainIntensity, 0.0, 1.0) * upFactor;

    // 5. 材质物理变化
    // 漫反射变暗 (泥土吸水颜色变深)
    vec3 albedoColor = texColor.rgb * mix(1.0, 0.5, wetness);
    // 高光变强变锐利 (水面反光)
    float currentShininess = mix(uShininess, 256.0, wetness); // 湿润处极其光滑
    float currentSpecStrength = mix(uSpecularStrength, 1.0, wetness); // 满反射

    // =================【核心修复：微表面涟漪扰动】=================
    // 只有在积水的地方才产生高频扰动，彻底打散浮点数阶梯条纹
    vec3 baseNormal = normal;
    if (wetness > 0.01) {
        // 使用极高频的坐标采样噪声，模拟雨滴砸在水坑上的微小波纹
        float microNoise = noise2D(FragPos.xz * 15.0 + uTime * 2.0);
        // 给予极其轻微的法线倾斜 (0.03)
        baseNormal.x += (microNoise - 0.5) * 0.03 * wetness;
        baseNormal.z += (microNoise - 0.5) * 0.03 * wetness;
        baseNormal = normalize(baseNormal);
    }
    // ==============================================================

    // 环境光：使用 uAmbientColor（动态日夜环境色），乘以 SSAO 遮蔽因子
    // SSAO 仅衰减环境光，太阳直射光 (diffuse) 和点光源不受影响
    vec2 screenTexCoords = gl_FragCoord.xy / uScreenSize;
    float ssao = texture(uSsaoMap, screenTexCoords).r;
    vec3 ambient = uAmbientColor * albedoColor * ssao;

    // 漫反射（使用湿润后的 albedoColor）
    float diff = max(dot(baseNormal, lightDir), 0.0);
    vec3 diffuse = diff * uLightColor * albedoColor;

    // 镜面反射 — 仅手持道具 (uMaterialType==5) 或湿润积水区保留高光
    // 泥土(0)、草(1)、树叶(2)、发光体(3) 均消除塑料反光，呈现纯粹漫反射
    vec3 halfway = normalize(lightDir + viewDir);
    float spec = pow(max(dot(baseNormal, halfway), 0.0), currentShininess);
    float specWeight = (uMaterialType == 5) ? 1.0 : 0.0;
    float finalSpecStrength = currentSpecStrength * max(specWeight, wetness);
    vec3 specular = uLightColor * spec * finalSpecStrength;

    // PCF 阴影计算
    float shadow = ShadowCalculation(FragPosLightSpace, baseNormal, lightDir);

    // 最终颜色：环境光不受阴影影响，漫反射+镜面反射受阴影衰减
    // 背光面 (shadow=1.0) 依然保留 uAmbientColor，不会死黑
    vec3 finalColor = ambient + (1.0 - shadow) * (diffuse + specular);

    // 降雨时整体压暗
    finalColor *= mix(1.0, 0.4, uRainIntensity);

    // --- 积水的天空环境反射 (Environment Reflection) ---
    if (uRainIntensity > 0.0) {
        // 1. 计算视线反射向量
        vec3 reflectDir = reflect(-viewDir, baseNormal);

        // 2. 直接采样 C++ 传入的、100% 动态昼夜同步的真实天空色！
        // 这样在深夜，uHorizonColor 接近纯黑，积水反射也会自动变得深邃暗淡
        vec3 fakeSkyColor = mix(uHorizonColor, uZenithColor, clamp(reflectDir.y, 0.0, 1.0));

        // 3. 菲涅尔效应 (Fresnel Schlick 近似)
        // 视线越平，反光越强；垂直看时反光极弱 (基础反射率水大约为 0.02)
        float cosTheta = max(dot(viewDir, baseNormal), 0.0);
        float fresnel = 0.02 + 0.98 * pow(1.0 - cosTheta, 5.0);

        // 4. 计算最终的反射强度，夜间自动压至极低，避免深夜积水刺眼
        // 乘以 upFactor 确保只有朝上的面(地面)才反射天空
        float nightDamp = mix(0.4, 0.02, uNightFade); // 白天 40%，夜晚暴跌至 2%
        float reflectionStrength = puddleMask * uRainIntensity * fresnel * upFactor * nightDamp;

        // 5. 将镜面天空反射叠加到最终颜色上
        finalColor = mix(finalColor, fakeSkyColor, reflectionStrength);
    }

    // 点光源贡献：从火把/灯/岩浆等自发光方块发出温暖火光
    vec3 pointLightColor = vec3(0.0);
    if (uMaterialType != 3) {
        vec3 fireColor = vec3(0.7, 0.45, 0.2);
        for (int i = 0; i < uNumPointLights; i++) {
            vec3 lightDir = uPointLights[i] - FragPos;
            float distance = length(lightDir);

            // 性能优化：距离超过 12 米的光源不产生影响
            float maxDist = 12.0;
            if (distance > maxDist) continue;

            lightDir = normalize(lightDir);
            float diff = max(dot(baseNormal, lightDir), 0.0);

            // 物理距离衰减
            float attenuation = 1.0 / (1.0 + 0.14 * distance + 0.07 * (distance * distance));

            // =================【核心修复：边缘平滑消隐】=================
            // 当距离在 8.0 到 12.0 之间时，让光线平滑、线性地淡出到 0
            // 这样在 12.0 米触发 continue 之前，光线就已经完全融入黑暗，消灭硬边！
            float fade = clamp((maxDist - distance) / 4.0, 0.0, 1.0);

            pointLightColor += diff * fireColor * texColor.rgb * attenuation * fade * 0.8;
        }
    }
    finalColor += pointLightColor;

    // 自发光：uMaterialType==3 的物体无视阴影和暗光，直接输出材质本色
    if (uMaterialType == 3) {
        vec3 emission = texColor.rgb * 1.2;
        finalColor = mix(finalColor, emission, 0.9);
    }

    // 受击闪红：受到攻击时，全身混合 40% 的纯红色
    if (uIsHit) {
        finalColor = mix(finalColor, vec3(1.0, 0.0, 0.0), 0.3);
    }

    // 距离雾：物理大气散射，晴天通透浅蓝、雨天灰暗浓雾
    float dist = gl_FragCoord.z / gl_FragCoord.w;
    float fogDensity = mix(0.0005, 0.005, uRainIntensity); // 晴天极度通透，雨天才起浓雾
    float fogFactor = exp(-pow(dist * fogDensity, 2.0));
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    vec3 atmosphereColor = mix(vec3(0.6, 0.8, 0.95), vec3(0.4, 0.45, 0.5), uRainIntensity);
    finalColor = mix(atmosphereColor, finalColor, fogFactor);

    // --- 曝光色调映射 (Exposure Tone Mapping) ---
    float exposure = 1.0;
    finalColor = vec3(1.0) - exp(-finalColor * exposure);

    float alpha = texColor.a; // 默认透明度

    if (uMaterialType == 4) {
        // 利用有限差分法从打包噪点贴图实时计算出波浪法线
        vec3 waterNorm = calculateWaterNormal(normal, FragPos, uTime);

        vec3 viewDirSafe = normalize(uCameraPos - FragPos);
        float cosTheta = max(dot(viewDirSafe, waterNorm), 0.0);
        float fresnel = 0.02 + 0.98 * pow(1.0 - cosTheta, 5.0);

        // =================【核心修复一：极度幽暗的水底色】=================
        // 白天是清澈的青蓝色，夜晚必须转为几乎无色、极度深邃的黑蓝色，防止夜间发出荧光
        vec3 deepWater = mix(vec3(0.0, 0.12, 0.25), vec3(0.001, 0.003, 0.008), uNightFade);
        vec3 shallowWater = mix(vec3(0.0, 0.35, 0.5), vec3(0.005, 0.01, 0.02), uNightFade);
        vec3 waterBase = mix(shallowWater, deepWater, cosTheta);

        // =================【核心修复二：真实天空反射】=================
        // 直接反射 C++ 传进来的、正在发生昼夜/黄昏渐变的天空色！
        // 视线越平，越反射地平线色(uHorizonColor)；越往上看，越反射天顶色(uZenithColor)
        vec3 viewReflect = reflect(-viewDirSafe, waterNorm);
        vec3 skyColor = mix(uHorizonColor, uZenithColor, clamp(viewReflect.y, 0.0, 1.0));

        // =================【核心修复三：柔和的月光高光 (Sun/Moon Glint)】=================
        // 太阳高光极锐利(256.0)，但月光高光必须极柔和(64.0)，让夜间反光是一片温润的水光
        float shininessFactor = mix(256.0, 64.0, uNightFade);
        vec3 reflectDir = reflect(normalize(uLightDir), waterNorm);
        float specFactor = pow(max(dot(viewDirSafe, reflectDir), 0.0), shininessFactor);
        float glintIntensity = mix(8.0, 0.4, uNightFade); // 压低夜间月光反光强度
        vec3 waterGlint = uLightColor * specFactor * glintIntensity;

        // 混合最终颜色
        finalColor = waterBase + skyColor * fresnel + waterGlint;

        // =================【核心修复四：释放物理通透度】=================
        // 夜晚水面稍微加厚保持深邃，但依旧通透
        float targetAlpha = mix(0.12, 0.75, fresnel);
        alpha = mix(targetAlpha, 0.95, uNightFade * 0.5); // 夜晚水面加厚至约 0.5，通透但深邃
    }

    OutColor = vec4(finalColor, alpha);
}
