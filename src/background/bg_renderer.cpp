// Port of u4ick's bgmaketool MonoForm.Draw rendering pipeline.
//
// References to the original (paths relative to repo root):
//   src/dpu4/stagesrc/bgmaketool/MonoForm.cs        (Draw method)
//   src/dpu4/stagesrc/bgmaketool/bgmake_app.cs      (camera/state globals)
//
// What we replicate:
//   - Orthographic projection: CreateOrthographicOffCenter(0, W, H, 0, 0, 1).
//     Same as XNA's screen-space ortho with Y growing downward.
//   - View matrix = Scale(zoom). No translation (camera/pan is in sprite pos).
//   - Sprite screen position = panLast + (pan - panLast) * (parallax/256) + offset.
//     The (pan - panLast) delta is only non-zero during a live drag, so this
//     produces u4ick's transient parallax-during-drag effect.
//   - Per-sprite blend modes: blendMode == 2 -> additive (GL_SRC_ALPHA, GL_ONE);
//     anything else -> custom alpha (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA).
//   - Depth test enabled with GL_LEQUAL and depth writes on. Per-vertex z is
//     layerDepth = layer/1024 - 1. Lower layer = smaller depth = wins the
//     test = ends up on top. This matches u4ick's SpriteSortMode.FrontToBack
//     ordering through the depth buffer rather than a CPU sort.
//   - The dur=0 / aniType=1 frame skip from MonoForm.cs:211-214 (transient
//     loop-boundary placeholder frames should not be rendered).
//
// What we DON'T do (out of scope here; matches u4ick's interactive view):
//   - SaveRender's 1057x818 fixed render target.
//   - AlphaTestEffect (Greater, ReferenceAlpha=0). With our blend funcs alpha=0
//     fragments produce no visible output anyway, and depth writes for fully
//     transparent pixels don't cause visible artifacts at our render scale.

#include "bg_renderer.h"
#include "../cg.h"
#include "../render.h"
#include "../texture.h"
#include "../parts/parts.h"
#include "bg_pat.h"
#include <algorithm>
#include <utility>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <chrono>

namespace bg {

// ---- shaders ---------------------------------------------------------------

static const char* kVS = R"GLSL(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
uniform mat4 uProjView;
out vec2 vUV;
void main() {
    gl_Position = uProjView * vec4(aPos, 1.0);
    vUV = aUV;
}
)GLSL";

static const char* kFS = R"GLSL(
#version 330 core
in vec2 vUV;
uniform sampler2D uTexture;
uniform float uAlpha;
uniform vec4 uTint;   // vertex diffuse colour (D3D COLOROP/ALPHAOP = MODULATE)
out vec4 FragColor;
void main() {
    vec4 c = texture(uTexture, vUV);
    FragColor = vec4(c.rgb * uTint.rgb, c.a * uAlpha * uTint.a);
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

	// No VAO — glad here doesn't expose glGenVertexArrays. Bind VBO and
	// re-set attribute pointers on every DrawSprite call instead. Cheap.
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 5 * 6, nullptr, GL_DYNAMIC_DRAW);

	glInit = true;
}

void Renderer::ClearTextureCache() {
	for (auto& kv : textureCache) glDeleteTextures(1, &kv.second.id);
	textureCache.clear();
}

void Renderer::SetFile(File* f) {
	if (file != f) {
		ClearTextureCache();
		patParts.reset();
		patPartsBuilt = false;
	}
	file = f;
	if (f) dumpCountdown = 30;   // debug self-screenshot ~0.5s after load
}

void Renderer::Update() {
	if (!enabled || !file || paused) return;
	// The game's Background_UpdateLayerAnimations runs at a fixed 60 Hz tick.
	// The editor renders at the display refresh rate, so stepping the animation
	// once per render frame runs it 2-2.4x too fast on a high-refresh monitor,
	// which strobes/skips frames. Accumulate real time and step at exactly 60 Hz.
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

	textureCache[spriteId] = { id, img->width, img->height, img->offsetX, img->offsetY };
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

// Draw one PAT pattern as flat textured quads in this renderer's ortho.
// Quad math is MBAA.exe Background_RenderLayerWithPalette's PAT branch:
// per part, local quad spans (-origin)..(quad-origin), scaled by the part
// scale, offset by the part position, then the pattern's world position.
// No perspective — the bg projection is a pure translate.
void Renderer::DrawPatPatternFlat(int pattern, float worldX, float worldY,
                                  float alpha, int frameBlend) {
	if (!patParts || pattern < 0 ||
	    pattern >= (int)patParts->partSets.size())
		return;
	const PartSet<>& ps = patParts->partSets[pattern];

	// worldX/Y is the pattern origin (0,0). Each part's cutout quad spans
	// (-origin)..(quad-origin), so the cutout's origin POINT lands exactly at
	// worldX/Y + part.pos — i.e. the pattern is PIVOT-anchored on its origin.
	// This is what MBAA.exe Background_RenderLayerWithPalette does (the quad
	// corner (0,0) maps through scale*translate(partPos)*translate(frameOff)),
	// so no bounding-box shift is applied here.
	static const int tX[6] = {0,1,1, 1,0,0};
	static const int tY[6] = {0,0,1, 1,1,0};

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	GLint uAlpha = glGetUniformLocation(program, "uAlpha");

	for (const PartProperty& part : ps.groups) {
		if (part.ppId < 0 || part.ppId >= (int)patParts->cutOuts.size())
			continue;
		const CutOut<>& cut = patParts->cutOuts[part.ppId];
		if (cut.texture < 0 || cut.texture >= (int)patParts->gfxMeta.size())
			continue;
		GLuint glTex = (GLuint)patParts->gfxMeta[cut.texture].textureIndex;
		if (glTex == 0) continue;

		// The game's PAT branch ignores the frame's blend/opacity: only the
		// part's +49 additive flag and its ARGB diffuse colour apply.
		(void)frameBlend;
		bool additive = part.additive;
		glBlendFunc(GL_SRC_ALPHA, additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
		glUniform4f(glGetUniformLocation(program, "uTint"),
		            part.bgra[2] / 255.0f, part.bgra[1] / 255.0f,
		            part.bgra[0] / 255.0f, part.bgra[3] / 255.0f);

		float verts[5 * 6];
		for (int i = 0; i < 6; ++i) {
			float lx = (float)(-cut.xy[0] + cut.wh[0] * tX[i]);
			float ly = (float)(-cut.xy[1] + cut.wh[1] * tY[i]);
			float wx = worldX + part.x + lx * part.scaleX;
			float wy = worldY + part.y + ly * part.scaleY;
			int ux = (part.flip & 1) ? (1 - tX[i]) : tX[i];
			int vy = (part.flip & 2) ? (1 - tY[i]) : tY[i];
			// cut.uv is the texel rect normalised into the tag-format's
			// 256-unit space (see BuildPatParts) — divide by 256 for [0,1].
			float u = (cut.uv[0] + cut.uv[2] * ux) / 256.0f;
			float v = (cut.uv[1] + cut.uv[3] * vy) / 256.0f;
			verts[i*5+0] = wx; verts[i*5+1] = wy; verts[i*5+2] = 0.0f;
			verts[i*5+3] = u;  verts[i*5+4] = v;
		}
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float),
		                      (void*)(3*sizeof(float)));
		glEnableVertexAttribArray(1);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, glTex);
		glUniform1i(uTexture, 0);
		glUniform1f(uAlpha, 1.0f);   // frame opacity not applied to PAT
		(void)alpha;
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
}

// ---- projection ------------------------------------------------------------

// XNA's CreateOrthographicOffCenter(left, right, bottom, top, near, far)
// in row-major form. Composed with Scale(zoom) on the view side; we fold
// the scale directly into the same matrix so there's only one uniform.
void Renderer::BuildProjView(int W, int H, float zoom, float m[16]) {
	const float l = 0.0f, r = (float)W, b = (float)H, t = 0.0f, n = 0.0f, f = 1.0f;
	// glm::ortho-style result (column-major in GLSL but we send transposed below).
	// proj * scale(zoom):
	//   X: 2/(r-l) * zoom, Y: 2/(t-b) * zoom (negative because b>t), Z: -2/(f-n)
	//   Translation column accounts for -(r+l)/(r-l), -(t+b)/(t-b), -(f+n)/(f-n)
	float sx = 2.0f / (r - l) * zoom;
	float sy = 2.0f / (t - b) * zoom;
	float sz = -2.0f / (f - n);
	float tx = -(r + l) / (r - l);
	float ty = -(t + b) / (t - b);
	float tz = -(f + n) / (f - n);
	// Column-major (OpenGL).
	m[0]=sx; m[1]=0;  m[2]=0;  m[3]=0;
	m[4]=0;  m[5]=sy; m[6]=0;  m[7]=0;
	m[8]=0;  m[9]=0;  m[10]=sz;m[11]=0;
	m[12]=tx;m[13]=ty;m[14]=tz;m[15]=1;
}

// ---- sprite quad submission -----------------------------------------------

void Renderer::DrawSprite(int spriteId,
                          float x, float y, float w, float h,
                          float alpha, int blendMode,
                          float layerDepth)
{
	int sw, sh, ox, oy;
	GLuint tex = GetOrCreateTexture(spriteId, sw, sh, ox, oy);
	if (tex == 0) return;
	(void)sw; (void)sh; // sprite is drawn at the size requested by caller.

	// Blend mode — set right before the draw, per-sprite (matches u4ick's
	// per-batch BlendState swap in the FullBlend path).
	// MBAA.exe Background_DrawInstance: frame +9 = 0/1 alpha, 2 additive
	// (SRCALPHA, ONE), 3 = blend mode 4 = MULTIPLY (DESTCOLOR, ZERO) — not
	// subtractive as older notes claimed.
	if (blendMode == 2) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	} else if (blendMode == 3) {
		glBlendFunc(GL_DST_COLOR, GL_ZERO);
	} else {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	// CG stage sprites are emitted with vertex RGB 0xEA (234/255) — MBAA.exe
	// 0x4b791e — so they render ~8% darker than the raw CG.
	{
		const float k = 234.0f / 255.0f;
		glUniform4f(glGetUniformLocation(program, "uTint"), k, k, k, 1.0f);
	}

	const float z = layerDepth;
	float verts[5 * 6] = {
		x,       y,       z,  0.0f, 0.0f,
		x + w,   y,       z,  1.0f, 0.0f,
		x + w,   y + h,   z,  1.0f, 1.0f,

		x + w,   y + h,   z,  1.0f, 1.0f,
		x,       y + h,   z,  0.0f, 1.0f,
		x,       y,       z,  0.0f, 0.0f,
	};

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

	// Re-bind attribs per draw (no VAO support in this glad).
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
	                      (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	glUniform1i(uTexture, 0);
	GLint uAlpha = glGetUniformLocation(program, "uAlpha");
	glUniform1f(uAlpha, alpha);

	glDrawArrays(GL_TRIANGLES, 0, 6);
}

// ---- main entry ------------------------------------------------------------

void Renderer::Render(const Camera& camera, int clientW, int clientH) {
	if (!enabled || !file) return;
	CG* cg = file->GetCG();
	if (!cg || !cg->m_loaded) return;
	if (clientW <= 0 || clientH <= 0) return;

	InitGL();

	auto& objects = file->GetObjects();
	if (objects.empty()) return;

	// --- GL state ---
	glUseProgram(program);

	// Save host depth/blend so we don't leak state to character/grid.
	GLboolean prevDepthTest, prevDepthMask, prevBlend;
	glGetBooleanv(GL_DEPTH_TEST, &prevDepthTest);
	glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);
	glGetBooleanv(GL_BLEND, &prevBlend);

	// Disable depth test — we sort on the CPU instead so we don't have to
	// worry about how our layerDepth range interacts with the host editor's
	// depth buffer, projection range, or character render afterwards. (My
	// earlier attempt set GL_DEPTH_TEST + layerDepth z = -1..0, which fell
	// outside our ortho's [0, 1] z range and clipped most sprites — "everything
	// looks worse." Pure paint sidesteps the whole problem.)
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);

	// Projection (Scale(zoom) folded in).
	float pview[16];
	BuildProjView(clientW, clientH, camera.zoom, pview);
	glUniformMatrix4fv(uProjView, 1, GL_FALSE, pview);

	// --- draw order (MBAA.exe Background_DrawAllInstances @0x4b8f80) ---
	// Live instances are bucketed by (objhdr+21 foreground band, layer);
	// band 0 is drawn entirely before band 1 (render priority 10 vs 600, the
	// latter in front of the fighters), layers ascending within a band
	// (layer 255 in band 0 becomes 279), slot ascending within a layer.
	const auto& insts = file->GetInstances();
	struct DrawItem { int objIdx; int slot; int key; };
	std::vector<DrawItem> order;
	order.reserve(insts.size());
	for (int slot = 0; slot < (int)insts.size(); ++slot) {
		const Instance& in = insts[slot];
		if (in.state < 2 || in.objIndex < 0 || in.objIndex >= (int)objects.size()) continue;
		const Object& o = objects[in.objIndex];
		int layer = (int16_t)o.layer;
		if (layer == 255 && !o.foreground) layer = 279;
		order.push_back({in.objIndex, slot, (o.foreground ? 1000 : 0) + layer});
	}
	std::stable_sort(order.begin(), order.end(),
	                 [](const DrawItem& a, const DrawItem& b) { return a.key < b.key; });

	// --- emit sprites ---
	// Build the PAT->Parts conversion once (needs a live GL context, hence
	// done here rather than at file-load time).
	if (!patPartsBuilt) BuildPatParts();
	static int s_geomLog = 0;
	bool dbgGeom = (s_geomLog < 1);
	std::ofstream dbgLg;
	if (dbgGeom) {
		++s_geomLog;
		dbgLg.open("C:/dev/bggeom_log.txt", std::ios::trunc);
		dbgLg << "clientW=" << clientW << " clientH=" << clientH
		      << " zoom=" << camera.zoom
		      << " panLast=(" << camera.panLastX << "," << camera.panLastY << ")"
		      << " STAGE_CENTER_X=" << STAGE_CENTER_X
		      << " STAGE_FLOOR_Y=" << STAGE_FLOOR_Y << "\n";
	}
	for (const DrawItem& item : order) {
		const size_t i = (size_t)item.objIdx;
		const auto& obj = objects[i];
		const Instance& inst = insts[item.slot];
		if (obj.frames.empty()) continue;
		if (!obj.visible) continue;   // layer-debug hide / solo
		if (inst.curFrame < 0 || inst.curFrame >= (int)obj.frames.size())
			continue;
		const Frame& fr = obj.frames[inst.curFrame];
		// NOTE: u4ick skipped dur==0 && aniType==1 frames here; the game does
		// not (a dur-0 frame is shown for exactly one tick), so no skip.
		if (fr.spriteId < 0) continue;

		int para = parallaxEnabled ? obj.parallax : 256;
		// worldX/worldY: editor-world position WITHOUT the camera pan.
		// -STAGE_CENTER_X / -STAGE_FLOOR_Y shift the stage so u4ick's
		// playfield centre / floor land on the grid; obj.posX/posY is the
		// integrator drift (1/128 px). The CG path adds panLastX/Y itself;
		// the PAT path lets Render's transform (render.x/y) supply it.
		// CG path draws at (pos >> 7) + offset (integer), PAT at pos/128.
		float worldX = camera.ScreenX((float)fr.offsetX, para)
		               - STAGE_CENTER_X + (float)(inst.posX >> 7);
		float worldY = camera.ScreenY((float)fr.offsetY, para)
		               - STAGE_FLOOR_Y + (float)(inst.posY >> 7);
		float alpha = (fr.blendMode > 0) ? (fr.opacity / 255.0f) : 1.0f;
		// frame +20: lerp alpha toward the next frame's over the duration
		// (MBAA.exe Background_DrawInstance CG branch).
		if (fr.interpolate && inst.nextFrame >= 0 && inst.nextFrame < (int)obj.frames.size()) {
			const Frame& nx = obj.frames[inst.nextFrame];
			float a0 = (fr.blendMode > 0) ? fr.opacity : 255.0f;
			float a1 = nx.blendMode ? (float)nx.opacity : 255.0f;
			float t = (uint16_t)fr.duration ? (float)inst.timer / (float)(uint16_t)fr.duration : 1.0f;
			if (!(uint16_t)fr.duration) t = 0.0f;
			alpha = ((1.0f - t) * a0 + t * a1) / 255.0f;
		}

		if (fr.spriteId >= 10000) {
			// --- CG sprite ---
			int cgIdx = fr.spriteId - 10000;
			int sw, sh, ox, oy;
			GLuint tex = GetOrCreateTexture(cgIdx, sw, sh, ox, oy);
			if (tex == 0) continue;
			// Compensate for the cg lib returning a TIGHT bounded region:
			// add (ox, oy) so the content lands where the full canvas would.
			glUseProgram(program);   // a prior PAT draw unsets the program
			if (dbgGeom)
				dbgLg << "obj_" << i << " CG spr=" << cgIdx
				      << " off=(" << fr.offsetX << "," << fr.offsetY << ")"
				      << " para=" << para << " posXY=(" << obj.posX << ","
				      << obj.posY << ") world=(" << worldX << "," << worldY << ")"
				      << " ox/oy=(" << ox << "," << oy << ")"
				      << " wh=(" << sw << "," << sh << ")"
				      << " screenXY=(" << (worldX+camera.panLastX+ox) << ","
				      << (worldY+camera.panLastY+oy) << ")\n";
			DrawSprite(cgIdx,
			           worldX + camera.panLastX + (float)ox,
			           worldY + camera.panLastY + (float)oy,
			           (float)sw, (float)sh, alpha, fr.blendMode, 0.0f);
		} else if (patParts && patParts->loaded) {
			if (dbgGeom) {
				float ax = worldX + camera.panLastX;
				float ay = worldY + camera.panLastY;
				dbgLg << "obj_" << i << " PAT spr=" << fr.spriteId
				      << " off=(" << fr.offsetX << "," << fr.offsetY << ")"
				      << " para=" << para << " posXY=(" << obj.posX << ","
				      << obj.posY << ") anchor=(" << ax << "," << ay << ")";
				if (fr.spriteId >= 0 && fr.spriteId < (int)patParts->partSets.size()) {
					const PartSet<>& ps = patParts->partSets[fr.spriteId];
					for (const PartProperty& pp : ps.groups) {
						if (pp.ppId < 0 || pp.ppId >= (int)patParts->cutOuts.size())
							continue;
						const CutOut<>& c = patParts->cutOuts[pp.ppId];
						float minx = ax + pp.x + (-c.xy[0]) * pp.scaleX;
						float maxx = ax + pp.x + (c.wh[0]-c.xy[0]) * pp.scaleX;
						float miny = ay + pp.y + (-c.xy[1]) * pp.scaleY;
						float maxy = ay + pp.y + (c.wh[1]-c.xy[1]) * pp.scaleY;
						dbgLg << " | part ppId=" << pp.ppId
						      << " partXY=(" << pp.x << "," << pp.y << ")"
						      << " scale=(" << pp.scaleX << "," << pp.scaleY << ")"
						      << " quadWH=(" << c.wh[0] << "," << c.wh[1] << ")"
						      << " origin=(" << c.xy[0] << "," << c.xy[1] << ")"
						      << " screenRect=(" << minx << "," << miny << ")..("
						      << maxx << "," << maxy << ")";
					}
				}
				dbgLg << "\n";
			}
			// --- older-PAT pattern --- (sprite-id < 10000)
			// Drawn flat in THIS renderer's ortho — same coordinate space
			// as the CG path above (worldX/Y + panLast). The bg has no
			// perspective, so no editor part-renderer / DrawBgPattern.
			// No (128,224) CG pivot on PAT objects in the game — add back
			// what STAGE_CENTER_X/STAGE_FLOOR_Y removed; and PAT uses pos/128
			// as float, not pos >> 7.
			DrawPatPatternFlat(fr.spriteId,
			                   worldX + CG_PIVOT_X + camera.panLastX
			                       + (inst.posX * STAGE_POS_SCALE - (float)(inst.posX >> 7)),
			                   worldY + CG_PIVOT_Y + camera.panLastY
			                       + (inst.posY * STAGE_POS_SCALE - (float)(inst.posY >> 7)),
			                   alpha, fr.blendMode);
		}
	}

	// --- restore state ---
	if (prevDepthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	glDepthMask(prevDepthMask);
	if (!prevBlend) glDisable(GL_BLEND);
	glUseProgram(0);

	// --- debug self-screenshot (continuous: re-arms so a solo can be caught) ---
	if (dumpCountdown > 0) {
		--dumpCountdown;
		if (dumpCountdown == 0 && clientW > 0 && clientH > 0) {
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
			// Skip near-black captures (caught while the stage view wasn't
			// on screen) — retry sooner instead of writing a useless dump.
			uint64_t sum = 0;
			for (size_t i = 0; i < rgb.size(); ++i) sum += rgb[i];
			if (rgb.empty() || sum / rgb.size() < 8) {
				dumpCountdown = 20;   // not a real frame — retry
			} else {
				WritePNG("C:/dev/bg_dump.png", rgb.data(), clientW, clientH);
				std::cerr << "[bg] wrote C:/dev/bg_dump.png (" << clientW << "x"
				          << clientH << ")" << std::endl;
				dumpCountdown = 90;   // re-arm: dump again ~1.5s later
			}
		}
	}
}

} // namespace bg
