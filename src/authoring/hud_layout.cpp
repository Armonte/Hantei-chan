// [authoring] TAG HUD layout sidecar — see hud_layout.h and docs/HANTEI_AUTHORING_MODE.md §9.4.
#include "hud_layout.h"
#include "../tag_tuning/tag_ini.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace authoring {

namespace {

const char* kSection = "taghud";

bool* BoolField(TagHudValues& v, const std::string& k)
{
	if (k == "enabled") return &v.enabled;
	if (k == "nativeRedirect") return &v.nativeRedirect;
	if (k == "reserveFace") return &v.reserveFace;
	if (k == "partnerBar") return &v.partnerBar;
	if (k == "teamName") return &v.teamName;
	if (k == "banners") return &v.banners;
	if (k == "assist") return &v.assist;
	if (k == "swapCooldown") return &v.swapCooldown;
	return nullptr;
}

HudRect* RectField(TagHudValues& v, const std::string& k)
{
	if (k == "mainPortrait") return &v.mainPortrait;
	if (k == "mainSrc") return &v.mainSrc;
	if (k == "reservePortrait") return &v.reservePortrait;
	if (k == "reserveSrc") return &v.reserveSrc;
	if (k == "partnerThumb") return &v.partnerThumb;
	if (k == "partnerBar.rect") return &v.partnerBarRect;
	if (k == "swapSliver") return &v.swapSliver;
	if (k == "assistPip") return &v.assistPip;
	if (k == "banner") return &v.banner;
	return nullptr;
}

const HudKey* FindKey(const std::string& k)
{
	for (const HudKey& h : HudKeys()) if (tagtune::ieq(h.key, k)) return &h;
	return nullptr;
}

// Parse up to n comma-separated numbers; exactly n required.
bool ParseNums(const std::string& s, float* out, int n)
{
	const char* p = s.c_str();
	for (int i = 0; i < n; ++i) {
		while (*p == ' ' || *p == '\t') ++p;
		char* end = nullptr;
		const double d = std::strtod(p, &end);
		if (end == p || !std::isfinite(d)) return false;
		out[i] = (float)d;
		p = end;
		while (*p == ' ' || *p == '\t') ++p;
		if (i + 1 < n) {
			if (*p != ',') return false;
			++p;
		}
	}
	while (*p == ' ' || *p == '\t') ++p;
	return *p == 0;
}

std::string Num(float f)
{
	char b[32];
	if (f == std::floor(f) && std::fabs(f) < 1e7f) std::snprintf(b, sizeof b, "%d", (int)f);
	else std::snprintf(b, sizeof b, "%g", (double)f);
	return b;
}

// Apply one key=value; false = bad value (warning, keep).
bool ApplyKey(TagHudValues& v, const HudKey& h, const std::string& val)
{
	const std::string kind = h.kind;
	if (kind == "bool") {
		bool* b = BoolField(v, h.key);
		for (const char* t : { "1", "true", "on", "yes" }) if (tagtune::ieq(val, t)) { *b = true; return true; }
		for (const char* f : { "0", "false", "off", "no" }) if (tagtune::ieq(val, f)) { *b = false; return true; }
		return false;
	}
	if (kind == "float") {
		float f;
		if (!ParseNums(val, &f, 1) || f < 0 || f > 10000) return false;
		v.tweenMs = f;
		return true;
	}
	if (kind == "rect") {
		float r[4];
		if (!ParseNums(val, r, 4) || r[2] < 0 || r[3] < 0) return false;
		*RectField(v, h.key) = { r[0], r[1], r[2], r[3] };
		return true;
	}
	if (kind == "xy") {
		float r[2];
		if (!ParseNums(val, r, 2)) return false;
		v.assistLabelX = r[0]; v.assistLabelY = r[1];
		return true;
	}
	if (kind == "xyp") {
		float r[3];
		if (!ParseNums(val, r, 3) || r[2] <= 0) return false;
		v.nameX = r[0]; v.nameY = r[1]; v.namePx = r[2];
		return true;
	}
	return false;
}

void ApplyDoc(TagHudValues& v, const tagtune::TagIni* doc, const char* layer, std::vector<std::string>* warn)
{
	if (!doc) return;
	for (const tagtune::KeyValue& kv : doc->Keys(tagtune::SecKind::Other, kSection)) {
		const HudKey* h = FindKey(kv.key);
		if (!h) { if (warn) warn->push_back(std::string(layer) + " hud.ini line " + std::to_string(kv.line) + ": unknown key '" + kv.key + "'"); continue; }
		if (!ApplyKey(v, *h, kv.value) && warn)
			warn->push_back(std::string(layer) + " hud.ini line " + std::to_string(kv.line) + ": bad value '" + kv.value + "' for " + h->key + " (kept)");
	}
}

} // namespace

const std::vector<HudKey>& HudKeys()
{
	static const std::vector<HudKey> k = {
		{ "enabled", "bool" }, { "nativeRedirect", "bool" }, { "reserveFace", "bool" }, { "partnerBar", "bool" },
		{ "teamName", "bool" }, { "banners", "bool" }, { "assist", "bool" }, { "swapCooldown", "bool" },
		{ "tweenMs", "float" },
		{ "mainPortrait", "rect" }, { "mainSrc", "rect" }, { "reservePortrait", "rect" }, { "reserveSrc", "rect" },
		{ "partnerThumb", "rect" }, { "partnerBar.rect", "rect" }, { "swapSliver", "rect" },
		{ "assistLabel", "xy" }, { "assistPip", "rect" }, { "name", "xyp" }, { "banner", "rect" },
	};
	return k;
}

TagHudValues ResolveHud(const tagtune::TagIni* shipped, const tagtune::TagIni* local, std::vector<std::string>* warnings)
{
	TagHudValues v;
	ApplyDoc(v, shipped, "shipped", warnings);
	ApplyDoc(v, local, "local", warnings);
	return v;
}

std::string HudValueText(const TagHudValues& vin, const std::string& key)
{
	TagHudValues v = vin;
	const HudKey* h = FindKey(key);
	if (!h) return {};
	const std::string kind = h->kind;
	if (kind == "bool") return *BoolField(v, h->key) ? "1" : "0";
	if (kind == "float") return Num(v.tweenMs);
	if (kind == "rect") { const HudRect& r = *RectField(v, h->key); return Num(r.x) + "," + Num(r.y) + "," + Num(r.w) + "," + Num(r.h); }
	if (kind == "xy") return Num(v.assistLabelX) + "," + Num(v.assistLabelY);
	if (kind == "xyp") return Num(v.nameX) + "," + Num(v.nameY) + "," + Num(v.namePx);
	return {};
}

void SetHudKey(tagtune::TagIni& doc, const std::string& key, const std::string& value)
{
	doc.Set(tagtune::SecKind::Other, kSection, key, value);
}

bool ClearHudKey(tagtune::TagIni& doc, const std::string& key)
{
	return doc.Remove(tagtune::SecKind::Other, kSection, key);
}

} // namespace authoring
