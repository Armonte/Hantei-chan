#include "ha6_notes.h"
#include "misc.h"
#include "../third_party/json/json.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <filesystem>
#include <numeric>
#include "framedata.h"

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

std::string Ha6Notes::StateKey(int pattern, int frame)
{
	return PatternKey(pattern) + ".f" + std::to_string(frame);
}

bool Ha6Notes::ParseKey(const std::string& key, int* pattern, int* frame, bool* isEffect, int* index, int* type)
{
	int p = -1, f = -1, i = -1, t = 0;
	char kind[3] = {}; int n = 0;
	*frame = -1; *index = -1; *type = 0; *isEffect = false;
	if (std::sscanf(key.c_str(), "p%d.f%d.%2[efi]%d.t%d%n", &p, &f, kind, &i, &t, &n) == 5 &&
	    n == (int)key.size() && p >= 0 && f >= 0 && i >= 0 && (std::string(kind) == "ef" || std::string(kind) == "if")) {
		*pattern = p; *frame = f; *index = i; *type = t;
		*isEffect = kind[0] == 'e';
		return true;
	}
	n = 0;
	if (std::sscanf(key.c_str(), "p%d.f%d%n", &p, &f, &n) == 2 && n == (int)key.size() && p >= 0 && f >= 0) {
		*pattern = p; *frame = f; return true;
	}
	n = 0;
	if (std::sscanf(key.c_str(), "p%d%n", &p, &n) == 1 && n == (int)key.size() && p >= 0) { *pattern = p; return true; }
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
		if (it != notes.end()) { notes.erase(it); dirty = notes != savedNotes; }
		return;
	}
	if (it == notes.end() || it->second != text) { notes[key] = text; dirty = notes != savedNotes; }
}

bool Ha6Notes::load(const std::string& path, std::string* error)
{
	notes.clear();
	dirty = false;
	legacyPayload.clear();
	savedNotes.clear();
	std::ifstream f(std::filesystem::u8path(path), std::ios::binary);
	if (!f) {
		std::error_code ec;
		if (std::filesystem::exists(std::filesystem::u8path(path), ec) || ec) {
			if (error) *error = path + ": notes file is unreadable";
			return false;
		}
		return true;
	}
	const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	try {
		json root = json::parse(text);
		if (!root.is_object() || !root.contains("notes") || !root["notes"].is_object())
			throw std::runtime_error("expected an object with a \"notes\" object");
		if (root.value("version", 1) != 1) throw std::runtime_error("unsupported notes version");
		for (auto it = root["notes"].begin(); it != root["notes"].end(); ++it)
			if (it.value().is_string()) notes[it.key()] = it.value().get<std::string>();
			else throw std::runtime_error("note text must be a string");
	} catch (const std::exception& e) {
		notes.clear();
		if (error) *error = path + ": " + e.what();
		return false;
	}
	markSaved();
	return true;
}

bool Ha6Notes::save(const std::string& path) const
{
	// Save As backs up the imported bytes beside the destination, never in the
	// original live data folder. Existing migration backups are immutable.
	if (!legacyPayload.empty()) {
		const std::string backup = path + ".legacy-backup";
		std::ifstream f(std::filesystem::u8path(backup), std::ios::binary);
		if (f) {
			std::string existing((std::istreambuf_iterator<char>(f)), {});
			if (existing != legacyPayload) return false;
		} else if (std::filesystem::exists(std::filesystem::u8path(backup)) ||
		           !WriteFileAtomic(backup.c_str(), legacyPayload.data(), legacyPayload.size())) return false;
	}
	json root = json::object();
	root["version"] = 1;
	root["notes"] = json::object();
	for (const auto& kv : notes) root["notes"][kv.first] = kv.second;
	const std::string text = root.dump(2, ' ', false, json::error_handler_t::replace) + "\n";
	return WriteFileAtomic(path.c_str(), text.data(), text.size());
}

bool Ha6Notes::loadForHa6(const std::string& path, const std::vector<Sequence>& sequences, std::string* error)
{
	*this = Ha6Notes{};
	const auto native = PathFor(path);
	if (std::filesystem::exists(std::filesystem::u8path(native))) {
		if (!load(native, error)) return false;
		const auto original = notes;
		for (const auto& kv : original) {
			int p, f, i, t; bool ef;
			if (!ParseKey(kv.first, &p, &f, &ef, &i, &t)) continue;
			bool valid = p < (int)sequences.size() && !sequences[p].empty;
			if (valid && f >= 0) {
				valid = f < (int)sequences[p].frames.size();
				if (valid && i >= 0) {
					const auto& state = sequences[p].frames[f];
					valid = ef ? i < (int)state.EF.size() && state.EF[i].type == t : i < (int)state.IF.size() && state.IF[i].type == t;
				}
			}
			if (!valid) {
				notes.erase(kv.first);
				std::string key = "unmatched:" + kv.first;
				while (notes.count(key)) key = "unmatched:" + key;
				notes[key] = kv.second;
			}
		}
		markSaved();
		event("native load: " + std::to_string(notes.size()) + " notes");
		return true;
	}
	std::string legacy = path + ".notes";
	if (!std::filesystem::exists(std::filesystem::u8path(legacy))) legacy = path + ".gonpnotes.json";
	if (!std::filesystem::exists(std::filesystem::u8path(legacy))) return true;
	if (!load(legacy, error)) return false;
	std::ifstream f(std::filesystem::u8path(legacy), std::ios::binary);
	legacyPayload.assign(std::istreambuf_iterator<char>(f), {});
	auto imported = std::move(notes); notes.clear();
	for (const auto& kv : imported) {
		if (kv.second.empty()) continue;
		int p = -1, s = -1, i = -1, n = 0; char kind[10] = {};
		bool record = std::sscanf(kv.first.c_str(), "pattern/%d/frame/%d/%9[a-z]/%d%n", &p, &s, kind, &i, &n) == 4 && n == (int)kv.first.size();
		std::string key = "legacy:" + kv.first;
		const bool validP = p >= 0 && p < (int)sequences.size() && !sequences[p].empty;
		if (record && validP && s >= 0 && s < (int)sequences[p].frames.size() && i >= 0) {
			const auto& state = sequences[p].frames[s];
			if (std::string(kind) == "effect" && i < (int)state.EF.size()) key = RecordKey(p, s, true, i, state.EF[i].type);
			if (std::string(kind) == "condition" && i < (int)state.IF.size()) key = RecordKey(p, s, false, i, state.IF[i].type);
		} else {
			n = 0;
			if (std::sscanf(kv.first.c_str(), "pattern/%d/frame/%d%n", &p, &s, &n) == 2 && n == (int)kv.first.size()) {
				if (p >= 0 && p < (int)sequences.size() && !sequences[p].empty && s >= 0 && s < (int)sequences[p].frames.size()) key = StateKey(p, s);
			} else {
				n = 0;
				if (std::sscanf(kv.first.c_str(), "pattern/%d%n", &p, &n) == 1 && n == (int)kv.first.size() && p >= 0 && p < (int)sequences.size() && !sequences[p].empty) key = PatternKey(p);
			}
		}
		notes[key] = kv.second;
	}
	markSaved(); // Import alone never writes or marks the HA6 edited.
	event("legacy import: " + std::to_string(notes.size()) + " notes");
	return true;
}

void Ha6Notes::event(const std::string& message)
{
	if (trace.size() == 64) trace.erase(trace.begin());
	trace.push_back(message);
}

void Ha6Notes::remap(int pattern, int frame, bool isEffect, const std::vector<int>& mapping)
{
	auto result = notes;
	std::vector<std::pair<std::string, std::string>> moved;
	for (const auto& kv : notes) {
		int p, f, i, t; bool ef;
		if (!ParseKey(kv.first, &p, &f, &ef, &i, &t) || p != pattern || f < 0) continue;
		if (frame >= 0 && (f != frame || i < 0 || ef != isEffect)) continue;
		const int old = frame < 0 ? f : i;
		result.erase(kv.first);
		std::string key;
		if (old >= (int)mapping.size() || mapping[old] < 0) {
			key = "unmatched:" + kv.first;
			while (result.count(key)) key = "unmatched:" + key;
		} else {
			if (frame < 0) f = mapping[old]; else i = mapping[old];
			key = i < 0 ? StateKey(p, f) : RecordKey(p, f, ef, i, t);
		}
		moved.emplace_back(key, kv.second);
	}
	for (const auto& kv : moved) {
		std::string key = kv.first;
		// A pre-existing unmatched index must not be overwritten by a moved note.
		while (result.count(key)) key = "unmatched:" + key;
		result[key] = kv.second;
	}
	if (result != notes) { notes = std::move(result); dirty = notes != savedNotes; }
	event("remap p" + std::to_string(pattern) + " state " + std::to_string(frame));
}

void Ha6Notes::insert(int p, int f, bool ef, int at, int count, int size)
{
	std::vector<int> map(size); std::iota(map.begin(), map.end(), 0);
	for (int& i : map) if (i >= at) i += count;
	remap(p, f, ef, map);
}
void Ha6Notes::erase(int p, int f, bool ef, int at, int size)
{
	std::vector<int> map(size); std::iota(map.begin(), map.end(), 0);
	for (int& i : map) if (i == at) i = -1; else if (i > at) --i;
	remap(p, f, ef, map);
}
void Ha6Notes::move(int p, int f, bool ef, int from, int to, int size)
{
	std::vector<int> map(size); std::iota(map.begin(), map.end(), 0);
	for (int& i : map) {
		if (i == from) i = to;
		else if (from < to && i > from && i <= to) --i;
		else if (to < from && i >= to && i < from) ++i;
	}
	remap(p, f, ef, map);
}
void Ha6Notes::cloneState(int p, int source, int destination)
{
	auto copy = notes;
	for (const auto& kv : copy) {
		int pat, f, i, t; bool ef;
		if (ParseKey(kv.first, &pat, &f, &ef, &i, &t) && pat == p && f == source)
			set(i < 0 ? StateKey(p, destination) : RecordKey(p, destination, ef, i, t), kv.second);
	}
}
void Ha6Notes::archivePattern(int pattern)
{
	auto copy = notes;
	for (const auto& kv : copy) {
		int p, f, i, t; bool ef;
		if (ParseKey(kv.first, &p, &f, &ef, &i, &t) && p == pattern) {
			notes.erase(kv.first);
			std::string key = "unmatched:" + kv.first;
			while (notes.count(key)) key = "unmatched:" + key;
			notes[key] = kv.second; dirty = true;
		}
	}
}

void Ha6Notes::archiveState(int pattern, int frame)
{
	auto copy = notes;
	for (const auto& kv : copy) {
		int p, f, i, t; bool ef;
		if (ParseKey(kv.first, &p, &f, &ef, &i, &t) && p == pattern && f == frame) {
			notes.erase(kv.first);
			std::string key = "unmatched:" + kv.first;
			while (notes.count(key)) key = "unmatched:" + key;
			notes[key] = kv.second;
		}
	}
	dirty = notes != savedNotes;
}

void Ha6Notes::movePatterns(const std::vector<int>& from, const std::vector<int>& to)
{
	std::map<int, int> mapping;
	for (size_t k = 0; k < from.size() && k < to.size(); ++k) mapping[from[k]] = to[k];
	const auto original = notes;
	for (const auto& kv : mapping) if (!mapping.count(kv.second)) archivePattern(kv.second);
	for (const auto& kv : original) {
		int p, f, i, t; bool ef;
		if (ParseKey(kv.first, &p, &f, &ef, &i, &t) && mapping.count(p)) notes.erase(kv.first);
	}
	for (const auto& kv : original) {
		int p, f, i, t; bool ef;
		if (!ParseKey(kv.first, &p, &f, &ef, &i, &t) || !mapping.count(p)) continue;
		const int dest = mapping[p];
		set(f < 0 ? PatternKey(dest) : i < 0 ? StateKey(dest, f) : RecordKey(dest, f, ef, i, t), kv.second);
	}
	dirty = notes != savedNotes;
	event("pattern move: " + std::to_string(mapping.size()));
}
