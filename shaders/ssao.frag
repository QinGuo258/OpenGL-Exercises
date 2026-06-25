#version 460 core
out float FragColor;
in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D texNoise;

uniform vec3 samples[64];
uniform mat4 projection;

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
    float radius = 0.5; // SSAO 采样半径
    float bias = 0.025; // 偏移消除表面自阴影

    for(int i = 0; i < 16; ++i) {
        // 采样点在视图空间的位置
        vec3 samplePos = TBN * samples[i];
        samplePos = fragPos + samplePos * radius;

        // 投影到屏幕空间去查 G-Buffer
        vec4 offset = vec4(samplePos, 1.0);
        offset = projection * offset;
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        // 获取采样点对应的场景表面真实深度
        float sampleDepth = texture(gPosition, offset.xy).z;

        // 范围检查 (防止远处的物体遮蔽近处)
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / 16.0);
    FragColor = occlusion;
}
