// .pal round trip: load with PalFile, save, compare bytes. Exit 0 = identical.
//   pal_roundtrip <in.pal> <out.pal>
#include <cstdio>
#include <cstring>
#include <iostream>
#include "pal_file.h"
#include "misc.h"

int main(int argc, char **argv)
{
	if (argc < 3) { std::cerr << "usage: pal_roundtrip <in.pal> <out.pal>\n"; return 1; }
	PalFile p;
	if (!p.load(argv[1])) { std::cerr << "load failed\n"; return 2; }
	if (!p.save(argv[2])) { std::cerr << "save failed\n"; return 3; }
	char *a = nullptr, *b = nullptr; unsigned int na = 0, nb = 0;
	ReadInMem(argv[1], a, na); ReadInMem(argv[2], b, nb);
	const bool same = na == nb && a && b && !memcmp(a, b, na);
	std::cout << (p.modern ? "UNI/MBTL" : "MBAACC") << " palettes=" << p.count() << " split=" << p.split
	          << (same ? " identical\n" : " DIFFER\n");
	delete[] a; delete[] b;
	return same ? 0 : 7;
}
