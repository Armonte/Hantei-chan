#include "cgm_texsrc.h"
#include "../parts/parts.h"
#include "../parts/pat_texconv.h"
#include "../framedata.h"
#include "../png_writer.h"
#include "../han2_parts_edit.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>

namespace fs = std::filesystem;

namespace cgm {

// ---------------------------------------------------------------- shared operations
bool ExportPng(const TexSource &s, int i, const std::string &path, std::string *err) {
	Rgba px;
	if (!s.decode(i, px) || px.w <= 0 || px.h <= 0) { if (err) *err = "nothing to export (empty slot)"; return false; }
	std::string e; if (!WritePngRgba(path, px.px.data(), px.w, px.h, e)) { if (err) *err = e; return false; }
	return true;
}

bool ImportPng(TexSource &s, int i, const std::string &path, std::string *err, bool *changed) {
	Rgba px; std::string e;
	if (!ReadImageRgba(path, px.px, px.w, px.h, e)) { if (err) *err = e; return false; }
	return s.replace(i, px, err, changed);
}

// Pixels equal as drawn: fully transparent pixels compare equal whatever colour they carry.
static bool SameDrawn(const Rgba &a, const Rgba &b) {
	if (a.w != b.w || a.h != b.h || a.px.size() != b.px.size()) return false;
	for (size_t k = 0; k < a.px.size(); k += 4) {
		if (!a.px[k + 3] && !b.px[k + 3]) continue;
		if (memcmp(&a.px[k], &b.px[k], 4)) return false;
	}
	return true;
}

// ---------------------------------------------------------------- CG images
static const char *CgTypeName(int t) {
	switch (t) { case 0: return "8-bit (bank palette)"; case 1: return "32-bit BGRA"; case 2: return "8-bit + palette"; case 3: return "colour + alpha";
	case 4: return "8-bit + palette + alpha"; case 5: return "index + alpha"; case -1: return "placeholder"; }
	return "?";
}

bool CgTexSource::entry(int i, TexEntry &o) const {
	o = TexEntry(); ImgInfo in;
	if (!m_io.info(i, in)) return false;
	o.present = in.present && in.drawable;
	if (!in.present) return true;
	o.w = in.boundsW(); o.h = in.boundsH(); o.name.assign(in.name, strnlen(in.name, 32)); o.format = CgTypeName(in.type);
	return true;
}

bool CgTexSource::decode(int i, Rgba &o) const { return m_io.decode(i, o, m_pal); }

bool CgTexSource::replace(int i, const Rgba &px, std::string *err, bool *changed) {
	if (changed) *changed = false;
	Rgba cur;
	if (decode(i, cur) && SameDrawn(cur, px)) { lastReport = ReplaceReport(); return true; }
	ReplaceReport rep;
	if (!m_io.replace(i, px.px.data(), px.w, px.h, err, &rep, nullptr)) return false;
	lastReport = rep;
	if (changed) *changed = true;
	return true;
}

// ---------------------------------------------------------------- .pat textures
int PatTexSource::count() const { return (int)m_p.gfxMeta.size(); }

const char *PatTextureFormat(const Parts &p, int i) {
	if (i < 0 || i >= (int)p.gfxMeta.size()) return "?";
	const PartGfx<> &g = p.gfxMeta[i];
	if (g.imageSize == 0 && g.data) return "PGTX 32-bit";
	switch (g.type) { case 1: return "DXT1"; case 3: return "DXT3"; case 5: return "DXT5"; case 21: return "A8R8G8B8"; case 22: return "X8R8G8B8"; }
	return "?";
}

bool PatTexSource::entry(int i, TexEntry &o) const {
	o = TexEntry();
	if (i < 0 || i >= count()) return false;
	const PartGfx<> &g = m_p.gfxMeta[i];
	const bool pgtx = g.imageSize == 0 && g.data, kindOk = pgtx || pattex::IsDxt(g.type) || pattex::IsRawBgra(g.type);
	// the data must hold the whole texture (a PGT2 kind the loader does not read keeps a stale type and no full data)
	const size_t need = pgtx ? 0 : pattex::IsDxt(g.type) ? (size_t)((g.w + 3) / 4) * ((g.h + 3) / 4) * (g.type == 1 ? 8 : 16) : (size_t)g.w * g.h * 4;
	const bool known = kindOk && (pgtx || (g.imageData && (size_t)g.imageSize >= need));
	o.present = g.w > 0 && (g.imageData || g.data) && known;   // a kind the loader does not read is listed, not editable
	o.w = g.w; o.h = g.h; o.name = g.name.c_str(); o.format = g.w <= 0 ? "empty" : known ? PatTextureFormat(m_p, i) : "unsupported kind";
	return true;
}

bool PatTexSource::decode(int i, Rgba &o) const {
	if (i < 0 || i >= count()) return false;
	o = Rgba(); std::string e;
	return pattex::Decode(m_p.gfxMeta[i], o.px, o.w, o.h, &e);
}

bool PatTexSource::replace(int i, const Rgba &px, std::string *err, bool *changed) {
	if (changed) *changed = false;
	touchedParts.clear();
	if (i < 0 || i >= count()) { if (err) *err = "no such texture"; return false; }
	PartGfx<> &g = m_p.gfxMeta[i];
	Rgba cur;
	if (encodeType == -1 && decode(i, cur) && SameDrawn(cur, px)) return true;
	if (!pattex::Encode(g, px.px.data(), px.w, px.h, encodeType, err)) return false;
	for (auto &c : m_p.cutOuts) {   // parts sampling the texture follow its size (the PAT editor's import does the same)
		if (c.texture != i) continue;
		c.ppte[0] = g.pgte[0]; c.ppte[1] = g.pgte[1];
		touchedParts.push_back(c.id);
	}
	if (changed) *changed = true;
	return true;
}

// ---------------------------------------------------------------- RBO / GOF2 part sheets
int SheetTexSource::count() const { return (int)m_p.gfxMeta.size(); }

bool SheetTexSource::entry(int i, TexEntry &o) const {
	o = TexEntry();
	if (i < 0 || i >= count()) return false;
	const PartGfx<> &g = m_p.gfxMeta[i];
	o.present = g.data && g.w > 0 && g.h > 0 && g.bpp == 32;
	o.w = g.w; o.h = g.h; o.name = g.name.c_str(); o.format = o.present ? "32-bit sheet" : "empty";
	return true;
}

bool SheetTexSource::decode(int i, Rgba &o) const {
	o = Rgba();
	return han2::edit::SheetRgba(m_p, i, o.px, o.w, o.h);
}

bool SheetTexSource::replace(int i, const Rgba &px, std::string *err, bool *changed) {
	if (changed) *changed = false;
	touchedParts.clear();
	if (i < 0 || i >= count()) { if (err) *err = "no such sprite sheet"; return false; }
	Rgba cur;
	if (decode(i, cur) && SameDrawn(cur, px)) return true;
	if (!han2::edit::ImportSheetRgba(m_p, i, px.px.data(), px.w, px.h, err)) return false;
	for (auto &c : m_p.cutOuts) if (c.texture == i) touchedParts.push_back(c.id);
	if (changed) *changed = true;
	return true;
}

// ---------------------------------------------------------------- undo state
struct PatTexState { std::vector<PartGfx<>> gfx; std::vector<CutOut<>> cuts; };

static bool SameGfx(const PartGfx<> &a, const PartGfx<> &b) {
	return a.data == b.data && a.imageData == b.imageData && a.imageSize == b.imageSize && a.w == b.w && a.h == b.h && a.bpp == b.bpp && a.type == b.type
		&& a.name == b.name && !memcmp(a.pgte, b.pgte, sizeof a.pgte) && !memcmp(a.uvBpp, b.uvBpp, sizeof a.uvBpp) && a.noCompress == b.noCompress
		&& !memcmp(a.ddsHeader, b.ddsHeader, sizeof a.ddsHeader);
}

PatTexSnap CapturePatTex(const Parts &p) {
	auto s = std::make_shared<PatTexState>();
	s->gfx = p.gfxMeta; s->cuts = p.cutOuts;
	return s;
}

bool SamePatTex(const Parts &p, const PatTexSnap &s) {
	if (!s || s->gfx.size() != p.gfxMeta.size() || s->cuts.size() != p.cutOuts.size()) return false;
	for (size_t i = 0; i < s->gfx.size(); i++) if (!SameGfx(s->gfx[i], p.gfxMeta[i])) return false;
	for (size_t i = 0; i < s->cuts.size(); i++) if (memcmp(s->cuts[i].ppte, p.cutOuts[i].ppte, sizeof s->cuts[i].ppte)) return false;
	return true;
}

std::vector<int> RestorePatTex(Parts &p, const PatTexSnap &s) {
	std::vector<int> changed;
	if (!s) return changed;
	for (size_t i = 0; i < s->gfx.size(); i++) {
		const bool same = i < p.gfxMeta.size() && SameGfx(s->gfx[i], p.gfxMeta[i]);
		if (same) continue;
		const int texIndex = i < p.gfxMeta.size() ? p.gfxMeta[i].textureIndex : s->gfx[i].textureIndex;
		changed.push_back((int)i);
		if (i >= p.gfxMeta.size()) p.gfxMeta.resize(i + 1);
		p.gfxMeta[i] = s->gfx[i];
		p.gfxMeta[i].textureIndex = texIndex;   // the GL copy stays the record's own: the caller re-uploads its pixels
	}
	if (p.gfxMeta.size() > s->gfx.size()) p.gfxMeta.resize(s->gfx.size());   // a sheet appended by the step
	// cut-outs: only the fields a texture replace writes (the rest belongs to the PAT editor's own history)
	for (size_t i = 0; i < s->cuts.size() && i < p.cutOuts.size(); i++) memcpy(p.cutOuts[i].ppte, s->cuts[i].ppte, sizeof p.cutOuts[i].ppte);
	return changed;
}

// ---------------------------------------------------------------- references
std::vector<TexUse> PatTextureUsers(const Parts &p, int t, FrameData *fd) {
	std::vector<TexUse> out;
	std::set<int> parts;
	for (int c = 0; c < (int)p.cutOuts.size(); c++)
		if (p.cutOuts[c].texture == t) { parts.insert(c); TexUse u; u.part = c; out.push_back(u); }
	std::set<int> sets;
	for (int s = 0; s < (int)p.partSets.size(); s++)
		for (const auto &gr : p.partSets[s].groups)
			if (gr.ppId >= 0 && parts.count(gr.ppId)) { sets.insert(s); TexUse u; u.part = gr.ppId; u.partSet = s; out.push_back(u); }
	if (!fd || sets.empty()) return out;
	const int n = fd->get_sequence_count();
	for (int q = 0; q < n; q++) {
		Sequence *seq = fd->get_sequence(q); if (!seq) continue;
		for (int f = 0; f < (int)seq->frames.size(); f++) {
			const auto &layers = seq->frames[f].AF.layers;
			for (int l = 0; l < (int)layers.size(); l++)
				if (layers[l].usePat && sets.count(layers[l].spriteId)) { TexUse u; u.pattern = q; u.frame = f; u.layer = l; u.partSet = layers[l].spriteId; out.push_back(u); }
		}
	}
	return out;
}

std::vector<TexUse> CgImageUsers(FrameData &fd, int image) {
	std::vector<TexUse> out;
	const int n = fd.get_sequence_count();
	for (int q = 0; q < n; q++) {
		Sequence *seq = fd.get_sequence(q); if (!seq) continue;
		for (int f = 0; f < (int)seq->frames.size(); f++) {
			const auto &layers = seq->frames[f].AF.layers;
			for (int l = 0; l < (int)layers.size(); l++)
				if (!layers[l].usePat && layers[l].spriteId == image) { TexUse u; u.pattern = q; u.frame = f; u.layer = l; out.push_back(u); }
		}
	}
	return out;
}

// ---------------------------------------------------------------- mod folder
bool UsesModFolder(const std::string &g) { return g == "uni2" || g == "mbtl"; }

std::string ModFolderPath(const std::string &game, const std::string &gameDir, const std::string &key) {
	const char *im = game == "uni2" ? "UNI2-IM" : game == "mbtl" ? "MBTL-IM" : nullptr;
	if (!im || gameDir.empty() || key.empty()) return "";
	std::string k = key; std::replace(k.begin(), k.end(), '/', '\\');
	while (!k.empty() && (k[0] == '\\' || k[0] == '.')) k.erase(0, 1);
	std::string d = gameDir; if (d.back() != '\\' && d.back() != '/') d += '\\';
	return d + im + "\\Mods\\" + k;
}

std::string GameKeyOf(const std::string &path) {
	std::string p = path; std::replace(p.begin(), p.end(), '\\', '/');
	std::string low = p; for (char &c : low) c = (char)tolower((unsigned char)c);
	size_t at = low.rfind("/data/");
	if (at == std::string::npos) { if (low.rfind("data/", 0) == 0) return p; return ""; }
	return p.substr(at + 1);
}

bool WriteModFile(const std::string &dst, const std::vector<uint8_t> &bytes, std::string *err) {
	std::error_code ec;
	const fs::path d = fs::u8path(dst), bak = fs::u8path(dst + ".hcbak"), tmp = fs::u8path(dst + ".hctmp");
	fs::create_directories(d.parent_path(), ec);
	if (fs::exists(d, ec) && !fs::exists(bak, ec)) { fs::copy_file(d, bak, ec); if (ec) { if (err) *err = "cannot back up " + dst + ": " + ec.message(); return false; } }
	{
		std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
		if (!f) { if (err) *err = "cannot write " + tmp.u8string(); return false; }
		f.write((const char *)bytes.data(), (std::streamsize)bytes.size());
		if (!f) { if (err) *err = "write failed: " + tmp.u8string(); return false; }
	}
	fs::rename(tmp, d, ec);
	if (ec) { if (err) *err = "cannot replace " + dst + ": " + ec.message(); fs::remove(tmp, ec); return false; }
	return true;
}

bool RestoreModFile(const std::string &dst, std::string *err) {
	std::error_code ec;
	const fs::path d = fs::u8path(dst), bak = fs::u8path(dst + ".hcbak");
	if (fs::exists(bak, ec)) { fs::rename(bak, d, ec); if (ec && err) *err = ec.message(); return !ec; }
	fs::remove(d, ec);
	if (ec && err) *err = ec.message();
	return !ec;
}

} // namespace cgm
