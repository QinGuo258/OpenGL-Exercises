#version 460 core
in vec2 TexCoords;
out vec4 FragColor;
uniform sampler2D uTexture;

void main() {
    float occ = texture(uTexture, TexCoords).r;
    FragColor = vec4(occ, occ, occ, 1.0);
}
