#version 460 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;
out vec2 TexCoords;
uniform mat4 uProjection;
uniform mat4 uModel;
uniform vec2 uUvScale;
uniform vec2 uUvOffset;
void main() {
    TexCoords = aTexCoords * uUvScale + uUvOffset;
    gl_Position = uProjection * uModel * vec4(aPos, 0.0, 1.0);
}
