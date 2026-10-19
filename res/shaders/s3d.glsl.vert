#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aUv;
uniform mat4 uWorld, uView, uProj;
uniform float uFlipV;
out vec2 vUv;
out vec4 vColor;
out float vFog;
void main() {
	vec4 p = uProj * (uView * (uWorld * vec4(aPos, 1.0)));
	gl_Position = p;
	vUv = vec2(aUv.x, uFlipV > 0.5 ? 1.0 - aUv.y : aUv.y);
	vColor = aColor;
	vFog = p.w;   // Diffuse_VS_Easy: output.Fog = output.Position.w
}