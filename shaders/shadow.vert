#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec2 TexCoords;

uniform mat4 uLightSpaceMatrix;
uniform mat4 uModel;
uniform float uTime;
uniform float uRainIntensity;
uniform int uMaterialType;

void main()
{
    vec4 localPos = uModel * vec4(aPos, 1.0);
    vec3 worldPos = vec3(localPos);

    // 风力摇摆：通过法线检测十字交叉面片（草丛/小麦/花），与 model.vert 保持 100% 一致
    bool isCrossedQuad = abs(aNormal.x) > 0.1 && abs(aNormal.x) < 0.9 && abs(aNormal.z) > 0.1 && abs(aNormal.z) < 0.9;
    if (isCrossedQuad) {
        float windSpeed = 2.0 + uRainIntensity * 4.0;
        float windStrength = 0.05 + uRainIntensity * 0.05;
        float windWeight = aTexCoords.y; // 根部锚定

        float offsetX = sin(worldPos.x * 2.0 + uTime * windSpeed) * windStrength * windWeight;
        float offsetZ = cos(worldPos.z * 2.0 + uTime * windSpeed) * windStrength * windWeight;

        worldPos.x += offsetX;
        worldPos.z += offsetZ;
    }

    TexCoords = aTexCoords;
    gl_Position = uLightSpaceMatrix * vec4(worldPos, 1.0);
}
