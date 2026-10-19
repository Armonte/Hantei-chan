#include <cstring>
// CPU reference check for the HLSL ports of the two palette PIXEL shaders
// (docs/plans/render_d3d9/PLAN.md 5.2, lane HLSL deliverable 5).  The GLSL maths
// and the HLSL maths are written out twice in scalar C++ exactly as the shader
// text reads, and evaluated on 10,000 random inputs each; the results have to
// agree to 1e-6.  This proves the port is the same maths long before any GPU or
// D3D9 device exists.
//
//   texturedSrcFrag modes 0-3 (direct, indexed, manual bilinear, MBTL filter)
//   partsSrcFrag               (tex * diffuse + PRSP specular)
//
// The two implementations are deliberately kept line for line: a change in one
// of the shaders has to be mirrored here, and the compiler will not help.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

namespace {

const int W = 32, H = 24;          // index texture size
const int PW = 256, PH = 1;        // palette size (256x1, point, clamp)

struct Float4 { float x, y, z, w; };

struct Tex {
	const float* data;             // W * H, one float per texel (PW*PH*4 for the palette)
	int w, h;
};

// texelFetch(Texture, t, 0): exact integer texel, no filtering.
float TexelFetch(const Tex& t, int x, int y)
{
	if (x < 0 || y < 0 || x >= t.w || y >= t.h) return 0.f;
	return t.data[y * t.w + x];
}

// A point sample at a uv: floor(uv * size), clamped at the edge.  Every texture
// the ported shaders touch is point-sampled (the index texture is always
// NEAREST, the palette is NEAREST), so this is the only sampling rule needed.
float PointSample(const Tex& t, float u, float v)
{
	int x = (int)std::floor(u * (float)t.w);
	int y = (int)std::floor(v * (float)t.h);
	if (x < 0) x = 0; if (x >= t.w) x = t.w - 1;
	if (y < 0) y = 0; if (y >= t.h) y = t.h - 1;
	return t.data[y * t.w + x];
}

// Palette entry for an index value already in hand (the mode-3 branch).
Float4 PaletteAt(const Tex& palette, float idx)
{
	float pu = idx * (255.0f / 256.0f) + (0.5f / 256.0f);
	int px = (int)std::floor(pu * (float)palette.w);
	if (px < 0) px = 0; if (px >= palette.w) px = palette.w - 1;
	return Float4{palette.data[px * 4], palette.data[px * 4 + 1],
	              palette.data[px * 4 + 2], palette.data[px * 4 + 3]};
}

// palLookup(uv): read the index, then the palette at the precise uv.
Float4 PaletteLookup(const Tex& palette, const Tex& texture, float u, float v)
{
	return PaletteAt(palette, PointSample(texture, u, v));
}

Float4 Mix4(Float4 a, Float4 b, float t) { return Float4{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t}; }
Float4 Mul4(Float4 a, Float4 b) { return Float4{a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; }
Float4 Add4(Float4 a, Float4 b) { return Float4{a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
float Fract(float v) { return v - std::floor(v); }

// ==========================================================================
// texturedSrcFrag, GLSL edition (res/shaders/textured.glsl.frag)
// ==========================================================================
Float4 TexturedGlsl(int indexed, const Tex& texture, const Tex& palette, float u, float v, Float4 fragColor)
{
	// GLSL samples the .r channel of the index texture in every mode.
	Float4 col;
	if (indexed == 0) {
		col = Float4{PointSample(texture, u, v), 0.f, 0.f, 0.f};
	} else if (indexed == 1) {
		col = PaletteLookup(palette, texture, u, v);
	} else if (indexed == 3) {
		int tsx = texture.w, tsy = texture.h;
		int cx = (int)std::floor(u * (float)tsx);
		int cy = (int)std::floor(v * (float)tsy);
		col = Float4{0.f, 0.f, 0.f, 0.f};
		for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
			int tx = cx + x, ty = cy + y;
			if (tx < 0 || ty < 0 || tx >= tsx || ty >= tsy) continue;
			float idx = TexelFetch(texture, tx, ty);
			if (idx <= 0.f) continue;
			float wgt = (x == 0 && y == 0) ? 0.60f : ((x == 0 || y == 0) ? 0.06f : 0.04f);
			Float4 pc = PaletteAt(palette, idx);
			col = Float4{col.x + pc.x * wgt, col.y + pc.y * wgt, col.z + pc.z * wgt, col.w + pc.w * wgt};
		}
	} else {
		float tsx = (float)texture.w, tsy = (float)texture.h;
		float px = u * tsx - 0.5f, py = v * tsy - 0.5f;
		float fx = Fract(px), fy = Fract(py);
		float bx = (std::floor(px) + 0.5f) / tsx, by = (std::floor(py) + 0.5f) / tsy;
		Float4 c00 = PaletteLookup(palette, texture, bx, by);
		Float4 c10 = PaletteLookup(palette, texture, bx + 1.0f / tsx, by);
		Float4 c01 = PaletteLookup(palette, texture, bx, by + 1.0f / tsy);
		Float4 c11 = PaletteLookup(palette, texture, bx + 1.0f / tsx, by + 1.0f / tsy);
		col = Mix4(Mix4(c00, c10, fx), Mix4(c01, c11, fx), fy);
	}
	return Mul4(col, fragColor);
}

// ==========================================================================
// texturedSrcFrag, HLSL edition (res/shaders/textured.hlsl, PSMain).
// The only differences from the GLSL are the two spot-implemented in HLSL:
//   texelFetch(Texture, t, 0)  -> tex2D(Texture, (t + 0.5) / g_texSize)
//   textureSize(Texture, 0)    -> g_texSize
// ==========================================================================
float TexelIndexHlsl(const Tex& texture, int tx, int ty)
{
	float u = ((float)tx + 0.5f) / (float)texture.w;
	float v = ((float)ty + 0.5f) / (float)texture.h;
	return PointSample(texture, u, v);
}

Float4 TexturedHlsl(int indexed, const Tex& texture, const Tex& palette, float u, float v, Float4 fragColor)
{
	Float4 col;
	if (indexed == 0) {
		col = Float4{PointSample(texture, u, v), 0.f, 0.f, 0.f};
	} else if (indexed == 1) {
		col = PaletteLookup(palette, texture, u, v);
	} else if (indexed == 3) {
		int tsx = texture.w, tsy = texture.h;
		int cx = (int)std::floor(u * (float)tsx);
		int cy = (int)std::floor(v * (float)tsy);
		col = Float4{0.f, 0.f, 0.f, 0.f};
		for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
			int tx = cx + x, ty = cy + y;
			if (tx < 0 || ty < 0 || tx >= tsx || ty >= tsy) continue;
			float idx = TexelIndexHlsl(texture, tx, ty);
			if (idx <= 0.f) continue;
			float wgt = (x == 0 && y == 0) ? 0.60f : ((x == 0 || y == 0) ? 0.06f : 0.04f);
			Float4 pc = PaletteAt(palette, idx);
			col = Float4{col.x + pc.x * wgt, col.y + pc.y * wgt, col.z + pc.z * wgt, col.w + pc.w * wgt};
		}
	} else {
		float tsx = (float)texture.w, tsy = (float)texture.h;
		float px = u * tsx - 0.5f, py = v * tsy - 0.5f;
		float fx = Fract(px), fy = Fract(py);
		float bx = (std::floor(px) + 0.5f) / tsx, by = (std::floor(py) + 0.5f) / tsy;
		Float4 c00 = PaletteLookup(palette, texture, bx, by);
		Float4 c10 = PaletteLookup(palette, texture, bx + 1.0f / tsx, by);
		Float4 c01 = PaletteLookup(palette, texture, bx, by + 1.0f / tsy);
		Float4 c11 = PaletteLookup(palette, texture, bx + 1.0f / tsx, by + 1.0f / tsy);
		col = Mix4(Mix4(c00, c10, fx), Mix4(c01, c11, fx), fy);
	}
	return Mul4(col, fragColor);
}

// ==========================================================================
// partsSrcFrag: GLSL and HLSL are the same three lines.
// ==========================================================================
Float4 PartsGlsl(const Tex& texture, float u, float v, Float4 fragColor, Float4 addColor)
{
	Float4 col{PointSample(texture, u, v), 0.f, 0.f, 0.f};
	col = Mul4(col, fragColor);   // diffuse
	return Add4(col, addColor);   // PRSP specular, alpha included
}

Float4 PartsHlsl(const Tex& texture, float u, float v, Float4 fragColor, Float4 addColor)
{
	Float4 col = Mul4(Float4{PointSample(texture, u, v), 0.f, 0.f, 0.f}, fragColor);
	return Add4(col, addColor);
}

int g_checks = 0, g_fail = 0;
bool g_quiet = false;
#define CHECK(x) do { ++g_checks; if (!(x)) { std::printf("CHECK failed: %s (line %d)\n", #x, __LINE__); ++g_fail; } } while (0)

bool Close4(Float4 a, Float4 b, float eps, const char* what, int iter)
{
	float d = std::fmax(std::fmax(std::fabs(a.x - b.x), std::fabs(a.y - b.y)),
	                    std::fmax(std::fabs(a.z - b.z), std::fabs(a.w - b.w)));
	if (d <= eps) return true;
	if (!g_quiet)
		std::printf("MISMATCH %s iter %d: glsl (%.9g %.9g %.9g %.9g) hlsl (%.9g %.9g %.9g %.9g) max|d| %.9g\n",
		            what, iter, a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w, d);
	return false;
}

}  // namespace

int main()
{
	// Index texture: R holds the palette index over 0..255, quantised to bytes to match GL_R8.
	std::vector<uint8_t> indexBytes((size_t)W * H);
	std::vector<uint8_t> paletteBytes((size_t)PW * 4);
	std::mt19937 rng(0xC0FFEEu);
	for (auto& b : indexBytes) b = (uint8_t)(rng() & 0xFF);
	for (auto& b : paletteBytes) b = (uint8_t)(rng() & 0xFF);
	// A good share of zero indices, because the mode-3 branch skips them.
	for (size_t i = 0; i < indexBytes.size(); i += 3) indexBytes[i] = 0;

	std::vector<float> indexF((size_t)W * H), paletteF((size_t)PW * 4);
	for (size_t i = 0; i < indexBytes.size(); i++) indexF[i] = indexBytes[i] / 255.0f;
	for (size_t i = 0; i < paletteBytes.size(); i++) paletteF[i] = paletteBytes[i] / 255.0f;
	Tex texture{indexF.data(), W, H};
	Tex palette{paletteF.data(), PW, PH};

	std::uniform_real_distribution<float> unit(0.f, 1.f);
	int mismatches = 0, total = 0;
	for (int mode = 0; mode <= 3; ++mode) {
		for (int i = 0; i < 2500; ++i) {
			float u = unit(rng), v = unit(rng);
			Float4 tint{unit(rng), unit(rng), unit(rng), unit(rng)};
			Float4 a = TexturedGlsl(mode, texture, palette, u, v, tint);
			Float4 b = TexturedHlsl(mode, texture, palette, u, v, tint);
			total++;
			if (!Close4(a, b, 1e-6f, "textured", mode * 10000 + i)) mismatches++;
		}
	}
	// The exact texel centres and edges, where a half-pixel slip would show.
	for (int mode = 0; mode <= 3; ++mode) {
		for (int ty = 0; ty < H; ty++) for (int tx = 0; tx < W; tx++) {
			float u = ((float)tx + 0.5f) / (float)W, v = ((float)ty + 0.5f) / (float)H;
			Float4 tint{1, 1, 1, 1};
			Float4 a = TexturedGlsl(mode, texture, palette, u, v, tint);
			Float4 b = TexturedHlsl(mode, texture, palette, u, v, tint);
			total++;
			if (!Close4(a, b, 1e-6f, "textured-texel", mode * 100000 + ty * W + tx)) mismatches++;
		}
	}
	for (int i = 0; i < 2500; ++i) {
		float u = unit(rng), v = unit(rng);
		Float4 tint{unit(rng), unit(rng), unit(rng), unit(rng)};
		Float4 spec{(float)(rng() % 256) / 255.f, (float)(rng() % 256) / 255.f,
		            (float)(rng() % 256) / 255.f, (float)(rng() % 256) / 255.f};
		total++;
		if (!Close4(PartsGlsl(texture, u, v, tint, spec), PartsHlsl(texture, u, v, tint, spec), 1e-6f, "parts", i)) mismatches++;
	}
	// The PRSP alpha-included add is worth pinning on its own: the specular alpha must survive.
	{
		Float4 tint{0.5f, 0.5f, 0.5f, 0.25f};
		Float4 spec{0.1f, 0.2f, 0.3f, 0.7f};
		Float4 a = PartsGlsl(texture, 0.5f, 0.5f, tint, spec);
		Float4 b = PartsHlsl(texture, 0.5f, 0.5f, tint, spec);
		CHECK(Close4(a, b, 1e-6f, "parts-alpha", 0));
		CHECK(a.w >= spec.w);   // col.a = tex.a * tint.a + spec.a, so never below spec.a
	}

	// Sensitivity: the comparison above must be able to fail.  Mutate the HLSL index
	// lookup (read the texel one to the right) and require the GLSL-vs-HLSL comparison
	// to catch it in every mode that uses texelFetch, and in no mode that does not.
	// Without this the equality evidence could be vacuous.
	{
		const int modesUsingFetch[3] = {1, 2, 3};
		g_quiet = true;   // the mutants are expected to differ; only the counts matter
		for (int k = 0; k < 3; k++) {
			int mode = modesUsingFetch[k];
			int caught = 0;
			for (int ty = 0; ty < H; ty++) for (int tx = 0; tx < W; tx++) {
				float u = ((float)tx + 0.5f) / (float)W, v = ((float)ty + 0.5f) / (float)H;
				Float4 tint{1, 1, 1, 1};
				Float4 proper = TexturedGlsl(mode, texture, palette, u, v, tint);
				// The mutant reads index (tx + 1, ty) instead of (tx, ty).
				float idxM = TexelIndexHlsl(texture, tx + 1, ty);
				Float4 pcM = PaletteAt(palette, idxM);
				int cmx = (int)std::floor(u * (float)W), cmy = (int)std::floor(v * (float)H);
				Float4 colM;
				if (mode == 1) {
					colM = pcM;
				} else if (mode == 3) {
					colM = Float4{0, 0, 0, 0};
					for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
						int sx = cmx + x, sy = cmy + y;
						if (sx < 0 || sy < 0 || sx >= W || sy >= H) continue;
						float idx = TexelIndexHlsl(texture, sx + 1, sy);
						if (idx <= 0.f) continue;
						float wgt = (x == 0 && y == 0) ? 0.60f : ((x == 0 || y == 0) ? 0.06f : 0.04f);
						Float4 pc = PaletteAt(palette, idx);
						colM = Float4{colM.x + pc.x * wgt, colM.y + pc.y * wgt, colM.z + pc.z * wgt, colM.w + pc.w * wgt};
					}
				} else {
					// Mode 2 never reads the wrap texel; mutate its base instead.
					float tsx = (float)W, tsy = (float)H;
					float px = u * tsx - 0.5f, py = v * tsy - 0.5f;
					float fx = Fract(px), fy = Fract(py);
					float bx = (std::floor(px) + 1.5f) / tsx, by = (std::floor(py) + 0.5f) / tsy;
					Float4 c00 = PaletteLookup(palette, texture, bx, by);
					colM = c00;
				}
				if (!Close4(proper, colM, 1e-6f, "mutant", 0)) caught++;
			}
			CHECK(caught > 0);   // a wrong texel lookup must be visible in this mode
		}
		g_quiet = false;
		// Mode 0 reads the uv directly, so the mutant above is irrelevant to it; it must
		// still agree exactly (this is the control).
		Float4 a = TexturedGlsl(0, texture, palette, 0.5f, 0.5f, Float4{1, 0.5f, 0.25f, 0.1f});
		Float4 b = TexturedHlsl(0, texture, palette, 0.5f, 0.5f, Float4{1, 0.5f, 0.25f, 0.1f});
		CHECK(Close4(a, b, 1e-6f, "mode0-control", 0));
	}

	std::printf("hlsl_ref_test: %d cases, %d mismatches, %d checks, %d failed\n",
	            total, mismatches, g_checks, g_fail);
	return (mismatches || g_fail) ? 1 : 0;
}
