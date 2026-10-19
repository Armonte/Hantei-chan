// Port of bg_blur.glsl.vert / bg_blur.glsl.frag (background/bg_renderer.cpp,
// kBlurVS/kBlurFS, MBAACC sakura bloom TecSakuraBloom).
// vs_3_0 / ps_3_0. Full-screen quad; the vertex stage makes the UV the same way
// (aPos * 0.5 + 0.5). uMode 0 = 9-tap blur along uStep, 1 = plain copy.

float2   g_uStep;
int      g_uMode;

sampler2D uTex : register(s0);

struct VS_OUTPUT {
	float4 Position : POSITION;
	float2 UV       : TEXCOORD0;
};

VS_OUTPUT VSMain(float2 aPos : POSITION)
{
	VS_OUTPUT output;
	output.UV = aPos * 0.5 + 0.5;
	output.Position = float4(aPos, 0.0, 1.0);
	return output;
}

float4 PSMain(VS_OUTPUT input) : COLOR
{
	if (g_uMode == 1) { return tex2D(uTex, input.UV); }
	float2 uv = input.UV;
	float2 step = g_uStep;
	float4 o = float4(0.0, 0.0, 0.0, 0.0);
	o += tex2D(uTex, uv + step *  4.0) * 0.05;
	o += tex2D(uTex, uv + step *  3.0) * 0.075;
	o += tex2D(uTex, uv + step *  2.0) * 0.1;
	o += tex2D(uTex, uv + step *  1.0) * 0.15;
	o += tex2D(uTex, uv)               * 0.25;
	o += tex2D(uTex, uv + step * -1.0) * 0.15;
	o += tex2D(uTex, uv + step * -2.0) * 0.1;
	o += tex2D(uTex, uv + step * -3.0) * 0.075;
	o += tex2D(uTex, uv + step * -4.0) * 0.05;
	return o;
}
