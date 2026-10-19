// Port of textured.glsl.vert / textured.glsl.frag (render.cpp, Render::sTextured).
// vs_3_0 / ps_3_0. Same maths as the GLSL, same constant names minus the prefix:
//   ProjMtx  -> g_ProjMtx,  indexed -> g_indexed,  Texture/Palette stay the sampler names.
// The GLSL vertex shader has a vec4 Color attribute at location 2, but the C++ disables
// that attribute array and feeds it with glVertexAttrib4fv(2, colorRgba) (render.cpp:441,
// :500), so it is a shader constant here (PLAN.md 5.1: the RHI makes it a constant).
// g_texSize replaces textureSize(); texelFetch(t, 0) becomes a point sample at
// (t + 0.5) / g_texSize. No D3D9 half-pixel offset in the shader (PLAN.md 5.3).

float4x4 g_ProjMtx;
float2   g_texSize;    // Texture dimensions, (w, h)
float4   g_Color;      // glVertexAttrib4fv(2, colorRgba)
int      g_indexed;    // 0 direct RGBA, 1 indexed nearest, 2 indexed + bilinear, 3 MBTL zoom > 1 CG filter

sampler2D Texture : register(s0);
sampler2D Palette : register(s1);

struct VS_INPUT {
	float2 Position : POSITION;
	float2 UV       : TEXCOORD0;
};

struct VS_OUTPUT {
	float4 Position : POSITION;
	float2 UV       : TEXCOORD0;
	float4 Color    : COLOR0;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
	VS_OUTPUT output;
	output.UV = input.UV;
	output.Color = g_Color;
	output.Position = mul(g_ProjMtx, float4(input.Position.xy, 0, 1));
	return output;
}

// Palette lookup for an 8bpp index texture (index stored as R/255).
float4 palLookup(float2 uv)
{
	float idx = tex2D(Texture, uv).r;
	return tex2D(Palette, float2(idx * (255.0 / 256.0) + (0.5 / 256.0), 0.5));
}

// Point sample of the index texture, the texelFetch(Texture, t, 0) of the GLSL.
float texelIndex(int2 t)
{
	return tex2D(Texture, (float2(t) + 0.5) / g_texSize).r;
}

float4 PSMain(VS_OUTPUT input) : COLOR
{
	float4 col;
	if (g_indexed == 0) {
		col = tex2D(Texture, input.UV.xy);
	} else if (g_indexed == 1) {
		col = palLookup(input.UV.xy);
	} else if (g_indexed == 3) {
		// MBTL TecCharaDrawBgColorFilter_V / ...SpecularFilter_V (gamedata/mbtl/Shader/sh_chara.txt
		// CharaDrawBgColorBaseFilterBokashi), picked by Han6Draw_SubmitCgSprite_ParentMtx 0x4A0C40 when the camera
		// zoom is > 1: a 3x3 point-sampled neighbourhood one texel apart (g_fStepTexture1DotSizeW/H = 1 / (rect w+1),
		// 1 / (rect h+1), and the CG texture is exactly (w+1) x (h+1)), palette colour weighted .04/.06/.04,
		// .06/.60/.06, .04/.06/.04, summed over the texels whose index is not 0, with no renormalisation; none -> 0.
		// Texels outside the image read as index 0 (the editor image's own border; the game's sampler wraps).
		int2 ts = int2(g_texSize);
		int2 c = int2(floor(input.UV.xy * g_texSize));
		col = float4(0.0, 0.0, 0.0, 0.0);
		for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
			int2 t = c + int2(x, y);
			if (t.x < 0 || t.y < 0 || t.x >= ts.x || t.y >= ts.y) continue;
			float idx = texelIndex(t);
			if (idx <= 0.0) continue;
			float w = (x == 0 && y == 0) ? 0.60 : ((x == 0 || y == 0) ? 0.06 : 0.04);
			col += tex2D(Palette, float2(idx * (255.0 / 256.0) + (0.5 / 256.0), 0.5)) * w;
		}
	} else {
		// Manual bilinear AFTER palette lookup (filtering raw indices is garbage).
		float2 ts = g_texSize;
		float2 p = input.UV.xy * ts - 0.5;
		float2 f = frac(p);
		float2 base = (floor(p) + 0.5) / ts;
		float4 c00 = palLookup(base);
		float4 c10 = palLookup(base + float2(1.0, 0.0) / ts);
		float4 c01 = palLookup(base + float2(0.0, 1.0) / ts);
		float4 c11 = palLookup(base + float2(1.0, 1.0) / ts);
		col = lerp(lerp(c00, c10, f.x), lerp(c01, c11, f.x), f.y);
	}

	return col * input.Color;
}
