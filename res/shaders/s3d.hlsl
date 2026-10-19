// Port of s3d.glsl.vert / s3d.glsl.frag (stage3d/s3d_view.cpp, kVs/kFs).
// vs_3_0 / ps_3_0. Diffuse_VS_Easy / Diffuse_PS_Easy from Shader\fbxshader.txt:
// tex * vertex colour, unclamped fog lerp when g_uFogColor.a >= 0.001 (alpha kept),
// alpha test GREATER 0. The vertex stage passes Fog = Position.w.

float4x4 g_uWorld;
float4x4 g_uView;
float4x4 g_uProj;
float    g_uFlipV;
float4   g_uFogColor;
float2   g_uFogCoord;
float    g_uAlphaRef;

sampler2D uTex : register(s0);

struct VS_INPUT {
	float3 aPos   : POSITION;
	float4 aColor : COLOR0;
	float2 aUv    : TEXCOORD0;
};

struct VS_OUTPUT {
	float4 Position : POSITION;
	float2 UV       : TEXCOORD0;
	float4 Color    : COLOR0;
	float  Fog      : TEXCOORD1;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
	VS_OUTPUT output;
	float4 p = mul(g_uProj, mul(g_uView, mul(g_uWorld, float4(input.aPos, 1.0))));
	output.Position = p;
	output.UV = float2(input.aUv.x, g_uFlipV > 0.5 ? 1.0 - input.aUv.y : input.aUv.y);
	output.Color = input.aColor;
	output.Fog = p.w;   // Diffuse_VS_Easy: output.Fog = output.Position.w
	return output;
}

float4 PSMain(VS_OUTPUT input) : COLOR
{
	float4 c = tex2D(uTex, input.UV) * input.Color;
	if (g_uFogColor.a >= 0.001) {
		float f = g_uFogCoord.x + input.Fog * g_uFogCoord.y;
		c.rgb = lerp(g_uFogColor.rgb, c.rgb, f);
	}
	if (c.a <= g_uAlphaRef) discard;   // alpha test GREATER 0
	return float4(c.rgb, c.a);
}
