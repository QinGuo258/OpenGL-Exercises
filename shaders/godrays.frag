#version 460 core
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D uInputTex;     // 输入的亮部提取图 (只有太阳/高光的黑白图)
uniform vec2 uLightScreenPos;    // 光源在 2D 屏幕上的位置 (0.0 ~ 1.0)
uniform float uDensity;          // 光柱密度/拖拽长度
uniform float uDecay;            // 光柱衰减率
uniform float uWeight;           // 采样权重

const int NUM_SAMPLES = 60;      // 采样次数，控制光柱的连贯性

void main() {
    vec2 textCoo = TexCoords;
    // 计算从当前像素指向太阳中心的向量
    vec2 deltaTextCoord = (textCoo - uLightScreenPos);
    // 将步长平分
    deltaTextCoord *= 1.0 / float(NUM_SAMPLES) * uDensity;

    vec3 color = texture(uInputTex, textCoo).rgb;
    float illuminationDecay = 1.0;

    // 沿着向量向太阳中心步进采样
    for(int i = 0; i < NUM_SAMPLES; i++) {
        textCoo -= deltaTextCoord; // 向着中心收缩
        vec3 sampleColor = texture(uInputTex, textCoo).rgb;
        // 采样颜色乘以衰减和权重
        sampleColor *= illuminationDecay * uWeight;
        color += sampleColor;
        // 每次迭代光线按比例衰减
        illuminationDecay *= uDecay;
    }

    FragColor = vec4(color, 1.0);
}
