#version 460 core
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uAlpha = 1.0;

uniform bool uUseTexture = true;   // 是否使用纹理（false = 纯色块模式）
uniform vec4 uSolidColor = vec4(0.0); // 纯色块颜色
uniform bool uIsFont = false;      // 是否是单通道字体贴图
uniform vec3 uFontColor = vec3(1.0); // 字体颜色

void main() {
    vec4 texColor;
    if (uUseTexture) {
        vec4 sampled = texture(uTexture, TexCoords);
        if (uIsFont) {
            // 字体渲染：Red通道是字体的 Alpha 值 (由于 GL_RED 格式)
            float alphaVal = sampled.r;
            if (alphaVal < 0.05) discard; // 极微小过滤
            texColor = vec4(uFontColor, alphaVal);
        } else {
            // 【核心修复】：正常的 UI 贴图，只能通过 Alpha 通道过滤，绝对不能过滤亮度！
            texColor = sampled;
            if (texColor.a < 0.1) discard;
        }
    } else {
        // 纯色背景板
        texColor = uSolidColor;
    }

    FragColor = texColor;
    FragColor.a *= uAlpha;
}
