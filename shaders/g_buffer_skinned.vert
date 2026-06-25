#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in ivec4 aBoneIDs;
layout (location = 4) in vec4 aWeights;

const int MAX_BONES = 100;
uniform mat4 finalBonesMatrices[MAX_BONES];
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

void main()
{
    mat4 boneTransform = mat4(1.0);
    if (aBoneIDs[0] >= 0 || aBoneIDs[1] >= 0 || aBoneIDs[2] >= 0 || aBoneIDs[3] >= 0)
    {
        boneTransform  = finalBonesMatrices[max(aBoneIDs[0], 0)] * aWeights[0];
        boneTransform += finalBonesMatrices[max(aBoneIDs[1], 0)] * aWeights[1];
        boneTransform += finalBonesMatrices[max(aBoneIDs[2], 0)] * aWeights[2];
        boneTransform += finalBonesMatrices[max(aBoneIDs[3], 0)] * aWeights[3];
    }

    vec4 worldPos = uModel * boneTransform * vec4(aPos, 1.0);
    vec4 viewPos = uView * worldPos;
    FragPos = viewPos.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(uView * uModel * boneTransform)));
    Normal = normalMatrix * aNormal;

    TexCoords = aTexCoords;
    gl_Position = uProjection * viewPos;
}
