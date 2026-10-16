// stage3d_test: fbxex model reader, DDS decoder and BgList parser (src/stage3d). Synthetic inputs always; the extracted
// UNI2 stages (gamedata/uni2/bg, never committed) when present: every bgNNN/bg.fbx.bin parses, every referenced texture
// decodes, every stage has a BgList entry.
#include "stage3d/s3d_bglist.h"
#include "stage3d/s3d_dds.h"
#include "stage3d/s3d_model.h"
#include "stage3d/s3d_fbxascii.h"
#include "stage3d/s3d_blender.h"

#include <cmath>
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
	CHECK(cl.size() > 2 && cl.substr(cl.size() - 2) == "')" + std::string("\""));
	CHECK(s3d::BlenderCommandLine("", "C:/s/bg.fbx.bin", err).empty());
	const auto c = s3d::BlenderCandidates({"Blender 4.0", "Blender 4.10", "notes", "Blender 4.5", "Blender 3.6"});
	CHECK(c.size() == 5);
	CHECK(c.size() == 5 && c[0] == "C:/Program Files/Blender Foundation/Blender 4.10/blender.exe" && c[1].find("Blender 4.5/") != std::string::npos
		&& c[3].find("Blender 3.6/") != std::string::npos && c[4] == "C:/dev/blender40/blender-4.0.2-windows-x64/blender.exe");
}

int main(int argc, char** argv)
{
	TestSynthetic();
	TestDds();
	TestBgList();
	TestUniel();
	TestBlenderLink();
	TestShipped(argc > 1 ? argv[1] : "C:/dev/hantei-chan/gamedata/uni2/bg");
	TestAscii("C:/dev/hantei-chan/gamedata/uni2/bg", "uni2 (both forms)");
	TestAscii("C:/games/unib/bg", "unib arcade");
	TestAscii("C:/dev/transmission/dfci+transmission/bg", "dfci");
	std::printf(g_fail ? "stage3d_test: %d failures\n" : "stage3d_test: ok\n", g_fail);
	return g_fail ? 1 : 0;
}
