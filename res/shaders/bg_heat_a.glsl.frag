
#version 330 core
in vec2 vUV;
uniform sampler2D uScene;
uniform vec4 uRect;       // game view in scene-texture uv: x0, y0, w, h (GL, y up)
uniform float fPower, fValue, fColorHosei;
layout (location = 0) out vec4 oT0;
layout (location = 1) out vec4 oT1;
vec4 S(vec2 g) { return texture(uScene, uRect.xy + vec2(g.x, 1.0 - g.y) * uRect.zw); }
void main() {
    vec2 p = vec2(vUV.x, 1.0 - vUV.y);          // game uv, y down
    vec2 d = normalize(vec2(0.5) - p + 1e-9);
    float st = fPower * 0.2;
    float w[8] = float[](0.244, 0.189, 0.162, 0.135, 0.108, 0.081, 0.054, 0.027);
    vec4 acc = vec4(0.0);
    for (int i = 0; i < 8; ++i) acc += S(p + d * st * float(i + 1)) * w[i];
    float k = 1.0 + fColorHosei * fValue;
    oT0 = acc * k;
    oT1 = S(p) * k;
}
