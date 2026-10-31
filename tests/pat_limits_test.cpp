// UNI2 / MBTL .pat limits (src/parts/pat_limits.h)
#include "parts/pat_limits.h"
#include <cstdio>

static int fails = 0;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #x); ++fails; } } while (0)

int main()
{
	using namespace patlimits;
	CHECK(Check(4000, 2000, {80, 1, 0}).empty());
	CHECK(Check(65536, 0, {}).empty());                 // ids 0..65535
	CHECK(Check(65537, 0, {}).size() == 1);
	CHECK(Check(1, 2001, {}).size() == 1);
	CHECK(Check(3, 0, {0, 81, 80}).size() == 1);
	CHECK(Check(65537, 2001, {81, 81}).size() == 4);
	return fails ? 1 : 0;
}
