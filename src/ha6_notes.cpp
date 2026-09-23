#include "ha6_notes.h"
#include "misc.h"
#include "../third_party/json/json.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>

using json = nlohmann::json;

std::string Ha6Notes::PatternKey(int pattern)
{
	return "p" + std::to_string(pattern);
}

std::string Ha6Notes::RecordKey(int pattern, int frame, bool isEffect, int index, int type)
{
	return "p" + std::to_string(pattern) + ".f" + std::to_string(frame) + (isEffect ? ".ef" : ".if") +
	       std::to_string(index) + ".t" + std::to_string(type);
}

bool Ha6Notes::ParseKey(const std::string& key, int* pattern, int* frame, bool* isEffect, int* index, int* type)
{
	int p = -1, f = -1, i = -1, t = 0;
	char kind[3] = {};
	*frame = -1; *index = -1; *type = 0; *isEffect = false;
	if (std::sscanf(key.c_str(), "p%d.f%d.%2[efi]%d.t%d", &p, &f, kind, &i, &t) == 5) {
		*pattern = p; *frame = f; *index = i; *type = t;
		*isEffect = kind[0] == 'e';
		return true;
	}
	char tail;
	if (std::sscanf(key.c_str(), "p%d%c", &p, &tail) == 1) { *pattern = p; return true; }
	return false;
}

const std::string* Ha6Notes::get(const std::string& key) const
{
	auto it = notes.find(key);
	return it == notes.end() ? nullptr : &it->second;
}

void Ha6Notes::set(const std::string& key, const std::string& text)
{
	auto it = notes.find(key);
	if (text.empty()) {
		if (it != notes.end()) { notes.erase(it); dirty = true; }
		return;
	}
	if (it == notes.end() || it->second != text) { notes[key] = text; dirty = true; }
}

bool Ha6Notes::load(const std::string& path, std::string* error)
{
	notes.clear();
	dirty = false;
	std::ifstream f(path, std::ios::binary);
	if (!f) return true;
	const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	try {
		json root = json::parse(text);
		if (!root.is_object() || !root.contains("notes") || !root["notes"].is_object())
			throw std::runtime_error("expected an object with a \"notes\" object");
		for (auto it = root["notes"].begin(); it != root["notes"].end(); ++it)
			if (it.value().is_string()) notes[it.key()] = it.value().get<std::string>();
	} catch (const std::exception& e) {
		notes.clear();
		if (error) *error = path + ": " + e.what();
		return false;
	}
	return true;
}

bool Ha6Notes::save(const std::string& path) const
{
	json root = json::object();
	root["version"] = 1;
	root["notes"] = json::object();
	for (const auto& kv : notes) root["notes"][kv.first] = kv.second;
	const std::string text = root.dump(2, ' ', false, json::error_handler_t::replace) + "\n";
	return WriteFileAtomic(path.c_str(), text.data(), text.size());
}
