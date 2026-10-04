#include "cgm_export.h"
#include "cgm_ops.h"
#include "../png_writer.h"
#include "../../third_party/json/json.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace cgm {
using json = nlohmann::json;
namespace fs = std::filesystem;

std::string HashHex(uint64_t h) { char b[20]; snprintf(b, sizeof(b), "%016llx", (unsigned long long)h); return b; }

std::string FileSafeName(const char *n32) {
	std::string s(n32, strnlen(n32, 32));
	if (s.size() > 4 && !_stricmp(s.c_str() + s.size() - 4, ".bmp")) s.resize(s.size() - 4);
	for (char &c : s) if (!(isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.')) c = '_';
	return s;
}

static fs::path P(const std::string &u) { return fs::path(Utf8ToWide(u)); }
static uint64_t PalRgbHash(const uint32_t pal[256]) {   // colours 1..255, alpha ignored: identifies "the palette the indices refer to"
	uint64_t h = 1469598103934665603ull;
	for (int i = 1; i < 256; i++) { const uint32_t c = pal[i] & 0xFFFFFF; for (int k = 0; k < 3; k++) { h ^= (c >> (8 * k)) & 255; h *= 1099511628211ull; } }
	return h;
}
static std::string Name4(int n) { char b[16]; snprintf(b, sizeof(b), "%04d", n); return b; }

static void WritePal(const std::string &path, const uint32_t pal[256], bool &ok) {
	std::ofstream f(P(path), std::ios::binary); if (!f) { ok = false; return; }
	uint32_t n = 1; f.write((const char *)&n, 4); f.write((const char *)pal, 1024);   // MBAACC .pal: count + 256 x RGBA
}

bool ExportBank(const Bank &bank, const std::string &dir, const std::string &bankName, const ExportOptions &opt, ExportResult &res, const uint32_t *extPal) {
	std::string err;
	if (!CreateDirectoriesUtf8(dir, err)) { res.error = err; return false; }
	const fs::path root = P(dir);
	if (opt.rgba && !opt.cgtoolLayout) fs::create_directories(root / "rgba");
	if (opt.indexed && !opt.cgtoolLayout) fs::create_directories(root / "indexed");
	json man; man["format"] = "hantei-cg-manifest"; man["version"] = 1; man["bank"] = bankName;
	man["cellUnit"] = bank.cellUnit(); man["pages"] = bank.pages(); man["imageSlots"] = (int)bank.images.size();
	uint32_t palUse[256]; if (extPal) memcpy(palUse, extPal, 1024); else bank.palette(0, palUse);
	man["indexPaletteHash"] = HashHex(PalRgbHash(palUse));
	man["palettes"] = json::array();
	if (opt.palettes && !opt.cgtoolLayout) {
		fs::create_directories(root / "palettes");
		for (int s = 0; s < 8; s++) {
			uint32_t p[256]; memcpy(p, bank.palettes.data() + 0x400 * s, 1024);
			bool ok = true; const std::string rel = "palettes/bank_slot" + std::to_string(s) + ".pal";
			WritePal(dir + "/" + rel, p, ok);
			if (!ok) { res.error = "could not write " + rel; return false; }
			man["palettes"].push_back({{"slot", s}, {"file", rel}}); res.files++;
		}
	}
	auto atlas = bank.buildAtlas();
	man["images"] = json::array();
	for (int n = 0; n < (int)bank.images.size(); n++) {
		const Image &im = bank.images[n];
		json j; j["id"] = n; j["present"] = im.present;
		if (!im.present) { man["images"].push_back(j); continue; }
		j["name"] = std::string(im.name, strnlen(im.name, 32)); j["type"] = im.type; j["bpp"] = im.bpp;
		j["canvas"] = {im.w, im.h}; j["bounds"] = {im.x1, im.y1, im.x2, im.y2};
		j["blocks"] = (int)im.blocks.size();
		auto own = bank.owners(n, atlas), dep = bank.dependants(n, atlas);
		if (!own.empty()) j["drawsCellsOf"] = own;
		if (!dep.empty()) j["cellsDrawnBy"] = dep;
		Rgba r;
		const bool wanted = opt.onlyIds.empty() || std::find(opt.onlyIds.begin(), opt.onlyIds.end(), n) != opt.onlyIds.end();
		if (im.drawable() && bank.decode(n, r, extPal)) {
			j["hash"] = HashHex(HashRgba(r.px.data(), r.px.size()));
			const std::string base = Name4(n) + "_" + FileSafeName(im.name);
			if (opt.cgtoolLayout) {
				// cgtool names: <stored name>_ID_<n>.png at canvas size (padded with transparent / index 0)
				const std::string fn = std::string(im.name, strnlen(im.name, 32)) + "_ID_" + std::to_string(n) + ".png";
				std::vector<uint8_t> canvas((size_t)im.w * im.h * 4, 0);
				for (int y = 0; y < r.h; y++) for (int x = 0; x < r.w; x++) { const int cx = x + im.x1, cy = y + im.y1; if (cx >= 0 && cy >= 0 && cx < im.w && cy < im.h) memcpy(&canvas[((size_t)cy * im.w + cx) * 4], &r.px[((size_t)y * r.w + x) * 4], 4); }
				if (!WritePngRgba(dir + "/" + fn, canvas.data(), im.w, im.h, err)) { res.error = err; return false; }
				res.files++; res.images++; continue;
			}
			if (opt.rgba && wanted) {
				const std::string rel = "rgba/" + base + ".png";
				if (!WritePngRgba(dir + "/" + rel, r.px.data(), r.w, r.h, err)) { res.error = err; return false; }
				j["rgba"] = rel; res.files++;
			}
			if (opt.indexed && wanted && (im.type == 0 || im.type == 2)) {
				std::vector<uint8_t> idx; uint32_t pal[256];
				if (bank.decodeIndexed(n, idx, pal, nullptr, extPal)) {
					const std::string rel = "indexed/" + base + ".png";
					if (!WritePngIndexed(dir + "/" + rel, idx.data(), r.w, r.h, pal, err)) { res.error = err; return false; }
					j["indexed"] = rel; res.files++;
				}
			}
		}
		res.images++;
		man["images"].push_back(j);
	}
	if (!opt.cgtoolLayout) {
		std::ofstream f(root / "manifest.json", std::ios::binary);
		if (!f) { res.error = "could not write manifest.json"; return false; }
		const std::string txt = man.dump(1); f.write(txt.data(), (std::streamsize)txt.size()); res.files++;
	}
	return true;
}

bool ImportBank(Bank &bank, const std::string &dir, ImportResult &res) {
	const fs::path root = P(dir);
	json man;
	try { std::ifstream f(root / "manifest.json", std::ios::binary); if (!f) { res.error = "no manifest.json in " + dir; return false; } f >> man; }
	catch (const std::exception &e) { res.error = std::string("manifest.json: ") + e.what(); return false; }
	if (man.value("format", "") != "hantei-cg-manifest") { res.error = "not a CG manager manifest"; return false; }
	if (man.value("imageSlots", -1) != (int)bank.images.size()) { res.error = "the manifest was made from a bank with a different image count"; return false; }
	for (const json &j : man["images"]) {
		const int n = j.value("id", -1);
		if (n < 0 || n >= (int)bank.images.size() || !j.value("present", false) || !j.contains("hash")) continue;
		const Image &im = bank.images[n];
		if (!im.present || im.type != j.value("type", -99)) { res.warnings.push_back("image " + std::to_string(n) + ": type changed since the export, skipped"); continue; }
		const std::string was = j["hash"];
		std::vector<uint8_t> px; int w = 0, h = 0; std::string err, src;
		std::vector<uint8_t> chosenIdx; bool haveIdx = false;
		// the indexed PNG and the RGBA PNG can both be edited; whichever differs from the recorded hash wins (indexed first)
		auto rp = [&](const char *key) { return WideToUtf8((root / Utf8ToWide(j[key].get<std::string>())).wstring()); };
		if (j.contains("indexed")) {
			std::vector<uint8_t> idx; uint32_t pal[256]; int iw, ih;
			if (ReadImageIndexed(rp("indexed"), idx, pal, iw, ih, err)) {
				std::vector<uint8_t> rgba((size_t)iw * ih * 4);
				for (int i = 0; i < iw * ih; i++) { const uint32_t c = pal[idx[i]]; memcpy(&rgba[i * 4], &c, 4); }
				if (im.type == 0 || im.type == 2) for (int i = 0; i < iw * ih; i++) if (idx[i] == 0) memset(&rgba[i * 4], 0, 4);
				if (HashHex(HashRgba(rgba.data(), rgba.size())) != was) {
					px = std::move(rgba); w = iw; h = ih; src = "indexed";
					// exact indices only when the PNG still uses the palette the export wrote; otherwise colours are matched
					haveIdx = im.type == 0 && HashHex(PalRgbHash(pal)) == man.value("indexPaletteHash", "");
					if (haveIdx) chosenIdx = std::move(idx);
				}
			}
		}
		if (px.empty() && j.contains("rgba")) {
			std::vector<uint8_t> rgba;
			if (ReadImageRgba(rp("rgba"), rgba, w, h, err)) {
				if (HashHex(HashRgba(rgba.data(), rgba.size())) != was) { px = std::move(rgba); src = "rgba"; }
			} else { res.warnings.push_back("image " + std::to_string(n) + ": " + err); continue; }
		}
		if (px.empty()) { res.skipped++; continue; }
		std::string e; ReplaceReport rep;
		if (!ReplaceImage(bank, n, px.data(), w, h, &e, &rep, haveIdx ? &chosenIdx : nullptr)) { res.warnings.push_back("image " + std::to_string(n) + ": " + e); continue; }
		if (rep.changedPixels || rep.paletteChanged) res.changed.push_back(n); else res.skipped++;
		if (rep.lostPixels) { std::string o; for (int x : rep.borrowedFrom) o += " " + std::to_string(x); res.warnings.push_back("image " + std::to_string(n) + ": " + std::to_string(rep.lostPixels) + " changed pixel(s) sit in cells borrowed from image(s)" + o + " and were not written (edit the owner)"); }
		if (!rep.alsoChanges.empty()) { std::string o; for (int x : rep.alsoChanges) o += " " + std::to_string(x); res.warnings.push_back("image " + std::to_string(n) + ": cells are shared, image(s)" + o + " change too"); }
	}
	return true;
}

} // namespace cgm
