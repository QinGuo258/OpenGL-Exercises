#version 460 core

uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uCameraPos;
uniform float uTime;
uniform float uRainIntensity;
uniform mat4 uRainSpaceMatrix;

out vec4 FragPosRainSpace;

// 随机哈希函数
float hash(float n) { return fract(sin(n) * 43758.5453123); }

void main() {
    // 因为绘制 GL_LINES，每两个顶点组成一根雨丝
    int dropID = gl_VertexID / 2;
    int isTop = gl_VertexID % 2; // 0 是底部，1 是顶部

    // 为这滴雨生成独特的随机起点 (0.0 ~ 1.0)
    float rx = hash(float(dropID) * 1.123);
    float ry = hash(float(dropID) * 2.345);
    float rz = hash(float(dropID) * 3.456);
    float speed = mix(10.0, 25.0, hash(float(dropID) * 4.567)); // 随机下落速度

    // 设定跟随相机的雨滴盒子大小 (40x40x40 米)
    float boxSize = 40.0;
    float halfBox = boxSize * 0.5;

    // 初始局部坐标
    vec3 localPos = vec3(rx * boxSize - halfBox, ry * boxSize - halfBox, rz * boxSize - halfBox);

    // 倾斜的下落方向 (风向) — 需与 C++ 端 rainDir 一致
    vec3 fallDir = normalize(vec3(0.1, -1.0, 0.1));

    // 应用下落与无限循环 (Modulo wrap-around)
    localPos += fallDir * uTime * speed;
    localPos.y = mod(localPos.y + halfBox, boxSize) - halfBox;
    localPos.x = mod(localPos.x + halfBox, boxSize) - halfBox;
    localPos.z = mod(localPos.z + halfBox, boxSize) - halfBox;

    // 如果是雨滴顶部，向风的相反方向拉伸形成长条雨丝
    float dropLength = mix(0.2, 0.5, uRainIntensity);
    if (isTop == 1) {
        localPos -= fallDir * dropLength;
    }

    // 加上相机位置，让盒子永远跟着玩家走
    vec3 worldPos = uCameraPos + localPos;

    // 不再整体下推雨滴，避免把雨滴推进低矮天花板内部
    // worldPos.y -= 1.0;

    gl_Position = uProjection * uView * vec4(worldPos, 1.0);
    FragPosRainSpace = uRainSpaceMatrix * vec4(worldPos, 1.0);
}
