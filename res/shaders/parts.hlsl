// Port of parts.glsl.vert / parts.glsl.frag (src/render.cpp, Render::sPartShader).
// vs_3_0 / ps_3_0. Same maths as the GLSL, same constant names:
//   ProjMtx, addColor; the vertex colour is the constant g_color (PLAN.md 5.1) because
//   the C++ disables attribute 2 and calls glVertexAttrib4fv(2, newColor) per part
//   (src/parts/parts.cpp:743). PRRV flips are baked into the UVs by Parts::Draw.
//
// col = tex * diffuse; col += spec (the game's PS_CharaPtDrawV; PRSP, alpha included).

float4x4 ProjMtx;
float4   g_color;      // glVertexAttrib4fv(2, newColor): frame tint * PRCL * PRCB
float4   addColor;     // PRSP specular

sampler2D Texture : register(s0);

struct VS_INPUT {
	float3 Position : POSITION;
	float4 UV       : TEXCOORD0;   // st = sample coordinate, pq = the flipped pair
};

struct VS_OUTPUT {
	float4 Position   : POSITION;
	float2 Frag_UV    : TEXCOORD0;
	float4 Frag_Color : COLOR0;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
	VS_OUTPUT output;

	output.Frag_UV = input.UV.xy;
	output.Frag_Color = g_color;
	output.Position = mul(ProjMtx, float4(input.Position, 1));
	return output;
}

float4 PSMain(VS_OUTPUT input) : COLOR
{
	float4 col = tex2D(Texture, input.Frag_UV);
	col *= input.Frag_Color;   // diffuse
	col += addColor;           // specular (PRSP), alpha included
	return col;
}
