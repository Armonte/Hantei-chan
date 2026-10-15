#include "s3d_model.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace s3d {
namespace {

struct Cursor {
	const uint8_t* p;
	size_t size;
	size_t pos = 0;
	bool ok = true;
	bool need(uint64_t n) { if (!ok || pos > size || n > size - pos) ok = false; return ok; }
	uint32_t u32() { uint32_t v = 0; if (need(4)) { memcpy(&v, p + pos, 4); pos += 4; } return v; }
	void bytes(void* d, size_t n) { if (need(n)) { memcpy(d, p + pos, n); pos += n; } }
};

std::string FixedString(const uint8_t* b, size_t n)
{
	size_t len = 0;
	while (len < n && b[len]) ++len;
	return std::string(reinterpret_cast<const char*>(b), len);
}

struct Block { size_t body = 0, end = 0; uint32_t count = 0; };

bool ReadBlock(Cursor& c, Block& b, std::string& err)
{
	const uint32_t sz = c.u32();
	b.count = c.u32();
	if (!c.ok || sz < 8 || sz - 8 > c.size - c.pos) { err = "block size out of range"; return false; }
	b.body = c.pos;
	b.end = c.pos + (sz - 8);
	c.pos = b.end;
	return true;
}

bool ReadMesh(Cursor& c, Node& n, std::string& err)
{
	n.additive = c.u32() != 0;
	n.flag2 = c.u32();
	c.bytes(n.world.data(), 64);
	const uint32_t nv = c.u32();
	if (!c.need(uint64_t(nv) * sizeof(Vertex))) { err = "vertex count out of range"; return false; }
	n.verts.resize(nv);
	if (nv) c.bytes(n.verts.data(), size_t(nv) * sizeof(Vertex));
	const uint32_t ns = c.u32();
	if (!c.ok || ns > (c.size - c.pos) / 8 + 1) { err = "submesh count out of range"; return false; }
	n.subs.resize(ns);
	for (SubMesh& s : n.subs) {
		s.material = c.u32();
		const uint32_t ni = c.u32();
		if (!c.need(uint64_t(ni) * 4)) { err = "index count out of range"; return false; }
		s.indices.resize(ni);
		if (ni) c.bytes(s.indices.data(), size_t(ni) * 4);
		for (uint32_t i : s.indices) if (i >= nv) { err = "index past vertex count"; return false; }
	}
	return c.ok;
}

}  // namespace

bool ParseFbxEx(const uint8_t* data, size_t size, Model& out, std::string& err)
{
	out = Model{};
	static const uint8_t kMagic[8] = {'f', 'b', 'x', 'e', 'x', 0, 0, 0};
	if (!data || size < 16 || memcmp(data, kMagic, 8) != 0) { err = "not an fbxex file"; return false; }
	Cursor c{data, size};
	c.pos = 8;
	if (c.u32() != 0 || c.u32() != 0) { err = "unexpected header words"; return false; }
	Block tex, mat, nodes, anim;
	if (!ReadBlock(c, tex, err) || !ReadBlock(c, mat, err) || !ReadBlock(c, nodes, err) || !ReadBlock(c, anim, err)) return false;

	if (uint64_t(tex.count) * 128 != tex.end - tex.body) { err = "texture block size mismatch"; return false; }
	for (uint32_t i = 0; i < tex.count; ++i) out.textures.push_back(FixedString(data + tex.body + size_t(i) * 128, 128));

	if (uint64_t(mat.count) * 204 != mat.end - mat.body) { err = "material block size mismatch"; return false; }
	out.materials.resize(mat.count);
	for (uint32_t i = 0; i < mat.count; ++i) {
		const uint8_t* p = data + mat.body + size_t(i) * 204;
		Material& m = out.materials[i];
		m.name = FixedString(p, 128);
		uint32_t tx;
		memcpy(&tx, p + 132, 4);
		m.texture = tx < out.textures.size() ? int32_t(tx) : -1;
		memcpy(m.v, p + 136, sizeof(m.v));
	}

	c.pos = nodes.body;
	out.nodes.resize(nodes.count);
	for (uint32_t i = 0; i < nodes.count; ++i) {
		const size_t start = c.pos;
		const uint32_t sz = c.u32();
		Node& n = out.nodes[i];
		n.type = c.u32();
		n.firstChild = int32_t(c.u32());
		n.nextSibling = int32_t(c.u32());
		if (!c.ok || sz < 16 || sz > nodes.end - start) { err = "node size out of range"; return false; }
		if (n.type == 1 && !ReadMesh(c, n, err)) return false;
		if (c.pos > start + sz) { err = "node payload overruns its size"; return false; }
		c.pos = start + sz;
	}
	for (uint32_t i = 0; i < nodes.count; ++i) {
		uint32_t guard = 0;
		for (int32_t ch = out.nodes[i].firstChild; ch >= 0; ch = out.nodes[ch].nextSibling) {
			if (uint32_t(ch) >= nodes.count || ++guard > nodes.count) { err = "bad child link"; return false; }
			out.nodes[ch].parent = int32_t(i);
		}
	}

	c.pos = anim.body;
	if (anim.count != nodes.count) { err = "anime count != node count"; return false; }
	out.tracks.resize(anim.count);
	for (auto& t : out.tracks) {
		const uint32_t frames = c.u32();
		if (!c.ok || frames == 0 || !c.need(uint64_t(frames) * 64)) { err = "anime frame count out of range"; return false; }
		t.resize(frames);
		c.bytes(t.data(), size_t(frames) * 64);
	}
	if (!c.ok) { err = "truncated"; return false; }
	for (const Node& n : out.nodes)
		for (const SubMesh& s : n.subs)
			if (s.material >= out.materials.size()) { err = "submesh material out of range"; return false; }
	return true;
}

bool LoadFbxExFile(const std::string& utf8Path, Model& out, std::string& err)
{
	std::ifstream f(std::filesystem::u8path(utf8Path), std::ios::binary);
	if (!f) { err = "cannot open " + utf8Path; return false; }
	std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	return ParseFbxEx(buf.data(), buf.size(), out, err);
}

Mat4 MatIdentity()
{
	Mat4 m{};
	m[0] = m[5] = m[10] = m[15] = 1.f;
	return m;
}

Mat4 MatMul(const Mat4& a, const Mat4& b)
{
	Mat4 o{};
	for (int r = 0; r < 4; ++r)
		for (int col = 0; col < 4; ++col) {
			float s = 0;
			for (int k = 0; k < 4; ++k) s += a[r * 4 + k] * b[k * 4 + col];
			o[r * 4 + col] = s;
		}
	return o;
}

void EvaluateWorld(const Model& m, uint64_t frame30, std::vector<Mat4>& out)
{
	out.assign(m.nodes.size(), MatIdentity());
	std::vector<uint8_t> done(m.nodes.size(), 0);
	std::vector<int32_t> chain;
	for (size_t i = 0; i < m.nodes.size(); ++i) {   // parents may follow children in the table: resolve up the chain
		chain.clear();
		for (int32_t n = int32_t(i); n >= 0 && !done[n] && chain.size() <= m.nodes.size(); n = m.nodes[n].parent) chain.push_back(n);
		for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
			const int32_t n = *it;
			const auto& tr = m.tracks[n];
			const Mat4& local = tr[tr.size() > 1 ? frame30 % tr.size() : 0];
			const int32_t p = m.nodes[n].parent;
			out[n] = p >= 0 ? MatMul(local, out[p]) : local;
			done[n] = 1;
		}
	}
}

Stats ComputeStats(const Model& m)
{
	Stats st;
	st.nodes = uint32_t(m.nodes.size());
	for (const auto& t : m.tracks) st.animated += t.size() > 1;
	std::vector<Mat4> world;
	EvaluateWorld(m, 0, world);
	bool first = true;
	for (size_t i = 0; i < m.nodes.size(); ++i) {
		const Node& n = m.nodes[i];
		if (n.type != 1) continue;
		++st.meshes;
		st.additive += n.additive;
		for (const SubMesh& s : n.subs) st.triangles += uint32_t(s.indices.size() / 3);
		const Mat4& w = world[i];
		for (const Vertex& v : n.verts) {
			++st.vertices;
			for (int k = 0; k < 3; ++k) {
				const float p = v.pos[0] * w[k] + v.pos[1] * w[4 + k] + v.pos[2] * w[8 + k] + w[12 + k];
				if (first || p < st.bmin[k]) st.bmin[k] = p;
				if (first || p > st.bmax[k]) st.bmax[k] = p;
			}
			first = false;
		}
	}
	return st;
}

}  // namespace s3d
