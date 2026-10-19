// Port of s3d_grade.glsl.vert / s3d_grade.glsl.frag (src/stage3d/s3d_view.cpp,
// kGradeVs/kGradeFs). vs_3_0 / ps_3_0. Stage composite (TecBgNoSpeculer):
// rgb = clamp(lift + c * gain, 0, 1), alpha 1. Same maths as the GLSL, same names.
//
// The GLSL vertex stage has no attributes: it grows a full-screen triangle out of
// gl_VertexID (i -> p = ((i & 1) * 2, (i >> 1) * 2), gl_Position = vec4(p * 2 - 1, 0, 1)).
// A D3D9 vs_3_0 has no gl_VertexID, so the caller passes the same three points as a
// vertex buffer: (0,0), (2,0), (0,2). The maths below is the GLSL's, unchanged.

float uLift;
float uGain;

sampler2D uTex : register(s0);

struct VS_OUTPUT {
	float4 Position : POSITION;
	float2 vUv      : TEXCOORD0;
};

VS_OUTPUT VSMain(float2 aPos : POSITION)
{
	VS_OUTPUT output;

	float2 p = aPos;
	output.vUv = p;
	output.Position = float4(p * 2.0 - 1.0, 0.0, 1.0);
	return output;
}

float4 PSMain(VS_OUTPUT input) : COLOR
{
	float3 c = tex2D(uTex, input.vUv).rgb;
	return float4(clamp(uLift + c * uGain, 0.0, 1.0), 1.0);
}
