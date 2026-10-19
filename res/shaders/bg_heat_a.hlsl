// Port of bg_heat.glsl.vert + bg_heat_a.glsl.frag (src/background/bg_renderer.cpp,
// kHeatVS/kHeatFS_A, BgPointBlur / sh_blur.cfx pass A = sh_blur_1). vs_3_0 / ps_3_0.
// Same maths as the GLSL, same uniform names. The vertex shader is shared with
// bg_heat_b.hlsl so it appears in both files with the same text.
//
// k = 1 + fColorHosei*fValue, dir = normalize(centre - p),
//   T0 = k * sum_{i=1..8} w_i S(p + i*dir*fPower*0.2), T1 = k * S(p).
// Writes T0 and T1 as MRT.
//
// The GLSL loop over the weight array is unrolled here: ps_3_0 forbids dynamically
// indexing a local array (the uniform array in the GLSL is not what is indexed).
// The weights are the ones written out in the GLSL.

float4 uRect;         // game view in scene-texture uv: x0, y0, w, h (GL, y up)
float  fPower;
float  fValue;
float  fColorHosei;

sampler2D uScene : register(s0);

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

float4 S(float2 g)
{
	return tex2D(uScene, uRect.xy + float2(g.x, 1.0 - g.y) * uRect.zw);
}

struct PS_OUTPUT {
	float4 oT0 : COLOR0;
	float4 oT1 : COLOR1;
};

PS_OUTPUT PSMain(VS_OUTPUT input) : COLOR
{
	PS_OUTPUT output;

	float2 p = float2(input.vUV.x, 1.0 - input.vUV.y);   // game uv, y down
	float2 d = normalize(float2(0.5, 0.5) - p + 1e-9);
	float st = fPower * 0.2;
	float4 acc = float4(0, 0, 0, 0);
	acc += S(p + d * st * 1.0) * 0.244;
	acc += S(p + d * st * 2.0) * 0.189;
	acc += S(p + d * st * 3.0) * 0.162;
	acc += S(p + d * st * 4.0) * 0.135;
	acc += S(p + d * st * 5.0) * 0.108;
	acc += S(p + d * st * 6.0) * 0.081;
	acc += S(p + d * st * 7.0) * 0.054;
	acc += S(p + d * st * 8.0) * 0.027;
	float k = 1.0 + fColorHosei * fValue;
	output.oT0 = acc * k;
	output.oT1 = S(p) * k;
	return output;
}
