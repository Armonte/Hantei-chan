// Port of parts.glsl.vert / parts.glsl.frag (render.cpp, Render::sPartShader).
// vs_3_0 / ps_3_0. Same maths as the GLSL: col = tex * diffuse; col += spec
// (the game's PS_CharaPtDrawV; PRSP specular, alpha included). PRRV flips are
// baked into the UVs by Parts::Draw. Attribute 2 is a constant for the same
// reason as textured.hlsl: the C++ disables the array and uses
// glVertexAttrib4fv(2, newColor) per part (parts.cpp:743).

float4x4 g_ProjMtx;
float4   g_Color;      // glVertexAttrib4fv(2, newColor): frame tint * PRCL * PRCB
float4   g_addColor;   // PRSP specular (addColor namespace in the GLSL)

sampler2D Texture : register(s0);

struct VS_INPUT {
	float3 Position : POSITION;
	float4 UV       : TEXCOORD0;   // st = sample coordinate, the other pair is the flip
};

struct VS_OUTPUT {
	float4 Position : POSITION;
	float2 UV       : TEXCOORD0;
	float4 Color    : COLOR0;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
	VS_OUTPUT output;
	output.UV = input.UV.xy;
	output.Color = g_Color;
	output.Position = mul(g_ProjMtx, float4(input.Position, 1));
	return output;
}

float4 PSMain(VS_OUTPUT input) : COLOR
{
	float4 col = tex2D(Texture, input.UV);
	col *= input.Color;   // diffuse
	col += g_addColor;    // specular (PRSP), alpha included
	return col;
}
