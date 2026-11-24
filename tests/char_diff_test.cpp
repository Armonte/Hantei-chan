// Character diff model (issue #131): pattern-by-pattern comparison by value.
#include "char_diff.h"
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
using namespace chardiff;
static std::vector<Sequence> MakeDoc(int n)
{
	std::vector<Sequence> doc(n);
	for (int p = 0; p < n; ++p) {
		if (p == n - 1) continue;   // last slot empty
		doc[p].name = "P" + std::to_string(p);
		doc[p].frames.resize(4);
		for (int i = 0; i < 4; ++i) {
			auto& f = doc[p].frames[i];
			f.AF.duration = 2 + i; f.AF.layers.resize(1); f.AF.layers[0].spriteId = p * 10 + i;
			f.AT.damage = 100 * p; f.hitboxes[1] = Hitbox{{-10, -50, 10, 0}};
		}
	}
	return doc;
}
int main()
{
	const auto base = MakeDoc(8);
	// identical -> nothing
	auto r = Compare(base, base);
	CHECK(r.valid && r.total() == 0 && r.patterns.empty() && r.kindOf.size() == 8);
	// editor state is not data
	auto doc = base;
	doc[1].modified = true; doc[1].initialized = true; doc[1].frames[0].ha6 = Ha6FrameEnc{};
	CHECK(Compare(doc, base).total() == 0);
	// one AT field, one box, one AF layer value, a header field, a frame count, an added and a removed pattern
	doc = base;
	doc[0].frames[2].AT.damage = 999;
	doc[1].frames[1].hitboxes[25] = Hitbox{{0, -40, 30, -20}};
	doc[2].frames[3].AF.layers[0].rgba[1] = 0.5f;
	doc[3].psts = 1;
	doc[4].frames.pop_back();
	doc[7].name = "new";                 // slot empty in base -> added
	doc[6] = Sequence();                 // existed in base -> removed
	r = Compare(doc, base);
	CHECK(r.changed == 5 && r.added == 1 && r.removed == 1);
	CHECK(r.kind(0) == Kind::Changed && r.find(0)->areas == aAT && r.find(0)->frames.size() == 1 && r.find(0)->frames[0].frame == 2);
	CHECK(r.find(1)->areas == aBoxes);
	CHECK(r.find(2)->areas == aAF);
	CHECK(r.find(3)->areas == aHeader && r.find(3)->frames.empty());
	CHECK(r.find(4)->areas == aFrameCount && r.find(4)->framesMine == 3 && r.find(4)->framesOther == 4);
	CHECK(r.kind(5) == Kind::Same && !r.find(5));
	CHECK(r.kind(6) == Kind::Removed && r.kind(7) == Kind::Added);
	CHECK(Describe(*r.find(0)) == "1 frame: AT");
	CHECK(Describe(*r.find(4)) == "frame count 4 -> 3");
	// field rows
	auto rows = FrameFields(&doc[0].frames[2], &base[0].frames[2]);
	CHECK(rows.size() == 1 && rows[0].field == "damage" && rows[0].mine == "999" && rows[0].other == "0");
	rows = FrameFields(&doc[1].frames[1], &base[1].frames[1]);
	CHECK(rows.size() == 1 && rows[0].field == "attack box 1" && rows[0].other == "-");
	rows = HeaderFields(&doc[3], &base[3]);
	CHECK(rows.size() == 1 && rows[0].field == "PSTS");
	// pattern tables of different sizes: the extra slots count as added / removed when they hold something
	auto longer = base; longer.resize(10); longer[9].name = "tail";
	r = Compare(longer, base);
	CHECK(r.added == 1 && r.kind(9) == Kind::Added && r.kindOf.size() == 10);
	r = Compare(base, longer);
	CHECK(r.removed == 1 && r.kind(9) == Kind::Removed);
	// list marks: the other side's table sees added / removed swapped
	FrameData a, b;
	ListMarks m; m.mine = &a; m.other = &b; m.kinds = &r.kindOf;
	CHECK(m.kindFor(&a, 9) == Kind::Removed && m.kindFor(&b, 9) == Kind::Added && m.kindFor(nullptr, 9) == Kind::Same);
	std::printf("char_diff_test: ok\n");
	return 0;
}
