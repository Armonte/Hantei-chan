
#version 330 core
in vec2 vUV;
in vec4 vCol;
uniform sampler2D uTexture;
uniform float uAlpha;
uniform vec4 uTint;   // per-draw diffuse
uniform vec3 uAdd;    // per-draw specular (added after the texture stage)
out vec4 FragColor;
void main() {
    vec4 c = texture(uTexture, vUV);
    vec4 d = uTint * vCol;
    FragColor = vec4(c.rgb * d.rgb + uAdd, c.a * uAlpha * d.a);
}
