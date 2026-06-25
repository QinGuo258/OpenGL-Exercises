#version 460 core
layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec4 gNormal;

in vec3 FragPos; // 视图空间坐标
in vec3 Normal;  // 视图空间法线
in vec2 TexCoords;

layout(binding = 0) uniform sampler2D uDiffuseTexture;

void main() {
    // 【核心修复：在 G-Buffer 阶段执行 Alpha 测试】
    // 如果采样到的像素是透明的，直接 discard 丢弃，不准它写入坐标和法线缓冲！
    float alpha = texture(uDiffuseTexture, TexCoords).a;
    if (alpha < 0.1) {
        discard;
    }

    gPosition = vec4(FragPos, 1.0);
    gNormal = vec4(normalize(Normal), 1.0);
}
