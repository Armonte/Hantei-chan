// Tests for the reference tools: variable batch replace (issue #75).
//
//   refs_test [HA6...]
// Synthetic cases always run; each HA6 given also gets a reversible
// replace check (rename a used ID to an unused one and back: the saved bytes
// must match the untouched save).

#include "framedata.h"
#include "var_refs.h"

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

int main(int argc, char** argv)
{
	Synthetic();
	for (int i = 1; i < argc; ++i) RealFile(argv[i]);
	std::printf(g_fail == 0 ? "REFS_TEST_PASS\n" : "REFS_TEST_FAIL (%d)\n", g_fail);
	return g_fail == 0 ? 0 : 1;
}
