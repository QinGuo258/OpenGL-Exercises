#version 460 core
in vec2 TexCoords;
out vec4 FragColor;
uniform sampler2D uSceneTex; // FBO 场景原图
uniform sampler2D uBloomTex; // 模糊后的发光图
uniform float uBloomIntensity;
uniform float uExposure;

// 好莱坞工业级 ACES 电影色调映射算法
vec3 ACESFilm(vec3 x) {
    float a = 2.51; float b = 0.05; float c = 2.43; float d = 0.59; float e = 0.16;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main() {
    vec3 sceneColor = texture(uSceneTex, TexCoords).rgb;
    vec3 bloomColor = texture(uBloomTex, TexCoords).rgb;

    // 1. 线性叠加原图与金黄色光晕
    vec3 result = sceneColor + bloomColor * uBloomIntensity;

    // 2. 曝光度映射
    result = vec3(1.0) - exp(-result * uExposure);

    // 3. 电影级 Tone Mapping
    result = ACESFilm(result);

    // 3.5. 饱和度提升 (让草地和鲜花更鲜艳)
    float saturation = 1.25; // 提升 25%
    float luminance = dot(result, vec3(0.2126, 0.7152, 0.0722));
    vec3 grayColor = vec3(luminance);
    result = mix(grayColor, result, saturation);

    // 4. 伽马校正 (解除显示器物理偏色)
    result = pow(result, vec3(1.0 / 2.4));

    FragColor = vec4(result, 1.0);
}
