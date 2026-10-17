#include "qoh_dat.h"
#include <cstring>

namespace han2 { namespace qoh {

static uint32_t U32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

bool ComputeLayout98(const uint8_t *p, size_t size, Layout &L, std::string *err)
{
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	if (size < 28 + 1024) return fail("too short for a QoH98 character");
	L = Layout(); L.version = V98; L.imageRecSize = 8; L.frameSize = 52;
	L.images = U32(p + 4); L.tiles = U32(p + 8); L.actions = U32(p + 12); L.frames = U32(p + 16); L.boxes = U32(p + 20); L.attacks = U32(p + 24);
	if (L.images > 100000 || L.tiles > 1000000 || L.actions > 100000 || L.frames > 1000000 || L.boxes > 1000000 || L.attacks > 1000000) return fail("implausible counts");
	size_t q = 28;
	L.timPaths = q; q += 260ull * L.images;
	L.imageRecs = q; q += 8ull * L.images;
	L.tileOff = q; q += 260ull * L.tiles;
	L.actionOff = q; q += 24ull * L.actions;
	L.frameOff = q; q += 52ull * L.frames;
	L.paletteOff = q; q += 1024;
	L.boxOff = q; q += 8ull * L.boxes;
	L.attackOff = q; q += 24ull * L.attacks;
	L.end = q;
	if (q != size) return fail("section sizes do not add up to the file size");
	return true;
}

bool ComputeLayout99(const uint8_t *p, size_t size, Layout &L, std::string *err)
{
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	if (size < 44 + 1024) return fail("too short for a QoH99 character");
	L = Layout(); L.version = V99; L.imageRecSize = 12; L.frameSize = 64;
	L.images = U32(p + 0x14); L.tiles = U32(p + 0x18); L.actions = U32(p + 0x1C); L.frames = U32(p + 0x20); L.boxes = U32(p + 0x24); L.attacks = U32(p + 0x28);
	if (L.images > 100000 || L.tiles > 1000000 || L.actions > 100000 || L.frames > 1000000 || L.boxes > 1000000 || L.attacks > 1000000) return fail("implausible counts");
	size_t q = 44;
	L.boxOff = q; q += 8ull * L.boxes;
	L.actionOff = q; q += 24ull * L.actions;
	L.imageRecs = q; q += 12ull * L.images;
	L.paletteOff = q; q += 1024;
	L.tileOff = q; q += 260ull * L.tiles;
	L.frameOff = q; q += 64ull * L.frames;
	L.attackOff = q; q += 24ull * L.attacks;
	L.end = q;
	if (q != size) return fail("section sizes do not add up to the file size");
	return true;
}

bool LooksLike98(const uint8_t *p, size_t size)
{
	Layout L;
	return size >= 28 + 1024 && U32(p) == 5 && ComputeLayout98(p, size, L) && L.actions == 256;
}

// ---- '99 cipher ---------------------------------------------------------------------------------------------------------------------------------
static const uint8_t kPrim[17] = { 0x92,0x4f,0x89,0xba,0x8d,0xf7,0x20,0x90,0xbc,0x91,0xba,0x82,0xbf,0x82,0xc8,0x82,0xdd };   // g_primary_xor_key ^ 0x7E ^ 0x53

std::string StemOfPath(const std::string &path)
{
	size_t s = path.find_last_of("/\\"); std::string b = s == std::string::npos ? path : path.substr(s + 1);
	size_t d = b.find_last_of('.'); if (d != std::string::npos) b = b.substr(0, d);
	for (auto &c : b) if (c >= 'a' && c <= 'z') c -= 32;
	return b;
}

static uint8_t Mask(size_t i) { return (uint8_t)((~(8 * i)) + (i >> 3)); }
static void DecRegion(uint8_t *b, size_t n, const std::string &stem) { for (size_t i = 0; i < n; i++) b[i] = (uint8_t)(((b[i] ^ (uint8_t)stem[i % stem.size()] ^ Mask(i)) - kPrim[i % 17] - 109) & 0xFF); }
static void EncRegion(uint8_t *b, size_t n, const std::string &stem) { for (size_t i = 0; i < n; i++) b[i] = (uint8_t)((((b[i] + kPrim[i % 17] + 109) & 0xFF) ^ (uint8_t)stem[i % stem.size()] ^ Mask(i))); }

bool Decrypt99(std::vector<uint8_t> &f, const std::string &stem, std::string *err)
{
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	if (stem.empty() || f.size() < 44 + 1024) return fail("too short");
	uint8_t nb[16]; for (int i = 0; i < 16; i++) nb[i] = (uint8_t)(f[i] ^ kPrim[i % 17]);
	for (int i = 0; i < 16; i++) if (nb[i] != (uint8_t)stem[i % stem.size()]) return fail("the name block does not match the file name");
	std::vector<uint8_t> w = f;
	DecRegion(&w[0x10], 4, stem); DecRegion(&w[0x14], 24, stem);
	Layout L; std::string e;
	// sizes come from the decoded counts: temporarily lay the regions out with the same arithmetic as ComputeLayout99
	struct R { size_t off, n; } regs[7];
	const uint32_t c0 = U32(&w[0x14]), c1 = U32(&w[0x18]), c2 = U32(&w[0x1C]), c3 = U32(&w[0x20]), c4 = U32(&w[0x24]), c5 = U32(&w[0x28]);
	if (c0 > 100000 || c1 > 1000000 || c2 > 100000 || c3 > 1000000 || c4 > 1000000 || c5 > 1000000) return fail("implausible counts");
	size_t q = 44; const size_t sz[7] = { 8ull * c4, 24ull * c2, 12ull * c0, 1024, 260ull * c1, 64ull * c3, 24ull * c5 };
	for (int i = 0; i < 7; i++) { regs[i] = { q, sz[i] }; q += sz[i]; }
	if (q != f.size()) return fail("section sizes do not add up to the file size");
	for (auto &r : regs) DecRegion(&w[r.off], r.n, stem);
	f = w;
	(void)L; (void)e;
	return true;
}

void Encrypt99(std::vector<uint8_t> &f, const std::string &stem)
{
	uint8_t nb[16]; for (int i = 0; i < 16; i++) nb[i] = (uint8_t)((uint8_t)stem[i % stem.size()] ^ kPrim[i % 17]);
	memcpy(f.data(), nb, 16);
	const uint32_t c0 = U32(&f[0x14]), c1 = U32(&f[0x18]), c2 = U32(&f[0x1C]), c3 = U32(&f[0x20]), c4 = U32(&f[0x24]), c5 = U32(&f[0x28]);
	size_t q = 44; const size_t sz[7] = { 8ull * c4, 24ull * c2, 12ull * c0, 1024, 260ull * c1, 64ull * c3, 24ull * c5 };
	EncRegion(&f[0x10], 4, stem); EncRegion(&f[0x14], 24, stem);
	for (int i = 0; i < 7; i++) { EncRegion(&f[q], sz[i], stem); q += sz[i]; }
}

}} // namespace han2::qoh
