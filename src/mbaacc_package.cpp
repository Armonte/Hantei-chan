#include "mbaacc_package.h"
#include "mbaacc_pack.h"

#include "framedata.h"
#include "../third_party/json/json.hpp"

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace mbpackage {

namespace {

std::wstring W(const std::string& utf8)
{
	if (utf8.empty()) return std::wstring();
	const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
	std::wstring w(n > 0 ? n : 0, L'\0');
	if (n > 0) MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), w.data(), n);
	return w;
}

std::string U(const std::wstring& w)
{
	if (w.empty()) return std::string();
	const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string s(n > 0 ? n : 0, '\0');
	if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
	return s;
}

std::string U(const fs::path& p) { return U(p.wstring()); }

// Descriptor values are CP932 (Shift-JIS) bytes; show them as UTF-8 and turn
// them into UTF-16 file names.
std::wstring FromSjis(const std::string& bytes)
{
	if (bytes.empty()) return std::wstring();
	const int n = MultiByteToWideChar(932, 0, bytes.data(), (int)bytes.size(), nullptr, 0);
	std::wstring w(n > 0 ? n : 0, L'\0');
	if (n > 0) MultiByteToWideChar(932, 0, bytes.data(), (int)bytes.size(), w.data(), n);
	return w;
}

// A narrow path FrameData::load (CreateFileA) can open: the ANSI spelling if
// it is lossless, else the 8.3 short name.
std::string LoaderPath(const fs::path& p)
{
	const std::wstring w = p.wstring();
	BOOL lossy = FALSE;
	const int n = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, w.c_str(), -1, nullptr, 0, nullptr, &lossy);
	if (n > 0 && !lossy) {
		std::string s(n, '\0');
		WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, w.c_str(), -1, s.data(), n, nullptr, nullptr);
		s.pop_back();
		return s;
	}
	wchar_t shortPath[MAX_PATH * 2];
	if (GetShortPathNameW(w.c_str(), shortPath, MAX_PATH * 2)) {
		std::string s(MAX_PATH * 2, '\0');
		const int m = WideCharToMultiByte(CP_ACP, 0, shortPath, -1, s.data(), (int)s.size(), nullptr, nullptr);
		s.resize(m > 0 ? m - 1 : 0);
		return s;
	}
	return std::string();
}

std::string Trim(std::string v)
{
	auto sp = [](unsigned char c) { return std::isspace(c) != 0; };
	v.erase(v.begin(), std::find_if(v.begin(), v.end(), [&](unsigned char c) { return !sp(c); }));
	v.erase(std::find_if(v.rbegin(), v.rend(), [&](unsigned char c) { return !sp(c); }).base(), v.end());
	return v;
}

std::string Lower(std::string v)
{
	for (char& c : v) c = (char)std::tolower((unsigned char)c);
	return v;
}

bool ReadBytes(const fs::path& path, std::vector<char>& bytes)
{
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f) return false;
	const auto size = f.tellg();
	if (size < 0) return false;
	bytes.resize((size_t)size);
	f.seekg(0);
	return bytes.empty() || (bool)f.read(bytes.data(), size);
}

// temp file + MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH)
bool WriteAtomic(const fs::path& dest, const std::vector<char>& bytes)
{
	const std::wstring tmp = dest.wstring() + L".hantei-tmp";
	HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE) return false;
	DWORD written = 0;
	BOOL ok = bytes.empty() || (WriteFile(h, bytes.data(), (DWORD)bytes.size(), &written, nullptr) && written == bytes.size());
	ok = ok && FlushFileBuffers(h);
	CloseHandle(h);
	if (ok) ok = MoveFileExW(tmp.c_str(), dest.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
	if (!ok) DeleteFileW(tmp.c_str());
	return ok != FALSE;
}

std::string Timestamp()
{
	const std::time_t t = std::time(nullptr);
	std::tm local{};
	localtime_s(&local, &t);
	char buf[32];
	std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &local);
	return buf;
}

// "<base><timestamp>[_N]<suffix>" that does not exist yet.
fs::path UniquePath(const fs::path& dir, const std::wstring& base, const std::wstring& suffix)
{
	const std::wstring stamp = W(Timestamp());
	std::error_code ec;
	fs::path candidate = dir / (base + stamp + suffix);
	for (int n = 2; fs::exists(candidate, ec); ++n)
		candidate = dir / (base + stamp + L"_" + std::to_wstring(n) + suffix);
	return candidate;
}

struct Descriptor {
	// section (lower case) -> key (lower case) -> raw value bytes (CP932)
	std::map<std::string, std::map<std::string, std::string>> sections;
	std::map<std::string, std::string> sectionNames;   // lower -> as written
	std::vector<std::string> errors;
};

Descriptor Parse(const std::string& text)
{
	Descriptor d;
	std::istringstream in(text);
	std::string line, section;
	int lineNo = 0;
	while (std::getline(in, line)) {
		++lineNo;
		if (!line.empty() && line.back() == '\r') line.pop_back();
		line = Trim(line);
		if (line.empty() || line[0] == ';' || line.rfind("//", 0) == 0) continue;
		if (line.front() == '[' && line.back() == ']') {
			const std::string name = Trim(line.substr(1, line.size() - 2));
			section = Lower(name);
			d.sectionNames.emplace(section, name);
			d.sections[section];
			continue;
		}
		const size_t eq = line.find('=');
		if (eq == std::string::npos) {
			d.errors.push_back("Line " + std::to_string(lineNo) + " is not a section, comment or key=value row.");
			continue;
		}
		if (section.empty()) {
			d.errors.push_back("Line " + std::to_string(lineNo) + " appears before any [Section].");
			continue;
		}
		const std::string key = Lower(Trim(line.substr(0, eq)));
		if (key.empty()) { d.errors.push_back("Line " + std::to_string(lineNo) + " has an empty key."); continue; }
		if (!d.sections[section].emplace(key, Trim(line.substr(eq + 1))).second)
			d.errors.push_back("Duplicate [" + section + "] key " + key + ".");
	}
	return d;
}

std::string FileKey(int i)
{
	char buf[16];
	snprintf(buf, sizeof(buf), "file%02d", i);
	return buf;
}

bool HasMagic(const std::vector<uint8_t>& bytes, const char* magic)
{
	const size_t n = strlen(magic);
	return bytes.size() >= n && memcmp(bytes.data(), magic, n) == 0;
}

struct PackSet {
	std::vector<std::unique_ptr<mbpack::Archive>> archives;
	std::vector<std::string> openErrors;
	void load(const fs::path& gameDir)
	{
		for (const std::wstring& p : mbpack::FindPacks(gameDir.wstring())) {
			auto a = std::make_unique<mbpack::Archive>();
			std::string err;
			if (a->open(p, err)) archives.push_back(std::move(a));
			else openErrors.push_back(U(fs::path(p).filename()) + ": " + err);
		}
	}
	// Last archive wins, matching a numbered overlay order.
	std::pair<const mbpack::Archive*, const mbpack::Entry*> find(const std::string& rel) const
	{
		for (auto it = archives.rbegin(); it != archives.rend(); ++it)
			if (const mbpack::Entry* e = (*it)->find(rel)) return {it->get(), e};
		return {nullptr, nullptr};
	}
};

struct SectionSpec { const char* key; const char* label; const char* magic; int minCount; };
const SectionSpec kSections[] = {
	{"datafile", "HA6", "Hantei6DataFile", 1},
	{"bmpcutfile", "CG", "BMP Cutter", 1},        // "BMP Cutter3" (Cutter2 in MBAC)
	{"panifile", "PAT", "PAniDataFile", 0},       // FileNum=0: no PAT (vanilla)
};

void ValidateSection(const Descriptor& d, const SectionSpec& spec, const fs::path& folder,
	const fs::path& dataRel, const PackSet* packs, ValidationResult& r)
{
	auto sec = d.sections.find(spec.key);
	const std::string shown = d.sectionNames.count(spec.key) ? d.sectionNames.at(spec.key) : std::string(spec.key);
	if (sec == d.sections.end()) {
		if (spec.minCount > 0) r.errors.push_back("Missing [" + shown + "] section.");
		return;
	}
	int count = -1;
	auto cnt = sec->second.find("filenum");
	if (cnt == sec->second.end()) {
		r.errors.push_back("[" + shown + "] has no FileNum.");
		return;
	}
	try {
		size_t used = 0;
		count = std::stoi(cnt->second, &used, 10);
		if (used != cnt->second.size()) count = -1;
	} catch (...) { count = -1; }
	if (count < spec.minCount || count > 99) {
		r.errors.push_back("[" + shown + "] FileNum must be an integer from " + std::to_string(spec.minCount) + " to 99.");
		return;
	}
	for (int i = 0; i < count; ++i) {
		const std::string key = FileKey(i);
		auto row = sec->second.find(key);
		RowReport rep;
		rep.section = shown;
		rep.key = "File" + key.substr(4);
		if (row == sec->second.end() || row->second.empty()) {
			r.errors.push_back("[" + shown + "] is missing " + rep.key + " required by FileNum=" + std::to_string(count) + ".");
			continue;
		}
		const std::wstring wname = FromSjis(row->second);
		rep.file = U(wname);
		const fs::path rel(wname);
		if (rel.is_absolute() || rel.has_parent_path()) {
			r.errors.push_back("[" + shown + "] " + rep.key + " must be a file name beside the descriptor: " + rep.file);
			continue;
		}
		const fs::path full = folder / rel;
		std::error_code ec;
		if (fs::is_regular_file(full, ec)) {
			std::vector<char> head;
			std::ifstream f(full, std::ios::binary);
			std::vector<uint8_t> probe(16, 0);
			f.read((char*)probe.data(), 16);
			probe.resize((size_t)f.gcount());
			rep.where = U(full);
			rep.status = HasMagic(probe, spec.magic) ? RowStatus::loose : RowStatus::badHeader;
		} else if (packs) {
			// e.g. "data/akiha.HA6" inside 0002.p
			const std::string relPath = U((dataRel / rel).wstring());
			auto [archive, entry] = packs->find(relPath);
			if (archive && entry) {
				std::vector<uint8_t> probe;
				// The first 4 KiB decrypt the same in PC and Steam mode.
				archive->read(*entry, probe, mbpack::CryptMode::pc, 16);
				rep.where = U(fs::path(archive->path()).filename()) + ": " + entry->relativePath();
				rep.status = HasMagic(probe, spec.magic) ? RowStatus::packed : RowStatus::badHeader;
			}
		}
		if (rep.status == RowStatus::missing)
			r.errors.push_back(std::string(spec.label) + " file does not exist: " + U(full) +
				(packs ? " (not in the game archives either)" : ""));
		else if (rep.status == RowStatus::badHeader)
			r.errors.push_back(std::string(spec.label) + " has an invalid header: " + rep.where);
		r.rows.push_back(rep);
	}
	for (const auto& kv : sec->second) {
		if (kv.first.rfind("file", 0) != 0 || kv.first == "filenum") continue;
		try {
			if (std::stoi(kv.first.substr(4)) >= count)
				r.errors.push_back("[" + shown + "] contains undeclared " + kv.first + " while FileNum=" + std::to_string(count) + ".");
		} catch (...) {
			r.errors.push_back("[" + shown + "] has a malformed file key: " + kv.first + ".");
		}
	}
}

std::string BuildReport(const fs::path& descriptor, const ValidationResult& r)
{
	std::ostringstream out;
	out << "MBAACC character package\n" << U(descriptor) << "\n\n";
	out << "Descriptor line endings: " << r.crlf << " CRLF, " << r.loneLf << " lone LF";
	if (r.loneCr) out << ", " << r.loneCr << " lone CR";
	out << "\n";
	for (const auto& row : r.rows)
		out << "  [" << row.section << "] " << row.key << " = " << row.file << "  -> "
		    << RowStatusName(row.status) << (row.where.empty() ? "" : " (" + row.where + ")") << "\n";
	for (const auto& n : r.notes) out << "note: " << n << "\n";
	if (r.errors.empty()) {
		out << "\nPASS: descriptor rows and referenced HA6/CG/PAT files are valid.";
	} else {
		out << "\nFAILED (" << r.errors.size() << " issue" << (r.errors.size() == 1 ? "" : "s") << "):\n";
		for (const auto& e : r.errors) out << "- " << e << "\n";
	}
	return out.str();
}

fs::path DefaultGameDir(const fs::path& descriptor)
{
	// <game>\data\chara.txt -> <game>
	return descriptor.parent_path().parent_path();
}

} // namespace

const char* RowStatusName(RowStatus s)
{
	switch (s) {
	case RowStatus::loose: return "ok";
	case RowStatus::packed: return "packed";
	case RowStatus::badHeader: return "BAD HEADER";
	default: return "MISSING";
	}
}

ValidationResult ValidateCharacterPackage(const std::string& descriptorUtf8, const ValidateOptions& options)
{
	ValidationResult r;
	std::error_code ec;
	const fs::path descriptor = fs::absolute(fs::path(W(descriptorUtf8)), ec);
	std::vector<char> bytes;
	if (!fs::is_regular_file(descriptor, ec) || !ReadBytes(descriptor, bytes)) {
		r.errors.push_back("Descriptor does not exist or could not be read.");
		r.report = "Descriptor does not exist or could not be read:\n" + descriptorUtf8;
		return r;
	}
	for (size_t i = 0; i < bytes.size(); ++i) {
		if (bytes[i] == '\n') { if (i > 0 && bytes[i - 1] == '\r') ++r.crlf; else ++r.loneLf; }
		else if (bytes[i] == '\r' && (i + 1 >= bytes.size() || bytes[i + 1] != '\n')) ++r.loneCr;
	}
	r.newlineRepairAvailable = r.loneLf || r.loneCr;
	if (r.newlineRepairAvailable)
		r.errors.push_back("Descriptor mixes line endings. MBAACC requires CRLF; a lone LF can crash character initialisation.");

	const Descriptor d = Parse(std::string(bytes.begin(), bytes.end()));
	r.errors.insert(r.errors.end(), d.errors.begin(), d.errors.end());

	PackSet packs;
	const PackSet* packsPtr = nullptr;
	fs::path dataRel = descriptor.parent_path().filename();   // "data"
	if (options.searchPacks) {
		const fs::path gameDir = options.gameDir.empty() ? DefaultGameDir(descriptor) : fs::path(W(options.gameDir));
		packs.load(gameDir);
		for (const auto& e : packs.openErrors) r.notes.push_back("archive skipped: " + e);
		if (!packs.archives.empty()) {
			packsPtr = &packs;
			r.notes.push_back(std::to_string(packs.archives.size()) + " game archive(s) searched in " + U(gameDir));
		}
	}
	for (const SectionSpec& spec : kSections)
		ValidateSection(d, spec, descriptor.parent_path(), dataRel, packsPtr, r);

	r.valid = r.errors.empty();
	r.report = BuildReport(descriptor, r);
	return r;
}

ActionResult RepairDescriptorNewlines(const std::string& descriptorUtf8)
{
	ActionResult result;
	std::error_code ec;
	const fs::path descriptor = fs::absolute(fs::path(W(descriptorUtf8)), ec);
	std::vector<char> bytes;
	if (!fs::is_regular_file(descriptor, ec) || !ReadBytes(descriptor, bytes)) {
		result.message = "Descriptor does not exist or could not be read.";
		return result;
	}
	std::vector<char> normalized;
	normalized.reserve(bytes.size() + 16);
	for (size_t i = 0; i < bytes.size(); ++i) {
		if (bytes[i] == '\r') {
			if (i + 1 < bytes.size() && bytes[i + 1] == '\n') continue;   // the LF writes the pair
			normalized.push_back('\r'); normalized.push_back('\n');
		} else if (bytes[i] == '\n') {
			normalized.push_back('\r'); normalized.push_back('\n');
		} else {
			normalized.push_back(bytes[i]);
		}
	}
	if (normalized == bytes) {
		result.success = true;
		result.message = "The descriptor already uses CRLF everywhere; nothing was changed.";
		return result;
	}
	const fs::path backup = UniquePath(descriptor.parent_path(), descriptor.filename().wstring() + L".pre-crlf-repair_", L".bak");
	if (!fs::copy_file(descriptor, backup, fs::copy_options::none, ec) || ec) {
		result.message = "Could not create the backup; the descriptor was not changed.";
		return result;
	}
	result.backup = U(backup);
	if (!WriteAtomic(descriptor, normalized)) {
		result.message = "Could not install the repaired descriptor. The original is unchanged; backup: " + result.backup;
		return result;
	}
	const ValidationResult v = ValidateCharacterPackage(U(descriptor));
	result.success = true;
	result.message = v.report + "\n\nOriginal descriptor backup:\n" + result.backup;
	return result;
}

namespace {

fs::path NotesPath(const fs::path& ha6) { return fs::path(ha6.wstring() + L".notes"); }
fs::path LegacyNotesPath(const fs::path& ha6) { return fs::path(ha6.wstring() + L".gonpnotes.json"); }

bool HasEveryPattern(const fs::path& top, const std::vector<fs::path>& lower, std::string& why)
{
	FrameData complete;
	const std::string topPath = LoaderPath(top);
	if (topPath.empty() || !complete.load(topPath.c_str())) {
		why = "The highest HA6 layer could not be parsed: " + U(top.filename());
		return false;
	}
	for (const auto& path : lower) {
		FrameData older;
		const std::string p = LoaderPath(path);
		if (p.empty() || !older.load(p.c_str())) {
			why = "An older HA6 layer could not be parsed: " + U(path.filename());
			return false;
		}
		for (int id = 0; id < (int)older.get_sequence_count(); ++id) {
			auto* oldSeq = older.get_sequence(id);
			if (!oldSeq || oldSeq->frames.empty()) continue;
			auto* newSeq = complete.get_sequence(id);
			if (!newSeq || newSeq->frames.empty()) {
				why = "Pattern " + std::to_string(id) + " exists only in " + U(path.filename()) +
					". This character needs a real merge, so no files were changed.";
				return false;
			}
		}
	}
	return true;
}

std::string RewriteDataFileSection(const std::string& input, const std::string& mainNameSjis)
{
	std::istringstream lines(input);
	std::ostringstream out;
	std::string line;
	bool inDataFile = false, wrote = false;
	auto rows = [&] { out << "FileNum= 1\r\nFile00=" << mainNameSjis << "\r\n"; wrote = true; };
	while (std::getline(lines, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		const std::string t = Trim(line);
		if (!t.empty() && t.front() == '[') {
			if (inDataFile && !wrote) rows();
			inDataFile = Lower(t) == "[datafile]";
		}
		const std::string lk = Lower(t);
		if (inDataFile && (lk.rfind("filenum", 0) == 0 || (lk.rfind("file", 0) == 0 && lk.size() > 4 && std::isdigit((unsigned char)lk[4])))) {
			if (!wrote) rows();
			continue;
		}
		out << line << "\r\n";
	}
	if (inDataFile && !wrote) rows();
	return out.str();
}

bool MergeNotes(const std::vector<fs::path>& layers, const fs::path& main, std::vector<char>& out, int& sources, std::string& why)
{
	json merged = {{"version", 1}, {"asset", U(main.filename())}, {"effect_notes_expanded", false},
		{"condition_notes_expanded", false}, {"notes", json::object()}};
	sources = 0;
	try {
		for (const auto& layer : layers) {   // low -> high priority; later wins
			fs::path sidecar = NotesPath(layer);
			std::error_code ec;
			if (!fs::is_regular_file(sidecar, ec)) sidecar = LegacyNotesPath(layer);
			if (!fs::is_regular_file(sidecar, ec)) continue;
			std::ifstream in(sidecar, std::ios::binary);
			json root;
			in >> root;
			if (!root.is_object()) throw std::runtime_error("root is not an object");
			const bool shared = root.value("notes_expanded", false);
			merged["effect_notes_expanded"] = root.value("effect_notes_expanded", shared);
			merged["condition_notes_expanded"] = root.value("condition_notes_expanded", shared);
			if (root.contains("notes")) {
				if (!root["notes"].is_object()) throw std::runtime_error("notes is not an object");
				for (auto it = root["notes"].begin(); it != root["notes"].end(); ++it)
					if (it.value().is_string()) merged["notes"][it.key()] = it.value();
			}
			++sources;
		}
	} catch (const std::exception& e) {
		why = std::string("Could not merge the layer notes safely (") + e.what() + "). Nothing was changed.";
		return false;
	}
	if (sources) {
		const std::string text = merged.dump(2) + "\n";
		out.assign(text.begin(), text.end());
	}
	return true;
}

} // namespace

ActionResult ConsolidateHa6Layers(const std::string& descriptorUtf8, const std::string& mainFileName, bool mergeNotes)
{
	ActionResult result;
	try {
		std::error_code ec;
		const fs::path descriptor = fs::absolute(fs::path(W(descriptorUtf8)), ec);
		const ValidationResult before = ValidateCharacterPackage(U(descriptor), ValidateOptions{false, {}});
		if (!before.valid) {
			result.message = "The package must pass validation (loose files only) before consolidation.\n\n" + before.report;
			return result;
		}
		std::vector<fs::path> layers;
		for (const auto& row : before.rows)
			if (Lower(row.section) == "datafile") layers.push_back(fs::absolute(fs::path(W(row.where)), ec));
		if (layers.size() < 2) {
			result.message = "This character already uses one HA6. Nothing needs consolidation.";
			return result;
		}
		// MBAACC loads the rows in order; the highest index is the top layer.
		const fs::path top = layers.back();
		std::string why;
		if (!HasEveryPattern(top, std::vector<fs::path>(layers.begin(), layers.end() - 1), why)) {
			result.message = why;
			return result;
		}

		const std::wstring mainName = mainFileName.empty() ? descriptor.stem().wstring() + L".HA6" : W(mainFileName);
		const fs::path mainRel(mainName);
		std::wstring ext = mainRel.extension().wstring();
		for (auto& c : ext) c = (wchar_t)towlower(c);
		if (mainName.empty() || mainRel.has_parent_path() || ext != L".ha6") {
			result.message = "The canonical name must be a plain .HA6 file name (no folders).";
			return result;
		}
		const fs::path mainPath = descriptor.parent_path() / mainRel;
		auto isLayer = [&](const fs::path& p) {
			for (const auto& l : layers) if (fs::equivalent(l, p, ec)) return true;
			return false;
		};
		if (fs::exists(mainPath, ec) && !isLayer(mainPath)) {
			result.message = "The chosen output name belongs to an unrelated file. Nothing was changed.";
			return result;
		}
		// The descriptor stores CP932 names.
		std::string mainSjis;
		{
			const int n = WideCharToMultiByte(932, 0, mainName.c_str(), (int)mainName.size(), nullptr, 0, nullptr, nullptr);
			mainSjis.resize(n > 0 ? n : 0);
			if (n > 0) WideCharToMultiByte(932, 0, mainName.c_str(), (int)mainName.size(), mainSjis.data(), n, nullptr, nullptr);
		}

		std::vector<char> notesBytes;
		int noteSources = 0;
		if (mergeNotes && !MergeNotes(layers, mainPath, notesBytes, noteSources, why)) {
			result.message = why;
			return result;
		}

		std::vector<char> topBytes, descriptorBytes;
		if (!ReadBytes(top, topBytes) || topBytes.size() < 36 || memcmp(topBytes.data(), "Hantei6DataFile", 15) != 0 ||
		    memcmp(topBytes.data() + 32, "_STR", 4) != 0) {
			result.message = "The highest layer is not a supported HA6 root. Nothing was changed.";
			return result;
		}
		if (!ReadBytes(descriptor, descriptorBytes)) {
			result.message = "Could not read the descriptor.";
			return result;
		}

		// Backup first: descriptor, every layer and their note sidecars.
		const fs::path backupDir = UniquePath(descriptor.parent_path(), L"ha6-consolidation-backup_", L"");
		fs::create_directories(backupDir, ec);
		if (ec) { result.message = "Could not create the backup folder. Nothing was changed."; return result; }
		auto backup = [&](const fs::path& p) {
			std::error_code probe, copyErr;
			if (!fs::is_regular_file(p, probe)) return true;   // nothing to back up
			fs::copy_file(p, backupDir / p.filename(), fs::copy_options::overwrite_existing, copyErr);
			return !copyErr;
		};
		bool backedUp = backup(descriptor);
		for (const auto& l : layers) backedUp = backup(l) && backup(NotesPath(l)) && backup(LegacyNotesPath(l)) && backedUp;
		result.backup = U(backupDir);
		if (!backedUp) { result.message = "The backup is incomplete; nothing was changed. Backup folder: " + result.backup; return result; }

		const std::string rewritten = RewriteDataFileSection(std::string(descriptorBytes.begin(), descriptorBytes.end()), mainSjis);
		if (!WriteAtomic(mainPath, topBytes) ||
		    (noteSources > 0 && !WriteAtomic(NotesPath(mainPath), notesBytes)) ||
		    !WriteAtomic(descriptor, std::vector<char>(rewritten.begin(), rewritten.end()))) {
			// Put the descriptor back; the old layers are untouched.
			fs::copy_file(backupDir / descriptor.filename(), descriptor, fs::copy_options::overwrite_existing, ec);
			result.message = "Could not install every file. The descriptor was restored; backup: " + result.backup;
			return result;
		}
		const ValidationResult after = ValidateCharacterPackage(U(descriptor), ValidateOptions{false, {}});
		if (!after.valid) {
			fs::copy_file(backupDir / descriptor.filename(), descriptor, fs::copy_options::overwrite_existing, ec);
			result.message = "The consolidated package failed validation. The original descriptor was restored and the old "
				"layers were kept.\n\n" + after.report + "\n\nBackup: " + result.backup;
			return result;
		}
		// The backup is the rollback copy; remove superseded live layers.
		for (const auto& l : layers) {
			for (const fs::path& n : {NotesPath(l), LegacyNotesPath(l)})
				if (!(noteSources > 0 && n == NotesPath(mainPath))) fs::remove(n, ec);
			if (!fs::equivalent(l, mainPath, ec)) fs::remove(l, ec);
		}
		result.success = true;
		result.message = "Consolidation complete.\n\nMain HA6: " + U(mainPath.filename()) +
			(mergeNotes ? "\nMerged note layers: " + std::to_string(noteSources) : std::string("\nNotes: archived only")) +
			"\nOld layers archived in: " + result.backup + "\n\nReload the character from its descriptor before editing.";
		return result;
	} catch (const std::exception& e) {
		result.message = std::string("Consolidation failed safely: ") + e.what();
		return result;
	}
}

ActionResult ExtractPackedRows(const std::string& descriptorUtf8, const ValidateOptions& options)
{
	ActionResult result;
	std::error_code ec;
	const fs::path descriptor = fs::absolute(fs::path(W(descriptorUtf8)), ec);
	const fs::path gameDir = options.gameDir.empty() ? DefaultGameDir(descriptor) : fs::path(W(options.gameDir));
	PackSet packs;
	packs.load(gameDir);
	if (packs.archives.empty()) { result.message = "No game archives (*.p) in " + U(gameDir); return result; }
	const ValidationResult v = ValidateCharacterPackage(descriptorUtf8, options);
	std::map<const mbpack::Archive*, mbpack::CryptMode> modes;
	int written = 0;
	std::ostringstream log;
	for (const auto& row : v.rows) {
		if (row.status != RowStatus::packed) continue;
		const fs::path dest = descriptor.parent_path() / fs::path(W(row.file));
		if (fs::exists(dest, ec)) continue;
		const std::string rel = U((descriptor.parent_path().filename() / fs::path(W(row.file))).wstring());
		auto [archive, entry] = packs.find(rel);
		if (!archive) continue;
		if (!modes.count(archive)) modes[archive] = archive->detectMode();
		std::vector<uint8_t> bytes;
		if (!archive->read(*entry, bytes, modes[archive])) { log << "read failed: " << rel << "\n"; continue; }
		if (!WriteAtomic(dest, std::vector<char>(bytes.begin(), bytes.end()))) { log << "write failed: " << U(dest) << "\n"; continue; }
		log << "extracted " << row.file << " from " << U(fs::path(archive->path()).filename()) << " ("
		    << mbpack::CryptModeName(modes[archive]) << " cipher)\n";
		++written;
	}
	result.success = true;
	result.message = (written ? log.str() : std::string("No packed-only rows to extract.\n")) +
		"\n" + ValidateCharacterPackage(descriptorUtf8, options).report;
	return result;
}

} // namespace mbpackage
