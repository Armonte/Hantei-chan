// Port of s3d.glsl.vert / s3d.glsl.frag (src/stage3d/s3d_view.cpp, kVs/kFs).
// vs_3_0 / ps_3_0. Same maths as the GLSL, same uniform names.
// Diffuse_VS_Easy / Diffuse_PS_Easy (Shader\fbxshader.txt): tex * vertex colour,
// unclamped fog lerp when g_FogColor.a >= 0.001 (alpha kept), alpha test GREATER 0.
// Diffuse_VS_Easy sets Fog = Position.w; the GLSL reads that back as vFog.

float4x4 uWorld;
float4x4 uView;
float4x4 uProj;
float    uFlipV;
float4   uFogColor;
float2   uFogCoord;
float    uAlphaRef;

sampler2D uTex : register(s0);

struct VS_INPUT {
	float3 aPos   : POSITION;
	float4 aColor : COLOR0;
	float2 aUv    : TEXCOORD0;
};

struct VS_OUTPUT {
	float4 Position : POSITION;
	float2 vUv      : TEXCOORD0;
	float4 vColor   : COLOR0;
	float  vFog     : TEXCOORD1;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
	VS_OUTPUT output;

	float4 p = mul(uProj, mul(uView, mul(uWorld, float4(input.aPos, 1.0))));
	output.Position = p;
	output.vUv = float2(input.aUv.x, uFlipV > 0.5 ? 1.0 - input.aUv.y : input.aUv.y);
	output.vColor = input.aColor;
	output.vFog = p.w;   // Diffuse_VS_Easy: output.Fog = output.Position.w
	return output;
}

float4 PSMain(VS_OUTPUT input) : COLOR
{
	float4 c = tex2D(uTex, input.vUv) * input.vColor;
	if (uFogColor.a >= 0.001) {
		float f = uFogCoord.x + input.vFog * uFogCoord.y;
		c.rgb = lerp(uFogColor.rgb, c.rgb, f);
	}
	if (c.a <= uAlphaRef) discard;   // alpha test GREATER 0
	return float4(c.rgb, c.a);
}
