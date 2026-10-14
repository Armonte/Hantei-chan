#ifndef UNI_VECTOR_TABLE_H_GUARD
#define UNI_VECTOR_TABLE_H_GUARD
// UNI2/MBTL hit-vector names. The games resolve an ATV2 vector id through the
// Squirrel table data/VectorTable.txt (vectmpl.add({ number = N, name = "..." })),
// not through MBAACC's fixed list, so the MBAACC names don't apply there.
// Looked up next to the character's HA6 (its folder or up to two parents).
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include "misc.h"

namespace univec {
using Names = std::map<int, std::string>;

// Parses `vectmpl.add({ number = N, ... name = "..." ...` blocks. Never throws.
inline Names Parse(const std::string& text)
{
	// `key = ` at statement level inside a block (not t.key / def_key).
	auto field = [](const std::string& blk, const char* key) -> size_t {
		const size_t k = strlen(key);
		for (size_t at = blk.find(key); at != std::string::npos; at = blk.find(key, at + k)) {
			if (at > 0 && (isalnum((unsigned char)blk[at - 1]) || blk[at - 1] == '_' || blk[at - 1] == '.')) continue;
			size_t e = at + k;
			while (e < blk.size() && (blk[e] == ' ' || blk[e] == '\t')) ++e;
			if (e < blk.size() && blk[e] == '=') return e + 1;
		}
		return std::string::npos;
	};
	// "const def_X = N;" constants (some entries use number = def_VL_KoVector).
	std::map<std::string, int> consts;
	for (size_t c = text.find("const "); c != std::string::npos; c = text.find("const ", c + 6)) {
		size_t a = c + 6, b = a;
		while (b < text.size() && (isalnum((unsigned char)text[b]) || text[b] == '_')) ++b;
		size_t e = b;
		while (e < text.size() && (text[e] == ' ' || text[e] == '\t')) ++e;
		if (b > a && e < text.size() && text[e] == '=') consts[text.substr(a, b - a)] = atoi(text.c_str() + e + 1);
	}
	Names out;
	static const char kAdd[] = "vectmpl.add({";
	size_t at = 0;
	while ((at = text.find(kAdd, at)) != std::string::npos) {
		at += sizeof(kAdd) - 1;
		const size_t next = text.find(kAdd, at);
		const std::string blk = text.substr(at, next == std::string::npos ? std::string::npos : next - at);
		const size_t n = field(blk, "number"), nm = field(blk, "name");
		if (n == std::string::npos || nm == std::string::npos) continue;
		size_t v = n;
		while (v < blk.size() && (blk[v] == ' ' || blk[v] == '\t')) ++v;
		int id;
		if (v < blk.size() && (isdigit((unsigned char)blk[v]) || blk[v] == '-')) id = atoi(blk.c_str() + v);
		else {
			size_t w = v;
			while (w < blk.size() && (isalnum((unsigned char)blk[w]) || blk[w] == '_')) ++w;
			auto k = consts.find(blk.substr(v, w - v));
			if (k == consts.end()) continue;
			id = k->second;
		}
		const size_t q0 = blk.find('"', nm), q1 = q0 == std::string::npos ? q0 : blk.find('"', q0 + 1);
		if (q1 == std::string::npos || blk.find('\n', nm) < q0) continue;
		out[id] = sj2utf8(blk.substr(q0 + 1, q1 - q0 - 1));
	}
	return out;
}

// Names for the data folder of `ha6Path` (cached per table file; null if none).
inline const Names* ForFile(const std::string& ha6Path)
{
	static std::map<std::string, std::shared_ptr<Names>> cache;
	if (ha6Path.empty()) return nullptr;
	std::error_code ec;
	std::filesystem::path dir = std::filesystem::u8path(ha6Path).parent_path();
	for (int up = 0; up < 3 && !dir.empty(); ++up, dir = dir.parent_path()) {
		const std::filesystem::path p = dir / "VectorTable.txt";
		const std::string key = p.u8string();
		auto it = cache.find(key);
		if (it != cache.end()) { if (it->second) return it->second.get(); continue; }
		if (!std::filesystem::exists(p, ec)) { cache[key] = nullptr; continue; }
		std::ifstream f(p, std::ios::binary);
		const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		auto names = std::make_shared<Names>(Parse(text));
		if (names->empty()) { cache[key] = nullptr; continue; }
		cache[key] = names;
		return names.get();
	}
	return nullptr;
}
}
#endif
