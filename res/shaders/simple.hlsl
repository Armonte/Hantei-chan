// Port of simple.glsl.vert / simple.glsl.frag (src/render.cpp, Render::sSimple).
// vs_3_0 / ps_3_0. Same maths as the GLSL, same names:
//   Frag_Color = vec4(Color, Alpha), position through ProjMtx.
// No D3D9 half-pixel offset here: it is applied once in the projection (PLAN.md 5.3).

float4x4 ProjMtx;
float    Alpha;

struct VS_INPUT {
	float3 Position : POSITION;
	float3 Color    : COLOR0;
};

struct VS_OUTPUT {
	float4 Position     : POSITION;
	float4 Frag_Color   : COLOR0;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
	VS_OUTPUT output;

	output.Frag_Color = float4(input.Color, Alpha);
	output.Position = mul(ProjMtx, float4(input.Position, 1));
	return output;
}

float4 PSMain(VS_OUTPUT input) : COLOR
{
	return input.Frag_Color;
}
