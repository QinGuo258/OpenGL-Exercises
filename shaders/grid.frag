#version 460 core
in vec3 WorldPos;
out vec4 FragColor;
void main() {
    // 基础灰色地板
    vec3 bgColor = vec3(0.5, 0.5, 0.5);
    // 白色网格线
    vec3 lineColor = vec3(1.0, 1.0, 1.0);

    // 使用 fwidth 使线条在远处依然清晰，并且利用 fract 取小数实现每 1 米一条线
    vec2 grid = abs(fract(WorldPos.xz - 0.5) - 0.5) / fwidth(WorldPos.xz);
    float line = min(grid.x, grid.y);

    // 线条粗细控制（比如小于 1.0 像素时认为是线）
    float lineWeight = 1.0 - min(line, 1.0);

    // 混合颜色
    vec3 color = mix(bgColor, lineColor, lineWeight);
    FragColor = vec4(color, 1.0);
}
