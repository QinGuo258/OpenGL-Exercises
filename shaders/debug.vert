#version 460 core
layout (location = 0) in vec3 aPos;
uniform mat4 view;
uniform mat4 projection;
void main() {
    // 顶点已在世界空间，直接乘 view/projection
    gl_Position = projection * view * vec4(aPos, 1.0);
}
