// Stage renderer. Started as a port of u4ick's bgmaketool MonoForm.Draw
// (orthographic, Y-down, camera pan in sprite positions, parallax delta only
// during a drag); everything the game does differently now follows the
// runtime RE of MBAA.exe / mbacPC.exe (docs/bg_research/BG_HA4_RE.md):
//   - live instances, not objects, are drawn (spawners, despawn, triggers)
//   - painter's order: band 0 by layer (255 -> 279), weather, band 1 by
//     layer; the host splits band 0 / band 1 around its characters (Pass)
//   - CG: frame +9 blend (1 alpha, 2 additive, 3 multiply), 0xEA vertex tint
//     (MBAACC), +20 alpha lerp, MBAC +16/+18 scale; objhdr+22 linear filter
//   - PAT: part ARGB diffuse + RGB specular, +49 additive, +50 linear, flip,
//     scale int/1000 lerped by frame +20; MBAC rotation +60, no part pos
//   - no u4ick dur-0 skip, no depth test (CPU sort)

#include "bg_renderer.h"
#include "../cg.h"
#include "../texture.h"
#include "../parts/parts.h"
#include "bg_pat.h"
#include "bg_gametex.h"
#include <algorithm>
#include <utility>
#include <fstream>
#include <iterator>
#include <string>
#include <cstring>
#include <vector>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <chrono>

namespace bg {

// ---- shaders ---------------------------------------------------------------
// Vertex: pos(3) uv(2) colour(4). The fragment stage models the D3D fixed
// function the game uses: texel * diffuse (COLOROP/ALPHAOP = MODULATE), then
// + specular RGB (D3DRS_SPECULARENABLE = 1; PAT part +68..70).

static const char* kVS = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec4 aColor;
uniform mat4 uProjView;
out vec2 vUV;
out vec4 vCol;
void main() {
    gl_Position = uProjView * vec4(aPos, 1.0);
    vUV = aUV;
    vCol = aColor;
}
)GLSL";

static const char* kFS = R"GLSL(
#version 330 core
in vec2 vUV;
in vec4 vCol;
uniform sampler2D uTexture;
uniform float uAlpha;
uniform vec4 uTint;   // per-draw diffuse
uniform vec3 uAdd;    // per-draw specular (added after the texture stage)
out vec4 FragColor;
void main() {
    vec4 c = texture(uTexture, vUV);
    vec4 d = uTint * vCol;
    FragColor = vec4(c.rgb * d.rgb + uAdd, c.a * uAlpha * d.a);
}
)GLSL";

static GLuint CompileShader(GLenum type, const char* src) {
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, nullptr);
	glCompileShader(s);
	GLint ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[1024];
		glGetShaderInfoLog(s, sizeof(log), nullptr, log);
		std::cerr << "[bg] shader compile failed: " << log << std::endl;
	}
	return s;
}

static GLuint LinkProgram(GLuint vs, GLuint fs) {
	GLuint p = glCreateProgram();
	glAttachShader(p, vs);
	glAttachShader(p, fs);
	glLinkProgram(p);
	GLint ok = 0;
	glGetProgramiv(p, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[1024];
		glGetProgramInfoLog(p, sizeof(log), nullptr, log);
		std::cerr << "[bg] program link failed: " << log << std::endl;
	}
	return p;
}

// ---- debug PNG writer (no zlib dependency — uncompressed deflate) ----------

namespace {

uint32_t g_crcTable[256];
bool     g_crcInit = false;

void InitCrc() {
	for (uint32_t n = 0; n < 256; ++n) {
		uint32_t c = n;
		for (int k = 0; k < 8; ++k)
			c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
		g_crcTable[n] = c;
	}
	g_crcInit = true;
}

uint32_t Crc32(const uint8_t* d, size_t n, uint32_t crc) {
	if (!g_crcInit) InitCrc();
	for (size_t i = 0; i < n; ++i)
		crc = g_crcTable[(crc ^ d[i]) & 0xff] ^ (crc >> 8);
	return crc;
}

uint32_t Adler32(const uint8_t* d, size_t n) {
	uint32_t a = 1, b = 0;
	for (size_t i = 0; i < n; ++i) { a = (a + d[i]) % 65521; b = (b + a) % 65521; }
	return (b << 16) | a;
}

// Write an 8-bit RGB image as a PNG using stored (uncompressed) deflate blocks.
void WritePNG(const char* path, const uint8_t* rgb, int w, int h) {
	std::vector<uint8_t> raw;
	raw.reserve((size_t)h * (1 + (size_t)w * 3));
	for (int y = 0; y < h; ++y) {
		raw.push_back(0);  // filter: none
		raw.insert(raw.end(), rgb + (size_t)y * w * 3,
		                      rgb + (size_t)y * w * 3 + (size_t)w * 3);
	}
	std::vector<uint8_t> zl;
	zl.push_back(0x78); zl.push_back(0x01);  // zlib header
	size_t pos = 0;
	while (pos < raw.size() || (pos == 0 && raw.empty())) {
		size_t chunk = std::min<size_t>(65535, raw.size() - pos);
		bool fin = (pos + chunk >= raw.size());
		zl.push_back(fin ? 1 : 0);
		zl.push_back(chunk & 0xff);        zl.push_back((chunk >> 8) & 0xff);
		uint16_t nlen = (uint16_t)~chunk;
		zl.push_back(nlen & 0xff);         zl.push_back((nlen >> 8) & 0xff);
		zl.insert(zl.end(), raw.begin() + pos, raw.begin() + pos + chunk);
		pos += chunk;
		if (fin) break;
	}
	uint32_t ad = Adler32(raw.data(), raw.size());
	zl.push_back((ad>>24)&0xff); zl.push_back((ad>>16)&0xff);
	zl.push_back((ad>>8)&0xff);  zl.push_back(ad&0xff);

	std::ofstream f(path, std::ios::binary);
	if (!f) return;
	const uint8_t sig[8] = {0x89,'P','N','G',0x0d,0x0a,0x1a,0x0a};
	f.write((const char*)sig, 8);
	auto wchunk = [&](const char* type, const uint8_t* d, size_t n) {
		uint8_t len[4] = {(uint8_t)(n>>24),(uint8_t)(n>>16),(uint8_t)(n>>8),(uint8_t)n};
		f.write((const char*)len, 4);
		f.write(type, 4);
		if (n) f.write((const char*)d, n);
		uint32_t c = 0xFFFFFFFFu;
		c = Crc32((const uint8_t*)type, 4, c);
		if (n) c = Crc32(d, n, c);
		c ^= 0xFFFFFFFFu;
		uint8_t cb[4] = {(uint8_t)(c>>24),(uint8_t)(c>>16),(uint8_t)(c>>8),(uint8_t)c};
		f.write((const char*)cb, 4);
	};
	uint8_t ihdr[13] = {
		(uint8_t)(w>>24),(uint8_t)(w>>16),(uint8_t)(w>>8),(uint8_t)w,
		(uint8_t)(h>>24),(uint8_t)(h>>16),(uint8_t)(h>>8),(uint8_t)h,
		8, 2, 0, 0, 0 };
	wchunk("IHDR", ihdr, 13);
	wchunk("IDAT", zl.data(), zl.size());
	wchunk("IEND", nullptr, 0);
}

} // namespace

// ---- lifecycle -------------------------------------------------------------

Renderer::Renderer() = default;

Renderer::~Renderer() {
	ClearTextureCache();
	if (whiteTex) glDeleteTextures(1, &whiteTex);
	if (standTex) glDeleteTextures(1, &standTex);
	if (vbo) glDeleteBuffers(1, &vbo);
	if (program) glDeleteProgram(program);
}

void Renderer::InitGL() {
	if (glInit) return;
	GLuint vs = CompileShader(GL_VERTEX_SHADER, kVS);
	GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFS);
	program = LinkProgram(vs, fs);
	glDeleteShader(vs);
	glDeleteShader(fs);
	uProjView = glGetUniformLocation(program, "uProjView");
	uTexture  = glGetUniformLocation(program, "uTexture");
	uTint     = glGetUniformLocation(program, "uTint");
	uAdd      = glGetUniformLocation(program, "uAdd");
	uAlphaLoc = glGetUniformLocation(program, "uAlpha");

	// No VAO — glad here doesn't expose glGenVertexArrays. Bind VBO and
	// re-set attribute pointers on every emit instead. Cheap.
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 9 * 6, nullptr, GL_DYNAMIC_DRAW);

	const uint8_t white[4] = {255, 255, 255, 255};
	glGenTextures(1, &whiteTex);
	glBindTexture(GL_TEXTURE_2D, whiteTex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glInit = true;
}

long Renderer::GetTextureBudgetKB() {
	if (budgetKB < 0) budgetKB = (file && file->GetCG() && file->GetCG()->m_loaded) ? GameTextureBudgetKB(*file->GetCG()) : 0;
	return budgetKB;
}

void Renderer::ClearTextureCache() {
	budgetKB = -1;
	for (auto& kv : textureCache) glDeleteTextures(1, &kv.second.id);
	textureCache.clear();
	if (dropTex) glDeleteTextures(1, &dropTex);
	dropTex = 0;
	dropTexW = dropTexH = 0;
	dropTexTried = false;
}

void Renderer::SetFile(File* f) {
	if (file != f) {
		ClearTextureCache();
		patParts.reset();
		patPartsBuilt = false;
	}
	file = f;
}

void Renderer::Update() {
	if (!enabled || !file || paused) return;
	// The game's background tick runs at a fixed 60 Hz. The editor renders
	// at the display refresh rate, so accumulate real time and step at 60 Hz.
	using clock = std::chrono::steady_clock;
	static clock::time_point last = clock::now();
	static double accum = 0.0;
	clock::time_point now = clock::now();
	double dt = std::chrono::duration<double>(now - last).count();
	last = now;
	if (dt > 0.25) dt = 0.25;            // clamp after a pause / window stall
	accum += dt;
	const double STEP = 1.0 / 60.0;
	for (int steps = 0; accum >= STEP && steps < 8; ++steps) {
		file->UpdateAnimations();
		accum -= STEP;
	}
}

// ---- texture cache ---------------------------------------------------------

GLuint Renderer::GetOrCreateTexture(int spriteId, int& outW, int& outH,
                                    int& outOriginX, int& outOriginY) {
	auto it = textureCache.find(spriteId);
	if (it != textureCache.end()) {
		outW = it->second.w;
		outH = it->second.h;
		outOriginX = it->second.originX;
		outOriginY = it->second.originY;
		return it->second.id;
	}
	if (!file) return 0;
	CG* cg = file->GetCG();
	if (!cg) return 0;
	if (gameTextures) {
		GameTexture gt;
		const bool dxt = file->GetGame() == Game::MBAACC && IsDxtStage();
		if (dxt) PrecomposeDxtBank(*cg);   // the game precomposes the whole bank at load
		if (!ComposeGameTexture(*cg, spriteId, dxt, gt)) return 0;
		GLuint id = 0;
		glGenTextures(1, &id);
		glBindTexture(GL_TEXTURE_2D, id);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		if (gt.dxt5) {
			// Let the GPU decode the DXT5 blocks, as the game's device does.
			glCompressedTexImage2D(GL_TEXTURE_2D, 0, 0x83F3 /*GL_COMPRESSED_RGBA_S3TC_DXT5_EXT*/, gt.texW, gt.texH, 0,
			                       (GLsizei)gt.blocks.size(), gt.blocks.data());
			if (glGetError() != GL_NO_ERROR) {
				std::vector<uint8_t> rgba;
				DecodeDxt5(gt.blocks.data(), gt.texW, gt.texH, rgba);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gt.texW, gt.texH, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
			}
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gt.texW, gt.texH, 0, GL_RGBA, GL_UNSIGNED_BYTE, gt.rgba.data());
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);   // D3DTADDRESS_CLAMP
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		textureCache[spriteId] = { id, gt.w, gt.h, gt.originX, gt.originY, gt.texW, gt.texH };
		outW = gt.w; outH = gt.h; outOriginX = gt.originX; outOriginY = gt.originY;
		return id;
	}
	ImageData* img = cg->draw_texture(spriteId, false, false);
	if (!img) return 0;

	GLuint id = 0;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img->width, img->height, 0,
	             GL_RGBA, GL_UNSIGNED_BYTE, img->pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	textureCache[spriteId] = { id, img->width, img->height, img->offsetX, img->offsetY, img->width, img->height };
	outW = img->width;
	outH = img->height;
	outOriginX = img->offsetX;
	outOriginY = img->offsetY;
	delete img;
	return id;
}

// ---- older-PAT -> Parts conversion ------------------------------------------

// Convert the embedded older-PAT into a Parts object so it draws through the
// editor's proven orthographic part renderer (Render::DrawBgPattern). The
// data models map 1:1: PAT texture -> PartGfx + Texture, cutout -> CutOut,
// pattern -> PartSet of PartProperty. Uploads textures to GL, so this must
// run with a current GL context (called lazily from Render()).
void Renderer::BuildPatParts() {
	patPartsBuilt = true;
	patParts.reset();
	if (!file) return;
	OldPat* op = file->GetOldPat();
	if (!op || !op->IsValid()) return;

	auto parts = std::make_unique<Parts>(file->GetCG());
	Parts& P = *parts;

	// --- textures: gfxMeta / textures are indexed by texture id ---
	// Reset pixel-unpack state — BuildPatParts runs mid-frame, so a stale
	// GL_UNPACK_ROW_LENGTH (left by ImGui / editor texture loads) would
	// shear every row and the texture comes out as a pale wash.
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	const std::vector<PatTexture>& texs = op->Textures();
	int maxTex = -1;
	for (int i = 0; i < (int)texs.size(); ++i)
		if (texs[i].size > 0) maxTex = i;
	for (int i = 0; i <= maxTex; ++i) {
		PartGfx<> g;
		g.id = i;
		Texture* tex = new Texture();
		if (i < (int)texs.size() && texs[i].size > 0) {
			g.w = g.h = texs[i].size;
			g.type = 21;
			// LoadDirect copies the data; bgr=true -> uploaded as GL_BGRA.
			tex->LoadDirect((char*)texs[i].bgra.data(),
			                texs[i].size, texs[i].size, true);
			tex->Apply(false, true);   // linear filtering
			g.textureIndex = tex->id;
		}
		P.textures.push_back(tex);
		P.gfxMeta.push_back(g);
	}

	// --- cutouts ---
	const std::map<int, PatCutout>& cuts = op->Cutouts();
	int maxCut = -1;
	for (auto& kv : cuts) maxCut = std::max(maxCut, kv.first);
	P.cutOuts.resize(maxCut + 1);
	for (int i = 0; i <= maxCut; ++i) P.cutOuts[i].id = i;
	for (auto& kv : cuts) {
		const PatCutout& c = kv.second;
		CutOut<>& co = P.cutOuts[kv.first];
		// Parts::Draw divides UVs by a fixed 256 (the tag-format UV unit), so
		// normalise the raw texel rect into 256-space: uv = texel*256/texSize.
		int tsz = (c.texture >= 0 && c.texture < (int)texs.size()
		           && texs[c.texture].size > 0) ? texs[c.texture].size : 256;
		// Cutout src rects are stored in 256-unit space already (MBAA.exe
		// BgPat_UploadTexturesAndScaleCutouts multiplies them by
		// texSize/256 before building texel UVs), so u = src/256 for every
		// texture size. The old texel*256/texSize conversion sampled only a
		// quarter of every 512px PAT texture (bg07/18/19/20/30/43/47/55-57).
		(void)tsz;
		co.uv[0] = c.srcX;
		co.uv[1] = c.srcY;
		co.uv[2] = c.srcW;
		co.uv[3] = c.srcH;
		co.xy[0] = c.originX;
		co.xy[1] = c.originY;
		co.wh[0] = c.quadW;
		co.wh[1] = c.quadH;
		co.texture = c.texture;
		co.shapeIndex = 0;
	}

	// --- patterns -> partSets ---
	const std::map<int, PatPattern>& pats = op->Patterns();
	int maxPat = -1;
	for (auto& kv : pats) maxPat = std::max(maxPat, kv.first);
	P.partSets.resize(maxPat + 1);
	for (int i = 0; i <= maxPat; ++i) P.partSets[i].partId = i;
	for (auto& kv : pats) {
		PartSet<>& ps = P.partSets[kv.first];
		ps.wasLoaded = true;
		int propId = 0;
		for (const PatPart& part : kv.second.parts) {
			PartProperty pp;
			pp.propId   = propId;
			pp.priority = (float)part.priority;
			pp.ppId     = part.cutoutRef;
			pp.x        = (int)part.posX;
			pp.y        = (int)part.posY;
			pp.scaleX   = part.scaleX;
			pp.scaleY   = part.scaleY;
			pp.flip     = part.flip;
			pp.additive = part.additive;
			pp.bgra[0]  = part.colB;
			pp.bgra[1]  = part.colG;
			pp.bgra[2]  = part.colR;
			pp.bgra[3]  = part.colA;
			ps.groups.push_back(pp);
			++propId;
		}
	}

	P.loaded = true;
	patParts = std::move(parts);
	std::cout << "[bg] built PAT Parts: " << P.gfxMeta.size() << " tex, "
	          << P.cutOuts.size() << " cutouts, "
	          << P.partSets.size() << " patterns" << std::endl;
}

// ---- helpers ---------------------------------------------------------------

namespace {

// mbacPC Math_LerpAngle10000ToDeg: angles in 1/10000 turn, shortest arc,
// result in degrees (x 0.036).
double LerpAngle10000ToDeg(int a, int b, float t) {
	const double k = 0.035999998;
	if (a <= b) {
		if (a >= b) return a * k;
		int d = b - a;
		if (d >= 5000) {
			double r = (a - (a - b + 10000) * t) * k;
			while (r < 0.0) r += 360.0;
			return r;
		}
		return (d * t + a) * k;
	}
	int d = a - b;
	if (d >= 5000) {
		double r = ((b - a + 10000) * t + a) * k;
		while (r > 360.0) r -= 360.0;
		return r;
	}
	return (a - d * t) * k;
}

// MBAA Math_Atan2Normalized (0x4d...): direction of (x, y) as a fraction of
// a turn, 0.5 = straight down. Note the game's pi constant 3.1415.
double Atan2Normalized(float x, float y) {
	if (x == 0.0f) return y > 0.0f ? 0.5 : 0.0;
	if (y == 0.0f) return x < 0.0f ? 0.75 : 0.25;
	float len = std::sqrt(y * y + x * x + 0.0f);
	float c = (float)(x / len);
	float v = (float)(std::acos(c) / 3.141499996185303 * 0.5);
	if (y < 0.0f) v = 1.0f - v;
	v += 0.25f;
	if (v >= 1.0f) return v - 1.0;
	return v;
}

bool LoadBmpRGBA(const std::string& path, std::vector<uint8_t>& rgba, int& w, int& h) {
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;
	std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	if (d.size() < 54 || d[0] != 'B' || d[1] != 'M') return false;
	auto r32 = [&](size_t o) { int32_t v; std::memcpy(&v, d.data() + o, 4); return v; };
	auto r16 = [&](size_t o) { uint16_t v; std::memcpy(&v, d.data() + o, 2); return v; };
	uint32_t off = (uint32_t)r32(10);
	w = r32(18);
	int hh = r32(22);
	int bpp = r16(28);
	if (w <= 0 || hh == 0 || (bpp != 32 && bpp != 24) || r32(30) != 0) return false;
	bool bottomUp = hh > 0;
	h = bottomUp ? hh : -hh;
	size_t stride = ((size_t)w * (bpp / 8) + 3) & ~(size_t)3;
	if (off + stride * h > d.size()) return false;
	rgba.assign((size_t)w * h * 4, 0);
	for (int y = 0; y < h; ++y) {
		const uint8_t* src = d.data() + off + stride * (bottomUp ? (h - 1 - y) : y);
		uint8_t* dst = rgba.data() + (size_t)y * w * 4;
		for (int x = 0; x < w; ++x) {
			const uint8_t* p = src + x * (bpp / 8);
			dst[x * 4 + 0] = p[2];
			dst[x * 4 + 1] = p[1];
			dst[x * 4 + 2] = p[0];
			dst[x * 4 + 3] = bpp == 32 ? p[3] : 255;
		}
	}
	return true;
}

} // namespace

// ---- low-level emit ----------------------------------------------------------

// xy: 4 corners (TL, TR, BR, BL) in stage-screen units, uv likewise.
void Renderer::EmitQuad(GLuint tex, const float xy[8], const float uv[8],
                        const float rgba[4], bool linear) {
	static const int idx[6] = {0, 1, 2, 2, 3, 0};
	float v[9 * 6];
	for (int i = 0; i < 6; ++i) {
		int c = idx[i];
		float* o = v + i * 9;
		o[0] = xy[c * 2]; o[1] = xy[c * 2 + 1]; o[2] = 0.0f;
		o[3] = uv[c * 2]; o[4] = uv[c * 2 + 1];
		o[5] = rgba[0]; o[6] = rgba[1]; o[7] = rgba[2]; o[8] = rgba[3];
	}
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(5 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	// Sampler mode: 1 = point (the game default), 2 = linear (object +22 /
	// PAT part +50 — MBAA sets byte 0x56447F).
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
	glUniform1i(uTexture, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

// A line as a thin quad `width` units wide, colour interpolated end to end.
void Renderer::EmitLine(float x0, float y0, float x1, float y1, float width,
                        const float c0[4], const float c1[4]) {
	float dx = x1 - x0, dy = y1 - y0;
	float len = std::sqrt(dx * dx + dy * dy);
	if (len <= 0.0f) return;
	float nx = -dy / len * width * 0.5f, ny = dx / len * width * 0.5f;
	const float xy[8] = { x0 + nx, y0 + ny, x1 + nx, y1 + ny,
	                      x1 - nx, y1 - ny, x0 - nx, y0 - ny };
	static const int idx[6] = {0, 1, 2, 2, 3, 0};
	const float* col[4] = {c0, c1, c1, c0};
	float v[9 * 6];
	for (int i = 0; i < 6; ++i) {
		int c = idx[i];
		float* o = v + i * 9;
		o[0] = xy[c * 2]; o[1] = xy[c * 2 + 1]; o[2] = 0.0f;
		o[3] = 0.5f; o[4] = 0.5f;
		o[5] = col[c][0]; o[6] = col[c][1]; o[7] = col[c][2]; o[8] = col[c][3];
	}
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(5 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, whiteTex);
	glUniform1i(uTexture, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

// ---- PAT pattern -------------------------------------------------------------

// Quad math is MBAA.exe Background_DrawInstance's PAT branch: per part (in
// BgPat_BuildPartDrawOrder order), the cutout quad spans
// (-origin)..(quad-origin), scaled by the part scale (int/1000), offset by the
// part position (int32 +36/+40), then by the instance position. No pivot, no
// perspective. MBAC (mbacPC 0x401ea0) differs: the part position is NOT read,
// and the quad is rotated by part +60 (1/10000 turn) about the cutout origin.
void Renderer::DrawPatPatternFlat(const PatPattern& pat, const PatPattern* next, float t,
                                  float worldX, float worldY, bool objLinear, bool mbac) {
	if (!patParts) return;
	static const int tX[4] = {0, 1, 1, 0};
	static const int tY[4] = {0, 0, 1, 1};
	glUniform1f(uAlphaLoc, 1.0f);   // the PAT branch ignores frame +9/+10

	for (const PatPart& part : pat.parts) {
		if (part.cutoutRef < 0 || part.cutoutRef >= (int)patParts->cutOuts.size())
			continue;
		const CutOut<>& cut = patParts->cutOuts[part.cutoutRef];
		if (cut.texture < 0 || cut.texture >= (int)patParts->gfxMeta.size())
			continue;
		GLuint glTex = (GLuint)patParts->gfxMeta[cut.texture].textureIndex;
		if (glTex == 0) continue;

		const int slot = part.partIndex;
		float sx = part.scaleX, sy = part.scaleY;
		double rotDeg = mbac ? part.rotation * 0.035999998 : 0.0;
		if (next && slot >= 0 && slot < 40) {
			// frame +20: lerp toward the next pattern's SAME slot.
			sx = ((float)(next->slotScaleX[slot] - pat.slotScaleX[slot]) * t + (float)pat.slotScaleX[slot]) * 0.001f;
			sy = ((float)(next->slotScaleY[slot] - pat.slotScaleY[slot]) * t + (float)pat.slotScaleY[slot]) * 0.001f;
			if (mbac) rotDeg = LerpAngle10000ToDeg(pat.slotRotation[slot], next->slotRotation[slot], t);
		}
		const float px = mbac ? 0.0f : part.posX;
		const float py = mbac ? 0.0f : part.posY;
		const float cr = (float)std::cos(rotDeg * 0.017453292519943295);
		const float sr = (float)std::sin(rotDeg * 0.017453292519943295);

		glBlendFunc(GL_SRC_ALPHA, part.additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
		glUniform4f(uTint, 1.0f, 1.0f, 1.0f, 1.0f);
		glUniform3f(uAdd, part.addR / 255.0f, part.addG / 255.0f, part.addB / 255.0f);
		const float col[4] = { part.colR / 255.0f, part.colG / 255.0f,
		                       part.colB / 255.0f, part.colA / 255.0f };

		float xy[8], uv[8];
		for (int i = 0; i < 4; ++i) {
			float lx = (float)(-cut.xy[0] + cut.wh[0] * tX[i]) * sx;
			float ly = (float)(-cut.xy[1] + cut.wh[1] * tY[i]) * sy;
			float rx = lx * cr - ly * sr;
			float ry = lx * sr + ly * cr;
			xy[i * 2]     = worldX + px + rx;
			xy[i * 2 + 1] = worldY + py + ry;
			if (!gameTextures) {
				int ux = (part.flip & 1) ? (1 - tX[i]) : tX[i];
				int vy = (part.flip & 2) ? (1 - tY[i]) : tY[i];
				// Cutout src rects are in 256-unit space (see BuildPatParts).
				uv[i * 2]     = (cut.uv[0] + cut.uv[2] * ux) / 256.0f;
				uv[i * 2 + 1] = (cut.uv[1] + cut.uv[3] * vy) / 256.0f;
			}
		}
		if (gameTextures) {
			// MBAA: BgPat_UploadTexturesAndScaleCutouts scales the src rect by
			// T/256 for textures over 256; the flip picks the start texel and
			// sign (Background_DrawInstance 0x4b73b9); HudVertex_SetTextureCoords
			// 0x4163b0 then maps u0 = (U + 0.5)/T, u1 = (U + dU)/T.
			const int T = std::max(1, patParts->gfxMeta[cut.texture].w);
			const int TH = std::max(1, patParts->gfxMeta[cut.texture].h);
			const int k = T > 256 ? T / 256 : 1, kh = TH > 256 ? TH / 256 : 1;
			int sx = cut.uv[0] * k, sy = cut.uv[1] * kh, sw = cut.uv[2] * k, sh = cut.uv[3] * kh;
			int U = sx, V = sy, dU = sw, dV = sh;
			if (part.flip & 1) { U = sx + sw - 1; dU = -sw; }
			if (part.flip & 2) { V = sy + sh - 1; dV = -sh; }
			const float u0 = (U + 0.5f) / T, v0 = (V + 0.5f) / TH;
			const float u1 = (float)(U + dU) / T, v1 = (float)(V + dV) / TH;
			// corners TL, TR, BR, BL
			uv[0] = u0; uv[1] = v0; uv[2] = u1; uv[3] = v0;
			uv[4] = u1; uv[5] = v1; uv[6] = u0; uv[7] = v1;
		}
		// MBAA 0x4b7389: part +49 (additive) sets blend 2 AND sampler 2
		// (linear); +50 or object +22 set linear alone.
		EmitQuad(glTex, xy, uv, col, part.linearFilter || objLinear || (!mbac && part.additive));
	}
	glUniform3f(uAdd, 0.0f, 0.0f, 0.0f);
}

// ---- projection ------------------------------------------------------------

// XNA's CreateOrthographicOffCenter(0, W, H, 0, 0, 1) with Scale(zoom)
// folded in (column-major for GL).
void Renderer::BuildProjView(int W, int H, float zoom, float m[16]) {
	const float l = 0.0f, r = (float)W, b = (float)H, t = 0.0f, n = 0.0f, f = 1.0f;
	float sx = 2.0f / (r - l) * zoom;
	float sy = 2.0f / (t - b) * zoom;
	float sz = -2.0f / (f - n);
	float tx = -(r + l) / (r - l);
	float ty = -(t + b) / (t - b);
	float tz = -(f + n) / (f - n);
	m[0]=sx; m[1]=0;  m[2]=0;  m[3]=0;
	m[4]=0;  m[5]=sy; m[6]=0;  m[7]=0;
	m[8]=0;  m[9]=0;  m[10]=sz;m[11]=0;
	m[12]=tx;m[13]=ty;m[14]=tz;m[15]=1;
	// D3D9 samples pixels at integer screen coordinates, GL at +0.5: move the
	// geometry half a pixel right/down so every edge covers the same pixels
	// and every UV lands where the game's does.
	m[12] += 1.0f / (float)W;
	m[13] -= 1.0f / (float)H;
}

// ---- CG sprite ---------------------------------------------------------------

void Renderer::DrawSprite(int spriteId,
                          float x, float y, float w, float h,
                          float alpha, int blendMode,
                          float tintRGB, bool linear)
{
	int sw, sh, ox, oy;
	GLuint tex = GetOrCreateTexture(spriteId, sw, sh, ox, oy);
	if (tex == 0) return;
	// MBAA.exe Background_DrawInstance: frame +9 = 0/1 alpha, 2 additive
	// (SRCALPHA, ONE), 3 = blend mode 4 = MULTIPLY (DESTCOLOR, ZERO).
	if (blendMode == 2)      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	else if (blendMode == 3) glBlendFunc(GL_DST_COLOR, GL_ZERO);
	else                     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// MBAACC emits CG stage sprites with vertex RGB 0xEA (0x4b791e); MBAC
	// uses 0xFFFFFF.
	glUniform4f(uTint, tintRGB, tintRGB, tintRGB, 1.0f);
	glUniform3f(uAdd, 0.0f, 0.0f, 0.0f);
	glUniform1f(uAlphaLoc, alpha);
	const float xy[8] = { x, y, x + w, y, x + w, y + h, x, y + h };
	float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
	auto it = textureCache.find(spriteId);
	if (gameTextures && it != textureCache.end()) {
		// Sprite_EmitTransformedQuad: u = 0.25 texel .. w texels of the pow2 texture.
		const Tex& t = it->second;
		u0 = 0.25f / t.texW; v0 = 0.25f / t.texH;
		u1 = (float)t.w / t.texW; v1 = (float)t.h / t.texH;
	}
	const float uv[8] = { u0, v0, u1, v0, u1, v1, u0, v1 };
	const float col[4] = { 1, 1, 1, 1 };
	EmitQuad(tex, xy, uv, col, linear);
}

// ---- weather / lights ------------------------------------------------------------

void Renderer::LoadDropTexture() {
	dropTexTried = true;
	if (!file) return;
	std::string path = file->DropBitmapPath();
	if (path.empty()) return;
	std::vector<uint8_t> rgba;
	int w = 0, h = 0;
	if (!LoadBmpRGBA(path, rgba, w, h)) return;
	glGenTextures(1, &dropTex);
	glBindTexture(GL_TEXTURE_2D, dropTex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	dropTexW = w;
	dropTexH = h;
}

// DropObject_RenderWithBloom 0x4b5cc0. Particles live in world space
// (parallax 256, same camera matrix as the fighters). The sakura bloom
// post-effect (TecSakuraBloom) is not reproduced.
void Renderer::DrawWeather(const Camera& camera) {
	const DropSystem& drops = file->GetDrops();
	if (!drops.IsActive() || file->GetGame() != Game::MBAACC) return;
	const StageInfo& cfg = drops.Info();
	const float ax = camera.panLastX, ay = camera.panLastY;   // world (0,0) on screen
	const float k = 234.0f / 255.0f;
	glUniform4f(uTint, 1, 1, 1, 1);
	glUniform3f(uAdd, 0, 0, 0);
	glUniform1f(uAlphaLoc, 1.0f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (drops.Type() == 1) {
		// Rain: a line from the drop to H px behind it (rotated by the
		// velocity direction), alpha DropObj_Alpha -> 0.
		const float width = 1.0f / (camera.zoom > 0.0f ? camera.zoom : 1.0f);
		const float c0[4] = { k, k, k, (cfg.alpha & 0xFF) / 255.0f };
		const float c1[4] = { 1, 1, 1, 0 };
		for (const DropParticle& p : drops.Particles()) {
			double th = Atan2Normalized(p.vx, p.vy) * 360.0 * 0.01745329238474369;
			float tx = (float)(-cfg.h * std::sin(th));
			float ty = (float)(cfg.h * std::cos(th));
			EmitLine(ax + p.x, ay + p.y, ax + p.x + tx, ay + p.y + ty, width, c0, c1);
		}
	} else if (drops.Type() == 0) {
		if (!dropTexTried) LoadDropTexture();
		if (!dropTex || dropTexW <= 0 || dropTexH <= 0) return;
		std::vector<std::array<float, 24>> petals;
		const int W = cfg.w, H = cfg.h;
		const float x0 = (float)(W / -2), x1 = (float)(W - W / 2);
		const float y0 = (float)(H / -2), y1 = (float)(H - H / 2);
		for (const DropParticle& p : drops.Particles()) {
			int a = std::max(0, std::min(255, (int)p.alpha));
			const float col[4] = { k, k, k, a / 255.0f };
			const float xy[8] = { ax + p.x + x0, ay + p.y + y0, ax + p.x + x1, ay + p.y + y0,
			                      ax + p.x + x1, ay + p.y + y1, ax + p.x + x0, ay + p.y + y1 };
			float u0 = (float)(W * (int)p.frame) / dropTexW, v0 = (float)(H * p.pat) / dropTexH;
			float u1 = u0 + (float)W / dropTexW, v1 = v0 + (float)H / dropTexH;
			const float uv[8] = { u0, v0, u1, v0, u1, v1, u0, v1 };
			// DropObject_RenderWithBloom sets sampler 2 (linear) for the petals.
			EmitQuad(dropTex, xy, uv, col, true);
			if (sakuraBloom) {
				std::array<float, 24> q;
				std::memcpy(q.data(), xy, sizeof(xy)); std::memcpy(q.data() + 8, uv, sizeof(uv));
				std::memcpy(q.data() + 16, col, sizeof(col));
				petals.push_back(q);
			}
		}
		if (sakuraBloom && !petals.empty()) SakuraBloom(camera, petals, dropTex);
	}
	// DropObj_Type -1 (bg99) is the grid room, drawn at priority 8 by
	// DrawGridRoom in the back pass.
}

// Stage lights only affect the fighters (an extra shadow pass per light,
// MBAA Character_Render 0x41b411), so they are shown as editor markers: a
// cross at the light source (x, -200) and a floor bar fading over +-Power.
void Renderer::DrawLights(const Camera& camera) {
	auto lights = file->ActiveLights();
	if (lights.empty()) return;
	const float ax = camera.panLastX, ay = camera.panLastY;
	const float z = camera.zoom > 0.0f ? camera.zoom : 1.0f;
	const float w = 2.0f / z;
	glUniform4f(uTint, 1, 1, 1, 1);
	glUniform3f(uAdd, 0, 0, 0);
	glUniform1f(uAlphaLoc, 1.0f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	const float on[4]  = { 1.0f, 0.85f, 0.25f, 0.9f };
	const float off[4] = { 1.0f, 0.85f, 0.25f, 0.0f };
	const float dim[4] = { 1.0f, 0.85f, 0.25f, 0.35f };
	for (const auto& l : lights) {
		float x = ax + (float)l.worldX, y = ay - 200.0f, fy = ay + 2.0f;
		float s = 10.0f / z;
		EmitLine(x - s, y - s, x + s, y + s, w, on, on);
		EmitLine(x - s, y + s, x + s, y - s, w, on, on);
		EmitLine(x, y, x, ay, w * 0.5f, dim, dim);
		if (l.power > 0) {
			EmitLine(x, fy, x - (float)l.power, fy, w * 2.0f, on, off);
			EmitLine(x, fy, x + (float)l.power, fy, w * 2.0f, on, off);
		}
	}
}

// ---- DropObj type -1: training-room grid -----------------------------------------

namespace {
// Color_ScaleAlpha 0x4bfc10: each RGB byte scaled and truncated, OR'd back
// without clamping (a channel over 255 spills into the next byte).
uint32_t ScaleRgb(uint32_t c, double f) {
	uint32_t r = (uint32_t)(int)(((c >> 16) & 0xFF) * f), g = (uint32_t)(int)(((c >> 8) & 0xFF) * f),
	         b = (uint32_t)(int)((c & 0xFF) * f);
	return (c & 0xFF000000u) | (r << 16) | (g << 8) | b;
}
} // namespace

// BgGrid_RenderTrainingRoom 0x4b58b0 / BgGrid_EmitRoomQuads 0x4b5490 /
// BgGrid_AddCheckerQuad 0x4b53b0 (docs/bg_research/data/stage_system_re.md §3):
// untextured checker quads on a floor, ceiling, back wall and two side walls,
// projected with a 45 deg perspective that follows the game camera.
void Renderer::DrawGridRoom(const Camera& camera) {
	const StageInfo& info = file->GetStageInfo();
	if (!info.loaded || !info.dropObj || info.dropType != -1 || file->GetGame() != Game::MBAACC) return;
	const float ax = camera.panLastX, ay = camera.panLastY;
	const double zoom = camera.zoom > 0.0f ? camera.zoom : 1.0;
	const double k = 2.414213562373095;            // cot(22.5 deg)
	const double u = 100.0 * k / 3.0;              // px per unit at z = 0
	const double camX = camera.camX, camY = camera.camY;
	const double cx = camX / u, cy = camY / u - 1.5;
	// game screen -> editor world: world = (s - (320, 432)) / zoom + cam
	auto proj = [&](double x, double y, double z, float& wx, float& wy) {
		double dz = 0.7 * z + 3.0;
		double sx = 320.0 + 100.0 * zoom * (x - cx) * k / dz;
		double sy = 432.0 - camY * zoom + 100.0 * zoom * ((y - cy) * k / dz + cy * k / 3.0);
		wx = (float)((sx - 320.0) / zoom + camX);
		wy = (float)((sy - 432.0) / zoom + camY);
	};
	const double bright = 1.0;   // g_BgBrightness (screen-effect overlay state; 1 in the editor)
	glUniform4f(uTint, 1, 1, 1, 1);
	glUniform3f(uAdd, 0, 0, 0);
	glUniform1f(uAlphaLoc, 1.0f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	auto quad = [&](const double p[4][3], uint32_t base, double f, int ci, int ri) {
		uint32_t c = ScaleRgb(base, f);
		if ((ci + ri) & 1) c = ScaleRgb(c, 0.5);
		c = ScaleRgb(c, bright);
		const float col[4] = { ((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f, (c & 0xFF) / 255.0f, ((c >> 24) & 0xFF) / 255.0f };
		float xy[8];
		for (int i = 0; i < 4; ++i) { float wx, wy; proj(p[i][0], p[i][1], p[i][2], wx, wy); xy[i * 2] = ax + wx; xy[i * 2 + 1] = ay + wy; }
		const float uv[8] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
		EmitQuad(whiteTex, xy, uv, col, false);
	};
	for (int c = 0; c < 14; ++c)
		for (int r = 0; r < 7; ++r) {
			double x0 = c - 7, x1 = c - 6;
			double fz0 = 4 - r, fz1 = 3 - r;                     // floor, y = 0
			const double fl[4][3] = {{x0, 0, fz0}, {x1, 0, fz0}, {x1, 0, fz1}, {x0, 0, fz1}};
			quad(fl, 0xFFC8C8C8u, 1.0 - (4 - r) / 8.0, c, r);
			double cz0 = r - 3, cz1 = r - 2;                     // ceiling, y = -9
			const double ce[4][3] = {{x0, -9, cz0}, {x1, -9, cz0}, {x1, -9, cz1}, {x0, -9, cz1}};
			quad(ce, 0xFFC8C8C8u, 1.0 - (r - 3) / 8.0, c, r);
		}
	for (int c = 0; c < 14; ++c)
		for (int r = 0; r < 9; ++r) {                            // back wall, z = 4
			const double bw[4][3] = {{c - 7.0, r - 9.0, 4}, {c - 6.0, r - 9.0, 4}, {c - 6.0, r - 8.0, 4}, {c - 7.0, r - 8.0, 4}};
			quad(bw, 0xFF80C8C8u, 0.5, c, r);
		}
	for (int i = 0; i < 9; ++i)
		for (int c = 0; c < 7; ++c) {
			const double lw[4][3] = {{-7, i - 9.0, c - 3.0}, {-7, i - 9.0, c - 2.0}, {-7, i - 8.0, c - 2.0}, {-7, i - 8.0, c - 3.0}};
			quad(lw, 0xFFC880C8u, 1.0 - (c - 3) / 8.0, i, c);
			const double rw[4][3] = {{7, i - 9.0, 4.0 - c}, {7, i - 9.0, 3.0 - c}, {7, i - 8.0, 3.0 - c}, {7, i - 8.0, 4.0 - c}};
			quad(rw, 0xFFC880C8u, 1.0 - (4 - c) / 8.0, i, c);
		}
}

// ---- TecSakuraBloom ---------------------------------------------------------------

static const char* kBlurVS = R"GLSL(
#version 330 core
layout (location = 0) in vec2 aPos;
out vec2 vUV;
void main() { vUV = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); }
)GLSL";
// uMode 0: PS_0_Update0/1, the 9-tap blur along uStep; 1: plain copy.
static const char* kBlurFS = R"GLSL(
#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uStep;
uniform int uMode;
out vec4 FragColor;
void main() {
    if (uMode == 1) { FragColor = texture(uTex, vUV); return; }
    vec4 o = vec4(0.0);
    o += texture(uTex, vUV + uStep *  4.0) * 0.05;
    o += texture(uTex, vUV + uStep *  3.0) * 0.075;
    o += texture(uTex, vUV + uStep *  2.0) * 0.1;
    o += texture(uTex, vUV + uStep *  1.0) * 0.15;
    o += texture(uTex, vUV)                 * 0.25;
    o += texture(uTex, vUV + uStep * -1.0) * 0.15;
    o += texture(uTex, vUV + uStep * -2.0) * 0.1;
    o += texture(uTex, vUV + uStep * -3.0) * 0.075;
    o += texture(uTex, vUV + uStep * -4.0) * 0.05;
    FragColor = o;
}
)GLSL";

// MBAA DropObject_RenderWithBloom 0x4b5cc0 + Shader/sh_bloom_sakura.txt:
//   Ready: temp RT0/RT1 = (0,0,0,1); each petal is also drawn into RT0 (MRT);
//   Update pass 0: RT1 = blur(RT0, (+1.5, +1.5) texel); pass 1: RT0 = blur(RT1,
//   (-1.5, +1.5) texel); then RT0 is drawn over the scene with blend 2
//   (SRCALPHA, ONE), linear filter. The game's RTs are 640x480; here a game
//   texel is `zoom` target pixels.
void Renderer::SakuraBloom(const Camera& camera, const std::vector<std::array<float, 24>>& petals, GLuint tex) {
	GLint prevFbo = 0, vp[4];
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
	glGetIntegerv(GL_VIEWPORT, vp);
	const int W = vp[2], H = vp[3];
	if (W <= 0 || H <= 0) return;
	if (!blurProg) {
		GLuint vs = CompileShader(GL_VERTEX_SHADER, kBlurVS), fs = CompileShader(GL_FRAGMENT_SHADER, kBlurFS);
		blurProg = LinkProgram(vs, fs);
		glDeleteShader(vs); glDeleteShader(fs);
		uBlurTex = glGetUniformLocation(blurProg, "uTex");
		uBlurStep = glGetUniformLocation(blurProg, "uStep");
		uBlurMode = glGetUniformLocation(blurProg, "uMode");
		const float quad[12] = {-1, -1, 1, -1, 1, 1, 1, 1, -1, 1, -1, -1};
		glGenBuffers(1, &blurVbo);
		glBindBuffer(GL_ARRAY_BUFFER, blurVbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
	}
	if (W != bloomW || H != bloomH || !bloomFbo[0]) {
		for (int i = 0; i < 2; ++i) {
			if (bloomTex[i]) glDeleteTextures(1, &bloomTex[i]);
			if (bloomFbo[i]) glDeleteFramebuffers(1, &bloomFbo[i]);
			glGenTextures(1, &bloomTex[i]);
			glBindTexture(GL_TEXTURE_2D, bloomTex[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glGenFramebuffers(1, &bloomFbo[i]);
			glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloomTex[i], 0);
		}
		bloomW = W; bloomH = H;
	}
	// Ready + petals into RT0 (same projection / program as the scene draw).
	glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo[0]);
	glViewport(0, 0, W, H);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	for (const auto& q : petals) EmitQuad(tex, q.data(), q.data() + 8, q.data() + 16, true);
	// Two diagonal blurs.
	const float z = camera.zoom > 0.0f ? camera.zoom : 1.0f;
	const float sx = 1.5f * z / W, sy = 1.5f * z / H;
	glUseProgram(blurProg);
	glDisable(GL_BLEND);
	glBindBuffer(GL_ARRAY_BUFFER, blurVbo);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
	glActiveTexture(GL_TEXTURE0);
	glUniform1i(uBlurTex, 0);
	glUniform1i(uBlurMode, 0);
	// GL's v runs bottom-up: the game's (+x,+y down) diagonal is (+x,-v).
	glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo[1]);
	glBindTexture(GL_TEXTURE_2D, bloomTex[0]);
	glUniform2f(uBlurStep, sx, -sy);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo[0]);
	glBindTexture(GL_TEXTURE_2D, bloomTex[1]);
	glUniform2f(uBlurStep, -sx, -sy);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	// Add onto the scene.
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFbo);
	glViewport(vp[0], vp[1], vp[2], vp[3]);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glBindTexture(GL_TEXTURE_2D, bloomTex[0]);
	glUniform1i(uBlurMode, 1);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glUseProgram(program);
}

// ---- stand-in fighters and their shadows ---------------------------------------

namespace {
// A simple standing figure as an alpha mask, 64 x 192, feet on the bottom row.
std::vector<uint8_t> MakeSilhouette(int W, int H) {
	std::vector<uint8_t> rgba((size_t)W * H * 4, 0);
	auto inEll = [](float x, float y, float cx, float cy, float rx, float ry) {
		float dx = (x - cx) / rx, dy = (y - cy) / ry; return dx * dx + dy * dy <= 1.0f; };
	for (int y = 0; y < H; ++y)
		for (int x = 0; x < W; ++x) {
			float fx = x + 0.5f, fy = y + 0.5f;
			bool on = inEll(fx, fy, 32, 22, 13, 16)                                  // head
			       || (fy > 36 && fy < 110 && std::fabs(fx - 32) < 20 - (fy - 36) * 0.06f)  // torso
			       || (fy > 40 && fy < 104 && (std::fabs(fx - 9) < 5 || std::fabs(fx - 55) < 5))   // arms
			       || (fy >= 106 && fy < 190 && (std::fabs(fx - 23) < 8 || std::fabs(fx - 41) < 8)); // legs
			if (on) { uint8_t* p = &rgba[((size_t)y * W + x) * 4]; p[0] = p[1] = p[2] = 255; p[3] = 255; }
		}
	return rgba;
}
} // namespace

void Renderer::DrawStandIns(const Camera& camera) {
	if (!standIns.enabled) return;
	const int TW = 64, TH = 192;
	if (!standTex) {
		std::vector<uint8_t> img = MakeSilhouette(TW, TH);
		glGenTextures(1, &standTex);
		glBindTexture(GL_TEXTURE_2D, standTex);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TW, TH, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.data());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	const float ax = camera.panLastX, ay = camera.panLastY;   // world (0,0) on screen
	const float Hh = standIns.height, Ww = Hh * TW / TH;
	glUniform4f(uTint, 1, 1, 1, 1);
	glUniform3f(uAdd, 0, 0, 0);
	glUniform1f(uAlphaLoc, 1.0f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	// Shadow projection (V-rt, checked against captures of bg46 and bg28):
	//   w = d + y/640, x' = Lx + (x - Lx) d / w, y' = -200 + 200 d / w,
	//   d = cot(32.5 deg). A floor point stays put; higher points fall behind
	//   the feet, away from the light. No lights: one flat shadow with Lx = 0,
	//   alpha 127. Lights: Lx = Pos - 512, alpha w * 127 with
	//   w = 1 - |x - Lx| / Power (skipped when w <= 0).
	const double d = 1.5696855771174903;
	auto lights = file->ActiveLights();
	const bool mbacLights = file->GetGame() == Game::MBAC;
	for (int f = 0; f < 2; ++f) {
		const float fx = standIns.x[f];
		struct L { float x; float a; };
		std::vector<L> ls;
		if (lights.empty()) ls.push_back({0.0f, 127.0f / 255.0f});
		for (const auto& l : lights) {
			if (l.power <= 0) continue;
			float w = 1.0f - std::fabs(fx - (float)l.worldX) / (float)l.power;
			if (w <= 0.0f) continue;
			ls.push_back({(float)l.worldX, std::min(255.0f, w * 127.0f) / 255.0f});
		}
		(void)mbacLights;
		// Subdivide so the per-vertex projection stays accurate under affine
		// interpolation.
		const int NX = 8, NY = 32;
		for (const L& l : ls) {
			const float col[4] = { 0, 0, 0, l.a };
			for (int j = 0; j < NY; ++j)
				for (int i = 0; i < NX; ++i) {
					float xy[8], uv[8];
					static const int cx[4] = {0, 1, 1, 0}, cy[4] = {0, 0, 1, 1};
					for (int k = 0; k < 4; ++k) {
						float u = (float)(i + cx[k]) / NX, v = (float)(j + cy[k]) / NY;
						double wx = fx - Ww * 0.5 + u * Ww, wy = -Hh + v * Hh;
						double w = d + wy / 640.0;
						double px = l.x + (wx - l.x) * d / w, py = -200.0 + 200.0 * d / w;
						xy[k * 2] = ax + (float)px; xy[k * 2 + 1] = ay + (float)py;
						uv[k * 2] = u; uv[k * 2 + 1] = v;
					}
					EmitQuad(standTex, xy, uv, col, false);
				}
		}
		// the stand-in itself (priority 384, above its shadows at 366)
		const float col[4] = { 0.55f, 0.58f, 0.66f, 0.92f };
		const float xy[8] = { ax + fx - Ww * 0.5f, ay - Hh, ax + fx + Ww * 0.5f, ay - Hh,
		                      ax + fx + Ww * 0.5f, ay, ax + fx - Ww * 0.5f, ay };
		const float uv[8] = { 0, 0, 1, 0, 1, 1, 0, 1 };
		EmitQuad(standTex, xy, uv, col, true);
	}
}

// ---- one band of instances ----------------------------------------------------

void Renderer::DrawPass(const Camera& camera, int clientW, int clientH, int band) {
	(void)clientW; (void)clientH;
	auto& objects = file->GetObjects();
	const bool mbac = file->GetGame() == Game::MBAC;
	const OldPat* oldPat = file->GetOldPat();

	// --- draw order (MBAA.exe Background_DrawAllInstances @0x4b8f80) ---
	// Live instances are bucketed by (objhdr+21 band, layer); band 0 is drawn
	// at render priority 10 (behind the fighters), band 1 at 600 (in front),
	// layers ascending within a band (layer 255 in band 0 becomes 279), slot
	// ascending within a layer.
	const auto& insts = file->GetInstances();
	struct DrawItem { int objIdx; int slot; int key; };
	std::vector<DrawItem> order;
	order.reserve(insts.size());
	for (int slot = 0; slot < (int)insts.size(); ++slot) {
		const Instance& in = insts[slot];
		if (in.state < 2 || in.objIndex < 0 || in.objIndex >= (int)objects.size()) continue;
		const Object& o = objects[in.objIndex];
		if ((o.foreground ? 1 : 0) != band) continue;
		int layer = (int16_t)o.layer;
		if (layer == 255 && !o.foreground) layer = 279;
		order.push_back({in.objIndex, slot, layer});
	}
	std::stable_sort(order.begin(), order.end(),
	                 [](const DrawItem& a, const DrawItem& b) { return a.key < b.key; });

	for (const DrawItem& item : order) {
		const auto& obj = objects[(size_t)item.objIdx];
		const Instance& inst = insts[item.slot];
		if (obj.frames.empty() || !obj.visible) continue;
		if (inst.curFrame < 0 || inst.curFrame >= (int)obj.frames.size()) continue;
		const Frame& fr = obj.frames[inst.curFrame];
		// The game shows a dur-0 frame for one tick (no u4ick skip).
		if (fr.spriteId < 0) continue;
		const Frame* nx = (inst.nextFrame >= 0 && inst.nextFrame < (int)obj.frames.size())
		                  ? &obj.frames[inst.nextFrame] : nullptr;
		const uint16_t dur = (uint16_t)fr.duration;
		const float t = dur ? (float)inst.timer / (float)dur : 0.0f;

		int para = parallaxEnabled ? obj.parallax : 256;
		// worldX/worldY: editor-world position of the CG canvas top-left
		// (without the camera pan). CG draws at (pos >> 7) + offset.
		// Game parallax (Background_DrawInstance): the layer is translated by
		// (1 - p/256) * cam in world space; see Camera::ParallaxX.
		float worldX = (float)fr.offsetX + camera.ParallaxX(para)
		               - STAGE_CENTER_X + (float)(inst.posX >> 7);
		float worldY = (float)fr.offsetY + camera.ParallaxY(para)
		               - STAGE_FLOOR_Y + (float)(inst.posY >> 7);

		if (fr.spriteId >= 10000) {
			// --- CG sprite ---
			float alpha = (fr.blendMode > 0) ? (fr.opacity / 255.0f) : 1.0f;
			// MBAC-only per-frame scale (+16/+18, 256 = 1.0, 0 = 1.0).
			float scx = 1.0f, scy = 1.0f;
			if (mbac) {
				scx = fr.scaleX ? fr.scaleX / 256.0f : 1.0f;
				scy = fr.scaleY ? fr.scaleY / 256.0f : 1.0f;
			}
			// frame +20: lerp alpha (and MBAC scale) toward the next frame.
			if (fr.interpolate && nx) {
				float a0 = (fr.blendMode > 0) ? fr.opacity : 255.0f;
				float a1 = nx->blendMode ? (float)nx->opacity : 255.0f;
				alpha = ((1.0f - t) * a0 + t * a1) / 255.0f;
				if (mbac) {
					float n0 = nx->scaleX ? nx->scaleX / 256.0f : 1.0f;
					float n1 = nx->scaleY ? nx->scaleY / 256.0f : 1.0f;
					scx = (1.0f - t) * scx + t * n0;
					scy = (1.0f - t) * scy + t * n1;
				}
			}
			int cgIdx = fr.spriteId - 10000;
			int sw, sh, ox, oy;
			if (GetOrCreateTexture(cgIdx, sw, sh, ox, oy) == 0) continue;
			// The cg lib returns the TIGHT bounded region: (ox, oy) is its
			// top-left in the canvas. Scale about the object origin (canvas
			// point 128, 224).
			float orgX = worldX + CG_PIVOT_X + camera.panLastX;
			float orgY = worldY + CG_PIVOT_Y + camera.panLastY;
			DrawSprite(cgIdx,
			           orgX + ((float)ox - CG_PIVOT_X) * scx,
			           orgY + ((float)oy - CG_PIVOT_Y) * scy,
			           (float)sw * scx, (float)sh * scy, alpha, fr.blendMode,
			           mbac ? 1.0f : 234.0f / 255.0f,
			           // MBAA 0x4b7c7f: an additive CG draw (blend 2) also
			           // switches the sampler to linear (0x56447F = 2).
			           obj.linearFilter != 0 || (!mbac && fr.blendMode == 2));
		} else if (patParts && patParts->loaded && oldPat) {
			// --- older-PAT pattern (sprite-id < 10000) ---
			const PatPattern* pat = oldPat->GetPattern(fr.spriteId);
			if (!pat) continue;
			// The game looks up the NEXT frame's pattern unconditionally and
			// skips the draw if it is missing.
			const PatPattern* nextPat = nullptr;
			if (nx && nx->spriteId >= 0 && nx->spriteId < 10000) {
				nextPat = oldPat->GetPattern(nx->spriteId);
				if (!nextPat) continue;
			}
			// No (128,224) pivot on PAT objects: add back what
			// STAGE_CENTER_X / STAGE_FLOOR_Y removed. MBAACC uses pos/128 as
			// a float, MBAC pos >> 7.
			float fx = mbac ? 0.0f : (inst.posX * STAGE_POS_SCALE - (float)(inst.posX >> 7));
			float fy = mbac ? 0.0f : (inst.posY * STAGE_POS_SCALE - (float)(inst.posY >> 7));
			DrawPatPatternFlat(*pat, (fr.interpolate && nextPat) ? nextPat : nullptr, t,
			                   worldX + CG_PIVOT_X + camera.panLastX + fx,
			                   worldY + CG_PIVOT_Y + camera.panLastY + fy,
			                   obj.linearFilter != 0, mbac);
		}
	}
}

// ---- main entry ------------------------------------------------------------

void Renderer::Render(const Camera& camera, int clientW, int clientH, Pass pass) {
	if (!enabled || !file) return;
	CG* cg = file->GetCG();
	if (!cg || !cg->m_loaded) return;
	if (clientW <= 0 || clientH <= 0) return;
	if (file->GetObjects().empty()) return;

	InitGL();
	// Build the PAT->Parts conversion once (needs a live GL context).
	if (!patPartsBuilt) BuildPatParts();

	glUseProgram(program);

	// Save host depth/blend so we don't leak state to character/grid.
	GLboolean prevDepthTest, prevDepthMask, prevBlend;
	glGetBooleanv(GL_DEPTH_TEST, &prevDepthTest);
	glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);
	glGetBooleanv(GL_BLEND, &prevBlend);

	// Painter's order on the CPU; no depth test.
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);

	float pview[16];
	BuildProjView(clientW, clientH, camera.zoom, pview);
	glUniformMatrix4fv(uProjView, 1, GL_FALSE, pview);
	glUniform4f(uTint, 1, 1, 1, 1);
	glUniform3f(uAdd, 0, 0, 0);
	glUniform1f(uAlphaLoc, 1.0f);

	if (pass == Pass::All || pass == Pass::Back) {
		if (showWeather) DrawGridRoom(camera);   // priority 8, behind band 0
		DrawPass(camera, clientW, clientH, 0);
	}
	if (pass == Pass::All || pass == Pass::Front) {
		// Fighter shadows (366) and fighters (384) sit above band 0 and below
		// the weather (522) and band 1 (600).
		DrawStandIns(camera);
		// Priority 522 (weather) sits between band 0 (10) and band 1 (600).
		if (showWeather) DrawWeather(camera);
		DrawPass(camera, clientW, clientH, 1);
		if (showLights) DrawLights(camera);
	}

	// --- restore state ---
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisableVertexAttribArray(2);   // host shaders use a constant colour at 2
	if (prevDepthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	glDepthMask(prevDepthMask);
	if (!prevBlend) glDisable(GL_BLEND);
	glUseProgram(0);

	// --- debug self-screenshot (on request, after the front pass) ---
	if (dumpCountdown > 0 && pass != Pass::Back && --dumpCountdown == 0) {
		std::vector<uint8_t> rgba((size_t)clientW * clientH * 4);
		glReadPixels(0, 0, clientW, clientH, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
		std::vector<uint8_t> rgb((size_t)clientW * clientH * 3);
		for (int yy = 0; yy < clientH; ++yy) {
			int sy = clientH - 1 - yy;        // GL origin is bottom-left
			for (int xx = 0; xx < clientW; ++xx) {
				const uint8_t* s = &rgba[((size_t)sy * clientW + xx) * 4];
				uint8_t* d = &rgb[((size_t)yy * clientW + xx) * 3];
				d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
			}
		}
		WritePNG("C:/dev/bg_dump.png", rgb.data(), clientW, clientH);
	}
}

} // namespace bg
