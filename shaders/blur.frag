#version 460 core
in vec2 TexCoords;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform bool uHorizontal;
uniform bool uExtract; // 是否执行亮部提取

float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    if (uExtract) {
        // 亮部提取：只保留亮度超过 1.0 的超亮像素 (火把、太阳、灯笼)
        vec3 color = texture(uTexture, TexCoords).rgb;
        float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722)); // 相对亮度公式
        if (brightness > 1.4) {
            FragColor = vec4(color, 1.0);
        } else {
            FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        }
        return;
    }

    // 标准的双通道高斯模糊
    vec2 tex_offset = 1.0 / textureSize(uTexture, 0);
    vec3 result = texture(uTexture, TexCoords).rgb * weight[0];
    if (uHorizontal) {
        for (int i = 1; i < 5; ++i) {
            result += texture(uTexture, TexCoords + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
            result += texture(uTexture, TexCoords - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
        }
    } else {
        for (int i = 1; i < 5; ++i) {
            result += texture(uTexture, TexCoords + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
            result += texture(uTexture, TexCoords - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
        }
    }
    FragColor = vec4(result, 1.0);
}
