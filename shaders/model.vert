#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in ivec4 boneIds;
layout (location = 4) in vec4 weights;

out vec3 FragPos;
out vec3 FragNormal;
out vec2 TexCoords;
out vec4 FragPosLightSpace;

const int MAX_BONES = 100;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightSpaceMatrix;
uniform mat4 finalBonesMatrices[MAX_BONES];
uniform float uTime;
uniform float uRainIntensity;
uniform int uMaterialType;

void main()
{
    mat4 boneTransform = mat4(1.0);
    if (boneIds[0] >= 0 || boneIds[1] >= 0 || boneIds[2] >= 0 || boneIds[3] >= 0)
    {
        boneTransform  = finalBonesMatrices[max(boneIds[0], 0)] * weights[0];
        boneTransform += finalBonesMatrices[max(boneIds[1], 0)] * weights[1];
        boneTransform += finalBonesMatrices[max(boneIds[2], 0)] * weights[2];
        boneTransform += finalBonesMatrices[max(boneIds[3], 0)] * weights[3];
    }

    mat4 modelBone = uModel * boneTransform;
    vec4 localPos = modelBone * vec4(aPos, 1.0);
    vec3 worldPos = vec3(localPos);

    // 风力摇摆：uMaterialType 1=草丛(底部锚定), 2=树叶(整体抖动)
    if (uMaterialType == 1 || uMaterialType == 2) {
        float windSpeed = 2.0 + uRainIntensity * 4.0;
        float windStrength = 0.05 + uRainIntensity * 0.05;

        // 草丛底部锚定不动 (aTexCoords.y≈0 为根部)，树叶整体 0.8 权重抖动
        float windWeight = (uMaterialType == 1) ? aTexCoords.y : 0.8;

        float offsetX = sin(worldPos.x * 2.0 + uTime * windSpeed) * windStrength * windWeight;
        float offsetZ = cos(worldPos.z * 2.0 + uTime * windSpeed) * windStrength * windWeight;

        worldPos.x += offsetX;
        worldPos.z += offsetZ;
    }

    // 水面整体微沉，防止与岸边方块 Z-Fighting
    if (uMaterialType == 4) {
        worldPos.y -= 0.1; // 平静、统一的基准水面
    }

    FragPos = worldPos;
    FragNormal = mat3(transpose(inverse(modelBone))) * aNormal;
    TexCoords = aTexCoords;
    gl_Position = uProjection * uView * vec4(FragPos, 1.0);

    FragPosLightSpace = uLightSpaceMatrix * vec4(FragPos, 1.0);
}
