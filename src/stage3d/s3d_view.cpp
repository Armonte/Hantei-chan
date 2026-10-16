// 3D stage view, see s3d_view.h and docs/stage3d/SCOPE.md.
#include "s3d_view.h"

#include "s3d_bglist.h"
#include "s3d_dds.h"
#include "s3d_model.h"
#include "s3d_fbxascii.h"
#include "s3d_blender.h"

#include "../app_log.h"
#include "../i18n.h"
#include "../ini.h"
#include "../png_writer.h"
#include "../render_target.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <imgui.h>
#include <imgui_internal.h>   // MarkIniSettingsDirty (Preferences)

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace stage3dview {
bool show = false;

namespace {
namespace fs = std::filesystem;

const char* kVs = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aUv;
uniform mat4 uWorld, uView, uProj;
uniform float uFlipV;
out vec2 vUv;
out vec4 vColor;
out float vFog;
void main() {
	vec4 p = uProj * (uView * (uWorld * vec4(aPos, 1.0)));
	gl_Position = p;
	vUv = vec2(aUv.x, uFlipV > 0.5 ? 1.0 - aUv.y : aUv.y);
	vColor = aColor;
	vFog = p.w;   // Diffuse_VS_Easy: output.Fog = output.Position.w
})";

// Diffuse_PS_Easy (Shader\fbxshader.txt): tex * vertex colour, unclamped fog lerp when g_FogColor.a >= 0.001 (alpha kept),
// The 8-bit stage target clamps; the composite grade runs as a separate pass (kGradeFs).
const char* kFs = R"(#version 330 core
in vec2 vUv;
in vec4 vColor;
in float vFog;
uniform sampler2D uTex;
uniform vec4 uFogColor;
uniform vec2 uFogCoord;
uniform float uAlphaRef;
out vec4 oColor;
void main() {
	vec4 c = texture(uTex, vUv) * vColor;
	if (uFogColor.a >= 0.001) {
		float f = uFogCoord.x + vFog * uFogCoord.y;
		c.rgb = mix(uFogColor.rgb, c.rgb, f);
	}
	if (c.a <= uAlphaRef) discard;   // alpha test GREATER 0
	oColor = vec4(c.rgb, c.a);
})";

// Stage composite (TecBgNoSpeculer): the stage target is drawn into the 2D list at priority 250 as
// rgb = 0.10 + tex * (g_StageTintColor.rgb * 0.6708) (+ specular variant, grayscale lerp; not modelled yet).
const char* kGradeVs = R"(#version 330 core
out vec2 vUv;
void main() { vec2 p = vec2((gl_VertexID & 1) * 2.0, (gl_VertexID >> 1) * 2.0); vUv = p; gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0); })";
const char* kGradeFs = R"(#version 330 core
in vec2 vUv;
uniform sampler2D uTex;
uniform float uLift, uGain;
out vec4 oColor;
void main() { vec3 c = texture(uTex, vUv).rgb; oColor = vec4(clamp(uLift + c * uGain, 0.0, 1.0), 1.0); })";

struct GpuMesh {
	unsigned vao = 0, vbo = 0;
	struct Sub { unsigned ibo = 0; int count = 0; int tex = -1; };
	std::vector<Sub> subs;
	int node = -1;
};

struct State {
	char bgDir[512] = "C:/dev/hantei-chan/gamedata/uni2/bg";
	std::string dataFile = "bg001";
	s3d::BgList list;
	s3d::Model model;
	s3d::StageParams params;
	std::string loadedName, error, nameUtf8;
	std::vector<unsigned> textures;   // GL ids per model texture (0 = white)
	unsigned white = 0;
	std::vector<GpuMesh> meshes;
	unsigned program = 0, gradeProgram = 0, emptyVao = 0;
	RenderTarget raw, rt;   // raw stage target, composited (graded) result
	s3d::Stats stats;
	// view controls
	float camX = 0.f, camY = 0.f, zoom = 1.f;   // fight camera in game px (1280x720 frame) and zoom, as CBtlCamera
	float fovOverride = 0.f;                    // 0 = BgList FOV (vertical degrees, aspect-1 projection)
	float lift = 0.10f, contrast = 0.6708f;     // stage composite TecBgNoSpeculer: 0.10 + tex * tint * 0.6708
	bool zMirror = true;
	bool flipV = true;   // stored v counts from the bottom of the DDS image (verified on bg001 against the game; pc-stage3d flips too)                        // FbxMesh_Draw negates the node matrix Z column
	bool fog = true, play = false, bgListTransform = true;
	int cull = 0;   // 0 none, 1 cull GL clockwise faces, 2 cull GL counter-clockwise faces (game: scene field +0x158, 1 CW / 2 CCW / else none)
	int frame = 0;
	double playStart = 0;
	int playStartFrame = 0;
	float clear[3] = {0.f, 0.392f, 0.f};       // game default clear 0xff006400 (UNI2), visible where nothing draws
	bool loaded = false;
	// MBAACC alignment (docs/stage3d/SCOPE.md section 4): the source camera driven from MBAACC's 2D camera, shown as MBAACC sees
	// it (centre 4:3 of the 16:9 frame, same vertical FOV: "crop"), with the floor row, walls and a fighter ruler.
	bool align = false;
	float mbCamX = 0.f, mbZoom = 1.f;    // MBAACC camera: sim px (clamp +-208) and zoom
	float fighterScale = 1.f;            // s: MBAACC px per source px at the plane (pc-stage3d fighter_px = 237 * s)
	float centerX = 0.f;                 // stage units under MBAACC camera x = 0 (pc-stage3d center_x)
	float floorPx = 432.f, yBias = 0.f;  // MBAACC floor row; small floor fix in MBAACC px
	int replaces = -1;                   // MBAACC 2D stage slot the port replaces (pc-stage3d replaces)
	std::string alignStatus;
	// Blender link (s3d_blender.h): the model file the stage was loaded from and its write time then
	// watch = s3d::BlenderWatchPaths(modelPath) (bg.fbx.bin first: the add-on writes it also for an ASCII bg.fbx stage),
	// watchTimes = their write times at load (min() = file absent)
	std::string modelPath, blenderStatus;
	std::vector<std::string> watch;
	std::vector<fs::file_time_type> watchTimes;
	double lastDiskCheck = 0;
	bool changedOnDisk = false;
};
State* S()
{
	static State s;
	return &s;
}

unsigned Compile(GLenum type, const char* src)
{
	unsigned sh = glCreateShader(type);
	glShaderSource(sh, 1, &src, nullptr);
	glCompileShader(sh);
	int ok = 0;
	glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[1024] = {};
		glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
		HC_LOG_ERROR("stage3d shader compile failed: {}", log);
	}
	return sh;
}

void EnsureProgram(State& s)
{
	if (s.program) return;
	const unsigned vs = Compile(GL_VERTEX_SHADER, kVs), fsh = Compile(GL_FRAGMENT_SHADER, kFs);
	s.program = glCreateProgram();
	glAttachShader(s.program, vs);
	glAttachShader(s.program, fsh);
	glLinkProgram(s.program);
	glDeleteShader(vs);
	glDeleteShader(fsh);
	const unsigned gv = Compile(GL_VERTEX_SHADER, kGradeVs), gf = Compile(GL_FRAGMENT_SHADER, kGradeFs);
	s.gradeProgram = glCreateProgram();
	glAttachShader(s.gradeProgram, gv);
	glAttachShader(s.gradeProgram, gf);
	glLinkProgram(s.gradeProgram);
	glDeleteShader(gv);
	glDeleteShader(gf);
	glGenVertexArrays(1, &s.emptyVao);
	const uint8_t px[4] = {255, 255, 255, 255};
	glGenTextures(1, &s.white);
	glBindTexture(GL_TEXTURE_2D, s.white);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void FreeGpu(State& s)
{
	for (GpuMesh& m : s.meshes) {
		for (auto& sub : m.subs) glDeleteBuffers(1, &sub.ibo);
		glDeleteBuffers(1, &m.vbo);
		glDeleteVertexArrays(1, &m.vao);
	}
	s.meshes.clear();
	for (unsigned t : s.textures) if (t) glDeleteTextures(1, &t);
	s.textures.clear();
}

// Write time of a watched file, min() when it does not exist (so a bg.fbx.bin appearing next to bg.fbx counts as a change).
static fs::file_time_type WatchTime(const std::string& p)
{
	std::error_code ec;
	const auto t = fs::last_write_time(fs::u8path(p), ec);
	return ec ? fs::file_time_type::min() : t;
}

static bool SameNameNoCase(const std::string& a, const std::string& b)
{
	return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
		return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y)); });
}

static bool WatchChanged(const State& s)
{
	for (size_t i = 0; i < s.watch.size() && i < s.watchTimes.size(); ++i)
		if (WatchTime(s.watch[i]) != s.watchTimes[i]) return true;
	return false;
}

bool Load(State& s, const std::string& bgDir, const std::string& dataFile)
{
	EnsureProgram(s);
	FreeGpu(s);
	s.loaded = false;
	s.error.clear();
	std::string err;
	if (!s3d::LoadBgListFile(bgDir + "/BgList.txt", s.list, err)) HC_LOG_WARN("stage3d: BgList: {}", err);
	const s3d::BgEntry* e = s.list.byDataFile(dataFile);
	s.params = e ? s3d::ParamsFrom(*e) : s3d::StageParams{};
	s.nameUtf8 = e ? AnsiToUtf8(e->str("Name")) : std::string();
	const std::string dir = bgDir + "/" + dataFile;
	std::string used;
	// bg.fbx.bin (UNI2, MBTL) or the ASCII bg.fbx (DFCI, the UNI[st]/[cl-r] arcade build)
	if (!s3d::LoadStageModel(dir, s.model, used, err)) {
		s.error = dir + ": " + err;
		HC_LOG_ERROR("stage3d: {}", s.error);
		return false;
	}
	s.stats = s3d::ComputeStats(s.model);
	{
		std::error_code tec;
		s.modelPath = used;
		s.watch = s3d::BlenderWatchPaths(used);
		s.watchTimes.clear();
		for (const std::string& w : s.watch) s.watchTimes.push_back(WatchTime(w));
		s.changedOnDisk = false;
	}
	// textures: straight decode, LINEAR + WRAP, no mips (the shipped DDS have none; fbxshader.txt diffuseSampler)
	s.textures.assign(s.model.textures.size(), 0);
	int missing = 0;
	for (size_t i = 0; i < s.model.textures.size(); ++i) {
		std::string name = s.model.textures[i];
		if (name.empty()) continue;   // empty table slots (UNI2 bg010, bg025)
		fs::path p = fs::u8path(dir) / fs::u8path(name);
		if (!fs::exists(p)) p.replace_extension(".dds");
		s3d::Image im;
		if (!s3d::LoadDdsFile(p.u8string(), im, err)) { ++missing; HC_LOG_WARN("stage3d: texture {}: {}", name, err); continue; }
		unsigned t = 0;
		glGenTextures(1, &t);
		glBindTexture(GL_TEXTURE_2D, t);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, im.width, im.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, im.rgba.data());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		s.textures[i] = t;
	}
	// Draw order = FbxScene_DrawNodeRecursive (UNI2 0x5BC430): depth-first from each root, child before next sibling.
	// With Z write on for blended nodes too, this order decides what the translucent planes hide.
	std::vector<int> order;
	{
		std::vector<uint8_t> seen(s.model.nodes.size(), 0);
		std::vector<int> stack;
		auto walk = [&](int root) {
			stack.assign(1, root);
			while (!stack.empty()) {
				const int n = stack.back();
				stack.pop_back();
				if (n < 0 || n >= (int)seen.size() || seen[n]) continue;
				seen[n] = 1;
				order.push_back(n);
				if (s.model.nodes[n].nextSibling >= 0) stack.push_back(s.model.nodes[n].nextSibling);
				if (s.model.nodes[n].firstChild >= 0) stack.push_back(s.model.nodes[n].firstChild);
			}
		};
		for (size_t i = 0; i < s.model.nodes.size(); ++i) if (s.model.nodes[i].parent < 0) walk(int(i));
		for (size_t i = 0; i < s.model.nodes.size(); ++i) if (!seen[i]) order.push_back(int(i));
	}
	for (int ni : order) {
		const s3d::Node& n = s.model.nodes[ni];
		if (n.type != 1 || n.verts.empty()) continue;
		GpuMesh g;
		g.node = int(ni);
		glGenVertexArrays(1, &g.vao);
		glBindVertexArray(g.vao);
		glGenBuffers(1, &g.vbo);
		glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
		glBufferData(GL_ARRAY_BUFFER, n.verts.size() * sizeof(s3d::Vertex), n.verts.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(s3d::Vertex), (void*)offsetof(s3d::Vertex, pos));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(s3d::Vertex), (void*)offsetof(s3d::Vertex, color));   // colour3 + alpha are contiguous
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(s3d::Vertex), (void*)offsetof(s3d::Vertex, uv));
		for (const s3d::SubMesh& sm : n.subs) {
			GpuMesh::Sub sub;
			glGenBuffers(1, &sub.ibo);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sub.ibo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sm.indices.size() * 4, sm.indices.data(), GL_STATIC_DRAW);
			sub.count = int(sm.indices.size());
			sub.tex = s.model.materials[sm.material].texture;
			g.subs.push_back(sub);
		}
		glBindVertexArray(0);
		s.meshes.push_back(std::move(g));
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	snprintf(s.bgDir, sizeof(s.bgDir), "%s", bgDir.c_str());
	s.dataFile = dataFile;
	s.loadedName = dataFile;
	s.loaded = true;
	s.frame = 0;
	HC_LOG_INFO("stage3d: loaded {} ({} nodes, {} meshes, {} tris, {} textures, {} missing), FOV {} scale {} fog {}", dir, s.stats.nodes, s.stats.meshes,
		s.stats.triangles, s.model.textures.size(), missing, s.params.fov, s.params.scale[0], s.params.fog ? 1 : 0);
	return true;
}

// --- UNI2 1.42 camera (IDA: BtlCamera_BuildMatrices 0x534910, Stage3D_InitFromBgRecord 0x51B4D0; SCOPE.md §3.2) -----
// D3DX row-vector helpers.
s3d::Mat4 Translation(float x, float y, float z) { s3d::Mat4 m = s3d::MatIdentity(); m[12] = x; m[13] = y; m[14] = z; return m; }
s3d::Mat4 Scaling(float x, float y, float z) { s3d::Mat4 m = s3d::MatIdentity(); m[0] = x; m[5] = y; m[10] = z; return m; }
s3d::Mat4 RotX(float deg) { const float a = deg * 3.14159265f / 180.f, c = std::cos(a), sn = std::sin(a); s3d::Mat4 m = s3d::MatIdentity(); m[5] = c; m[6] = sn; m[9] = -sn; m[10] = c; return m; }
s3d::Mat4 RotY(float deg) { const float a = deg * 3.14159265f / 180.f, c = std::cos(a), sn = std::sin(a); s3d::Mat4 m = s3d::MatIdentity(); m[0] = c; m[2] = -sn; m[8] = sn; m[10] = c; return m; }
s3d::Mat4 PerspectiveFovLH(float fovY, float aspect, float zn, float zf)
{
	s3d::Mat4 m{};
	const float ys = 1.f / std::tan(fovY * 0.5f);
	m[0] = ys / aspect; m[5] = ys; m[10] = zf / (zf - zn); m[11] = 1.f; m[14] = -zn * zf / (zf - zn);
	return m;
}
float FovDeg(const State& s) { return std::max(1.f, s.fovOverride > 0.f ? s.fovOverride : s.params.fov); }
float Cot(const State& s) { return 1.f / std::tan(FovDeg(s) * 3.14159265f / 360.f); }
float Iz(const State& s) { return 1.f / (s.zoom > 0.01f ? s.zoom : 1.f); }
// Proj = PerspectiveFovLH(FOV, aspect 1.0, 0.15, 1000) * T(0, -v, 0) with v = VanishingPoint (YOffsetPx is 0 in 1.42).
// Aspect 1 makes the projection anisotropic; StageWorld folds H/W (0.5625) into Scale.x, so 1 mesh unit = 360 px at z=0.
s3d::Mat4 GameProjection(const State& s)
{
	s3d::Mat4 p = s3d::MatMul(PerspectiveFovLH(FovDeg(s) * 3.14159265f / 180.f, 1.f, 0.15f, 1000.f), Translation(0.f, -s.params.vanishingPoint, 0.f));
	if (s.align) {
		// Lens shift (no perspective change): the floor, at source row 640 of 720, goes to MBAACC's floor row (x1.5 into the
		// 720-line frame) plus the bias. Row-vector translation after the projection adds dy*w to clip y: an NDC shift.
		const float want = (s.floorPx + s.yBias) * 1.5f;
		p = s3d::MatMul(p, Translation(0.f, -(want - 640.f) / 360.f, 0.f));
	}
	return p;
}
// eye = (x/640, -(y - 280*iz)/360 - iz*v, -cot*iz), LookAtLH towards +Z (no rotation): View = T(-eye).
// World y = 0 lands 280 px below the centre (screen y 640 of 720) at zoom 1; zoom dollies, FOV stays.
s3d::Mat4 GameView(const State& s)
{
	const float iz = Iz(s), v = s.params.vanishingPoint;
	const float ex = s.camX / 640.f, ey = -(s.camY - 280.f * iz) / 360.f - iz * v, ez = -Cot(s) * iz;
	return Translation(-ex, -ey, -ez);
}
// W = Scale(Sx*H/W, Sy, Sz) * T(Position) * RotX(ViewRotationX) * RotY(ViewRotationY)
s3d::Mat4 StageWorld(const State& s)
{
	if (!s.bgListTransform) return Scaling(0.5625f, 1.f, 1.f);
	const s3d::StageParams& p = s.params;
	s3d::Mat4 w = s3d::MatMul(Scaling(p.scale[0] * 0.5625f, p.scale[1], p.scale[2]), Translation(p.position[0], p.position[1], p.position[2]));
	w = s3d::MatMul(w, RotX(p.viewRotationX));
	return s3d::MatMul(w, RotY(p.viewRotationY));
}

void Render(State& s)
{
	const int W = s.params.targetW > 0 ? s.params.targetW : 1280, H = s.params.targetH > 0 ? s.params.targetH : 720;
	s.raw.ensure(W, H, true);
	s.rt.ensure(W, H, true);
	if (!s.raw.valid() || !s.rt.valid()) return;
	// save the GL state ImGui's own backend does not restore around us
	GLint prevProg = 0, prevVao = 0, prevTex = 0, prevActive = 0, prevSrc = 0, prevDst = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVao);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActive);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
	glGetIntegerv(GL_BLEND_SRC_RGB, &prevSrc);
	glGetIntegerv(GL_BLEND_DST_RGB, &prevDst);
	const GLboolean prevBlend = glIsEnabled(GL_BLEND), prevDepth = glIsEnabled(GL_DEPTH_TEST), prevCull = glIsEnabled(GL_CULL_FACE);
	GLboolean prevDepthMask = GL_TRUE;
	glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);
	{
		ScopedTargetBinding bind(s.raw);
		bind.clear(s.clear[0], s.clear[1], s.clear[2], 1.f, true);
		const s3d::Mat4 proj = GameProjection(s), view = GameView(s), place = StageWorld(s);
		std::vector<s3d::Mat4> world;
		s3d::EvaluateWorld(s.model, uint64_t(s.frame < 0 ? 0 : s.frame), world);
		glUseProgram(s.program);
		// row-vector D3D matrices uploaded as-is = the same transform for GLSL column vectors (M^T in column-major memory)
		glUniformMatrix4fv(glGetUniformLocation(s.program, "uView"), 1, GL_FALSE, view.data());
		glUniformMatrix4fv(glGetUniformLocation(s.program, "uProj"), 1, GL_FALSE, proj.data());
		const bool fogOn = s.fog && s.params.fog && s.params.fogColor[3] >= 0.001f;   // IsFog == 0 zeroes FogColor
		const float span = s.params.fogEnd - s.params.fogStart;
		glUniform4f(glGetUniformLocation(s.program, "uFogColor"), s.params.fogColor[0], s.params.fogColor[1], s.params.fogColor[2], fogOn ? s.params.fogColor[3] : 0.f);
		glUniform2f(glGetUniformLocation(s.program, "uFogCoord"), span != 0.f ? s.params.fogEnd / span : 1.f, span != 0.f ? -1.f / span : 0.f);
		glUniform1f(glGetUniformLocation(s.program, "uAlphaRef"), 0.f);
		glUniform1i(glGetUniformLocation(s.program, "uTex"), 0);
		glUniform1f(glGetUniformLocation(s.program, "uFlipV"), s.flipV ? 1.f : 0.f);
		const GLint locWorld = glGetUniformLocation(s.program, "uWorld");
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_BLEND);
		if (s.cull) { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(s.cull == 1 ? GL_CCW : GL_CW); } else glDisable(GL_CULL_FACE);
		for (const GpuMesh& g : s.meshes) {   // hierarchy order (built in Load)
			const s3d::Node& n = s.model.nodes[g.node];
			s3d::Mat4 nw = world[g.node];
			if (s.zMirror) { nw[2] = -nw[2]; nw[6] = -nw[6]; nw[10] = -nw[10]; nw[14] = -nw[14]; }
			const s3d::Mat4 w = s3d::MatMul(nw, place);
			glUniformMatrix4fv(locWorld, 1, GL_FALSE, w.data());
			// FbxMesh_Draw: SRCALPHA/INVSRCALPHA, additive (dest ONE) for the node flag; Z write stays on for both
			if (n.additive) glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
			else glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE);
			glDepthMask(GL_TRUE);
			glBindVertexArray(g.vao);
			for (const auto& sub : g.subs) {
				const unsigned t = sub.tex >= 0 && sub.tex < (int)s.textures.size() && s.textures[sub.tex] ? s.textures[sub.tex] : s.white;
				glBindTexture(GL_TEXTURE_2D, t);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sub.ibo);
				glDrawElements(GL_TRIANGLES, sub.count, GL_UNSIGNED_INT, nullptr);
			}
		}
		glBindVertexArray(0);
	}
	{
		ScopedTargetBinding bind(s.rt);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
		glDisable(GL_CULL_FACE);
		glUseProgram(s.gradeProgram);
		glUniform1f(glGetUniformLocation(s.gradeProgram, "uLift"), s.lift);
		glUniform1f(glGetUniformLocation(s.gradeProgram, "uGain"), s.contrast);
		glUniform1i(glGetUniformLocation(s.gradeProgram, "uTex"), 0);
		glBindTexture(GL_TEXTURE_2D, s.raw.texture());
		glBindVertexArray(s.emptyVao);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glBindVertexArray(0);
	}
	glDepthMask(prevDepthMask);
	if (prevBlend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
	if (prevDepth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	if (prevCull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
	glBlendFunc(prevSrc, prevDst);
	glBindTexture(GL_TEXTURE_2D, prevTex);
	glActiveTexture(prevActive);
	glBindVertexArray(prevVao);
	glUseProgram(prevProg);
}

double Now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

bool Capture(State& s, const std::string& path, std::string& err)
{
	if (!s.loaded) { err = "no stage loaded"; return false; }
	Render(s);
	std::vector<uint8_t> rgba;
	if (!s.rt.readRgba(rgba)) { err = "read failed"; return false; }
	for (size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
	std::error_code ec;
	fs::create_directories(fs::u8path(path).parent_path(), ec);
	return WritePngRgba(path, rgba.data(), s.rt.width(), s.rt.height(), err);
}

}  // namespace

void DrawAlignView(State& s, ImVec2 avail);
void WriteAlignIni(State& s);

// Blender link (s3d_blender.h). The configured path (default: the 5.2.2 portable build) when it exists, else the newest
// detected install (5.x first).
static std::string BlenderExe()
{
	std::error_code ec;
	if (!gSettings.blenderPath.empty() && fs::exists(fs::u8path(gSettings.blenderPath), ec)) return gSettings.blenderPath;
	const std::string d = s3d::DetectBlender();
	if (!gSettings.blenderPath.empty()) HC_LOG_WARN("stage3d: blender: {} not found, using {}", gSettings.blenderPath, d.empty() ? std::string("none") : d);
	return d;
}

// background: the same command plus --background (ui-script check of the launch path without a Blender window)
static bool LaunchBlender(State& s, std::string& cmdOut, bool background = false)
{
	std::string err;
	const std::string exe = BlenderExe();
	if (exe.empty()) { s.blenderStatus = TXT("Blender not found: set its path in Preferences."); HC_LOG_WARN("stage3d: blender: not found"); return false; }
	cmdOut = s3d::BlenderCommandLine(exe, fs::absolute(fs::u8path(s.modelPath)).u8string(), err);
	if (cmdOut.empty()) { s.blenderStatus = err; HC_LOG_ERROR("stage3d: blender: {}", err); return false; }
	if (background) cmdOut += " --background";
#ifdef _WIN32
	auto wide = [](const std::string& u) {
		std::wstring w(MultiByteToWideChar(CP_UTF8, 0, u.c_str(), int(u.size()), nullptr, 0), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, u.c_str(), int(u.size()), w.data(), int(w.size()));
		return w;
	};
	std::wstring cl = wide(cmdOut);
	const std::wstring dir = wide(fs::u8path(exe).parent_path().u8string());
	STARTUPINFOW si{};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi{};
	if (!CreateProcessW(nullptr, cl.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, dir.empty() ? nullptr : dir.c_str(), &si, &pi)) {
		s.blenderStatus = std::string(TXT("Could not start Blender:")) + " " + exe;
		HC_LOG_ERROR("stage3d: blender: CreateProcess failed ({}): {}", GetLastError(), cmdOut);
		return false;
	}
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
#endif
	s.blenderStatus = TXT("Blender started. Export from Blender, then Reload from Blender.");
	HC_LOG_INFO("stage3d: blender: {}", cmdOut);
	return true;
}

// Preferences > 3D stage: Blender executable for "Open in Blender".
void BlenderPrefsItems()
{
	char path[512];
	snprintf(path, sizeof path, "%s", gSettings.blenderPath.c_str());
	ImGui::SetNextItemWidth(360);
	if (ImGui::InputTextWithHint(LBL("Blender executable"), TXT("auto-detect"), path, sizeof path)) {
		gSettings.blenderPath = path;
		ImGui::MarkIniSettingsDirty();
	}
	ImGui::SameLine();
	if (ImGui::SmallButton(LBL("Auto-detect"))) {
		const std::string d = s3d::DetectBlender();
		if (!d.empty()) { gSettings.blenderPath = d; ImGui::MarkIniSettingsDirty(); }
		HC_LOG_INFO("stage3d: blender auto-detect: {}", d.empty() ? std::string("none") : d);
	}
	ImGui::TextDisabled("%s", TXT("Used by the 3D Stage view's Open in Blender (MBTL_fbx_json add-on, Blender 5.2). Missing or empty: newest Blender found."));
}

void MenuItem()
{
	if (ImGui::MenuItem("3D Stage view (UNI2 / UNI / MBTL)...", nullptr, show)) show = !show;
}

void Draw()
{
	if (!show) return;
	State& s = *S();
	ImGui::SetNextWindowSize(ImVec2(900, 640), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("3D Stage", &show)) { ImGui::End(); return; }
	ImGui::SetNextItemWidth(360);
	ImGui::InputText("bg folder", s.bgDir, sizeof(s.bgDir));
	ImGui::SameLine();
	if (ImGui::Button("Scan")) { std::string err; s3d::LoadBgListFile(std::string(s.bgDir) + "/BgList.txt", s.list, err); }
	ImGui::SameLine();
	ImGui::SetNextItemWidth(160);
	if (ImGui::BeginCombo("stage", s.dataFile.c_str())) {
		// BgList entries with a model (bg.fbx.bin, UNIEL enciphered included, or the ASCII bg.fbx), then stage folders
		// BgList does not name (no / unreadable BgList, ASCII-only DFCI / UNIB folders)
		const std::vector<std::string> folders = s3d::ScanStageFolders(s.bgDir);
		std::vector<bool> named(folders.size(), false);
		for (const s3d::BgEntry& e : s.list.entries) {
			const std::string df = e.str("DataFile");
			bool found = false;
			for (size_t i = 0; i < folders.size(); ++i)
				if (!df.empty() && SameNameNoCase(folders[i], df)) { found = true; named[i] = true; }
			if (!found) continue;
			const std::string label = df + "  " + AnsiToUtf8(e.str("Name"));
			if (ImGui::Selectable(label.c_str(), df == s.dataFile)) Load(s, s.bgDir, df);
		}
		for (size_t i = 0; i < folders.size(); ++i)
			if (!named[i] && ImGui::Selectable(folders[i].c_str(), folders[i] == s.dataFile)) Load(s, s.bgDir, folders[i]);
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	if (ImGui::Button(s.loaded ? "Reload" : "Load")) Load(s, s.bgDir, s.dataFile);
	if (s.loaded) {
		ImGui::SameLine();
		std::string cmd;
		if (ImGui::Button(LBL("Open in Blender"))) LaunchBlender(s, cmd);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TXT("Start Blender with the MBTL_fbx_json add-on importing this stage. Its export writes bg.fbx.bin back into the stage folder."));
		ImGui::SameLine();
		if (Now() - s.lastDiskCheck > 1.0) {   // poll the model file's write time once a second
			s.lastDiskCheck = Now();
			const bool was = s.changedOnDisk;
			s.changedOnDisk = WatchChanged(s);
			if (s.changedOnDisk && !was) HC_LOG_INFO("stage3d: blender: {} changed on disk", s.watch.empty() ? s.modelPath : s.watch[0]);
		}
		if (ImGui::Button(LBL("Reload from Blender"))) Load(s, s.bgDir, s.dataFile);
		if (s.changedOnDisk) { ImGui::SameLine(); ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "%s", TXT("(changed on disk)")); }
		if (!s.blenderStatus.empty()) ImGui::TextDisabled("%s", s.blenderStatus.c_str());
	}
	if (!s.error.empty()) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", s.error.c_str());
	if (s.loaded) {
		ImGui::Text("%s %s | %u nodes, %u meshes (%u additive, %u animated), %u tris | FOV %.1f, scale %.3g, pos %.3g %.3g %.3g, fog %s %.0f..%.0f",
			s.loadedName.c_str(), s.nameUtf8.c_str(), s.stats.nodes, s.stats.meshes, s.stats.additive, s.stats.animated, s.stats.triangles, s.params.fov,
			s.params.scale[0], s.params.position[0], s.params.position[1], s.params.position[2], s.params.fog ? "on" : "off", s.params.fogStart, s.params.fogEnd);
		ImGui::SetNextItemWidth(120); ImGui::DragFloat(LBL("cam x (px)"), &s.camX, 1.f); ImGui::SameLine();
		ImGui::SetNextItemWidth(120); ImGui::DragFloat(LBL("cam y (px)"), &s.camY, 1.f); ImGui::SameLine();
		ImGui::SetNextItemWidth(120); ImGui::DragFloat(LBL("zoom"), &s.zoom, 0.005f, 0.2f, 4.f); ImGui::SameLine();
		ImGui::SetNextItemWidth(120); ImGui::DragFloat(LBL("FOV override"), &s.fovOverride, 0.1f, 0.f, 120.f); ImGui::SameLine();
		ImGui::Checkbox(LBL("fog"), &s.fog); ImGui::SameLine();
		ImGui::SetNextItemWidth(140); ImGui::Combo(LBL("cull"), &s.cull, "none\0clockwise\0counter-clockwise\0");
		ImGui::SetNextItemWidth(120); ImGui::DragFloat(LBL("lift"), &s.lift, 0.005f, -1.f, 1.f); ImGui::SameLine();
		ImGui::SetNextItemWidth(120); ImGui::DragFloat(LBL("contrast"), &s.contrast, 0.005f, 0.f, 4.f); ImGui::SameLine();
		if (ImGui::Checkbox(LBL("play"), &s.play)) { s.playStart = Now(); s.playStartFrame = s.frame; }
		ImGui::SameLine();
		ImGui::SetNextItemWidth(160); ImGui::DragInt(LBL("frame (30 fps)"), &s.frame, 1.f, 0, 100000);
		if (s.play) s.frame = s.playStartFrame + int((Now() - s.playStart) * 30.0);
		ImGui::Checkbox(LBL("MBAACC alignment"), &s.align);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TXT("Show the stage as an MBAACC port sees it: the centre 4:3 of the frame (same vertical FOV), driven by MBAACC's 2D camera, with its floor row, walls and a fighter-height ruler. Tune, then write stage3d.ini for PovertyCaster's pc-stage3d."));
		if (s.align) {
			ImGui::SetNextItemWidth(110); ImGui::DragFloat(LBL("MBAACC cam x"), &s.mbCamX, 1.f, -208.f, 208.f, "%.0f"); ImGui::SameLine();
			ImGui::SetNextItemWidth(110); ImGui::DragFloat(LBL("MBAACC zoom"), &s.mbZoom, 0.005f, 0.5f, 1.5f); ImGui::SameLine();
			ImGui::SetNextItemWidth(110); ImGui::DragFloat(LBL("fighter scale"), &s.fighterScale, 0.005f, 0.3f, 2.f); ImGui::SameLine();
			ImGui::SetNextItemWidth(110); ImGui::DragFloat(LBL("center x (units)"), &s.centerX, 0.01f); ImGui::SameLine();
			ImGui::SetNextItemWidth(110); ImGui::DragFloat(LBL("floor row"), &s.floorPx, 0.5f, 300.f, 480.f, "%.1f"); ImGui::SameLine();
			ImGui::SetNextItemWidth(110); ImGui::DragFloat(LBL("y bias"), &s.yBias, 0.25f, -60.f, 60.f, "%.2f"); ImGui::SameLine();
			ImGui::SetNextItemWidth(110); ImGui::InputInt(LBL("replaces slot"), &s.replaces);
			if (ImGui::Button(LBL("Write stage3d.ini..."))) WriteAlignIni(s);
			if (!s.alignStatus.empty()) { ImGui::SameLine(); ImGui::TextUnformatted(s.alignStatus.c_str()); }
			s.mbCamX = std::clamp(s.mbCamX, -208.f, 208.f);
			// 1 MBAACC px = 1.5 / s source px; the source zoom shows the same world height (720/s px over 480 lines)
			s.camX = s.centerX * 640.f + s.mbCamX * 1.5f / std::max(0.01f, s.fighterScale);
			s.camY = 0.f;
			s.zoom = s.mbZoom * s.fighterScale;
		}
		Render(s);
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		if (s.align) {
			DrawAlignView(s, avail);
		} else if (const float sc = std::min(avail.x / s.rt.width(), avail.y / s.rt.height()); sc > 0.f) {
			ImGui::Image((ImTextureID)(intptr_t)s.rt.texture(), ImVec2(s.rt.width() * sc, s.rt.height() * sc), ImVec2(0, 1), ImVec2(1, 0));
			// Interactive fight camera: drag pans (game px of the 1280x720 frame), wheel zooms about the frame, double-click
			// resets. Same state the cam fields above edit, so values can still be typed exactly.
			if (ImGui::IsItemHovered()) {
				ImGuiIO& io = ImGui::GetIO();
				const float toGame = 1280.f / (s.rt.width() * sc);
				if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.f) && (io.MouseDelta.x != 0.f || io.MouseDelta.y != 0.f)) {
					s.camX -= io.MouseDelta.x * toGame * Iz(s);
					s.camY -= io.MouseDelta.y * toGame * Iz(s);
				}
				if (io.MouseWheel != 0.f) s.zoom = std::clamp(s.zoom * std::pow(1.1f, io.MouseWheel), 0.2f, 4.f);
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { s.camX = 0.f; s.camY = 0.f; s.zoom = 1.f; }
				ImGui::SetItemTooltip("%s", TXT("Drag: pan the fight camera. Wheel: zoom. Double-click: reset."));
			}
		}
	}
	ImGui::End();
}

// The 640x480 MBAACC picture: centre 4:3 crop (x 160..1120 of 1280) of the composited stage, plus MBAACC's 2D landmarks.
void DrawAlignView(State& s, ImVec2 avail)
{
	const float sc = std::min(avail.x / 640.f, avail.y / 480.f);
	if (sc <= 0.f) return;
	const ImVec2 o = ImGui::GetCursorScreenPos();
	ImGui::Image((ImTextureID)(intptr_t)s.rt.texture(), ImVec2(640.f * sc, 480.f * sc), ImVec2(0.125f, 1.f), ImVec2(0.875f, 0.f));
	ImDrawList* dl = ImGui::GetWindowDrawList();
	auto P = [&](float x, float y) { return ImVec2(o.x + x * sc, o.y + y * sc); };
	dl->PushClipRect(P(0, 0), P(640, 480), true);   // landmarks off the 640x480 picture (walls at cam 0) are not drawn
	const float z = s.mbZoom;
	auto screenX = [&](float worldX) { return 320.f + z * (worldX - s.mbCamX); };   // screen = zoom*(world - cam) + (320, 432)
	const ImU32 floorCol = IM_COL32(255, 220, 0, 220), wallCol = IM_COL32(255, 80, 80, 220), rulerCol = IM_COL32(80, 220, 255, 230),
		srcWallCol = IM_COL32(255, 160, 255, 200);
	dl->AddLine(P(0, s.floorPx), P(640, s.floorPx), floorCol, 1.5f);
	// MBAACC walls: camera clamp 67584 fixed-point - 40960/zoom -> +-528 px of world (pc-stage3d host_world_w 1056)
	for (float w : {-528.f, 528.f}) { const float x = screenX(w); dl->AddLine(P(x, 0), P(x, 480), wallCol, 2.f); }
	// The source game's own walls (BgList StageW, its 1280-wide px), in MBAACC world px: (+-W/2 - center_x*640) / (1.5/s)
	const float stageW = s.params.stageW > 0 ? s.params.stageW : 4096.f;
	for (float w : {-stageW / 2.f, stageW / 2.f}) {
		const float x = screenX((w - s.centerX * 640.f) * s.fighterScale / 1.5f);
		for (float y = 0; y < 480; y += 12) dl->AddLine(P(x, y), P(x, y + 6), srcWallCol, 1.f);
	}
	// fighter ruler at world x 0: measured MBAACC fighters ~195 px, the source character 237*s at this scale
	const float rx = screenX(0.f);
	dl->AddRectFilled(P(rx - 3, s.floorPx - 195.f * z), P(rx + 3, s.floorPx), IM_COL32(80, 220, 255, 90));
	dl->AddLine(P(rx - 10, s.floorPx - 195.f * z), P(rx + 10, s.floorPx - 195.f * z), rulerCol, 1.5f);
	dl->AddLine(P(rx - 6, s.floorPx - 237.f * s.fighterScale * z), P(rx + 6, s.floorPx - 237.f * s.fighterScale * z), srcWallCol, 1.5f);
	dl->AddText(P(rx + 12, s.floorPx - 195.f * z - 7), rulerCol, "195 MBAACC fighter");
	dl->AddText(P(rx + 12, s.floorPx - 237.f * s.fighterScale * z - 7), srcWallCol, "source character");
	dl->AddText(P(6, s.floorPx + 4), floorCol, "MBAACC floor");
	dl->PopClipRect();
}

// stage3d.ini for PovertyCaster's pc-stage3d (keys: pc-stage3d include/pc/stage3d/StageIni.hpp), next to the stage folder.
void WriteAlignIni(State& s)
{
	namespace fs = std::filesystem;
	const fs::path dir = fs::u8path(std::string(s.bgDir)) / fs::u8path(s.dataFile);
	char b[1024];
	snprintf(b, sizeof b,
		"; written by Hantei-chan's 3D Stage view, MBAACC alignment (%s)\n"
		"%s"
		"package = %s\n"
		"fit_mode = crop\n"
		"center_x = %.4f\n"
		"fighter_px = %.2f\n"
		"uni_char_px = 355\n"
		"floor_px = %.2f\n"
		"stage_w = %.0f\n"
		"host_world_w = 1056\n",
		s.loadedName.c_str(), s.replaces >= 0 ? ("replaces = " + std::to_string(s.replaces) + "\n").c_str() : "",
		dir.generic_u8string().c_str(), s.centerX, 237.f * s.fighterScale, s.floorPx + s.yBias,
		s.params.stageW > 0 ? s.params.stageW : 4096.f);
	const fs::path out = dir / "stage3d.ini";
	std::ofstream f(out, std::ios::binary);
	f << b;
	s.alignStatus = f ? std::string(TXT("Wrote")) + " " + out.u8string() : std::string(TXT("Save failed.")) + " " + out.u8string();
}

bool ChangedOnDiskNow()
{
	State& s = *S();
	return s.loaded && WatchChanged(s);
}

bool ScriptCommand(const std::string& cmd, const std::string& arg, std::string* msg)
{
	if (cmd != "stage3d") return false;
	State& s = *S();
	const size_t sp = arg.find(' ');
	const std::string sub = arg.substr(0, sp), rest = sp == std::string::npos ? "" : arg.substr(sp + 1);
	std::string err;
	auto out = [&](const std::string& m) { if (msg) *msg = m; };
	if (sub == "open") {
		show = true;
		const size_t sp2 = rest.rfind(' ');
		if (sp2 == std::string::npos) { out("usage: stage3d open <bg folder> <bgNNN>"); return true; }
		const bool ok = Load(s, rest.substr(0, sp2), rest.substr(sp2 + 1));
		out(ok ? "ok " + s.loadedName : "FAILED " + s.error);
		return true;
	}
	if (sub == "align") {   // align on|off [mbCamX mbZoom fighterScale centerX]
		show = true;
		char onoff[8] = {};
		float v[4] = {s.mbCamX, s.mbZoom, s.fighterScale, s.centerX};
		sscanf(rest.c_str(), "%7s %f %f %f %f", onoff, &v[0], &v[1], &v[2], &v[3]);
		s.align = std::string(onoff) != "off";
		s.mbCamX = v[0]; s.mbZoom = v[1]; s.fighterScale = v[2]; s.centerX = v[3];
		out("ok");
		return true;
	}
	if (sub == "blender") {   // blender [launch]: the launch command for the loaded stage; "launch" also starts Blender
		if (!s.loaded) { out("FAILED no stage loaded"); return true; }
		if (rest == "launch" || rest == "launch-bg") {
			std::string cmd;
			const bool ok = LaunchBlender(s, cmd, rest == "launch-bg");
			out(ok ? "ok " + cmd : "FAILED " + s.blenderStatus);
			return true;
		}
		const std::string exe = BlenderExe();
		const std::string cmd = s3d::BlenderCommandLine(exe.empty() ? std::string("blender.exe") : exe, fs::absolute(fs::u8path(s.modelPath)).u8string(), err);
		out(cmd.empty() ? "FAILED " + err : cmd);
		return true;
	}
	if (sub == "reload") { const bool ok = Load(s, s.bgDir, s.dataFile); out(ok ? "ok " + s.loadedName : "FAILED " + s.error); return true; }
	if (sub == "alignini") { WriteAlignIni(s); out(s.alignStatus); return true; }
	if (sub == "frame") { s.play = false; s.frame = atoi(rest.c_str()); out("ok"); return true; }
	if (sub == "cam") { sscanf(rest.c_str(), "%f %f %f", &s.camX, &s.camY, &s.zoom); out("ok"); return true; }
	if (sub == "set") {
		char key[64] = {};
		float v = 0;
		if (sscanf(rest.c_str(), "%63s %f", key, &v) != 2) { out("usage: stage3d set <key> <value>"); return true; }
		const std::string k = key;
		if (k == "lift") s.lift = v; else if (k == "contrast") s.contrast = v; else if (k == "cull") s.cull = int(v);
		else if (k == "fog") s.fog = v != 0; else if (k == "fov") s.fovOverride = v; else if (k == "zmirror") s.zMirror = v != 0; else if (k == "flipv") s.flipV = v != 0;
		else if (k == "bglist") s.bgListTransform = v != 0;
		else { out("unknown key " + k); return true; }
		out("ok");
		return true;
	}
	if (sub == "capture") { out(Capture(s, rest, err) ? "ok " + rest : "FAILED " + err); return true; }
	if (sub == "status") {
		char b[512];
		snprintf(b, sizeof(b), "%s loaded=%d nodes=%u meshes=%u tris=%u fov=%.2f bounds=[%.2f %.2f %.2f]..[%.2f %.2f %.2f]", s.loadedName.c_str(), s.loaded ? 1 : 0,
			s.stats.nodes, s.stats.meshes, s.stats.triangles, s.params.fov, s.stats.bmin[0], s.stats.bmin[1], s.stats.bmin[2], s.stats.bmax[0], s.stats.bmax[1], s.stats.bmax[2]);
		std::string line = b;
		if (s.loaded) {   // Blender link: the loaded model, the watched files and the "changed on disk" state (polled now)
			line += " model=" + s.modelPath + " watch=";
			for (size_t i = 0; i < s.watch.size(); ++i) line += (i ? "," : "") + s.watch[i];
			line += std::string(" changed_on_disk=") + (WatchChanged(s) ? "1" : "0");
		}
		out(line);
		return true;
	}
	if (sub == "stages") {   // stages: the combo's list (BgList entries with a model + unlisted stage folders)
		std::string line;
		for (const std::string& f : s3d::ScanStageFolders(rest.empty() ? std::string(s.bgDir) : rest)) line += (line.empty() ? "" : " ") + f;
		out(line.empty() ? "FAILED no stage folders" : line);
		return true;
	}
	out("unknown stage3d command " + sub);
	return true;
}

}  // namespace stage3dview
