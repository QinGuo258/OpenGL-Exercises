#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in ivec4 boneIds;
layout (location = 4) in vec4 weights;

out vec2 TexCoords;

const int MAX_BONES = 100;
uniform mat4 uLightSpaceMatrix;
uniform mat4 uModel;
uniform mat4 finalBonesMatrices[MAX_BONES];
uniform float uTime;          // 风力同步（骷髅/僵尸无植被，但需声明以兼容 SetFloat 调用）
uniform float uRainIntensity; // 同上

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

    gl_Position = uLightSpaceMatrix * uModel * boneTransform * vec4(aPos, 1.0);
    TexCoords = aTexCoords;
}
