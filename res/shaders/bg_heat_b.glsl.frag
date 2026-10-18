
#version 330 core
in vec2 vUV;
uniform sampler2D uT0, uT1, uMask;
uniform vec2 uCenter;     // game uv, y down
uniform float fPower, fValue;
out vec4 FragColor;
vec3 desat(vec3 x) { float a = (x.r + x.g + x.b) / 3.0; return x + fValue * 0.5 * (vec3(a) - x); }
void main() {
    vec2 p = vec2(vUV.x, 1.0 - vUV.y);
    float n = texture(uMask, p).r;
    vec2 d = normalize(uCenter - p + 1e-9);
    vec2 q = p + d * n * fPower * 3.0;
    vec4 B = texture(uT0, vec2(q.x, 1.0 - q.y));
    vec4 Sv = texture(uT1, vUV);
    FragColor = vec4(mix(desat(Sv.rgb), desat(B.rgb), n * B.a), 1.0);
}
