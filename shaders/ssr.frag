#version 460 core
in vec2 TexCoords;
out vec3 FragColor; // RGB 反射颜色

uniform sampler2D gPosition;     // G-Buffer: 视图空间位置 (RGBA16F)
uniform sampler2D gNormal;       // G-Buffer: 视图空间法线 (RGBA16F)
uniform sampler2D uSceneTex;     // HDR 场景颜色 — 反射源

uniform mat4 uProjection;        // 相机投影矩阵
uniform vec2 uScreenSize;        // SSR 输出分辨率 (e.g. 960x540 半分辨率)
uniform vec2 uFullScreenSize;    // HDR 场景实际分辨率 (1920x1080, 用于采样 uSceneTex)
uniform float uMaxDistance;      // 光线步进最大距离 (视图空间, 默认 15.0)
uniform int uRaySteps;           // 线性步进步数 (默认 40)
uniform int uRefinementSteps;    // 二分精炼迭代次数 (默认 4)

void main() {
    // G-Buffer 是全分辨率 (1920x1080)，需要使用完整 UV 采样
    // TexCoords 已经是从 fullscreenVAO 传入的正确 UV
    vec3 viewPos = texture(gPosition, TexCoords).xyz;
    vec3 N = normalize(texture(gNormal, TexCoords).xyz);
    vec3 V = normalize(-viewPos); // 视线方向 (指向相机)

    // 2. 计算反射方向 (视图空间)
    vec3 R = reflect(-V, N);

    // 3. 提前剔除：天空像素 (viewPos ≈ 0) 或 反射方向指向相机 (背面)
    if (length(viewPos) < 0.001 || dot(R, V) > 0.0) {
        FragColor = vec3(0.0);
        return;
    }

    // 4. 线性光线步进（穿越检测：检测从"表面前方"到"表面后方"的符号反转）
    float stepSize = uMaxDistance / float(uRaySteps);
    vec3 rayStep = R * stepSize;
    vec3 rayPos = viewPos + rayStep; // 从表面偏移一步，避免自相交

    // 记录"上一帧"穿越状态 & 最后一个有效 UV（用于天空回落）
    float prevBeyond = -1.0;
    vec2 lastValidUV = TexCoords;
    vec2 prevUV = TexCoords;

    float hit = -1.0;
    vec2 hitUV = vec2(0.0);
    vec3 hitRayPos = vec3(0.0);

    for (int i = 0; i < uRaySteps; ++i) {
        rayPos += rayStep;

        // 将当前光线位置投影到 HDR 场景分辨率下
        vec4 clip = uProjection * vec4(rayPos, 1.0);
        vec3 ndc = clip.xyz / clip.w;
        vec2 uv = ndc.xy * 0.5 + 0.5;

        // 屏幕边界检查 — 记录最后一个有效 UV
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
            prevBeyond = -1.0;
            prevUV = uv;
            continue;
        }

        lastValidUV = uv;

        // 获取该屏幕位置的真实场景深度（采样全分辨率 G-Buffer）
        float sceneZ = texture(gPosition, uv).z;

        // 跳过天空像素
        if (abs(sceneZ) < 0.001) {
            prevBeyond = -1.0;
            prevUV = uv;
            continue;
        }

        float currBeyond = sceneZ - rayPos.z;

        // 穿越检测：从表面前方翻转到表面后方
        if (prevBeyond <= 0.0 && currBeyond > 0.0) {
            hit = float(i) + 1.0;
            hitUV = uv;
            hitRayPos = rayPos;
            break;
        }

        prevBeyond = currBeyond;
        prevUV = uv;
    }

    // 5. 二分精炼
    if (hit > 0.0 && uRefinementSteps > 0) {
        vec3 refineStart = hitRayPos - rayStep;
        vec3 refineEnd = hitRayPos;

        for (int j = 0; j < uRefinementSteps; ++j) {
            vec3 testPos = (refineStart + refineEnd) * 0.5;

            vec4 clip = uProjection * vec4(testPos, 1.0);
            vec3 ndc = clip.xyz / clip.w;
            vec2 uv = ndc.xy * 0.5 + 0.5;

            if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
                break;

            float sceneZ = texture(gPosition, uv).z;
            if (abs(sceneZ) < 0.001)
                break;

            if (sceneZ > testPos.z) {
                refineEnd = testPos;
            } else {
                refineStart = testPos;
                hitUV = uv;
            }
        }
    }

    // 6. 采样反射颜色
    vec3 reflectColor = vec3(0.0);

    if (hit > 0.0 &&
        hitUV.x >= 0.0 && hitUV.x <= 1.0 &&
        hitUV.y >= 0.0 && hitUV.y <= 1.0) {
        // 命中几何体 → 从 HDR 场景采样颜色（注意 UV 需要换算到 HDR 纹理坐标）
        reflectColor = texture(uSceneTex, hitUV).rgb;
    } else if (R.y > 0.0) {
        // ===== 天空回落：光线射向天空未命中几何体 =====
        // 从 HDR 场景中采样天空颜色（包含云层、太阳光晕等）
        // 使用最后一个有效 UV 作为天空方向
        vec2 skyUV = lastValidUV;
        // 如果光线最终离开了屏幕，沿反射方向延伸到屏幕外以获得天空颜色
        if (lastValidUV.x <= 0.0 || lastValidUV.x >= 1.0 ||
            lastValidUV.y <= 0.0 || lastValidUV.y >= 1.0) {
            // 光线已经离开屏幕 — 使用反射方向投影到更远距离以获取纯天空
            vec3 farPos = viewPos + R * uMaxDistance;
            vec4 farClip = uProjection * vec4(farPos, 1.0);
            vec3 farNDC = farClip.xyz / farClip.w;
            skyUV = clamp(farNDC.xy * 0.5 + 0.5, 0.0, 1.0);
        }
        reflectColor = texture(uSceneTex, skyUV).rgb;
        // 标记为命中以启用衰减
        hitUV = skyUV;
        hit = 0.5; // 伪命中，用于距离衰减
    }

    // 7. 屏幕边缘淡出
    vec2 edgeDist = abs(hitUV * 2.0 - 1.0);
    float edgeFade = 1.0 - smoothstep(0.6, 1.0, max(edgeDist.x, edgeDist.y));

    // 8. 距离衰减
    float t = hit > 0.0 ? (hit / float(uRaySteps)) : 1.0;
    float distAtten = hit > 0.0 ? (1.0 - t) : 0.0;
    // 天空回落：全亮度保留（model.frag 自行按材质控制强度）
    if (hit < 1.0) distAtten = 1.0;

    // 注：菲涅尔不在此处计算，交由 model.frag 根据材质类型独立处理
    FragColor = reflectColor * edgeFade * distAtten;
}
