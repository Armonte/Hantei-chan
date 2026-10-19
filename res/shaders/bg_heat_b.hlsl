// Port of bg_heat.glsl.vert + bg_heat_b.glsl.frag (src/background/bg_renderer.cpp,
// kHeatVS/kHeatFS_B, BgPointBlur / sh_blur.cfx pass B = sh_blur_0). vs_3_0 / ps_3_0.
// Same maths as the GLSL, same uniform names. The vertex shader text is identical to
// bg_heat_a.hlsl; the two passes are separate programs because they have separate
// pixel entry points.
//
// n = EXFADE08(p).r, B = T0(p + normalize(c - p)*n*fPower*3),
//   both B and T1 desaturated by fValue/2, out = lerp(T1, B, n * B.a).

float4 uCenter;      // game uv, y down
float  fPower;
float  fValue;

sampler2D uT0    : register(s0);
sampler2D uT1    : register(s1);
sampler2D uMask  : register(s2);

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

float3 desat(float3 x) { float a = (x.r + x.g + x.b) / 3.0; return x + fValue * 0.5 * (float3(a, a, a) - x); }

float4 PSMain(VS_OUTPUT input) : COLOR
{
	float2 p = float2(input.vUV.x, 1.0 - input.vUV.y);
	float n = tex2D(uMask, p).r;
	float2 d = normalize(uCenter - p + 1e-9);
	float2 q = p + d * n * fPower * 3.0;
	float4 B = tex2D(uT0, float2(q.x, 1.0 - q.y));
	float4 Sv = tex2D(uT1, input.vUV);
	return float4(lerp(desat(Sv.rgb), desat(B.rgb), n * B.a), 1.0);
}
