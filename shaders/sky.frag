#version 460 core
in vec3 TexCoords;
out vec4 FragColor;
uniform vec3 uHorizonColor;
uniform vec3 uZenithColor;
uniform vec3 uSunDir;       // 指向太阳的向量
uniform mat4 uStarMatrix;   // 天球逆矩阵：将 viewDir 变换回天球本地坐标系
uniform float uTime;
uniform float uRainIntensity;

// --- 1. 3D 空间噪声与 FBM ---
float hash3D(vec3 p) {
    p = fract(p * vec3(12.9898, 78.233, 37.719));
    p += dot(p, p.yxz + 34.19);
    return fract(p.x * p.y * p.z);
}
float noise3D(vec3 x) {
    vec3 i = floor(x); vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash3D(i+vec3(0,0,0)), hash3D(i+vec3(1,0,0)), f.x),
                   mix(hash3D(i+vec3(0,1,0)), hash3D(i+vec3(1,1,0)), f.x), f.y),
               mix(mix(hash3D(i+vec3(0,0,1)), hash3D(i+vec3(1,0,1)), f.x),
                   mix(hash3D(i+vec3(0,1,1)), hash3D(i+vec3(1,1,1)), f.x), f.y), f.z);
}
float fbm3D(vec3 p) {
    float v = 0.0; float a = 0.5;
    for (int i = 0; i < 4; ++i) {
        v += a * noise3D(p);
        p = p * 2.0 + vec3(100.0); // 空间偏移
        a *= 0.5;
    }
    return v;
}

// --- 2. 气象学密度分布与相位函数 ---
float getCloudDensity(vec3 p, float h, float time) {
    vec3 uvw = p * 0.001; // 云的 3D 缩放
    uvw.x += time * 0.005;  // 3D 空间中的风向移动
    float n = fbm3D(uvw);
    // 高度遮罩：极其平整的底部(0.0-0.1) 和 蓬松消散的顶部(1.0-0.4)
    float mask = smoothstep(0.0, 0.1, h) * smoothstep(1.0, 0.4, h);
    float threshold = mix(0.3, 0.2, uRainIntensity); // 下雨时阈值低，云层变多连成一片
    return smoothstep(threshold, threshold + 0.4, n * mask);
}

float hgPhase(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5);
}

float hash(vec3 p)
{
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

// 交错梯度噪声 (Interleaved Gradient Noise)，极其廉价的蓝噪点生成器
float getJitter(vec2 fragCoord) {
    return fract(52.9829189 * fract(dot(fragCoord, vec2(0.06711056, 0.00583715))));
}

void main()
{
    vec3 viewDir = normalize(TexCoords);

    // 天空渐变：从地平线 (dir.y=0) 到天顶 (dir.y=1)
    float blend = clamp(viewDir.y, 0.0, 1.0);
    vec3 skyColor = mix(uHorizonColor, uZenithColor, blend);

    // 黑夜系数：太阳高度越低，夜晚越深
    float nightFade = clamp(-uSunDir.y * 3.0, 0.0, 1.0);

    // --- 1. 太阳光晕 (Sun Halo) — 模拟大气米氏散射 ---
    vec3 sunDirNorm = normalize(uSunDir);
    float sunDot = max(dot(viewDir, sunDirNorm), 0.0);

    // 双重 pow：宽大泛光 + 靠近中心的强光
    float sunGlowWide = pow(sunDot, 64.0) * 0.3;
    float sunGlowCore = pow(sunDot, 256.0) * 0.5;
    vec3 sunHaloColor = vec3(1.0, 0.95, 0.8) * (sunGlowWide + sunGlowCore);

    // 太阳落山时消散（+1.0 偏移让晚霞时仍有余辉）
    float sunVisibility = clamp(sunDirNorm.y * 10.0 + 1.0, 0.0, 1.0);
    sunVisibility *= (1.0 - uRainIntensity * 0.9);  // 降雨压抑光晕
    skyColor += sunHaloColor * sunVisibility;

    // --- 2. 月亮光晕 (Moon Halo) — 更弱、更冷 ---
    vec3 moonDirNorm = -sunDirNorm;
    float moonDot = max(dot(viewDir, moonDirNorm), 0.0);

    float moonGlow = pow(moonDot, 64.0) * 0.2;
    vec3 moonHaloColor = vec3(0.6, 0.7, 0.9) * moonGlow;

    float moonVisibility = clamp(moonDirNorm.y * 10.0 + 1.0, 0.0, 1.0);
    moonVisibility *= (1.0 - uRainIntensity * 0.8);  // 降雨压抑月光
    skyColor += moonHaloColor * moonVisibility;

    // 将视线方向变换回天球本地坐标系（星星与日月共用）
    vec3 celestialPos = (uStarMatrix * vec4(viewDir, 0.0)).xyz;

    // --- 程序化方块太阳与月亮 (Minecraft 风格) ---
    float bodySize = 0.05;

    // 太阳：二分包围盒——外围奶油色边框 + 内部纯白核心
    const float sunOuter = 0.05;
    const float sunInner = 0.043;
    bool inSunOuter = celestialPos.x > 0.0 && abs(celestialPos.y) < sunOuter && abs(celestialPos.z) < sunOuter;
    bool inSunInner = abs(celestialPos.y) < sunInner && abs(celestialPos.z) < sunInner;

    bool isMoon = celestialPos.x < 0.0 && abs(celestialPos.y) < bodySize && abs(celestialPos.z) < bodySize;

    if (inSunOuter)
    {
        if (inSunInner)
            skyColor = vec3(3.0, 3.0, 3.0);       // 核心：耀眼纯白
        else
            skyColor = vec3(2.55, 2.35, 1.95);     // 边框：极淡橙黄奶油色
    }
    else if (isMoon)
    {
        // 月亮区域：计算陨石坑纹理并直接覆盖（=），不画星星
        vec2 moonUV = (celestialPos.yz / bodySize) * 0.5 + 0.5;
        vec2 pixelGrid = floor(moonUV * 8.0);
        float crater = hash(vec3(pixelGrid, 0.0));

        vec3 baseMoonColor = vec3(0.6, 0.65, 0.7);
        if (crater > 0.7)
            baseMoonColor *= 0.7;
        else if (crater > 0.4)
            baseMoonColor *= 0.85;

        skyColor = baseMoonColor;
    }
    else
    {
        // 虚空区域：只在这里画星星，绝对不会穿透日月
        if (nightFade > 0.0)
        {
            vec3 gridPos = floor(celestialPos * 200.0);
            float starValue = hash(gridPos);
            if (starValue > 0.99)
            {
                float starIntensity = (starValue - 0.99) * 100.0 * nightFade;
                starIntensity *= clamp(viewDir.y * 5.0, 0.0, 1.0);
                skyColor += vec3(starIntensity);
            }
        }
    }

    // --- 3. 双重光线步进体积云 (True 3D Volumetric Raymarching) ---
    // 只有仰角大于 0.02 时才画云，忽略地面方向
    if (viewDir.y > 0.02) {
        float cloudMin = 300.0;
        float cloudMax = 550.0; // 云层现在有 200 米的真实物理厚度
        float tMin = cloudMin / viewDir.y;
        float tMax = cloudMax / viewDir.y;
        vec3 pMin = viewDir * tMin;
        vec3 pMax = viewDir * tMax;

        int steps = 16; // 主视觉步进次数（IG 抖动伪遮挡替代高步数）
        vec3 stepVec = (pMax - pMin) / float(steps);
        float stepSize = length(stepVec);
        vec3 currentPos = pMin;

        // 核心优化：起始点抖动，打散步进断层为蓝噪点
        float jitter = getJitter(gl_FragCoord.xy);
        currentPos += stepVec * jitter;

        float totalAlpha = 0.0;
        vec3 finalCloudColor = vec3(0.0);
        vec3 sunDirNorm = normalize(uSunDir);

        // 银边效应计算 (Henyey-Greenstein)
        float cosTheta = dot(viewDir, sunDirNorm);
        // 强烈的向前散射 (向阳银边) + 微弱的向后散射
        float phaseVal = mix(hgPhase(cosTheta, 0.5), hgPhase(cosTheta, -0.1), 0.5);

        // 云顶迎光面极其亮白，云底偏柔和的浅蓝灰
        vec3 sunColor = mix(vec3(1.5, 1.5, 1.5), vec3(0.2, 0.2, 0.25), nightFade);
        vec3 ambientColor = mix(vec3(0.65, 0.75, 0.85), vec3(0.05, 0.05, 0.08), nightFade);

        // 降雨时乌云密布：调暗云色
        vec3 rainTop = vec3(0.4, 0.45, 0.5);
        vec3 rainBottom = vec3(0.1, 0.12, 0.15);
        sunColor = mix(sunColor, rainTop, uRainIntensity);
        ambientColor = mix(ambientColor, rainBottom, uRainIntensity);

        for (int i = 0; i < steps; ++i) {
            if (totalAlpha >= 0.99) break;

            float h = (currentPos.y - cloudMin) / (cloudMax - cloudMin);
            float density = getCloudDensity(currentPos, h, uTime);

            if (density > 0.0) {
                // --- 第二重循环：光源步进 (计算真实物理自阴影) ---
                int lightSteps = 2;
                float lightStepSize = 40.0;
                vec3 lPos = currentPos;
                float lightDensity = 0.0;
                for (int j = 0; j < lightSteps; ++j) {
                    lPos += sunDirNorm * lightStepSize;
                    float lHeight = clamp((lPos.y - cloudMin) / (cloudMax - cloudMin), 0.0, 1.0);
                    lightDensity += getCloudDensity(lPos, lHeight, uTime) * lightStepSize;
                }

                // 比尔-朗伯定律：光在云中的物理衰减
                float transmittance = exp(-lightDensity * 0.03);

                // 云的受光总和：太阳直射光(带散射) + 环境天光
                vec3 S = sunColor * transmittance * phaseVal + ambientColor;

                // 视线上的消光
                float extinction = exp(-density * stepSize * 0.005);
                vec3 L = S * (1.0 - extinction);

                finalCloudColor += L * (1.0 - totalAlpha);
                totalAlpha += (1.0 - extinction) * (1.0 - totalAlpha);
            }
            currentPos += stepVec;
        }

        // 地平线消隐：防止远处的云出现生硬的切割线
        float horizonFade = smoothstep(0.02, 0.15, viewDir.y);
        totalAlpha *= horizonFade;

        // 将云层覆盖在原有的天空上
        skyColor = mix(skyColor, finalCloudColor, totalAlpha);
    }

    FragColor = vec4(skyColor, 1.0);
}
