#version 460 core
in vec4 FragPosRainSpace;
out vec4 FragColor;
uniform float uRainIntensity;
uniform sampler2D uRainDepthMap;

void main() {
    // 遮挡判断：雨滴在屋顶/遮挡物后面时直接丢弃
    vec3 projCoords = FragPosRainSpace.xyz / FragPosRainSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    // 只有在深度图覆盖范围内才进行剔除
    if (projCoords.z < 1.0 && projCoords.x > 0.0 && projCoords.x < 1.0 && projCoords.y > 0.0 && projCoords.y < 1.0) {
        float mapDepth = texture(uRainDepthMap, projCoords.xy).r;
        float currentDepth = projCoords.z;

        // 如果当前雨滴的深度大于地图深度(稍微加点偏移防止精度闪烁)，说明在房顶下方，直接丢弃！
        if (currentDepth - 0.005 > mapDepth) {
            discard;
        }
    }

    // 浅蓝白色的雨滴，透明度受全局雨量控制
    FragColor = vec4(0.8, 0.9, 1.0, 0.2 * uRainIntensity);
}
