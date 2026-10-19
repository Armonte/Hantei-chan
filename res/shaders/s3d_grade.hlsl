// Port of s3d_grade.glsl.vert / s3d_grade.glsl.frag (stage3d/s3d_view.cpp,
// kGradeVs/kGradeFs). Stage composite, TecBgNoSpeculer: rgb = clamp(lift + c * gain, 0, 1).
// vs_3_0 / ps_3_0. The GLSL vertex stage has no attributes: it synthesises a
// full-screen triangle from gl_VertexID. ps_3_0 has no VPOS-free equivalent, so the
// triangle comes from a vertex buffer the same way (the caller supplies three
// vertices; g_vs3d_grade.mattr covers it). Values match the GLSL exactly.

float g_uLift;
float g_uGain;

sampler2D uTex : register(s0);

struct VS_OUTPUT {
	float4 Position : POSITION;
	float2 UV       : TEXCOORD0;
};

// The GLSL triangle: vertex i -> p = ((i & 1) * 2, (i >> 1) * 2), vUv = p,
// gl_Position = vec4(p * 2 - 1, 0, 1). i = 0 (0,0), 1 (2,0), 2 (0,2).
VS_OUTPUT VSMain(float2 aPos : POSITION)
{
	VS_OUTPUT output;
	float2 p = aPos;
	output.UV = p;
	output.Position = float4(p * 2.0 - 1.0, 0.0, 1.0);
	return output;
}

float4 PSMain(VS_OUTPUT input) : COLOR
{
	float3 c = tex2D(uTex, input.UV).rgb;
	return float4(clamp(g_uLift + c * g_uGain, 0.0, 1.0), 1.0);
}
