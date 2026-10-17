#include "hud_colors.h"
#include "misc.h"
#include "../third_party/json/json.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>

namespace hud {

// Addresses from issue #56 (ThePwnzr). Re-located per exe in Locate().
const std::vector<ColorSlot>& Slots()
{
	static const std::vector<ColorSlot> slots = {
		{"meter.lower",       "Meter < 100%",                 0xffc80000u,  1, 0x2551F},
		{"meter.middle",      "Meter 100-199% (H: 100-149%)", 0xffc8c800u,  2, 0x25536},
		{"meter.upper",       "Meter 200%+ (H: 150%+)",       0xff00c800u,  3, 0x25544},
		{"meter.unlimited",   "UNLIMITED",                    0xff3296ffu,  2, 0x25466},
		{"meter.heat",        "HEAT",                         0xff5a5ae6u, -2, 0x2547B},
		{"meter.max",         "MAX",                          0xfffaa000u, -2, 0x2549C},
		{"meter.blood_heat",  "BLOOD HEAT",                   0xffb4b4b4u, -2, 0x254BA},
		{"meter.break",       "BREAK (black overlay)",        0xffbe64c8u, -2, 0x25567},
		{"guard.quality_high","Guard bar, best quality",      0xff00bee6u,  0, 0x252CC},
		{"guard.quality_low", "Guard bar, worst quality",     0xffe60a0au,  0, 0x252C6},
		{"guard.break",       "Guard break",                  0xff767676u,  0, 0x252B8},
	};
	return slots;
}

static bool ReadFile(const std::string& path, std::string& data)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) return false;
	data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return true;
}

static uint32_t U32(const std::string& d, size_t o) { uint32_t v; std::memcpy(&v, d.data() + o, 4); return v; }
static uint16_t U16(const std::string& d, size_t o) { uint16_t v; std::memcpy(&v, d.data() + o, 2); return v; }

static bool RvaToOffset(const std::string& d, uint32_t rva, uint32_t* off)
{
	if (d.size() < 0x40 || d[0] != 'M' || d[1] != 'Z') return false;
	const uint32_t pe = U32(d, 0x3C);
	if (pe + 24 > d.size() || std::memcmp(d.data() + pe, "PE\0\0", 4) != 0) return false;
	const uint16_t nsec = U16(d, pe + 6), optSize = U16(d, pe + 20);
	const size_t sec = pe + 24 + optSize;
	for (int i = 0; i < nsec; ++i) {
		const size_t s = sec + (size_t)i * 40;
		if (s + 40 > d.size()) return false;
		const uint32_t vsize = U32(d, s + 8), va = U32(d, s + 12), rsize = U32(d, s + 16), raw = U32(d, s + 20);
		const uint32_t size = vsize > rsize ? vsize : rsize;
		if (rva >= va && rva < va + size) { *off = rva - va + raw; return *off + 4 <= d.size(); }
	}
	return false;
}

// The immediate of "mov [esp+disp8], imm32" (C7 44 24 xx imm) or
// "mov r32, imm32" (B8+r imm) nearest the reported address.
static bool Locate(const std::string& d, uint32_t hintOff, uint32_t* immOff)
{
	int best = -1, bestDist = 1 << 30;
	for (int delta = -6; delta <= 6; ++delta) {
		const long long o = (long long)hintOff + delta;
		if (o < 4 || o + 4 > (long long)d.size()) continue;
		const unsigned char* p = (const unsigned char*)d.data() + o;
		const bool movEsp = p[-4] == 0xC7 && p[-3] == 0x44 && p[-2] == 0x24;
		const bool movReg = p[-1] >= 0xB8 && p[-1] <= 0xBF;
		if (!movEsp && !movReg) continue;
		// Colour immediates here are opaque (alpha 0xff).
		if ((U32(d, (size_t)o) >> 24) != 0xff) continue;
		const int dist = delta < 0 ? -delta : delta;
		if (dist < bestDist) { bestDist = dist; best = (int)o; }
	}
	if (best < 0) return false;
	*immOff = (uint32_t)best;
	return true;
}

bool ReadExeColors(const std::string& exePath, std::vector<ExeColor>& out, std::string* error)
{
	std::string d;
	if (!ReadFile(exePath, d)) { if (error) *error = "Cannot read " + exePath; return false; }
	out.assign(Slots().size(), ExeColor{});
	uint32_t probe;
	if (!RvaToOffset(d, 0x1000, &probe)) { if (error) *error = exePath + " is not a PE image"; return false; }
	for (size_t i = 0; i < Slots().size(); ++i) {
		uint32_t off, imm;
		if (!RvaToOffset(d, Slots()[i].rvaHint, &off) || !Locate(d, off, &imm)) continue;
		out[i].found = true;
		out[i].fileOffset = imm;
		out[i].argb = U32(d, imm);
	}
	return true;
}

bool WritePatchedExe(const std::string& srcExe, const std::string& dstExe,
                     const std::vector<uint32_t>& argb, std::string* error)
{
	if (srcExe == dstExe) { if (error) *error = "Refusing to overwrite the source exe; choose another file"; return false; }
	std::vector<ExeColor> loc;
	if (!ReadExeColors(srcExe, loc, error)) return false;
	std::string d;
	ReadFile(srcExe, d);
	for (size_t i = 0; i < loc.size() && i < argb.size(); ++i)
		if (loc[i].found) std::memcpy(&d[loc[i].fileOffset], &argb[i], 4);
	if (!WriteFileAtomic(dstExe.c_str(), d.data(), d.size())) { if (error) *error = "Cannot write " + dstExe; return false; }
	return true;
}

std::string ToHex(uint32_t argb)
{
	char b[16];
	std::snprintf(b, sizeof(b), "#%08x", argb);
	return b;
}

bool FromHex(const std::string& s, uint32_t* argb)
{
	const char* p = s.c_str();
	if (*p == '#') ++p;
	if (std::strlen(p) != 8) return false;
	char* end = nullptr;
	const unsigned long v = std::strtoul(p, &end, 16);
	if (!end || *end) return false;
	*argb = (uint32_t)v;
	return true;
}

using json = nlohmann::json;

static json* Walk(json& root, const std::string& key, bool create)
{
	// "meter.lower" -> root["colors"]["meter"]["lower"]
	json* node = &root["colors"];
	size_t start = 0;
	while (true) {
		const size_t dot = key.find('.', start);
		const std::string part = key.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
		if (!create && (!node->is_object() || !node->contains(part))) return nullptr;
		node = &(*node)[part];
		if (dot == std::string::npos) return node;
		start = dot + 1;
	}
}

bool LoadThemeColors(const std::string& jsonPath, std::vector<uint32_t>& argb, std::string* error)
{
	std::string text;
	if (!ReadFile(jsonPath, text)) { if (error) *error = "Cannot read " + jsonPath; return false; }
	try {
		json root = json::parse(text);
		argb.resize(Slots().size());
		for (size_t i = 0; i < Slots().size(); ++i) {
			argb[i] = Slots()[i].defaultArgb;
			json* n = Walk(root, Slots()[i].key, false);
			if (!n) continue;
			std::string hex = n->is_string() ? n->get<std::string>()
				: (n->is_object() && n->contains("argb") ? (*n)["argb"].get<std::string>() : "");
			uint32_t v;
			if (FromHex(hex, &v)) argb[i] = v;
		}
	} catch (const std::exception& e) {
		if (error) *error = jsonPath + ": " + e.what();
		return false;
	}
	return true;
}

bool SaveThemeColors(const std::string& jsonPath, const std::vector<uint32_t>& argb, std::string* error)
{
	json root = json::object();
	std::string text;
	if (ReadFile(jsonPath, text)) {
		try { root = json::parse(text); } catch (...) { root = json::object(); }
	}
	if (!root.is_object()) root = json::object();
	if (!root.contains("schema_version")) root["schema_version"] = 1;
	for (size_t i = 0; i < Slots().size() && i < argb.size(); ++i) {
		json* n = Walk(root, Slots()[i].key, true);
		if (Slots()[i].overlaySpeed != 0 || (n->is_object())) {
			if (!n->is_object()) *n = json::object();
			(*n)["argb"] = ToHex(argb[i]);
			if (!n->contains("overlay_speed")) (*n)["overlay_speed"] = Slots()[i].overlaySpeed;
		} else {
			*n = ToHex(argb[i]);
		}
	}
	const std::string out = root.dump(2) + "\n";
	if (!WriteFileAtomic(jsonPath.c_str(), out.data(), out.size())) { if (error) *error = "Cannot write " + jsonPath; return false; }
	return true;
}

} // namespace hud
