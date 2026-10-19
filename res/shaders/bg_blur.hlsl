// Port of bg_blur.glsl.vert / bg_blur.glsl.frag (src/background/bg_renderer.cpp,
// kBlurVS/kBlurFS, the MBAACC sakura bloom TecSakuraBloom). vs_3_0 / ps_3_0.
// Same maths as the GLSL, same uniform names. Full-screen quad:
// vUV = aPos * 0.5 + 0.5. uMode 0 = the 9-tap blur along uStep, 1 = plain copy.

float2   uStep;
int      uMode;

sampler2D uTex : register(s0);

struct VS_OUTPUT {
	float4 Position : POSITION;
	float2 vUV      : TEXCOORD0;
};

VS_OUTPUT VSMain(float2 aPos : POSITION)
{
	VS_OUTPUT output;

	output.vUV = aPos * 0.5 + 0.5;
	output.Position = float4(aPos, 0.0, 1.0);
	return output;
}

float4 PSMain(VS_OUTPUT input) : COLOR
{
	if (uMode == 1) { return tex2D(uTex, input.vUV); }
	float4 o = float4(0, 0, 0, 0);
	o += tex2D(uTex, input.vUV + uStep *  4.0) * 0.05;
	o += tex2D(uTex, input.vUV + uStep *  3.0) * 0.075;
	o += tex2D(uTex, input.vUV + uStep *  2.0) * 0.1;
	o += tex2D(uTex, input.vUV + uStep *  1.0) * 0.15;
	o += tex2D(uTex, input.vUV)                * 0.25;
	o += tex2D(uTex, input.vUV + uStep * -1.0) * 0.15;
	o += tex2D(uTex, input.vUV + uStep * -2.0) * 0.1;
	o += tex2D(uTex, input.vUV + uStep * -3.0) * 0.075;
	o += tex2D(uTex, input.vUV + uStep * -4.0) * 0.05;
	return o;
}
