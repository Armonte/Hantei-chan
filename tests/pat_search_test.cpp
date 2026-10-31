// PAT editor list filter (issue #113): src/PatEditor/pat_search.h
#include "PatEditor/pat_search.h"
#include <cstdio>

static int fails = 0;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #x); ++fails; } } while (0)

int main()
{
	using patsearch::Contains;
	CHECK(Contains("12: Ragna_arm", ""));
	CHECK(Contains("12: Ragna_arm", nullptr));
	CHECK(Contains("12: Ragna_arm", "ragna"));
	CHECK(Contains("12: Ragna_arm", "ARM"));
	CHECK(Contains("12: Ragna_arm", "12"));
	CHECK(!Contains("12: Ragna_arm", "leg"));
	CHECK(!Contains("ab", "abc"));
	CHECK(!Contains(nullptr, "a"));
	CHECK(Contains(u8"3: 剣の腕", u8"剣"));
	CHECK(!Contains(u8"3: 剣の腕", u8"足"));
	return fails ? 1 : 0;
}
