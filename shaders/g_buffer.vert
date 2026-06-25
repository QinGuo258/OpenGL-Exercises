#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 5) in vec3 aColor; // R 通道用于风摆

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uTime;
uniform float uRainIntensity;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);

    // 植被风摆同步
    if (aColor.r > 0.01) {
        float wSpeed = 2.0 + uRainIntensity * 4.0;
        float wStr = 0.05 + uRainIntensity * 0.05;
        float wWeight = aTexCoords.y * aColor.r;
        worldPos.x += sin(worldPos.x * 2.0 + uTime * wSpeed) * wStr * wWeight;
        worldPos.z += cos(worldPos.z * 2.0 + uTime * wSpeed) * wStr * wWeight;
    }

    vec4 viewPos = uView * worldPos;
    FragPos = viewPos.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(uView * uModel)));
    Normal = normalMatrix * aNormal;

    TexCoords = aTexCoords;
    gl_Position = uProjection * viewPos;
}
