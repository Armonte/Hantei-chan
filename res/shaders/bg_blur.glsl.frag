
#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uStep;
uniform int uMode;
out vec4 FragColor;
void main() {
    if (uMode == 1) { FragColor = texture(uTex, vUV); return; }
    vec4 o = vec4(0.0);
    o += texture(uTex, vUV + uStep *  4.0) * 0.05;
    o += texture(uTex, vUV + uStep *  3.0) * 0.075;
    o += texture(uTex, vUV + uStep *  2.0) * 0.1;
    o += texture(uTex, vUV + uStep *  1.0) * 0.15;
    o += texture(uTex, vUV)                 * 0.25;
    o += texture(uTex, vUV + uStep * -1.0) * 0.15;
    o += texture(uTex, vUV + uStep * -2.0) * 0.1;
    o += texture(uTex, vUV + uStep * -3.0) * 0.075;
    o += texture(uTex, vUV + uStep * -4.0) * 0.05;
    FragColor = o;
}
