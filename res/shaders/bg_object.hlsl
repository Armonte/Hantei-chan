// Port of bg_object.glsl.vert / bg_object.glsl.frag (src/background/bg_renderer.cpp,
// kVS/kFS). vs_3_0 / ps_3_0. Same maths as the GLSL, same uniform names.
// Vertex: pos(3) uv(2) colour(4). The fragment stage models the D3D fixed function
// the game uses: texel * diffuse (COLOROP/ALPHAOP = MODULATE), then + specular RGB
// (D3DRS_SPECULARENABLE = 1; PAT part +68..70).

float4x4 uProjView;
float    uAlpha;
float4   uTint;      // per-draw diffuse
float3   uAdd;       // per-draw specular (added after the texture stage)

sampler2D uTexture : register(s0);

struct VS_INPUT {
	float3 aPos   : POSITION;
	float2 aUV    : TEXCOORD0;
	float4 aColor : COLOR0;
};

struct VS_OUTPUT {
	float4 Position : POSITION;
	float2 vUV      : TEXCOORD0;
	float4 vCol     : COLOR0;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
	VS_OUTPUT output;

	output.Position = mul(uProjView, float4(input.aPos, 1.0));
	output.vUV = input.aUV;
	output.vCol = input.aColor;
	return output;
}

float4 PSMain(VS_OUTPUT input) : COLOR
{
	float4 c = tex2D(uTexture, input.vUV);
	float4 d = uTint * input.vCol;
	return float4(c.rgb * d.rgb + uAdd, c.a * uAlpha * d.a);
}
