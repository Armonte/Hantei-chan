// Port of bg_heat.glsl.vert and the two fragment passes bg_heat_a/b.glsl.frag
// (background/bg_renderer.cpp, kHeatVS + kHeatFS_A/B, BgPointBlur / sh_blur).
// vs_3_0 / ps_3_0. The two passes have different pixel shaders but share the
// one vertex shader, so they are two entry points, VSMain / PSMainA / PSMainB.
// Full-screen quad; vUV = aPos * 0.5 + 0.5, exactly as the GLSL.

// Pass A (sh_blur_1): k = 1 + fColorHosei*fValue, dir = normalize(centre - p),
//   T0 = k * sum_{i=1..8} w_i S(p + i*dir*fPower*0.2), T1 = k * S(p);
// Pass B (sh_blur_0): n = EXFADE08(p).r, B = T0(p + normalize(c - p)*n*fPower*3),
//   both B and T1 desaturated by fValue/2, out = lerp(T1, B, n * B.a).

float4 g_uRect;        // game view in scene-texture uv: x0, y0, w, h (GL, y up)
float  g_fPower;
float  g_fValue;
float  g_fColorHosei;
float2 g_uCenter;      // game uv, y down

sampler2D uScene : register(s0);
sampler2D uT0    : register(s0);
sampler2D uT1    : register(s1);
sampler2D uMask  : register(s2);

struct VS_OUTPUT {
	float4 Position : POSITION;
	float2 UV       : TEXCOORD0;
};

VS_OUTPUT VSMain(float2 aPos : POSITION)
{
	VS_OUTPUT output;
	output.UV = aPos * 0.5 + 0.5;
	output.Position = float4(aPos, 0.0, 1.0);
	return output;
}

struct PS_A_OUTPUT {
	float4 T0 : COLOR0;
	float4 T1 : COLOR1;
};

float4 SceneSample(float2 g)
{
	return tex2D(uScene, g_uRect.xy + float2(g.x, 1.0 - g.y) * g_uRect.zw);
}

// Pass A. Unrolled from the GLSL loop so ps_3_0 has no dynamic array index.
PS_A_OUTPUT PSMainA(VS_OUTPUT input) : COLOR
{
	PS_A_OUTPUT output;
	float2 p = float2(input.UV.x, 1.0 - input.UV.y);   // game uv, y down
	float2 d = normalize(float2(0.5, 0.5) - p + 1e-9);
	float st = g_fPower * 0.2;
	float4 acc = float4(0.0, 0.0, 0.0, 0.0);
	acc += SceneSample(p + d * st * 1.0) * 0.244;
	acc += SceneSample(p + d * st * 2.0) * 0.189;
	acc += SceneSample(p + d * st * 3.0) * 0.162;
	acc += SceneSample(p + d * st * 4.0) * 0.135;
	acc += SceneSample(p + d * st * 5.0) * 0.108;
	acc += SceneSample(p + d * st * 6.0) * 0.081;
	acc += SceneSample(p + d * st * 7.0) * 0.054;
	acc += SceneSample(p + d * st * 8.0) * 0.027;
	float k = 1.0 + g_fColorHosei * g_fValue;
	output.T0 = acc * k;
	output.T1 = SceneSample(p) * k;
	return output;
}

float3 desat(float3 x)
{
	float a = (x.r + x.g + x.b) / 3.0;
	return x + g_fValue * 0.5 * (float3(a, a, a) - x);
}

// Pass B.
float4 PSMainB(VS_OUTPUT input) : COLOR
{
	float2 p = float2(input.UV.x, 1.0 - input.UV.y);
	float n = tex2D(uMask, p).r;
	float2 d = normalize(g_uCenter - p + 1e-9);
	float2 q = p + d * n * g_fPower * 3.0;
	float4 B = tex2D(uT0, float2(q.x, 1.0 - q.y));
	float4 Sv = tex2D(uT1, input.UV);
	return float4(lerp(desat(Sv.rgb), desat(B.rgb), n * B.a), 1.0);
}
