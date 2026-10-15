// stage3d_test: fbxex model reader, DDS decoder and BgList parser (src/stage3d). Synthetic inputs always; the extracted
// UNI2 stages (gamedata/uni2/bg, never committed) when present: every bgNNN/bg.fbx.bin parses, every referenced texture
// decodes, every stage has a BgList entry.
#include "stage3d/s3d_bglist.h"
#include "stage3d/s3d_dds.h"
#include "stage3d/s3d_model.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
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

int main(int argc, char** argv)
{
	TestSynthetic();
	TestDds();
	TestBgList();
	TestShipped(argc > 1 ? argv[1] : "C:/dev/hantei-chan/gamedata/uni2/bg");
	std::printf(g_fail ? "stage3d_test: %d failures\n" : "stage3d_test: ok\n", g_fail);
	return g_fail ? 1 : 0;
}
