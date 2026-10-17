#include "s3d_bglist.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace s3d {
namespace {

std::string Lower(std::string s) { for (char& c : s) c = char(tolower((unsigned char)c)); return s; }

// Drops `//` comments outside string literals. Shift-JIS trail bytes never equal '"' or '/', so byte scanning is safe.
std::string StripComments(const std::string& t)
{
	std::string o;
	o.reserve(t.size());
	bool inStr = false;
	for (size_t i = 0; i < t.size(); ++i) {
		const char c = t[i];
		if (inStr) { o += c; if (c == '"') inStr = false; continue; }
		if (c == '"') { inStr = true; o += c; continue; }
		if (c == '/' && i + 1 < t.size() && t[i + 1] == '/') { while (i < t.size() && t[i] != '\n') ++i; o += '\n'; continue; }
		if (c == '/' && i + 1 < t.size() && t[i + 1] == '*') { size_t e = t.find("*/", i + 2); i = e == std::string::npos ? t.size() : e + 1; continue; }
		o += c;
	}
	return o;
}

struct Lexer {
	const std::string& s;
	size_t i = 0;
	void ws() { while (i < s.size() && (isspace((unsigned char)s[i]) || s[i] == ',' || s[i] == ';')) ++i; }
	bool eof() { ws(); return i >= s.size(); }
	std::string ident()
	{
		ws();
		size_t b = i;
		while (i < s.size() && (isalnum((unsigned char)s[i]) || s[i] == '_')) ++i;
		return s.substr(b, i - b);
	}
	// Balanced span starting at an opening bracket; returns the inner text.
	std::string span(char open, char close)
	{
		int depth = 0;
		size_t b = i;
		bool inStr = false;
		for (; i < s.size(); ++i) {
			const char c = s[i];
			if (inStr) { if (c == '"') inStr = false; continue; }
			if (c == '"') { inStr = true; continue; }
			if (c == open) ++depth;
			else if (c == close && --depth == 0) { ++i; return s.substr(b + 1, i - b - 2); }
		}
		return s.substr(b + 1);
	}
};

std::vector<float> Numbers(const std::string& t)
{
	std::vector<float> v;
	const char* p = t.c_str();
	while (*p) {
		if (isdigit((unsigned char)*p) || ((*p == '-' || *p == '+' || *p == '.') && (isdigit((unsigned char)p[1]) || p[1] == '.'))) {
			char* e = nullptr;
			v.push_back(strtof(p, &e));
			p = e > p ? e : p + 1;
		} else ++p;
	}
	return v;
}

// key = value pairs of one table body into `e`.
void ParseTable(const std::string& body, BgEntry& e)
{
	Lexer L{body};
	while (!L.eof()) {
		const std::string key = L.ident();
		if (key.empty()) { ++L.i; continue; }
		L.ws();
		if (L.i < body.size() && (body[L.i] == '=' || body[L.i] == ':')) ++L.i;
		L.ws();
		if (L.i >= body.size()) break;
		const char c = body[L.i];
		if (c == '"') {
			const size_t e2 = body.find('"', L.i + 1);
			e.strs[key] = body.substr(L.i + 1, (e2 == std::string::npos ? body.size() : e2) - L.i - 1);
			L.i = e2 == std::string::npos ? body.size() : e2 + 1;
		} else if (c == '[') {
			const std::string inner = L.span('[', ']');
			if (inner.find('{') != std::string::npos) e.raw[key] = inner;
			else e.nums[key] = Numbers(inner);
		} else if (c == '{') {
			e.raw[key] = L.span('{', '}');
		} else {
			size_t b = L.i;
			while (L.i < body.size() && body[L.i] != ',' && body[L.i] != '\n' && body[L.i] != '}') ++L.i;
			const std::string tok = body.substr(b, L.i - b);
			auto n = Numbers(tok);
			if (!n.empty()) e.nums[key] = {n[0]};
			else e.strs[key] = tok;
		}
	}
}

}  // namespace

float BgEntry::num(const std::string& k, float def, int i) const
{
	auto it = nums.find(k);
	return it != nums.end() && i < (int)it->second.size() ? it->second[i] : def;
}

std::string BgEntry::str(const std::string& k) const
{
	auto it = strs.find(k);
	return it != strs.end() ? it->second : std::string();
}

const BgEntry* BgList::byDataFile(const std::string& df) const
{
	const std::string want = Lower(df);
	for (const BgEntry& e : entries) if (Lower(e.str("DataFile")) == want) return &e;
	// fall back to the number in "bgNNN"
	const char* p = df.c_str();
	while (*p && !isdigit((unsigned char)*p)) ++p;
	return *p ? byNumber(atoi(p)) : nullptr;
}

const BgEntry* BgList::byNumber(int n) const
{
	for (const BgEntry& e : entries) if (e.number == n) return &e;
	return nullptr;
}

bool ParseBgList(const std::string& text, BgList& out, std::string& err)
{
	out = BgList{};
	const std::string t = StripComments(text);
	// the outer `BgList <- { ... }` (or a bare table)
	size_t open = t.find('{');
	if (open == std::string::npos) { err = "no table in BgList"; return false; }
	Lexer L{t};
	L.i = open;
	const std::string body = L.span('{', '}');
	Lexer B{body};
	while (!B.eof()) {
		const std::string key = B.ident();
		if (key.empty()) { ++B.i; continue; }
		B.ws();
		if (B.i < body.size() && body[B.i] == '=') ++B.i;
		B.ws();
		if (B.i >= body.size()) break;
		if (body[B.i] == '{') {
			BgEntry e;
			const char* p = key.c_str();
			while (*p && !isdigit((unsigned char)*p)) ++p;
			e.number = *p ? atoi(p) : -1;
			ParseTable(B.span('{', '}'), e);
			out.entries.push_back(std::move(e));
		} else if (body[B.i] == '[') {
			const std::string inner = B.span('[', ']');
			if (key == "BgSelectList") for (float f : Numbers(inner)) out.selectList.push_back(int(f));
		} else {
			while (B.i < body.size() && body[B.i] != ',' && body[B.i] != '\n') ++B.i;
		}
	}
	if (out.entries.empty()) { err = "no Bg_NNN entries"; return false; }
	return true;
}

bool LoadBgListFile(const std::string& utf8Path, BgList& out, std::string& err)
{
	std::ifstream f(std::filesystem::u8path(utf8Path), std::ios::binary);
	if (!f) { err = "cannot open " + utf8Path; return false; }
	std::stringstream ss;
	ss << f.rdbuf();
	return ParseBgList(ss.str(), out, err);
}

StageParams ParamsFrom(const BgEntry& e)
{
	StageParams p;
	for (int i = 0; i < 3; ++i) {
		p.scale[i] = e.num("Scale", 1.f, i);
		p.position[i] = e.has("Position") ? e.num("Position", 0.f, i) : e.num("Pos", 0.f, i);
	}
	p.fov = e.num("FOV", 45.f);
	p.vanishingPoint = e.num("VanishingPoint", 0.f);
	p.viewRotationX = e.num("ViewRotationX", 0.f);
	p.viewRotationY = e.num("ViewRotationY", 0.f);
	p.viewTranslationY = e.num("ViewTranslationY", 0.f);
	p.fog = e.num("IsFog", 0.f) != 0.f;
	p.fogStart = e.num("FogStart", 0.f);
	p.fogEnd = e.num("FogEnd", 0.f);
	for (int i = 0; i < 4; ++i) p.fogColor[i] = e.num("FogColor", 0.f, i);
	p.stageW = int(e.num("StageW", 2560.f));
	p.bloom = e.num("BGBloomEnable", 0.f) != 0.f;
	p.bloomBrightness = e.num("BGBloomBlightness", 0.f);
	p.bloomPower = e.num("BGBloomPower", 0.f);
	p.bloomBias[0] = e.num("BGBloomBiassR", 1.f);
	p.bloomBias[1] = e.num("BGBloomBiassG", 1.f);
	p.bloomBias[2] = e.num("BGBloomBiassB", 1.f);
	p.bloomBlurRadius = e.num("BGBloomBlurRadius", 0.f);
	p.bloomTexSize = int(e.num("BGBloomTextureSize", 256.f));
	p.bloomAlpha = e.num("BGBloomAlpha", 0.5f);
	p.shadowScale = e.num("ShadowScale", 0.6f);
	p.shadowAlpha = e.num("ShadowAlpha", 0.7f);
	p.targetW = int(e.num("TargetW", 1280.f));
	p.targetH = int(e.num("TargetH", 720.f));
	return p;
}

}  // namespace s3d
