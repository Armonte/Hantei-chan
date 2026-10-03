#include "misc_formats.h"
#include <cstring>

namespace han2 {

static uint16_t R16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static uint32_t R32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void W16(std::vector<uint8_t> &o, uint16_t v) { const uint8_t *b = (const uint8_t *)&v; o.insert(o.end(), b, b + 2); }
static void W32(std::vector<uint8_t> &o, uint32_t v) { const uint8_t *b = (const uint8_t *)&v; o.insert(o.end(), b, b + 4); }

// ---------------------------------------------------------------- RIFF
bool ParseRiff(const uint8_t *p, size_t n, Riff &r, std::string *err)
{
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	r = Riff();
	if (n < 12 || memcmp(p, "RIFF", 4) != 0) return fail("no RIFF header");
	r.riffSize = R32(p + 4); memcpy(r.form, p + 8, 4);
	size_t q = 12;
	const size_t end = std::min<size_t>(n, (size_t)r.riffSize + 8);
	while (q + 8 <= end) {
		RiffChunk c; memcpy(c.id, p + q, 4); c.declaredSize = R32(p + q + 4);
		if ((uint64_t)q + 8 + c.declaredSize > end) break;   // truncated last chunk: kept in tail
		c.data.assign(p + q + 8, p + q + 8 + c.declaredSize);
		q += 8 + c.declaredSize;
		if ((c.declaredSize & 1) && q < end) { c.padded = true; q++; }   // pad byte is stored as a zero in the shipped files; verified by round trip
		if (memcmp(c.id, "fmt ", 4) == 0 && c.data.size() >= 16) { r.fmtTag = R16(c.data.data()); r.channels = R16(c.data.data() + 2); r.sampleRate = R32(c.data.data() + 4); r.bits = R16(c.data.data() + 14); }
		r.chunks.push_back(std::move(c));
	}
	r.tail.assign(p + q, p + n);
	return true;
}

void SerializeRiff(const Riff &r, std::vector<uint8_t> &o)
{
	o.assign({ 'R', 'I', 'F', 'F' }); W32(o, r.riffSize); o.insert(o.end(), r.form, r.form + 4);
	for (auto &c : r.chunks) { o.insert(o.end(), c.id, c.id + 4); W32(o, c.declaredSize); o.insert(o.end(), c.data.begin(), c.data.end()); if (c.padded) o.push_back(0); }
	o.insert(o.end(), r.tail.begin(), r.tail.end());
}

// ---------------------------------------------------------------- MPEG audio
bool ValidateMpeg(const uint8_t *p, size_t n, MpegInfo &out, std::string *err)
{
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	out = MpegInfo();
	size_t q = 0;
	if (n >= 10 && memcmp(p, "ID3", 3) == 0) { size_t sz = ((p[6] & 0x7F) << 21) | ((p[7] & 0x7F) << 14) | ((p[8] & 0x7F) << 7) | (p[9] & 0x7F); q = 10 + sz; out.id3v2 = q; }
	static const int br1[3][16] = {
		{ 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0 },
		{ 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0 },
		{ 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 } };
	static const int br2[2][16] = {   // MPEG 2 / 2.5: [0] layer 1, [1] layers 2 and 3
		{ 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0 },
		{ 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 } };
	static const int sr[3][3] = { { 44100, 48000, 32000 }, { 22050, 24000, 16000 }, { 11025, 12000, 8000 } };
	while (q + 4 <= n) {
		if (n - q == 128 && memcmp(p + q, "TAG", 3) == 0) { out.trailer = 128; return true; }
		uint32_t h = (p[q] << 24) | (p[q + 1] << 16) | (p[q + 2] << 8) | p[q + 3];
		if ((h & 0xFFE00000u) != 0xFFE00000u) { if (out.frames == 0) return fail("no MPEG sync"); out.trailer = n - q; return true; }
		int ver = (h >> 19) & 3, layer = (h >> 17) & 3, bri = (h >> 12) & 15, sri = (h >> 10) & 3, pad = (h >> 9) & 1;
		if (ver == 1 || layer == 0 || bri == 0 || bri == 15 || sri == 3) { if (out.frames == 0) return fail("bad first MPEG frame header"); out.trailer = n - q; return true; }
		const bool v1 = ver == 3; const int L = 4 - layer;   // layer 1..3
		int br = v1 ? br1[L - 1][bri] : br2[L == 1 ? 0 : 1][bri];
		int rate = sr[v1 ? 0 : ver == 2 ? 1 : 2][sri];
		size_t len = L == 1 ? (size_t)(12 * br * 1000 / rate + pad) * 4 : (size_t)((L == 3 && !v1 ? 72 : 144) * br * 1000 / rate + pad);
		if (len < 4 || q + len > n) { if (out.frames == 0) return fail("truncated first MPEG frame"); out.trailer = n - q; return true; }
		out.frames++; out.sampleRate = rate; q += len;
	}
	out.trailer = n - q;
	return out.frames > 0;
}

// ---------------------------------------------------------------- EX3
bool ParseEx3(const uint8_t *p, size_t n, Ex3 &e, std::string *err)
{
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	e = Ex3();
	if (n < 64 || memcmp(p, "LLIF", 4) != 0) return fail("no LLIF header");
	memcpy(e.header, p, 64);
	const uint8_t *b = p + 64; const size_t sz = n - 64; size_t i = 0;
	while (i < sz) {
		Ex3Block blk; int v = 0;
		while (true) {
			if (i >= sz) return fail("table runs past the data");
			Ex3Group g; g.c = b[i++];
			int cnt;
			if (g.c > 127) { v += g.c - 127; if (v == 256) { blk.groups.push_back(g); break; } cnt = 1; } else cnt = g.c + 1;
			for (int k = 0; k < cnt; k++) {
				if (i >= sz || v >= 256) return fail("table entry past the end");
				uint8_t p1 = b[i++];
				if (p1 != v) { if (i >= sz) return fail("pair runs past the data"); g.entries.push_back({ p1, b[i++] }); } else g.entries.push_back({ p1, -1 });
				v++;
			}
			blk.groups.push_back(std::move(g));
			if (v >= 256) break;
		}
		if (v != 256) return fail("table does not end at index 256");
		if (i + 2 > sz) return fail("no symbol count");
		blk.count = (uint16_t)((b[i] << 8) | b[i + 1]); i += 2;
		if (i + blk.count > sz) return fail("symbols run past the data");
		blk.symbols.assign(b + i, b + i + blk.count); i += blk.count;
		e.blocks.push_back(std::move(blk));
	}
	// decoded size (the engine's expansion: pair stack), for the report
	for (auto &blk : e.blocks) {
		uint8_t tab[256][2]; bool pair[256];
		int v = 0;
		for (int k = 0; k < 256; k++) { tab[k][0] = (uint8_t)k; tab[k][1] = 0; pair[k] = false; }
		for (auto &g : blk.groups) {
			if (g.c > 127) v += g.c - 127;
			for (auto &en : g.entries) { if (v < 256) { tab[v][0] = en.first; if (en.second >= 0) { tab[v][1] = (uint8_t)en.second; pair[v] = true; } } v++; }
		}
		for (uint8_t s : blk.symbols) {
			std::vector<uint8_t> st; st.push_back(s);
			while (!st.empty()) { uint8_t c = st.back(); st.pop_back(); if (!pair[c]) e.decodedBytes++; else { st.push_back(tab[c][1]); st.push_back(tab[c][0]); } if (st.size() > 4096) break; }
		}
	}
	return true;
}

void SerializeEx3(const Ex3 &e, std::vector<uint8_t> &o)
{
	o.assign(e.header, e.header + 64);
	for (auto &blk : e.blocks) {
		for (auto &g : blk.groups) { o.push_back(g.c); for (auto &en : g.entries) { o.push_back(en.first); if (en.second >= 0) o.push_back((uint8_t)en.second); } }
		o.push_back((uint8_t)(blk.count >> 8)); o.push_back((uint8_t)blk.count);
		o.insert(o.end(), blk.symbols.begin(), blk.symbols.end());
	}
	o.insert(o.end(), e.tail.begin(), e.tail.end());
}

// ---------------------------------------------------------------- BMP
bool ParseBmp(const uint8_t *p, size_t n, Bmp &b, std::string *err)
{
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	b = Bmp();
	if (n < 54 || p[0] != 'B' || p[1] != 'M') return fail("no BM header");
	b.fileSize = R32(p + 2); b.reserved = R32(p + 6); b.dataOffset = R32(p + 10); b.headerSize = R32(p + 14);
	if (b.headerSize < 40 || 14 + (size_t)b.headerSize > n) return fail("bad info header size");
	b.width = (int32_t)R32(p + 18); b.height = (int32_t)R32(p + 22); b.planes = R16(p + 26); b.bpp = R16(p + 28);
	b.compression = R32(p + 30); b.imageSize = R32(p + 34); b.xppm = (int32_t)R32(p + 38); b.yppm = (int32_t)R32(p + 42); b.colorsUsed = R32(p + 46); b.colorsImportant = R32(p + 50);
	b.headerExtra.assign(p + 54, p + 14 + b.headerSize);
	if (b.dataOffset < 14 + b.headerSize || b.dataOffset > n) return fail("bad pixel offset");
	b.palette.assign(p + 14 + b.headerSize, p + b.dataOffset);
	b.pixels.assign(p + b.dataOffset, p + n);
	return true;
}

void SerializeBmp(const Bmp &b, std::vector<uint8_t> &o)
{
	o.assign({ 'B', 'M' }); W32(o, b.fileSize); W32(o, b.reserved); W32(o, b.dataOffset); W32(o, b.headerSize);
	W32(o, (uint32_t)b.width); W32(o, (uint32_t)b.height); W16(o, b.planes); W16(o, b.bpp); W32(o, b.compression); W32(o, b.imageSize);
	W32(o, (uint32_t)b.xppm); W32(o, (uint32_t)b.yppm); W32(o, b.colorsUsed); W32(o, b.colorsImportant);
	o.insert(o.end(), b.headerExtra.begin(), b.headerExtra.end());
	o.insert(o.end(), b.palette.begin(), b.palette.end());
	o.insert(o.end(), b.pixels.begin(), b.pixels.end());
}

// ---------------------------------------------------------------- CT
bool ParseCt(const uint8_t *p, size_t n, Ct &c, std::string *err)
{
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	c = Ct();
	if (n != 4632) return fail("not 4632 bytes");
	c.count = R32(p);
	if (c.count > 100) return fail("command count above 100");
	for (int i = 0; i < 100; i++) {
		const uint8_t *r = p + 4 + 46 * i; CtCommand &m = c.cmd[i];
		m.commandId = r[0]; m.unref01 = r[1]; memcpy(m.sequence, r + 2, 32); m.targetPattern = r[0x22]; m.moveClass = r[0x23]; m.meterCostLevels = R16(r + 0x24);
		m.requestParam = r[0x26]; m.unref27 = r[0x27]; m.counterRequirement = R16(r + 0x28); m.unref2A[0] = r[0x2A]; m.unref2A[1] = r[0x2B]; m.flags = r[0x2C]; m.flags2 = r[0x2D];
	}
	const uint8_t *h = p + 4 + 4600; CtHeader &t = c.hdr;
	t.unref00 = h[0]; t.unref01 = h[1]; t.recoveryStyle = h[2]; t.unref03 = h[3]; t.unref04 = h[4]; t.flags = h[5]; t.unref06[0] = h[6]; t.unref06[1] = h[7];
	memcpy(&t.knockbackScale, h + 8, 4); memcpy(t.damageScale, h + 12, 16);
	return true;
}

void SerializeCt(const Ct &c, std::vector<uint8_t> &o)
{
	o.clear(); W32(o, c.count);
	for (int i = 0; i < 100; i++) {
		const CtCommand &m = c.cmd[i];
		o.push_back(m.commandId); o.push_back(m.unref01); o.insert(o.end(), m.sequence, m.sequence + 32); o.push_back(m.targetPattern); o.push_back(m.moveClass); W16(o, m.meterCostLevels);
		o.push_back(m.requestParam); o.push_back(m.unref27); W16(o, m.counterRequirement); o.push_back(m.unref2A[0]); o.push_back(m.unref2A[1]); o.push_back(m.flags); o.push_back(m.flags2);
	}
	const CtHeader &t = c.hdr;
	o.push_back(t.unref00); o.push_back(t.unref01); o.push_back(t.recoveryStyle); o.push_back(t.unref03); o.push_back(t.unref04); o.push_back(t.flags); o.push_back(t.unref06[0]); o.push_back(t.unref06[1]);
	const uint8_t *k = (const uint8_t *)&t.knockbackScale; o.insert(o.end(), k, k + 4); const uint8_t *d = (const uint8_t *)t.damageScale; o.insert(o.end(), d, d + 16);
}

// ---------------------------------------------------------------- WMT
bool ParseWmt(const uint8_t *p, size_t n, Wmt &w, std::string *err)
{
	auto fail = [&](const char *m) { if (err) *err = m; return false; };
	w = Wmt();
	if (n < 4) return fail("short file");
	w.count = R32(p);
	if (4 + (uint64_t)w.count * 154 != n) return fail("size is not 4 + count * 154");
	for (uint32_t i = 0; i < w.count; i++) w.records.emplace_back(p + 4 + 154 * (size_t)i, p + 4 + 154 * (size_t)(i + 1));
	return true;
}

void SerializeWmt(const Wmt &w, std::vector<uint8_t> &o)
{
	o.clear(); W32(o, w.count);
	for (auto &r : w.records) o.insert(o.end(), r.begin(), r.end());
}

// ---------------------------------------------------------------- text
bool LooksLikeText(const uint8_t *p, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		uint8_t c = p[i];
		if (c == 0 || (c < 32 && c != '\r' && c != '\n' && c != '\t')) return false;
		if ((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC)) { if (i + 1 >= n || p[i + 1] < 0x40 || p[i + 1] == 0x7F || p[i + 1] > 0xFC) return false; i++; }
	}
	return true;
}

bool ParseText(const uint8_t *p, size_t n, Text &t, std::string *err)
{
	t = Text();
	size_t q = 0;
	while (q < n) {
		size_t e = q; while (e < n && p[e] != '\n' && p[e] != '\r') e++;
		t.lines.emplace_back((const char *)p + q, e - q);
		if (e >= n) t.eol.push_back(0);
		else if (p[e] == '\r' && e + 1 < n && p[e + 1] == '\n') { t.eol.push_back(2); e += 2; }
		else { t.eol.push_back(1); e += 1; }   // lone \n or lone \r: stored as 1 / kept verbatim below
		if (e > q && e <= n && n && p[e - 1] == '\r' && t.eol.back() == 1) t.lines.back() += '\r';   // lone CR stays part of the line text
		q = e;
	}
	(void)err;
	return true;
}

void SerializeText(const Text &t, std::vector<uint8_t> &o)
{
	o.clear();
	for (size_t i = 0; i < t.lines.size(); i++) {
		const std::string &l = t.lines[i];
		bool loneCr = t.eol[i] == 1 && !l.empty() && l.back() == '\r';
		if (loneCr) { o.insert(o.end(), l.begin(), l.end()); continue; }
		o.insert(o.end(), l.begin(), l.end());
		if (t.eol[i] == 2) { o.push_back('\r'); o.push_back('\n'); } else if (t.eol[i] == 1) o.push_back('\n');
	}
}

} // namespace han2
