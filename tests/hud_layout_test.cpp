// hud_layout_test: the TAG HUD layout sidecar (povertycaster\tag\hud.ini, docs/HANTEI_AUTHORING_MODE.md §9.4):
// defaults == PovertyCaster TagHud.hpp, every value kind, bad values, shipped/local layering, byte-preserving edits.
#include "authoring/hud_layout.h"
#include "tag_tuning/tag_ini.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace authoring;

static int g_fail = 0, g_checks = 0;
#define CHECK(c) do { ++g_checks; if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool Eq(const HudRect& r, float x, float y, float w, float h) { return r.x == x && r.y == y && r.w == w && r.h == h; }
static std::string ReadText(const std::string& p)
{
	std::ifstream f(p, std::ios::binary);
	std::ostringstream s;
	s << f.rdbuf();
	return s.str();
}
static tagtune::TagIni Doc(const std::string& t) { tagtune::TagIni d; d.LoadText(t); return d; }

static void TestDefaults()
{
	// TagHud.hpp TagHudSettings / TagHudLayout (PovertyCaster main, 2026-09-25)
	const TagHudValues v;
	CHECK(v.enabled && v.nativeRedirect && v.reserveFace && v.partnerBar && v.teamName && v.banners && v.assist && v.swapCooldown);
	CHECK(v.tweenMs == 260.0f);
	CHECK(Eq(v.mainPortrait, 0, 0, 256, 96) && Eq(v.mainSrc, 0, 0, 256, 96));
	CHECK(Eq(v.reservePortrait, 72, 0, 66, 37) && Eq(v.reserveSrc, 0, 0, 96, 54));
	CHECK(Eq(v.partnerThumb, 112, 119, 21, 12) && Eq(v.partnerBarRect, 134, 122, 139, 8) && Eq(v.swapSliver, 134, 131, 139, 2));
	CHECK(v.assistLabelX == 134 && v.assistLabelY == 136 && Eq(v.assistPip, 158, 137, 48, 6));
	CHECK(v.nameX == 134 && v.nameY == 148 && v.namePx == 1 && Eq(v.banner, 134, 147, 176, 11));
	CHECK(HudKeys().size() == 20);
	// resolving nothing = the defaults; the text of every default parses back to itself
	const TagHudValues r = ResolveHud(nullptr, nullptr);
	for (const HudKey& k : HudKeys()) CHECK(HudValueText(r, k.key) == HudValueText(v, k.key));
	CHECK(HudValueText(v, "partnerBar.rect") == "134,122,139,8" && HudValueText(v, "name") == "134,148,1");
	CHECK(HudValueText(v, "assistLabel") == "134,136" && HudValueText(v, "tweenMs") == "260" && HudValueText(v, "banners") == "1");
	CHECK(HudValueText(v, "nope").empty());
}

static void TestKinds()
{
	std::vector<std::string> w;
	tagtune::TagIni d = Doc("[taghud]\nenabled=off\nbanners=no\ntweenMs=120.5\nmainPortrait= 1, 2 ,3,4\nassistLabel=10,11\n"
	                        "name=20,21,2\npartnerBar=0\npartnerBar.rect=5,6,7,8\n");
	TagHudValues v = ResolveHud(&d, nullptr, &w);
	CHECK(w.empty());
	CHECK(!v.enabled && !v.banners && v.tweenMs == 120.5f && Eq(v.mainPortrait, 1, 2, 3, 4));
	CHECK(v.assistLabelX == 10 && v.assistLabelY == 11 && v.nameX == 20 && v.nameY == 21 && v.namePx == 2);
	CHECK(!v.partnerBar && Eq(v.partnerBarRect, 5, 6, 7, 8));
	CHECK(HudValueText(v, "tweenMs") == "120.5");
	// bad values: warning, the key keeps its previous (default) value
	w.clear();
	tagtune::TagIni bad = Doc("[taghud]\nenabled=maybe\ntweenMs=abc\nmainPortrait=1,2,3\nreserveSrc=1,2,3,4,5\nassistLabel=1\n"
	                          "name=1,2,0\nbanner=1,2,-3,4\nwhatever=1\n");
	v = ResolveHud(&bad, nullptr, &w);
	CHECK(w.size() == 8);
	CHECK(v.enabled && v.tweenMs == 260 && Eq(v.mainPortrait, 0, 0, 256, 96) && Eq(v.reserveSrc, 0, 0, 96, 54));
	CHECK(v.assistLabelX == 134 && v.namePx == 1 && Eq(v.banner, 134, 147, 176, 11));
	// keys outside [taghud] are not hud keys
	tagtune::TagIni other = Doc("[tuning]\nmainPortrait=9,9,9,9\n");
	CHECK(Eq(ResolveHud(&other, nullptr).mainPortrait, 0, 0, 256, 96));
	// every key round-trips through its text
	TagHudValues x;
	x.tweenMs = 99; x.mainPortrait = { 3, 4, 5, 6 }; x.assistLabelX = 7; x.nameY = 50; x.namePx = 1.5f; x.teamName = false;
	tagtune::TagIni rt = Doc("");
	for (const HudKey& k : HudKeys()) SetHudKey(rt, k.key, HudValueText(x, k.key));
	const TagHudValues y = ResolveHud(&rt, nullptr);
	for (const HudKey& k : HudKeys()) CHECK(HudValueText(y, k.key) == HudValueText(x, k.key));
}

static void TestLayering()
{
	const std::string shipped = ReadText("tests/fixtures/authoring/tag/hud.ini");
	const std::string local = ReadText("tests/fixtures/authoring/tag/local/hud.ini");
	CHECK(!shipped.empty() && !local.empty());
	tagtune::TagIni s = Doc(shipped), l = Doc(local);
	std::vector<std::string> w;
	const TagHudValues v = ResolveHud(&s, &l, &w);
	CHECK(w.empty());
	CHECK(Eq(v.reservePortrait, 80, 4, 66, 37));   // local wins
	CHECK(Eq(v.partnerBarRect, 134, 122, 139, 8) && v.tweenMs == 260);
	CHECK(Eq(ResolveHud(&s, nullptr).reservePortrait, 72, 0, 66, 37));
	tagtune::TagIni s2 = Doc("[taghud]\ntweenMs=100\nbanner=1,1,1,1\n"), l2 = Doc("[taghud]\ntweenMs=50\n");
	const TagHudValues v2 = ResolveHud(&s2, &l2);
	CHECK(v2.tweenMs == 50 && Eq(v2.banner, 1, 1, 1, 1));
	// a bad local value keeps the shipped one
	tagtune::TagIni l3 = Doc("[taghud]\ntweenMs=x\n");
	w.clear();
	CHECK(ResolveHud(&s2, &l3, &w).tweenMs == 100 && w.size() == 1 && w[0].find("local") == 0);
}

static void TestEdits()
{
	const std::string orig = "; my layout\n[taghud]\nreservePortrait = 80,4,66,37   ; moved right\n# note\ntweenMs=260\n";
	tagtune::TagIni d = Doc(orig);
	SetHudKey(d, "reservePortrait", "90,4,66,37");
	CHECK(d.Text() == "; my layout\n[taghud]\nreservePortrait = 90,4,66,37   ; moved right\n# note\ntweenMs=260\n");
	SetHudKey(d, "reservePortrait", "80,4,66,37");
	CHECK(d.Text() == orig);
	SetHudKey(d, "banner", "1,2,3,4");
	CHECK(d.Text() == orig + "banner=1,2,3,4\n");
	CHECK(ClearHudKey(d, "banner") && d.Text() == orig);
	CHECK(!ClearHudKey(d, "banner"));
	// a new file gets [taghud]
	tagtune::TagIni n = Doc("");
	SetHudKey(n, "tweenMs", "120");
	CHECK(n.Text() == "[taghud]\ntweenMs=120\n");
	// CRLF kept
	tagtune::TagIni c = Doc("[taghud]\r\ntweenMs=1\r\n");
	SetHudKey(c, "enabled", "0");
	CHECK(c.Text() == "[taghud]\r\ntweenMs=1\r\nenabled=0\r\n");
	// other sections untouched
	tagtune::TagIni o = Doc("[other]\nx=1\n");
	SetHudKey(o, "tweenMs", "5");
	CHECK(o.Text() == "[other]\nx=1\n\n[taghud]\ntweenMs=5\n");
}

int main()
{
	TestDefaults();
	TestKinds();
	TestLayering();
	TestEdits();
	std::printf(g_fail ? "hud_layout_test: %d of %d FAILED\n" : "hud_layout_test: all %d checks passed\n", g_fail ? g_fail : g_checks, g_checks);
	return g_fail ? 1 : 0;
}
