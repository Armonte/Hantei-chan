// stage3d_test: fbxex model reader, DDS decoder and BgList parser (src/stage3d). Synthetic inputs always; the extracted
// UNI2 stages (gamedata/uni2/bg, never committed) when present: every bgNNN/bg.fbx.bin parses, every referenced texture
// decodes, every stage has a BgList entry.
#include "stage3d/s3d_bglist.h"
#include "stage3d/s3d_dds.h"
#include "stage3d/s3d_model.h"
#include "stage3d/s3d_fbxascii.h"
#include "stage3d/s3d_blender.h"
#include "stage3d/s3d_align.h"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void Put32(std::vector<uint8_t>& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (8 * i))); }
static void PutF(std::vector<uint8_t>& b, float f) { uint32_t v; memcpy(&v, &f, 4); Put32(b, v); }
static void PutBlock(std::vector<uint8_t>& b, uint32_t count, const std::vector<uint8_t>& body) { Put32(b, uint32_t(body.size() + 8)); Put32(b, count); b.insert(b.end(), body.begin(), body.end()); }

static std::vector<uint8_t> Synthetic()
{
	std::vector<uint8_t> f = {'f', 'b', 'x', 'e', 'x', 0, 0, 0};
	Put32(f, 0); Put32(f, 0);
	std::vector<uint8_t> tex(128, 0); memcpy(tex.data(), "a.dds", 5);
	PutBlock(f, 1, tex);
	std::vector<uint8_t> mat(204, 0); memcpy(mat.data(), "a.dds", 5);
	PutBlock(f, 1, mat);
	std::vector<uint8_t> nodes;
	// node 0: root transform with child 1
	Put32(nodes, 16); Put32(nodes, 0); Put32(nodes, 1); Put32(nodes, uint32_t(-1));
	// node 1: one triangle
	std::vector<uint8_t> m;
	Put32(m, 1); Put32(m, 0);
	for (int i = 0; i < 16; ++i) PutF(m, i % 5 == 0 ? 1.f : 0.f);
	Put32(m, 3);
	for (int v = 0; v < 3; ++v) for (int k = 0; k < 12; ++k) PutF(m, float(v + k));
	Put32(m, 1); Put32(m, 0); Put32(m, 3); Put32(m, 0); Put32(m, 1); Put32(m, 2);
	Put32(nodes, uint32_t(16 + m.size())); Put32(nodes, 1); Put32(nodes, uint32_t(-1)); Put32(nodes, uint32_t(-1));
	nodes.insert(nodes.end(), m.begin(), m.end());
	PutBlock(f, 2, nodes);
	std::vector<uint8_t> an;
	Put32(an, 1); for (int i = 0; i < 16; ++i) PutF(an, i % 5 == 0 ? 1.f : 0.f);
	Put32(an, 2); for (int fr = 0; fr < 2; ++fr) for (int i = 0; i < 16; ++i) PutF(an, i % 5 == 0 ? 1.f : (i == 12 ? float(fr) : 0.f));
	PutBlock(f, 2, an);
	return f;
}

static void TestSynthetic()
{
	auto f = Synthetic();
	s3d::Model m;
	std::string err;
	CHECK(s3d::ParseFbxEx(f.data(), f.size(), m, err));
	CHECK(m.nodes.size() == 2 && m.nodes[1].parent == 0 && m.nodes[1].verts.size() == 3 && m.nodes[1].subs[0].indices.size() == 3);
	CHECK(m.textures.size() == 1 && m.textures[0] == "a.dds" && m.materials[0].texture == 0);
	std::vector<s3d::Mat4> w;
	s3d::EvaluateWorld(m, 1, w);
	CHECK(std::fabs(w[1][12] - 1.f) < 1e-6f);
	s3d::EvaluateWorld(m, 2, w);   // track of 2 wraps
	CHECK(std::fabs(w[1][12]) < 1e-6f);
	for (size_t cut = 0; cut < f.size(); cut += 7) CHECK(!s3d::ParseFbxEx(f.data(), cut, m, err));   // truncation never reads past the end
	auto bad = f; bad[0] = 'x';
	CHECK(!s3d::ParseFbxEx(bad.data(), bad.size(), m, err));
}

static void TestDds()
{
	// 4x4 DXT1 block: c0 = white, c1 = black, all indices 1 -> black; then 2x1 A8R8G8B8
	std::vector<uint8_t> d(128, 0);
	memcpy(d.data(), "DDS ", 4);
	d[4] = 124; d[12] = 4; d[16] = 4;
	d[80] = 4; memcpy(&d[84], "DXT1", 4);
	const uint8_t blk[8] = {0xff, 0xff, 0, 0, 0x55, 0x55, 0x55, 0x55};
	d.insert(d.end(), blk, blk + 8);
	s3d::Image im;
	std::string err;
	CHECK(s3d::DecodeDds(d.data(), d.size(), im, err));
	CHECK(im.width == 4 && im.rgba[0] == 0 && im.rgba[3] == 255);
	std::vector<uint8_t> r(128, 0);
	memcpy(r.data(), "DDS ", 4);
	r[4] = 124; r[12] = 1; r[16] = 2; r[80] = 0x41; r[88] = 32;
	const uint32_t masks[4] = {0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000};
	memcpy(&r[92], masks, 16);
	const uint8_t px[8] = {0x30, 0x20, 0x10, 0x80, 1, 2, 3, 4};   // BGRA in memory
	r.insert(r.end(), px, px + 8);
	CHECK(s3d::DecodeDds(r.data(), r.size(), im, err));
	CHECK(im.rgba[0] == 0x10 && im.rgba[1] == 0x20 && im.rgba[2] == 0x30 && im.rgba[3] == 0x80);
	CHECK(!s3d::DecodeDds(r.data(), 133, im, err));   // truncated pixel data
}

static void TestBgList()
{
	const char* t =
		"// comment = 1\nBgList <-\n{\n\tBgSelectList = [ 1, 2, 90 ],\n\tBg_001 =\n\t{\n\t\tName = \"a//b\", // x\n\t\tDataFile = \"bg001\",\n"
		"\t\tScale = [ 2.0, 2.0, 2.0 ],\n\t\tPosition = [ 0.0, -0.05, 0.0 ],\n\t\tFOV = 45.0,\n\t\tFogColor = [ 0.3, -0.2, 0.0, 1.0 ],\n"
		"\t\tShadowLightStatus = [ { Type = 1, Color = [1,1,1,1] }, ],\n\t}\n\tBg_090 = { DataFile = \"bg090\", FOV = 30.0, }\n}\n";
	s3d::BgList L;
	std::string err;
	CHECK(s3d::ParseBgList(t, L, err));
	CHECK(L.selectList.size() == 3 && L.entries.size() == 2);
	const s3d::BgEntry* e = L.byDataFile("BG001");
	CHECK(e && e->str("Name") == "a//b" && e->raw.count("ShadowLightStatus"));
	if (e) {
		s3d::StageParams p = s3d::ParamsFrom(*e);
		CHECK(p.scale[0] == 2.f && std::fabs(p.position[1] + 0.05f) < 1e-6f && p.fov == 45.f && std::fabs(p.fogColor[1] + 0.2f) < 1e-6f);
	}
	CHECK(L.byNumber(90) && L.byNumber(90)->num("FOV", 0) == 30.f);
}

static void TestShipped(const std::string& bg)
{
	namespace fs = std::filesystem;
	if (!fs::is_directory(fs::u8path(bg))) { std::printf("skip shipped: %s missing\n", bg.c_str()); return; }
	s3d::BgList L;
	std::string err;
	CHECK(s3d::LoadBgListFile(bg + "/BgList.txt", L, err));
	int stages = 0, textures = 0;
	for (auto& d : fs::directory_iterator(fs::u8path(bg))) {
		const fs::path bin = d.path() / "bg.fbx.bin";
		if (!fs::exists(bin)) continue;
		s3d::Model m;
		const bool ok = s3d::LoadFbxExFile(bin.u8string(), m, err);
		if (!ok) std::fprintf(stderr, "%s: %s\n", bin.u8string().c_str(), err.c_str());
		CHECK(ok);
		CHECK(L.byDataFile(d.path().filename().u8string()) != nullptr);
		for (const std::string& t : m.textures) {
			const fs::path tp = d.path() / fs::u8path(t);
			if (t.empty() || !fs::exists(tp)) continue;   // empty slots (bg010, bg025) and names the folder does not carry
			s3d::Image im;
			const bool tok = s3d::LoadDdsFile(tp.u8string(), im, err);
			if (!tok) std::fprintf(stderr, "%s: %s\n", tp.u8string().c_str(), err.c_str());
			CHECK(tok);
			++textures;
		}
		++stages;
	}
	std::printf("shipped: %d stages, %d textures\n", stages, textures);
	CHECK(stages > 0);
}

// UNIEL: RC4-enciphered bg.fbx.bin / DDS load like plain ones; baked world-matrix tracks are rebased onto their parents.
static s3d::Mat4 Trs(float tx, float ty, float rz, float sc)
{
	s3d::Mat4 m = s3d::MatIdentity();
	const float c = std::cos(rz) * sc, s = std::sin(rz) * sc;
	m[0] = c; m[1] = s; m[4] = -s; m[5] = c; m[10] = sc; m[12] = tx; m[13] = ty;
	return m;
}
static void TestUniel()
{
	namespace fs = std::filesystem;
	std::string err;
	// cipher: symmetric, and the loaders take either form
	auto plain = Synthetic();
	auto enc = plain;
	s3d::UnielCrypt(enc.data(), enc.size());
	CHECK(enc != plain);
	auto back = enc;
	s3d::UnielCrypt(back.data(), back.size());
	CHECK(back == plain);
	const fs::path tmp = fs::temp_directory_path() / "hc_stage3d_uniel";
	fs::create_directories(tmp);
	{ std::ofstream o(tmp / "bg.fbx.bin", std::ios::binary); o.write((const char*)enc.data(), (std::streamsize)enc.size()); }
	s3d::Model a, b;
	CHECK(s3d::LoadFbxExFile((tmp / "bg.fbx.bin").u8string(), a, err));
	CHECK(s3d::ParseFbxEx(plain.data(), plain.size(), b, err));
	CHECK(a.nodes.size() == b.nodes.size() && a.textures == b.textures);
	const fs::path dds = fs::u8path("C:/dev/hantei-chan/gamedata/uni2/bg/bg001");
	if (fs::is_directory(dds)) {
		for (auto& e : fs::directory_iterator(dds)) {
			if (e.path().extension() != ".dds") continue;
			std::ifstream f(e.path(), std::ios::binary);
			std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), {});
			s3d::UnielCrypt(d.data(), d.size());
			{ std::ofstream o(tmp / "t.dds", std::ios::binary); o.write((const char*)d.data(), (std::streamsize)d.size()); }
			s3d::Image x, y;
			CHECK(s3d::LoadDdsFile((tmp / "t.dds").u8string(), x, err));
			CHECK(s3d::LoadDdsFile(e.path().u8string(), y, err));
			CHECK(x.width == y.width && x.height == y.height && x.rgba == y.rgba);
			break;
		}
	}
	fs::remove_all(tmp);
	// rebase: root 0 -> 1 -> 2 -> 3, all meshes, 4 frames of local animation, stored as world matrices
	s3d::Model m;
	m.nodes.resize(4);
	for (int i = 1; i < 4; ++i) { m.nodes[i].type = 1; m.nodes[i].parent = i - 1; }
	m.nodes[0].firstChild = 1; m.nodes[1].firstChild = 2; m.nodes[2].firstChild = 3;
	std::vector<std::vector<s3d::Mat4>> local(4);
	local[0] = {s3d::MatIdentity()};
	for (int i = 1; i < 4; ++i)
		for (int f = 0; f < 4; ++f) local[i].push_back(Trs(1.f * i + 0.25f * f, 0.5f * f - i, 0.3f * i + 0.1f * f, 1.f + 0.05f * i));
	m.tracks.assign(4, {});
	m.tracks[0] = local[0];
	for (int i = 1; i < 4; ++i)
		for (int f = 0; f < 4; ++f) m.tracks[i].push_back(i == 1 ? local[i][f] : s3d::MatMul(local[i][f], m.tracks[i - 1][f]));
	for (int i = 1; i < 4; ++i) m.nodes[i].world = m.tracks[i][0];
	const auto worldTracks = m.tracks;
	CHECK(s3d::RebaseBakedWorldTracks(m) == 2);   // nodes 2 and 3; node 1 sits under the root
	for (int i = 1; i < 4; ++i)
		for (int f = 0; f < 4; ++f)
			for (int k = 0; k < 16; ++k) CHECK(std::fabs(m.tracks[i][f][k] - local[i][f][k]) < 1e-4f);
	std::vector<s3d::Mat4> w;
	for (int f = 0; f < 4; ++f) {
		s3d::EvaluateWorld(m, uint64_t(f), w);
		for (int i = 1; i < 4; ++i) for (int k = 0; k < 16; ++k) CHECK(std::fabs(w[i][k] - worldTracks[i][f][k]) < 1e-3f);
	}
	// local tracks (not baked) are left alone
	s3d::Model n = m;
	CHECK(s3d::RebaseBakedWorldTracks(n) == 0);
	std::printf("uniel: cipher round trip, enciphered fbxex + dds load, world->local rebase ok\n");
}

// ASCII FBX stages (DFCI, the UNI[st]/[cl-r] arcade build): every bg\<dir>\bg.fbx loads, has geometry and a BgList entry, and
// its textures decode. Where a folder also has bg.fbx.bin (six UNI2 stages), both forms must agree on counts and bounds.
static void TestAscii(const std::string& bg, const char* label)
{
	namespace fs = std::filesystem;
	if (!fs::is_directory(fs::u8path(bg))) { std::printf("skip %s: %s missing\n", label, bg.c_str()); return; }
	s3d::BgList L;
	std::string err;
	const bool haveList = s3d::LoadBgListFile(bg + "/BgList.txt", L, err);
	int stages = 0, textures = 0, both = 0;
	for (auto& d : fs::directory_iterator(fs::u8path(bg))) {
		const fs::path txt = d.path() / "bg.fbx";
		if (!fs::exists(txt)) continue;
		s3d::Model m;
		const bool ok = s3d::LoadFbxAsciiFile(txt.u8string(), m, err);
		const bool hasBin = fs::exists(d.path() / "bg.fbx.bin");
		if (!ok && hasBin && err.find("6.1") != std::string::npos) {   // UNI2 bg021 ships an FBX 7.7 source next to its .bin
			std::printf("  note %s: %s (the game reads the .bin)\n", d.path().filename().u8string().c_str(), err.c_str());
			continue;
		}
		if (!ok) std::fprintf(stderr, "%s: %s\n", txt.u8string().c_str(), err.c_str());
		CHECK(ok);
		if (!ok) continue;
		const s3d::Stats st = s3d::ComputeStats(m);
		CHECK(st.meshes > 0 && st.triangles > 0);
		if (haveList) CHECK(L.byDataFile(d.path().filename().u8string()) != nullptr);
		for (const std::string& t : m.textures) {
			fs::path tp = d.path() / fs::u8path(t);
			if (!fs::exists(tp)) tp.replace_extension(".dds");
			if (t.empty() || !fs::exists(tp)) continue;
			s3d::Image im;
			const bool tok = s3d::LoadDdsFile(tp.u8string(), im, err);
			if (!tok) std::fprintf(stderr, "%s: %s\n", tp.u8string().c_str(), err.c_str());
			CHECK(tok);
			++textures;
		}
		const fs::path bin = d.path() / "bg.fbx.bin";
		if (fs::exists(bin)) {
			s3d::Model b;
			CHECK(s3d::LoadFbxExFile(bin.u8string(), b, err));
			const s3d::Stats sb = s3d::ComputeStats(b);
			if (sb.meshes != st.meshes || sb.vertices != st.vertices) {
				// a different authoring revision of the source (the game reads the .bin): reported, not compared
				std::printf("  note %s: .fbx is another revision (%u meshes / %u verts vs .bin %u / %u)\n", d.path().filename().u8string().c_str(),
					st.meshes, st.vertices, sb.meshes, sb.vertices);
				continue;
			}
			CHECK(sb.triangles == st.triangles);
			for (int k = 0; k < 3; k++) CHECK(std::fabs(sb.bmin[k] - st.bmin[k]) < 0.01f && std::fabs(sb.bmax[k] - st.bmax[k]) < 0.01f);
			++both;
		}
		++stages;
	}
	std::printf("%s: %d ASCII stages, %d textures, %d also as .bin (agree)\n", label, stages, textures, both);
	CHECK(stages > 0);
}

static void TestBlenderLink()
{
	std::string err;
	const std::string e = s3d::BlenderImportExpr("C:\\dev\\gamedata\\mbtl\\bg\\bg001\\bg.fbx.bin", err);
	CHECK(e == "import addon_utils,bpy; addon_utils.enable('MBTL_fbx_json', default_set=True, persistent=True); "
	           "bpy.ops.mbtl.hantei_open(filepath=r'C:/dev/gamedata/mbtl/bg/bg001/bg.fbx.bin')");
	CHECK(s3d::BlenderImportExpr("C:/it's/bg.fbx", err).empty() && !err.empty());
	CHECK(s3d::BlenderImportExpr("C:/a\"b/bg.fbx", err).empty());
	CHECK(s3d::BlenderImportExpr("", err).empty());
	const std::string cl = s3d::BlenderCommandLine("C:/Program Files/Blender Foundation/Blender 4.5/blender.exe", "C:/s/bg.fbx.bin", err);
	CHECK(cl.rfind("\"C:/Program Files/Blender Foundation/Blender 4.5/blender.exe\" --python-expr \"import addon_utils", 0) == 0);
	CHECK(cl.size() > 3 && cl.substr(cl.size() - 3) == "')\"");
	CHECK(s3d::BlenderCommandLine("", "C:/s/bg.fbx.bin", err).empty());
	const auto c = s3d::BlenderCandidates({"Blender 4.0", "Blender 4.10", "notes", "Blender 4.5", "Blender 3.6"});
	CHECK(c.size() == 4);
	CHECK(c.size() == 4 && c[0] == "C:/Program Files/Blender Foundation/Blender 4.10/blender.exe" && c[1].find("Blender 4.5/") != std::string::npos
		&& c[3].find("Blender 3.6/") != std::string::npos);
	// default + migration: the add-on targets Blender 5.2; a saved pre-5 path is not migrated
	CHECK(std::string(s3d::DefaultBlenderPath()) == "C:/apps/blender-5.2.2-windows-x64/blender.exe");
	CHECK(s3d::BlenderPathIsPre5("C:/Program Files/Blender Foundation/Blender 4.5/blender.exe"));
	CHECK(s3d::BlenderPathIsPre5("C:\\dev\\blender40\\blender-4.0.2-windows-x64\\blender.exe"));
	CHECK(!s3d::BlenderPathIsPre5("C:/apps/blender-5.2.2-windows-x64/blender.exe"));
	CHECK(!s3d::BlenderPathIsPre5("C:/Program Files/Blender Foundation/Blender 5.2/blender.exe"));
	CHECK(!s3d::BlenderPathIsPre5("D:/tools/blender.exe"));
	// portable C:/apps/blender-X.Y.Z builds join the sort: 5.2.2 > 5.2 > 4.5.1 > 4.5 (portable first on a tie)
	const auto c2 = s3d::BlenderCandidates({"Blender 4.5", "Blender 5.2", "Blender 4.0"},
		{"blender-5.2.2-windows-x64", "blender-4.5-windows-x64", "blender-notes", "blender-4.5.1-windows-x64"});
	CHECK(c2.size() == 6);
	CHECK(c2.size() == 6 && c2[0] == "C:/apps/blender-5.2.2-windows-x64/blender.exe"
		&& c2[1] == "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe"
		&& c2[2] == "C:/apps/blender-4.5.1-windows-x64/blender.exe"
		&& c2[3] == "C:/apps/blender-4.5-windows-x64/blender.exe"
		&& c2[4] == "C:/Program Files/Blender Foundation/Blender 4.5/blender.exe"
		&& c2[5] == "C:/Program Files/Blender Foundation/Blender 4.0/blender.exe");
	// "changed on disk": the add-on always writes bg.fbx.bin next to the loaded model
	const auto w1 = s3d::BlenderWatchPaths("C:\\g\\dfci\\bg\\bg01\\bg.fbx");
	CHECK(w1.size() == 2 && w1[0] == "C:/g/dfci/bg/bg01/bg.fbx.bin" && w1[1] == "C:/g/dfci/bg/bg01/bg.fbx");
	const auto w2 = s3d::BlenderWatchPaths("C:/g/uni2/bg/bg001/bg.fbx.bin");
	CHECK(w2.size() == 1 && w2[0] == "C:/g/uni2/bg/bg001/bg.fbx.bin");
	CHECK(s3d::BlenderWatchPaths("").empty());
	// stage folder scan: bg.fbx.bin or bg.fbx, no BgList needed
	namespace fs = std::filesystem;
	std::error_code ec;
	const fs::path tmp = fs::temp_directory_path(ec) / ("s3d_scan_" + std::to_string(reinterpret_cast<uintptr_t>(&ec)));
	fs::remove_all(tmp, ec);
	for (const char* d : {"bg003", "bg001", "bg002", "empty"}) fs::create_directories(tmp / d, ec);
	auto touch = [](const fs::path& p) { std::ofstream(p, std::ios::binary) << "x"; };
	touch(tmp / "bg001" / "bg.fbx.bin");   // fbxex / UNIEL
	touch(tmp / "bg002" / "bg.fbx");       // ASCII only (DFCI / UNIB)
	touch(tmp / "bg003" / "bg.fbx");
	touch(tmp / "bg003" / "bg.fbx.bin");
	touch(tmp / "empty" / "readme.txt");
	touch(tmp / "bg.fbx");                 // a file, not a folder
	const auto sf = s3d::ScanStageFolders(tmp.u8string());
	CHECK(sf.size() == 3 && sf[0] == "bg001" && sf[1] == "bg002" && sf[2] == "bg003");
	fs::remove_all(tmp, ec);
	CHECK(s3d::ScanStageFolders((tmp / "missing").u8string()).empty());
}


// MBAACC alignment (SCOPE.md section 4). Expected values come from the spec, not from s3d_align:
// - MBAACC 2D camera: screen x = 320 + zoom*(world - cam), floor row 432, cam clamp 67584 - 40960/zoom fixed (= 528 - 320/zoom px).
// - The source camera at zoom Z puts a fighter-plane point P (source px) at 640 + Z*(P - camX) of 1280; the 4:3 crop is x 160..1120.
// - SCOPE 4.2: unitPx = 360 * 2/3 * s. DFCI_ISSUES.md "Ini-only workaround" tables: FOV 45 -> fov 72.73, VP 0.55 -> horizon 377.3.
// - The ini must parse with pc-stage3d's applyStageIni (StageIni.cpp): every key known, every value strict decimals.
static bool Near(float a, float b, float eps) { return std::fabs(a - b) <= eps; }
static void TestAlign()
{
	// clamp
	CHECK(Near(s3d::AlignCamLimit(1.f), 208.f, 1e-3f));
	CHECK(Near(s3d::AlignCamLimit(0.8f), 128.f, 1e-3f));
	CHECK(Near(s3d::AlignCamLimit(1.25f), 272.f, 1e-3f));
	// camera mapping: MBAACC world points land where MBAACC's 2D camera puts them
	for (float s : {1.f, 0.9f, 0.75f})
		for (float z : {1.f, 0.85f, 1.2f})
			for (float cam : {0.f, -100.f, 120.f}) {
				s3d::AlignInput in;
				in.mbCamX = cam; in.mbZoom = z; in.fighterScale = s; in.centerX = 0.4f;
				const s3d::AlignSourceCam c = s3d::AlignSourceCamera(in);
				CHECK(c.camY == 0.f);
				for (float wx : {-528.f, -200.f, 0.f, 37.f, 528.f}) {
					// MBAACC world px -> source px at the plane: centre slice + 1.5/s source px per MBAACC px
					const float P = 0.4f * 640.f + wx * 1.5f / s;
					const float src = 640.f + c.zoom * (P - c.camX);
					const float mb = (src - 160.f) / 1.5f;
					CHECK(Near(mb, 320.f + z * (wx - cam), 0.02f));
				}
				// a 355 px source character (720p) at this camera is 355 * s * z * 2/3 MBAACC px tall (spec fighter_px 237 * s at zoom 1)
				CHECK(Near(355.f * c.zoom / 1.5f, 236.667f * s * z, 0.01f));
			}
	{   // the clamp applies before mapping: cam 300 at zoom 1 is cam 208
		s3d::AlignInput a, b; a.mbCamX = 300.f; b.mbCamX = 208.f;
		CHECK(Near(s3d::AlignSourceCamera(a).camX, s3d::AlignSourceCamera(b).camX, 1e-3f));
	}
	// ini keys against the spec / DFCI_ISSUES tables
	s3d::AlignInput in;
	in.fighterScale = 0.9f; in.centerX = 0.5f; in.fovDeg = 45.f; in.vanishingPoint = 0.55f; in.floorPx = 432.f;
	in.scale[0] = in.scale[1] = in.scale[2] = 2.f; in.position[0] = 0.1125f; in.position[1] = -0.05f;
	in.stageW = 4096.f; in.replaces = 16; in.package = "C:/g/uni2/bg/bg001"; in.name = "bg001";
	CHECK(Near(s3d::AlignUnitsToPx(in), 216.f, 1e-3f));          // 360 * 2/3 * 0.9
	CHECK(Near(s3d::AlignIniFov(in), 72.73f, 0.01f));
	CHECK(Near(s3d::AlignIniHorizonPx(in), 377.3f, 0.05f));
	in.vanishingPoint = 0.3f; CHECK(Near(s3d::AlignIniHorizonPx(in), 317.3f, 0.05f));
	in.fovDeg = 35.f; CHECK(Near(s3d::AlignIniFov(in), 58.54f, 0.01f));
	in.fovDeg = 45.f; in.vanishingPoint = 0.55f;
	CHECK(Near(s3d::AlignIniCenterX(in), 0.5f / 0.5625f, 1e-4f));
	// parse like pc-stage3d applyStageIni
	static const char* kKnown[] = {"replaces", "package", "scale", "pos", "fit_mode", "center_x", "pan_boost", "pan_depth", "stage_w",
		"host_world_w", "yaw", "cam_pitch", "ground_auto", "ground_y", "fighter_px", "uni_char_px", "ref_fov", "debug_overlay",
		"depth_offset", "cam_height", "negate_z", "mirror_x", "flip_v", "fov", "world_scale", "units_to_px", "floor_px", "horizon_px",
		"cam_offset", "lift", "gain", "contrast", "gray", "vcol_gain", "fog", "fog_start", "fog_end", "fog_color", "torch", "torch_color",
		"torch_pos", "torch_falloff", "front_pass", "front_z"};
	auto trim = [](std::string x) { while (!x.empty() && std::isspace((unsigned char)x.back())) x.pop_back(); size_t a = 0; while (a < x.size() && std::isspace((unsigned char)x[a])) ++a; return x.substr(a); };
	const std::string ini = s3d::FormatAlignIni(in);
	std::vector<std::pair<std::string, std::string>> kv;
	size_t pos = 0;
	while (pos < ini.size()) {
		size_t nl = ini.find('\n', pos); if (nl == std::string::npos) nl = ini.size();
		std::string line = ini.substr(pos, nl - pos); pos = nl + 1;
		const size_t cm = line.find_first_of(";#"); if (cm != std::string::npos) line.erase(cm);
		line = trim(line); if (line.empty()) continue;
		const size_t eq = line.find('='); CHECK(eq != std::string::npos); if (eq == std::string::npos) continue;
		const std::string k = trim(line.substr(0, eq)), v = trim(line.substr(eq + 1));
		bool known = false; for (const char* n : kKnown) known |= k == n;
		CHECK(known);
		if (known && k != "package" && k != "fit_mode") {   // strict decimals, comma separated
			const char* c = v.c_str(); bool ok = true;
			while (*c) { char* e = nullptr; std::strtof(c, &e); if (e == c) { ok = false; break; } c = e; while (*c == ',' || std::isspace((unsigned char)*c)) ++c; }
			CHECK(ok);
		}
		kv.emplace_back(k, v);
	}
	auto val = [&](const char* k) { for (auto& p : kv) if (p.first == k) return p.second; return std::string("<missing>"); };
	auto num = [&](const char* k) { return std::strtof(val(k).c_str(), nullptr); };
	CHECK(val("fit_mode") == "crop");
	CHECK(val("package") == "C:/g/uni2/bg/bg001");
	CHECK(val("replaces") == "16");
	CHECK(Near(num("units_to_px"), 216.f, 1e-3f));
	CHECK(Near(num("fighter_px"), 237.f * 0.9f, 1e-2f));
	CHECK(Near(num("uni_char_px"), 355.f, 0.f));
	CHECK(Near(num("floor_px"), 432.f, 1e-3f));
	CHECK(Near(num("horizon_px"), 377.3f, 0.05f));
	CHECK(Near(num("fov"), 72.73f, 0.01f));
	CHECK(Near(num("center_x"), 0.8889f, 1e-3f));
	CHECK(Near(num("stage_w"), 4096.f, 0.f) && Near(num("host_world_w"), 1056.f, 0.f));
	CHECK(val("ground_auto") == "0" && val("pan_boost") == "0" && val("world_scale") == "1");
	CHECK(Near(num("scale"), 2.f, 1e-4f));
	CHECK(val("pos").rfind("0.2,", 0) == 0);   // Position.x / 0.5625 (DFCI_ISSUES.md)
	in.replaces = -1; in.yBias = 3.f;
	const std::string ini2 = s3d::FormatAlignIni(in);
	CHECK(ini2.find("replaces") == std::string::npos);
	CHECK(ini2.find("floor_px = 435\n") != std::string::npos);
	std::printf("align: ok\n");
}

int main(int argc, char** argv)
{
	TestSynthetic();
	TestDds();
	TestBgList();
	TestUniel();
	TestBlenderLink();
	TestAlign();
	TestShipped(argc > 1 ? argv[1] : "C:/dev/hantei-chan/gamedata/uni2/bg");
	TestAscii("C:/dev/hantei-chan/gamedata/uni2/bg", "uni2 (both forms)");
	TestAscii("C:/games/unib/bg", "unib arcade");
	TestAscii("C:/dev/transmission/dfci+transmission/bg", "dfci");
	std::printf(g_fail ? "stage3d_test: %d failures\n" : "stage3d_test: ok\n", g_fail);
	return g_fail ? 1 : 0;
}
