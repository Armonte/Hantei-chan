// See hmod_track.h.
#include "hmod_track.h"
#include "hmod_ha6.h"
#include "../pcmod/pcmod_package.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace hmod {

const char* StatusName(Status s)
{
	switch (s) {
	case Status::Unchanged: return "unchanged";
	case Status::Reencoded: return "re-encoded";
	case Status::Changed: return "changed";
	case Status::Added: return "added";
	case Status::Orphan: return "orphan";
	case Status::Clutter: return "clutter";
	}
	return "?";
}

static bool EndsWith(const std::string& s, const char* suf) { const size_t n = std::strlen(suf); return s.size() >= n && s.compare(s.size() - n, n, suf) == 0; }
static bool StartsWith(const std::string& s, const char* pre) { return s.rfind(pre, 0) == 0; }

std::string ClutterReason(const std::string& norm)
{
	const size_t slash = norm.find_last_of('\\');
	const std::string leaf = slash == std::string::npos ? norm : norm.substr(slash + 1);
	if (leaf == "thumbs.db" || leaf == "desktop.ini" || leaf == ".ds_store") return "OS metadata";
	if (leaf == "hantei4.inf" || leaf == "hanteichan.ini" || StartsWith(leaf, "lastdata") || StartsWith(leaf, "lastload")) return "editor cache / state";
	if (EndsWith(leaf, ".dmp") || EndsWith(leaf, ".crash.dmp")) return "crash dump";
	if (leaf.find(" - copy") != std::string::npos || EndsWith(leaf, ".bak") || EndsWith(leaf, ".orig") || EndsWith(leaf, "~")) return "backup copy";
	if (StartsWith(norm, "system\\_") || leaf == "_app.ini" || leaf == "_keyconfig.ini" || leaf == "concerto.ini" || leaf == "results.csv" || leaf == "p1name.txt")
		return "user state";
	if (StartsWith(norm, "cccaster") || StartsWith(norm, "replayvs\\") || StartsWith(norm, "hmod_")) return "tool / user data";
	if (EndsWith(leaf, ".exe") || EndsWith(leaf, ".dll") || EndsWith(leaf, ".bat") || EndsWith(leaf, ".cmd") || EndsWith(leaf, ".ps1") || EndsWith(leaf, ".sh") || EndsWith(leaf, ".ttf"))
		return "executable / foreign type: never ships";
	if (EndsWith(leaf, ".zip") || EndsWith(leaf, ".7z") || EndsWith(leaf, ".rar")) return "archive inside the tree";
	return {};
}

static std::string Lower(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a'); return s; }

// Text equal after CRLF/LF and trailing-space/blank-line normalisation.
static std::string NormText(const std::vector<uint8_t>& b)
{
	std::string out, line;
	for (size_t i = 0; i <= b.size(); ++i) {
		const char c = i < b.size() ? (char)b[i] : '\n';
		if (c == '\n') {
			while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
			out += line; out += '\n'; line.clear();
		} else line += c;
	}
	while (out.size() >= 2 && out[out.size() - 1] == '\n' && out[out.size() - 2] == '\n') out.pop_back();
	return out;
}

static bool IsTextExt(const std::string& norm)
{
	return EndsWith(norm, ".txt") || EndsWith(norm, ".ini") || EndsWith(norm, ".csv") || EndsWith(norm, ".json");
}

static bool GlobAny(const std::vector<std::string>& pats, const std::string& norm)
{
	for (const std::string& p : pats) if (GlobMatch(p, norm)) return true;
	return false;
}

std::vector<TrackedFile> TrackAsset(const Project& p, const Asset& a, const VanillaIndex* vanilla)
{
	std::vector<TrackedFile> out;
	std::error_code ec;
	const fs::path root = fs::u8path(p.AssetDir(a.id));
	if (!fs::is_directory(root, ec)) return out;
	for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
		if (!it->is_regular_file(ec)) continue;
		TrackedFile t;
		t.asset = a.id;
		std::string rel = it->path().lexically_relative(root).u8string();
		for (char& c : rel) if (c == '/') c = '\\';
		t.rel = rel;
		t.norm = NormKey(rel);
		t.abs = it->path().u8string();
		pcmod::FileEntry fe;
		std::string why;
		if (!pcmod::HashFile(t.abs, fe, why)) { t.status = Status::Orphan; t.note = "unreadable: " + why; out.push_back(t); continue; }
		t.sha256 = fe.sha256; t.size = fe.size;

		if (const std::string cl = ClutterReason(t.norm); !cl.empty()) { t.status = Status::Clutter; t.note = cl; out.push_back(t); continue; }
		if (!a.targets.empty() && !GlobAny(a.targets, t.norm)) { t.status = Status::Orphan; t.note = "matches none of the asset's targets"; out.push_back(t); continue; }

		const VanillaEntry* v = vanilla ? vanilla->Find(t.norm) : nullptr;
		if (!vanilla || vanilla->empty()) { t.status = Status::Changed; t.note = "vanilla index not built: assumed changed"; out.push_back(t); continue; }
		if (!v) { t.status = Status::Added; t.note = "not in vanilla"; out.push_back(t); continue; }
		t.vanillaPack = v->pack; t.vanillaSha = v->sha256;
		if (v->sha256 == t.sha256) { t.status = Status::Unchanged; t.note = "identical to " + v->pack; out.push_back(t); continue; }

		t.status = Status::Changed;
		if (t.size <= (64u << 20)) {
			std::vector<uint8_t> mine, base;
			if (pcmod::ReadAll(t.abs, mine) && vanilla->ReadBytes(t.norm, base)) {
				if (IsHa6(mine) && IsHa6(base)) {
					Ha6Slots sm, sb;
					std::string e1, e2;
					if (SplitHa6(mine, sm, e1) && SplitHa6(base, sb, e2)) {
						const SlotDiffSummary d = DiffSlots(sb, sm);
						t.slotsClaimed = d.claimed.size(); t.slotsNameOnly = d.nameOnly;
						if (d.claimed.empty()) {
							t.status = Status::Reencoded;
							t.note = d.nameOnly ? std::to_string(d.nameOnly) + " name-only slot change(s), no behaviour change" : "same slots after canonicalisation";
						} else {
							t.note = std::to_string(d.claimed.size()) + " slot(s): " + std::to_string(d.filled) + " fill, " + std::to_string(d.modified) + " modify, " + std::to_string(d.cleared) + " clear";
							if (d.nameOnly) t.note += ", " + std::to_string(d.nameOnly) + " name-only";
						}
					} else t.note = "HA6 not comparable (" + (e1.empty() ? e2 : e1) + ")";
				} else if (IsTextExt(t.norm) && NormText(mine) == NormText(base)) {
					t.status = Status::Reencoded; t.note = "line endings / trailing whitespace only";
				} else t.note = "bytes differ from " + v->pack;
			} else t.note = "bytes differ from " + v->pack + " (vanilla bytes unavailable)";
		} else t.note = "bytes differ from " + v->pack;
		out.push_back(std::move(t));
	}
	std::sort(out.begin(), out.end(), [](const TrackedFile& x, const TrackedFile& y) { return x.norm < y.norm; });
	return out;
}

std::vector<TrackedFile> TrackProject(const Project& p, const VanillaIndex* vanilla)
{
	std::vector<TrackedFile> all;
	for (const Asset& a : p.assets) {
		auto t = TrackAsset(p, a, vanilla);
		all.insert(all.end(), std::make_move_iterator(t.begin()), std::make_move_iterator(t.end()));
	}
	return all;
}

size_t FillBases(Project& p, const std::vector<TrackedFile>& tracked)
{
	size_t changed = 0;
	for (Asset& a : p.assets) {
		if (a.origin.kind != "vanilla-derived") continue;
		std::vector<BaseRef> bases;
		for (const TrackedFile& t : tracked)
			if (t.asset == a.id && (t.status == Status::Changed || t.status == Status::Reencoded) && !t.vanillaSha.empty()) bases.push_back({t.rel, t.vanillaPack, t.vanillaSha});
		if (bases.size() != a.origin.base.size() || !std::equal(bases.begin(), bases.end(), a.origin.base.begin(), [](const BaseRef& x, const BaseRef& y) { return x.path == y.path && x.pack == y.pack && x.sha256 == y.sha256; })) {
			a.origin.base = std::move(bases);
			++changed;
		}
	}
	return changed;
}

StatusCounts Count(const std::vector<TrackedFile>& t)
{
	StatusCounts c;
	for (const TrackedFile& f : t) switch (f.status) {
		case Status::Unchanged: ++c.unchanged; break;
		case Status::Reencoded: ++c.reencoded; break;
		case Status::Changed: ++c.changed; break;
		case Status::Added: ++c.added; break;
		case Status::Orphan: ++c.orphan; break;
		case Status::Clutter: ++c.clutter; break;
	}
	return c;
}

} // namespace hmod
