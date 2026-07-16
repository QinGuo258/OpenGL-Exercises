#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uTime;
uniform float uRainIntensity;
uniform float uWindPhase;
uniform int uMaterialType;
uniform float uWindBaseStrength;  // 基础风力强度 (tuning.json)
uniform float uWindRainStrength;  // 雨天附加风力 (tuning.json)

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);

    // 风力摇摆：uMaterialType 1=草丛(底部锚定), 2=树叶(整体抖动)
    // 必须与 model.vert 保持完全一致，否则 SSAO 会在错误位置计算遮蔽
    if (uMaterialType == 1 || uMaterialType == 2) {
        float windStrength = uWindBaseStrength + uRainIntensity * uWindRainStrength;

        // 草丛底部锚定不动 (aTexCoords.y≈0 为根部)，树叶整体 0.8 权重抖动
        float windWeight = (uMaterialType == 1) ? aTexCoords.y : 0.8;

        float offsetX = sin(worldPos.x * 2.0 + uWindPhase) * windStrength * windWeight;
        float offsetZ = cos(worldPos.z * 2.0 + uWindPhase) * windStrength * windWeight;

        worldPos.x += offsetX;
        worldPos.z += offsetZ;
    }

    vec4 viewPos = uView * worldPos;
    FragPos = viewPos.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(uView * uModel)));
    Normal = normalMatrix * aNormal;

    TexCoords = aTexCoords;
    gl_Position = uProjection * viewPos;
}
