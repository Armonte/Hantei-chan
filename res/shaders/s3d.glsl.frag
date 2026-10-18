#version 330 core
in vec2 vUv;
in vec4 vColor;
in float vFog;
uniform sampler2D uTex;
uniform vec4 uFogColor;
uniform vec2 uFogCoord;
uniform float uAlphaRef;
out vec4 oColor;
void main() {
	vec4 c = texture(uTex, vUv) * vColor;
	if (uFogColor.a >= 0.001) {
		float f = uFogCoord.x + vFog * uFogCoord.y;
		c.rgb = mix(uFogColor.rgb, c.rgb, f);
	}
	if (c.a <= uAlphaRef) discard;   // alpha test GREATER 0
	oColor = vec4(c.rgb, c.a);
}