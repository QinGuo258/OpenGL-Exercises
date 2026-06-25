#version 460 core
in vec2 TexCoords;
layout(binding = 0) uniform sampler2D uDiffuseTexture;

void main() {
    // 无条件采样并剔除透明像素，不依赖任何 C++ 的布尔变量传入！
    float alpha = texture(uDiffuseTexture, TexCoords).a;
    if (alpha < 0.1) {
        discard;
    }
}
