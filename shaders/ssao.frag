#version 460 core
out float FragColor;
in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D texNoise;

uniform vec3 samples[64];
uniform mat4 projection;
uniform float uRadius;   // SSAO 采样半径 (米)，默认 3.0
uniform float uBias;     // 深度偏移消除自阴影 (米)，默认 0.02

// 屏幕尺寸平铺噪声贴图 (1920/4 = 480, 1080/4 = 270)
const vec2 noiseScale = vec2(1920.0/4.0, 1080.0/4.0);

void main() {
    vec3 fragPos = texture(gPosition, TexCoords).xyz;
    vec3 normal = texture(gNormal, TexCoords).xyz;
    vec3 randomVec = texture(texNoise, TexCoords * noiseScale).xyz;

    // 构建 TBN 矩阵，把采样点从切线空间转到视图空间
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    float radius = uRadius; // 从 C++ tuning.json 读取
    float bias = uBias;     // 从 C++ tuning.json 读取
    float fragDist = length(fragPos); // 眼空间距离（比纯 Z 更准确判别遮挡）

    for(int i = 0; i < 16; ++i) {
        // 跨步采样：kernel[0..63] 从近到远分布，stride=4 均匀覆盖全范围
        int idx = i * 4;
        vec3 samplePos = TBN * samples[idx];
        samplePos = fragPos + samplePos * radius;
        float sampleDist = length(samplePos);

        // 投影到屏幕空间去查 G-Buffer
        vec4 offset = vec4(samplePos, 1.0);
        offset = projection * offset;
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        // 边界检查：UV 在 [0,1] 之外直接跳过
        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0)
            continue;

        // 获取采样点对应的场景表面真实位置
        vec3 scenePos = texture(gPosition, offset.xy).xyz;

        // 天空/背景像素保护：背景被清除为 (0,0,0)，其 depth≈0 会让比较短路
        if (length(scenePos) < 0.001)
            continue;

        float sceneDist = length(scenePos);

        // 范围检查 (防止远处的物体遮蔽近处)
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(sceneDist - fragDist));
        occlusion += (sceneDist <= sampleDist - bias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / 16.0);
    FragColor = occlusion;
}
