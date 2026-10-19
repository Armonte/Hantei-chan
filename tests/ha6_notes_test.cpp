#include "framedata.h"
#include "ha6_notes_undo.h"
#include "pattern_refs.h"
#include "../third_party/json/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>

namespace fs = std::filesystem;
using json = nlohmann::json;
static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " << #x << '\n'; ++failures; } } while (0)
static std::string Read(const fs::path& p) { std::ifstream f(p, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
static void Write(const fs::path& p, const std::string& s) { std::ofstream(p, std::ios::binary) << s; }
static std::vector<Sequence> Doc() {
	std::vector<Sequence> doc(3);
	for (auto& s : doc) {
		s.empty = false; s.initialized = true; s.frames.resize(3);
		for (auto& f : s.frames) {
			f.EF.resize(3); f.IF.resize(3);
			for (auto& ef : f.EF) ef.type = 101;
			for (auto& cond : f.IF) cond.type = 25;
		}
	}
	return doc;
}
static void Synthetic(const fs::path& root) {
	auto doc = Doc(); Ha6Notes n; std::string error;
	int p, f, i, t; bool ef;
	CHECK(Ha6Notes::ParseKey("p1.f2", &p, &f, &ef, &i, &t) && p == 1 && f == 2 && i == -1);
	for (const auto* key : {"p1.f2.ii0.t25", "p1.f2.if0.t25junk", "p-1", "p1.f-1", "p1.f2.if-1.t25", "p1junk"})
		CHECK(!Ha6Notes::ParseKey(key, &p, &f, &ef, &i, &t));
	const std::string ha6 = (root / "character.HA6").string();
	const std::string longText(5000, 'x');
	json source = {{"version", 1}, {"notes", {{"pattern/0", "pattern"}, {"pattern/0/frame/1", "state"},
		{"pattern/0/frame/1/effect/1", longText}, {"pattern/0/frame/1/condition/2", "condition"}, {"pattern/99/frame/9", "orphan"}}}};
	const auto original = source.dump(); Write(ha6 + ".notes", original);
	CHECK(n.loadForHa6(ha6, doc, &error));
	CHECK(n.notes.size() == 5 && !n.dirty);
	CHECK(n.get("p0.f1.ef1.t101") && *n.get("p0.f1.ef1.t101") == longText);
	CHECK(n.get("p0.f1.if2.t25") && n.get("p0.f1") && n.get("legacy:pattern/99/frame/9"));
	CHECK(!fs::exists(Ha6Notes::PathFor(ha6))); // Load does not migrate on disk.
	Ha6NotesUndoChannel channel(&n); UndoManager undo; undo.attach(&doc); undo.addChannel(&channel); undo.ensureBaseline();
	const auto baseline = n.notes;
	undo.beginTransaction("insert state");
	n.insert(0, -1, false, 1, 1, (int)doc[0].frames.size()); doc[0].frames.insert(doc[0].frames.begin() + 1, Frame{});
	CHECK(undo.commitTransaction()); CHECK(n.get("p0.f2") && !n.get("p0.f1"));
	CHECK(undo.undo()); CHECK(n.notes == baseline && doc[0].frames.size() == 3 && !n.dirty);
	CHECK(undo.redo()); CHECK(n.get("p0.f2.if2.t25"));
	CHECK(undo.undo());
	undo.beginTransaction("move record");
	n.move(0, 1, true, 1, 0, 3); std::swap(doc[0].frames[1].EF[0], doc[0].frames[1].EF[1]);
	CHECK(undo.commitTransaction()); CHECK(n.get("p0.f1.ef0.t101"));
	CHECK(undo.undo()); CHECK(n.notes == baseline);
	undo.beginTransaction("delete condition");
	n.erase(0, 1, false, 2, 3); doc[0].frames[1].IF.pop_back();
	CHECK(undo.commitTransaction()); CHECK(!n.get("p0.f1.if2.t25") && n.get("unmatched:p0.f1.if2.t25"));
	CHECK(undo.undo()); CHECK(n.notes == baseline);
	undo.beginTransaction("cancel notes"); n.set("p0.f1", "changed"); undo.cancelTransaction(); CHECK(n.notes == baseline && !n.dirty);
	n.move(0, -1, false, 1, 2, 3); CHECK(n.get("p0.f2") && n.get("p0.f2.ef1.t101"));
	n.cloneState(0, 2, 0); CHECK(n.get("p0.f0") && n.get("p0.f0.ef1.t101"));
	n.movePatterns({0}, {1}); CHECK(n.get("p1.f0") && !n.get("p0"));
	n.archiveState(1, 0); CHECK(!n.get("p1.f0") && n.get("unmatched:p1.f0"));
	n = Ha6Notes{}; CHECK(n.loadForHa6(ha6, doc, &error));
	const auto native = Ha6Notes::PathFor(ha6);
	CHECK(n.save(native)); CHECK(Read(native + ".legacy-backup") == original); CHECK(Read(ha6 + ".notes") == original);
	Ha6Notes reloaded; CHECK(reloaded.loadForHa6(ha6, doc, &error)); CHECK(reloaded.notes == n.notes && reloaded.legacyPayload.empty());
	Write(ha6 + ".notes", "broken legacy"); CHECK(reloaded.loadForHa6(ha6, doc, &error)); // Native wins.
	Write(native, "broken native"); CHECK(!reloaded.loadForHa6(ha6, doc, &error)); CHECK(Read(native) == "broken native");
	Write(native, "{\"version\":99,\"notes\":{}}"); CHECK(!reloaded.loadForHa6(ha6, doc, &error));
	const auto saveAs = (root / "other.HA6.notes.json").string();
	CHECK(n.save(saveAs) && Read(saveAs + ".legacy-backup") == original);
	Write(saveAs + ".legacy-backup", "another migration"); CHECK(!n.save(saveAs));
	for (int k = 0; k < 100; ++k) n.event("bounded event"); CHECK(n.trace.size() == 64);
	// Actual pattern-manager model entry points must move notes in the same
	// undo entry even when reference remapping is disabled.
	FrameData fd; fd.initEmpty(3); fd.m_sequences = Doc();
	fd.notes.set("p0.f1", "first"); fd.notes.set("p1.f1", "second"); fd.notes.markSaved();
	Ha6NotesUndoChannel patChannel(&fd.notes); UndoManager history; history.attach(&fd.m_sequences); history.addChannel(&patChannel); history.ensureBaseline();
	patrefs::MovePatterns(fd, {0, 1}, {1, 0}, false); history.flush();
	CHECK(fd.notes.get("p1.f1") && *fd.notes.get("p1.f1") == "first");
	CHECK(fd.notes.get("p0.f1") && *fd.notes.get("p0.f1") == "second");
	CHECK(history.undo()); CHECK(*fd.notes.get("p0.f1") == "first" && !fd.notes.dirty);
	patrefs::PastePatterns(fd, {fd.m_sequences[0]}, {0}, {1}, false); history.flush();
	CHECK(!fd.notes.get("p1.f1") && fd.notes.get("unmatched:p1.f1"));
	CHECK(history.undo()); CHECK(fd.notes.get("p1.f1") && !fd.notes.dirty);
}
static void Live(const fs::path& data, const fs::path& temp) {
	int total = 0;
	for (const auto* name : {"angel_0.HA6", "iori_0.HA6", "kyo_lo_0.HA6", "shermie_0.HA6"}) {
		FrameData fd; const auto ha6 = data / name;
		CHECK(fd.load(ha6.string().c_str()));
		const std::string source = Read(ha6.string() + ".notes");
		const auto legacy = json::parse(source); int count = 0;
		for (const auto& kv : legacy["notes"].items()) if (kv.value().is_string() && !kv.value().get<std::string>().empty()) ++count;
		const auto copy = (temp / name).string(); Write(copy + ".notes", source);
		Ha6Notes notes; std::string error; CHECK(notes.loadForHa6(copy, fd.m_sequences, &error));
		CHECK((int)notes.notes.size() == count);
		for (const auto& kv : legacy["notes"].items()) {
			if (!kv.value().is_string() || kv.value().get<std::string>().empty()) continue;
			const std::string text = kv.value(); bool found = false;
			for (const auto& imported : notes.notes) if (imported.second == text) found = true;
			CHECK(found);
		}
		CHECK(notes.save(Ha6Notes::PathFor(copy)));
		Ha6Notes again; CHECK(again.loadForHa6(copy, fd.m_sequences, &error)); CHECK(again.notes == notes.notes);
		CHECK(Read(ha6.string() + ".notes") == source);
		int unmatched = 0; for (const auto& kv : notes.notes) if (kv.first.rfind("legacy:", 0) == 0) ++unmatched;
		std::cout << name << ": " << count << " notes preserved; " << unmatched << " unresolved\n";
		if (unmatched) {
			for (const auto& kv : notes.notes) if (kv.first.rfind("legacy:", 0) == 0) {
				int p = -1, f = -1, i = -1;
				if (std::sscanf(kv.first.c_str(), "legacy:pattern/%d/frame/%d/condition/%d", &p, &f, &i) == 3 && p < (int)fd.m_sequences.size() && f < (int)fd.m_sequences[p].frames.size())
					std::cout << "  unmatched condition " << p << '/' << f << '/' << i << "; current condition count " << fd.m_sequences[p].frames[f].IF.size() << '\n';
			}
		}
		total += count;
	}
	std::cout << "Live nonempty notes preserved: " << total << '\n'; CHECK(total == 61);
}
int main(int argc, char** argv) {
	const fs::path temp = fs::temp_directory_path() / ("ha6-notes-contract-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	fs::create_directories(temp);
	try { Synthetic(temp); if (argc == 2) Live(fs::u8path(argv[1]), temp); }
	catch (const std::exception& e) { std::cerr << e.what() << '\n'; ++failures; }
	std::cout << "Artifacts: " << temp.string() << "\nFailures: " << failures << '\n'; return failures ? 1 : 0;
}
