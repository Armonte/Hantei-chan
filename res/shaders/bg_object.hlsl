// Port of bg_object.glsl.vert / bg_object.glsl.frag (background/bg_renderer.cpp, kVS/kFS).
// vs_3_0 / ps_3_0. Models the D3D fixed function the game uses:
// texel * diffuse (COLOROP/ALPHAOP = MODULATE), then + specular RGB.

float4x4 g_uProjView;
float    g_uAlpha;
float4   g_uTint;    // per-draw diffuse
float3   g_uAdd;     // per-draw specular (added after the texture stage)

sampler2D uTexture : register(s0);

struct VS_INPUT {
	float3 aPos   : POSITION;
	float2 aUV    : TEXCOORD0;
	float4 aColor : COLOR0;
};

struct VS_OUTPUT {
	float4 Position : POSITION;
	float2 UV       : TEXCOORD0;
	float4 Color    : COLOR0;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
	VS_OUTPUT output;
	output.Position = mul(g_uProjView, float4(input.aPos, 1.0));
	output.UV = input.aUV;
	output.Color = input.aColor;
	return output;
}

float4 PSMain(VS_OUTPUT input) : COLOR
{
	float4 c = tex2D(uTexture, input.UV);
	float4 d = g_uTint * input.Color;
	return float4(c.rgb * d.rgb + g_uAdd, c.a * g_uAlpha * d.a);
}
