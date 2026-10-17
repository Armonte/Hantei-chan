// Tests for the reference tools: variable batch replace (issue #75).
//
//   refs_test [HA6...]
// Synthetic cases always run; each HA6 given also gets a reversible
// replace check (rename a used ID to an unused one and back: the saved bytes
// must match the untouched save).

#include "framedata.h"
#include "var_refs.h"
#include "pattern_refs.h"
#include "ha6_notes.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
#include <string>

static int g_fail = 0;
#define CHECK(x) do { if (!(x)) { std::printf("CHECK failed: %s (line %d)\n", #x, __LINE__); ++g_fail; } } while (0)

static Frame_EF Ef(int type, int number, int pi, int v) { Frame_EF e{}; e.type = type; e.number = number; e.parameters[pi] = v; return e; }
static Frame_IF If(int type, int pi, int v) { Frame_IF c{}; c.type = type; c.parameters[pi] = v; return c; }

static void Synthetic()
{
	FrameData fd;
	fd.initEmpty(8);
	Sequence* s = fd.get_sequence(3);
	s->frames.resize(2);
	s->frames[0].EF.push_back(Ef(2, 105, 0, 7));   // change variable 7
	s->frames[0].EF.push_back(Ef(2, 100, 0, 73));  // proj var 7, +3
	s->frames[0].EF.push_back(Ef(1, 0, 8, 7));     // EF1 proj var 7
	s->frames[0].EF.push_back(Ef(2, 102, 0, 7));   // dash var amount: not an ID
	s->frames[1].IF.push_back(If(25, 1, 7));       // compare var 7
	s->frames[1].IF.push_back(If(24, 1, 71));      // proj flag 7 value 1
	s->frames[1].IF.push_back(If(17, 1, 7));       // hit count: not an ID
	s->frames[1].IF.push_back(If(38, 3, 5));       // other var

	auto all = varrefs::Find(fd, 7);
	CHECK(all.size() == 5);
	CHECK(varrefs::Find(fd, 7, varrefs::catVariable).size() == 2);
	CHECK(varrefs::Find(fd, 7, varrefs::catProjectile).size() == 3);
	CHECK(varrefs::Find(fd, 5).size() == 1);

	int skipped = -1;
	CHECK(varrefs::Replace(fd, 7, 12, varrefs::catAll, &skipped) == 5 && skipped == 0);
	CHECK(s->frames[0].EF[0].parameters[0] == 12);
	CHECK(s->frames[0].EF[1].parameters[0] == 123);   // tens place, value kept
	CHECK(s->frames[0].EF[2].parameters[8] == 12);
	CHECK(s->frames[0].EF[3].parameters[0] == 7);     // untouched
	CHECK(s->frames[1].IF[0].parameters[1] == 12);
	CHECK(s->frames[1].IF[1].parameters[1] == 121);
	CHECK(s->frames[1].IF[2].parameters[1] == 7);     // untouched
	CHECK(s->modified);
	CHECK(varrefs::Find(fd, 7).empty());

	// A negative ID cannot go in a tens place.
	CHECK(varrefs::Replace(fd, 12, -1, varrefs::catAll, &skipped) == 3 && skipped == 2);
}

static std::string ReadAll(const std::string& p)
{
	std::ifstream f(p, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(f), {});
}

static void RealFile(const char* path)
{
	FrameData fd;
	if (!fd.load(path)) { std::printf("load failed: %s\n", path); ++g_fail; return; }
	auto refs = varrefs::Find(fd, -1);
	std::set<int> used;
	for (auto& r : refs) used.insert(r.varId);
	const std::string a = "refs_test_a.tmp", b = "refs_test_b.tmp"; // cwd, never next to the input
	std::printf("%s: %zu variable reference(s), %zu distinct ID(s)\n", path, refs.size(), used.size());
	if (used.empty()) return;
	int unused = 90; // an ID no file uses, and never 0 (0 means "none" in some slots)
	while (used.count(unused)) ++unused;
	auto it = used.lower_bound(0);
	if (it == used.end()) return;
	const int from = *it;
	CHECK(fd.save(a.c_str()));
	const int n1 = varrefs::Replace(fd, from, unused);
	const int n2 = varrefs::Replace(fd, unused, from);
	CHECK(n1 > 0 && n1 == n2);
	CHECK(fd.save(b.c_str()));
	CHECK(ReadAll(a) == ReadAll(b));
	std::remove(a.c_str());
	std::remove(b.c_str());
}

static void PatternRefs()
{
	FrameData fd;
	fd.initEmpty(40);
	auto pat = [&](int id) { Sequence* s = fd.get_sequence(id); s->frames.resize(1); s->name = "p" + std::to_string(id); s->frames[0].AF.aniType = 1; return s; };
	Sequence* a = pat(10);
	a->frames[0].EF.push_back(Ef(1, 11, 0, 0));      // spawn 11 (absolute, in `number`)
	a->frames[0].EF.back().number = 11;
	Frame_EF rel{}; rel.type = 101; rel.number = 2;  // spawn 10+2 = 12 (relative)
	a->frames[0].EF.push_back(rel);
	a->frames[0].IF.push_back(If(3, 0, 10000 + 12)); // branch on hit: queue 12
	a->frames[0].IF.push_back(If(3, 0, 5));          // frame jump, not a pattern
	Sequence* b = pat(11);
	b->frames[0].AF.aniType = 0; b->frames[0].AF.jump = 12; // end -> pattern 12
	pat(12);
	Sequence* c = pat(20);
	c->frames[0].IF.push_back(If(18, 1, 256));       // standing check, not a pattern
	c->frames[0].IF.push_back(If(37, 0, 11));

	CHECK(patrefs::Find(fd, 12).size() == 3);
	CHECK(patrefs::Find(fd, 11).size() == 2);
	CHECK(patrefs::Find(fd, 256).empty());

	// Move 10,11,12 -> 30,31,32 with reference updates.
	const int changed = patrefs::MovePatterns(fd, {10, 11, 12}, {30, 31, 32}, true);
	CHECK(changed == 4); // EF1, IF3, AF jump, IF37 (the relative EF101 offset is unchanged)
	CHECK(patrefs::IsEmptySlot(fd, 10) && patrefs::IsEmptySlot(fd, 12));
	Sequence* a2 = fd.get_sequence(30);
	CHECK(a2->name == "p10");
	CHECK(a2->frames[0].EF[0].number == 31);
	CHECK(a2->frames[0].EF[1].number == 2);            // 30+2 = 32: offset unchanged
	CHECK(a2->frames[0].IF[0].parameters[0] == 10032);
	CHECK(a2->frames[0].IF[1].parameters[0] == 5);
	CHECK(fd.get_sequence(31)->frames[0].AF.jump == 32);
	CHECK(fd.get_sequence(20)->frames[0].IF[1].parameters[0] == 31);
	CHECK(fd.get_sequence(20)->frames[0].IF[0].parameters[1] == 256);

	// Paste copies of 30 and 32 into next empty slots from 0 with internal remap:
	// 30 -> 0, 32 -> 1. The copy's relative spawn (+2 from 30 = 32) must become
	// +1 (0 -> 1); its absolute spawn of 31 (not pasted) stays 31.
	std::vector<Sequence> pats = {*fd.get_sequence(30), *fd.get_sequence(32)};
	auto slots = patrefs::PlanSlots(fd, 2, 0, patrefs::Placement::NextEmpty, {30, 32});
	CHECK(slots.size() == 2 && slots[0] == 0 && slots[1] == 1);
	patrefs::PastePatterns(fd, pats, {30, 32}, slots, true);
	Sequence* cp = fd.get_sequence(0);
	CHECK(cp->frames[0].EF[1].number == 1);
	CHECK(cp->frames[0].EF[0].number == 31);
	CHECK(cp->frames[0].IF[0].parameters[0] == 10001);
	// Originals untouched.
	CHECK(fd.get_sequence(30)->frames[0].EF[1].number == 2);

	// Placement: next empty skips filled slots; consecutive does not.
	auto ne = patrefs::PlanSlots(fd, 3, 30, patrefs::Placement::NextEmpty, {});
	CHECK(ne[0] == 33 && ne[1] == 34 && ne[2] == 35);
	auto co = patrefs::PlanSlots(fd, 3, 30, patrefs::Placement::Consecutive, {});
	CHECK(co[0] == 30 && co[1] == 31 && co[2] == 32);
	auto og = patrefs::PlanSlots(fd, 2, 0, patrefs::Placement::OriginalIds, {7, 99});
	CHECK(og[0] == 7 && og[1] == -1);
}

// Move every pattern of a real file by +0 through a temporary slot and back:
// MovePatterns + remap must give the original bytes.
static void RealMove(const char* path)
{
	FrameData fd;
	if (!fd.load(path)) return;
	const int n = fd.get_sequence_count();
	int free1 = -1;
	for (int p = n - 1; p >= 0; --p) if (patrefs::IsEmptySlot(fd, p)) { free1 = p; break; }
	int src = -1;
	for (int p = 0; p < n; ++p) if (!patrefs::Find(fd, p).empty() && !fd.get_sequence(p)->frames.empty()) { src = p; if (p > 20) break; }
	if (free1 < 0 || src < 0) return;
	CHECK(fd.save("refs_test_m1.tmp"));
	const size_t refs = patrefs::Find(fd, src).size();
	patrefs::MovePatterns(fd, {src}, {free1}, true);
	CHECK(patrefs::Find(fd, src).empty() || src == 0);
	CHECK(patrefs::Find(fd, free1).size() == refs);
	patrefs::MovePatterns(fd, {free1}, {src}, true);
	CHECK(fd.save("refs_test_m2.tmp"));
	CHECK(ReadAll("refs_test_m1.tmp") == ReadAll("refs_test_m2.tmp"));
	std::printf("%s: moved pattern %d (%zu reference(s)) to %d and back\n", path, src, refs, free1);
	std::remove("refs_test_m1.tmp");
	std::remove("refs_test_m2.tmp");
}

static void Notes()
{
	int p, f, i, t; bool ef;
	CHECK(Ha6Notes::ParseKey("p12", &p, &f, &ef, &i, &t) && p == 12 && f == -1);
	CHECK(Ha6Notes::RecordKey(12, 3, true, 1, 101) == "p12.f3.ef1.t101");
	CHECK(Ha6Notes::ParseKey("p12.f3.ef1.t101", &p, &f, &ef, &i, &t) && p == 12 && f == 3 && ef && i == 1 && t == 101);
	CHECK(Ha6Notes::ParseKey("p7.f0.if2.t25", &p, &f, &ef, &i, &t) && !ef && i == 2 && t == 25);
	CHECK(!Ha6Notes::ParseKey("hello", &p, &f, &ef, &i, &t));

	Ha6Notes n;
	n.set("p1", "checks the animation");
	n.set("p1.f0.if0.t25", "\xe3\x83\x86\xe3\x82\xb9\xe3\x83\x88 UTF-8 \"quoted\"\nline 2");
	CHECK(n.dirty && n.notes.size() == 2);
	CHECK(n.save("refs_test_notes.tmp"));
	Ha6Notes m;
	std::string err;
	CHECK(m.load("refs_test_notes.tmp", &err) && m.notes == n.notes && !m.dirty);
	m.set("p1", "");
	CHECK(m.dirty && m.notes.size() == 1);
	Ha6Notes missing;
	CHECK(missing.load("refs_test_does_not_exist.json", &err) && missing.notes.empty());
	{ std::ofstream bad("refs_test_notes.tmp"); bad << "{ not json"; }
	CHECK(!missing.load("refs_test_notes.tmp", &err) && !err.empty());
	std::remove("refs_test_notes.tmp");
}

int main(int argc, char** argv)
{
	Synthetic();
	Notes();
	PatternRefs();
	for (int i = 1; i < argc; ++i) RealMove(argv[i]);
	for (int i = 1; i < argc; ++i) RealFile(argv[i]);
	std::printf(g_fail == 0 ? "REFS_TEST_PASS\n" : "REFS_TEST_FAIL (%d)\n", g_fail);
	return g_fail == 0 ? 0 : 1;
}
