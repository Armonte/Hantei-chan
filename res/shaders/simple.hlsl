// Port of simple.glsl.vert / simple.glsl.frag (render.cpp, Render::sSimple).
// vs_3_0 / ps_3_0. Same maths as the GLSL: Frag_Color = vec4(Color, Alpha),
// position through ProjMtx, fragment writes the interpolated colour.
// The pixel-centre half offset is applied once in the projection (PLAN.md 5.3), not here.

float4x4 g_ProjMtx;
float    g_Alpha;

struct VS_INPUT {
	float3 Position : POSITION;
	float3 Color    : COLOR0;
};

struct VS_OUTPUT {
	float4 Position : POSITION;
	float4 Color    : COLOR0;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
	VS_OUTPUT output;
	output.Color = float4(input.Color, g_Alpha);
	output.Position = mul(g_ProjMtx, float4(input.Position, 1.0));
	return output;
}

float4 PSMain(VS_OUTPUT input) : COLOR
{
	return input.Color;
}
