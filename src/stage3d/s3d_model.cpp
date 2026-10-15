#include "s3d_model.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <cmath>
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
	if (buf.size() >= 5 && memcmp(buf.data(), "fbxex", 5) != 0) {
		std::vector<uint8_t> plain = buf;
		UnielCrypt(plain.data(), plain.size());
		if (plain.size() >= 5 && memcmp(plain.data(), "fbxex", 5) == 0) {   // UNIEL
			if (!ParseFbxEx(plain.data(), plain.size(), out, err)) return false;
			RebaseBakedWorldTracks(out);
			return true;
		}
	}
	return ParseFbxEx(buf.data(), buf.size(), out, err);
}

void UnielCrypt(uint8_t* data, size_t size)
{
	static const uint8_t kKey[16] = {0xd3, 0x04, 0xf5, 0x27, 0xf3, 0x2e, 0x29, 0x9c, 0x96, 0xf6, 0xfe, 0x4f, 0x47, 0xdd, 0xf4, 0xa9};
	uint8_t st[256];
	for (int i = 0; i < 256; ++i) st[i] = uint8_t(i);
	uint8_t j = 0;
	for (int i = 0; i < 256; ++i) {
		j = uint8_t(j + st[i] + kKey[i % 16]);
		std::swap(st[i], st[j]);
	}
	uint8_t a = 0, b = 0;
	for (size_t n = 0; n < size; ++n) {
		a = uint8_t(a + 1);
		b = uint8_t(b + st[a]);
		std::swap(st[a], st[b]);
		data[n] ^= st[uint8_t(st[a] + st[b])];
	}
}

namespace {
bool Invert(const Mat4& m, Mat4& out)
{
	// general 4x4 inverse (cofactors), row-major
	const float* a = m.data();
	float inv[16];
	inv[0] = a[5]*a[10]*a[15] - a[5]*a[11]*a[14] - a[9]*a[6]*a[15] + a[9]*a[7]*a[14] + a[13]*a[6]*a[11] - a[13]*a[7]*a[10];
	inv[4] = -a[4]*a[10]*a[15] + a[4]*a[11]*a[14] + a[8]*a[6]*a[15] - a[8]*a[7]*a[14] - a[12]*a[6]*a[11] + a[12]*a[7]*a[10];
	inv[8] = a[4]*a[9]*a[15] - a[4]*a[11]*a[13] - a[8]*a[5]*a[15] + a[8]*a[7]*a[13] + a[12]*a[5]*a[11] - a[12]*a[7]*a[9];
	inv[12] = -a[4]*a[9]*a[14] + a[4]*a[10]*a[13] + a[8]*a[5]*a[14] - a[8]*a[6]*a[13] - a[12]*a[5]*a[10] + a[12]*a[6]*a[9];
	inv[1] = -a[1]*a[10]*a[15] + a[1]*a[11]*a[14] + a[9]*a[2]*a[15] - a[9]*a[3]*a[14] - a[13]*a[2]*a[11] + a[13]*a[3]*a[10];
	inv[5] = a[0]*a[10]*a[15] - a[0]*a[11]*a[14] - a[8]*a[2]*a[15] + a[8]*a[3]*a[14] + a[12]*a[2]*a[11] - a[12]*a[3]*a[10];
	inv[9] = -a[0]*a[9]*a[15] + a[0]*a[11]*a[13] + a[8]*a[1]*a[15] - a[8]*a[3]*a[13] - a[12]*a[1]*a[11] + a[12]*a[3]*a[9];
	inv[13] = a[0]*a[9]*a[14] - a[0]*a[10]*a[13] - a[8]*a[1]*a[14] + a[8]*a[2]*a[13] + a[12]*a[1]*a[10] - a[12]*a[2]*a[9];
	inv[2] = a[1]*a[6]*a[15] - a[1]*a[7]*a[14] - a[5]*a[2]*a[15] + a[5]*a[3]*a[14] + a[13]*a[2]*a[7] - a[13]*a[3]*a[6];
	inv[6] = -a[0]*a[6]*a[15] + a[0]*a[7]*a[14] + a[4]*a[2]*a[15] - a[4]*a[3]*a[14] - a[12]*a[2]*a[7] + a[12]*a[3]*a[6];
	inv[10] = a[0]*a[5]*a[15] - a[0]*a[7]*a[13] - a[4]*a[1]*a[15] + a[4]*a[3]*a[13] + a[12]*a[1]*a[7] - a[12]*a[3]*a[5];
	inv[14] = -a[0]*a[5]*a[14] + a[0]*a[6]*a[13] + a[4]*a[1]*a[14] - a[4]*a[2]*a[13] - a[12]*a[1]*a[6] + a[12]*a[2]*a[5];
	inv[3] = -a[1]*a[6]*a[11] + a[1]*a[7]*a[10] + a[5]*a[2]*a[11] - a[5]*a[3]*a[10] - a[9]*a[2]*a[7] + a[9]*a[3]*a[6];
	inv[7] = a[0]*a[6]*a[11] - a[0]*a[7]*a[10] - a[4]*a[2]*a[11] + a[4]*a[3]*a[10] + a[8]*a[2]*a[7] - a[8]*a[3]*a[6];
	inv[11] = -a[0]*a[5]*a[11] + a[0]*a[7]*a[9] + a[4]*a[1]*a[11] - a[4]*a[3]*a[9] - a[8]*a[1]*a[7] + a[8]*a[3]*a[5];
	inv[15] = a[0]*a[5]*a[10] - a[0]*a[6]*a[9] - a[4]*a[1]*a[10] + a[4]*a[2]*a[9] + a[8]*a[1]*a[6] - a[8]*a[2]*a[5];
	const float det = a[0]*inv[0] + a[1]*inv[4] + a[2]*inv[8] + a[3]*inv[12];
	if (std::fabs(det) < 1e-12f) return false;
	for (int i = 0; i < 16; ++i) out[i] = inv[i] / det;
	return true;
}
bool Same(const Mat4& a, const Mat4& b)
{
	for (int i = 0; i < 16; ++i) if (std::fabs(a[i] - b[i]) > 1e-4f * std::max(1.f, std::fabs(b[i]))) return false;
	return true;
}
}  // namespace

int RebaseBakedWorldTracks(Model& m)
{
	if (m.tracks.size() != m.nodes.size()) return 0;
	int meshes = 0;
	for (size_t i = 0; i < m.nodes.size(); ++i) {
		if (m.nodes[i].type != 1) continue;
		++meshes;
		if (m.tracks[i].empty() || !Same(m.tracks[i][0], m.nodes[i].world)) return 0;   // not baked world matrices
	}
	if (!meshes) return 0;
	const std::vector<std::vector<Mat4>> world = m.tracks;   // parents are read from the original (world) tracks
	int changed = 0;
	for (size_t i = 0; i < m.nodes.size(); ++i) {
		const int p = m.nodes[i].parent;
		if (p <= 0 || world[p].empty()) continue;
		bool moved = false;
		for (size_t f = 0; f < world[i].size(); ++f) {
			Mat4 inv;
			if (!Invert(world[p][std::min(f, world[p].size() - 1)], inv)) continue;
			const Mat4 local = MatMul(world[i][f], inv);   // world = local * parentWorld (row vectors)
			if (Same(local, world[i][f])) continue;
			m.tracks[i][f] = local;
			moved = true;
		}
		changed += moved;
	}
	return changed;
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
