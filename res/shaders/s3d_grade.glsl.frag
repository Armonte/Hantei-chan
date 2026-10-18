#version 330 core
in vec2 vUv;
uniform sampler2D uTex;
uniform float uLift, uGain;
out vec4 oColor;
void main() { vec3 c = texture(uTex, vUv).rgb; oColor = vec4(clamp(uLift + c * uGain, 0.0, 1.0), 1.0); }