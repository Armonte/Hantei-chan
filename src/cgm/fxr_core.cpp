#include "fxr_core.h"
#include "../framedata.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>

namespace cgm {

const FxPatternRow *FxClassification::find(int pattern) const {
	auto it = std::lower_bound(patterns.begin(), patterns.end(), pattern, [](const FxPatternRow &r, int p) { return r.pattern < p; });
	return it != patterns.end() && it->pattern == pattern ? &*it : nullptr;
}

void ClassifyEffects(const Bank &bank, const UsageIndex &usage, FrameData &fd, FxClassification &out) {
	out = FxClassification();
	const int n = (int)bank.images.size();
	out.imageType.assign((size_t)n, -2);
	out.patternsUsing.assign((size_t)n, 0);
	for (int i = 0; i < n; i++) if (bank.images[i].present) out.imageType[i] = bank.images[i].type;
	for (int i = 0; i < n && i < (int)usage.byImage.size(); i++) out.patternsUsing[i] = usage.patternsUsing(i);
	auto fixed = [&](int id) { return id >= 0 && id < n && out.imageType[id] != -2 && out.imageType[id] != 0 && out.imageType[id] != -1; };
	for (int i = 0; i < n; i++) if (fixed(i)) {
		out.fixedImages++;
		if (out.patternsUsing[i] >= 1) out.usedFixedImages++;
		if (out.patternsUsing[i] >= 2) out.sharedFixedImages++;
	}
	std::map<int, FxPatternRow> rows;
	for (int id = 0; id < (int)usage.byImage.size(); id++) {
		for (const UsageRef &r : usage.byImage[id]) {
			FxPatternRow &row = rows[r.pattern];
			row.pattern = r.pattern; row.layers++;
			if (!fixed(id)) { continue; }
			row.fixedLayers++;
			if (std::find(row.fixedSprites.begin(), row.fixedSprites.end(), id) == row.fixedSprites.end()) row.fixedSprites.push_back(id);
			Sequence *seq = fd.get_sequence(r.pattern);
			if (seq && r.frame < (int)seq->frames.size() && r.layer < (int)seq->frames[r.frame].AF.layers.size()) {
				const int bm = seq->frames[r.frame].AF.layers[r.layer].blend_mode;
				row.blendMask |= 1u << (bm >= 0 && bm < 31 ? bm : 0);
			}
		}
	}
	for (auto &kv : rows) {
		FxPatternRow &row = kv.second;
		if (!row.fixedLayers) continue;
		std::sort(row.fixedSprites.begin(), row.fixedSprites.end());
		Sequence *seq = fd.get_sequence(row.pattern);
		if (seq) row.name = seq->name.c_str();
		for (int s : row.fixedSprites) if (out.patternsUsing[s] >= 2) row.sharedSprites++;
		std::set<int> follow;
		for (int id = 0; id < (int)usage.byImage.size(); id++) if (id < n && out.imageType[id] == 0)
			for (const UsageRef &r : usage.byImage[id]) if (r.pattern == row.pattern) { follow.insert(id); break; }
		row.followSprites = (int)follow.size();
		row.effectScore = row.layers ? row.fixedLayers * 100 / row.layers : 0;
		out.patterns.push_back(row);
	}
}

void ToRuntimePalette(const uint32_t *p, uint32_t out[256]) {
	for (int i = 0; i < 256; i++) {
		const uint32_t c = p ? p[i] : 0;
		out[i] = (c & 0xFF00FF00u) | ((c & 0xFF) << 16) | ((c >> 16) & 0xFF);
	}
}

fx::Rgb AccentFor(const fx::Rule &r, const uint32_t *slotPalMem, int slot) {
	if (const fx::SlotOv *ov = fx::findOv(r, slot)) if (ov->hasAccent) return ov->accent;
	uint32_t pal[256]; ToRuntimePalette(slotPalMem, pal);
	return fx::autoAccent(slotPalMem ? pal : nullptr, r.accentIdx);
}

void BodyIndexHistogram(const Bank &bank, std::vector<uint32_t> &h) {
	h.assign(256, 0);
	for (size_t n = 0; n < bank.images.size(); n++) {
		if (!bank.images[n].present || bank.images[n].type != 0) continue;
		std::vector<uint8_t> idx; uint32_t pal[256];
		if (!bank.decodeIndexed((int)n, idx, pal)) continue;
		for (uint8_t i : idx) h[i]++;
	}
	h[0] = 0;
}

std::vector<AccentCandidate> SuggestAccentIndices(const std::vector<uint32_t> &hist, const std::vector<const uint32_t *> &slotPals, int maxN) {
	std::vector<AccentCandidate> out;
	if (hist.size() < 256 || slotPals.empty()) return out;
	for (int i = 1; i < 256; i++) {
		if (!hist[i]) continue;
		float vivid = 0, mean[3] = {}, sq[3] = {}; int n = 0;
		for (const uint32_t *p : slotPals) {
			if (!p) continue;
			const uint32_t c = p[i];
			const float ch[3] = { (c & 255) / 255.f, ((c >> 8) & 255) / 255.f, ((c >> 16) & 255) / 255.f };
			float h, s, v; fx::rgb2hsv({ ch[0], ch[1], ch[2] }, h, s, v);
			vivid += (v < 0.35f || s < 0.35f) ? 0.f : s * v; n++;
			for (int k = 0; k < 3; k++) { mean[k] += ch[k]; sq[k] += ch[k] * ch[k]; }
		}
		if (!n || vivid <= 0) continue;
		float var = 0; for (int k = 0; k < 3; k++) { const float m = mean[k] / (float)n; var += sq[k] / (float)n - m * m; }
		// vivid in the slots, actually changes between slots (an accent that never differs recolours nothing), and really used by the body
		out.push_back({ i, (vivid / (float)n) * (0.05f + std::sqrt(std::max(var, 0.f))) * std::log(1.f + (float)hist[i]) });
	}
	std::sort(out.begin(), out.end(), [](const AccentCandidate &a, const AccentCandidate &b) { return a.score > b.score; });
	if ((int)out.size() > maxN) out.resize((size_t)maxN);
	return out;
}

const fx::Rule *RuleFor(const fx::CharRules &c, int pattern, int sprite, int slot, int blend) {
	return fx::findRule(c, pattern, sprite, slot, false, blend);
}

const fx::Rule *RuleOfPattern(const fx::CharRules &c, const FxPatternRow &row, int slot, int *covered) {
	std::map<const fx::Rule *, int> hits;
	for (int s : row.fixedSprites) {
		const fx::Rule *r = RuleFor(c, row.pattern, s, slot, 1);
		if (!r) r = RuleFor(c, row.pattern, s, slot, 2);
		if (r) hits[r]++;
	}
	const fx::Rule *best = nullptr; int bn = 0;
	for (auto &kv : hits) if (kv.second > bn) { bn = kv.second; best = kv.first; }
	if (covered) *covered = bn;
	return best;
}

bool RecolorImage(const Bank &bank, int image, const fx::Rule *rule, int slot, const uint32_t *slotPalMem, Rgba &out) {
	if (image < 0 || image >= (int)bank.images.size()) return false;
	const int type = bank.images[image].type;
	uint32_t slotPal[256];
	const uint32_t *palArg = nullptr;
	if (type == 0 && slotPalMem) {
		memcpy(slotPal, slotPalMem, 1024);
		for (int i = 0; i < 256; i++) slotPal[i] = (slotPal[i] & 0xFFFFFF) | ((slotPal[i] >> 24) ? 0xFF000000u : 0);
		slotPal[0] = 0; palArg = slotPal;
	}
	if (!bank.decode(image, out, palArg)) return false;
	if (!rule || type == 0) return true;
	uint32_t rpal[256]; ToRuntimePalette(slotPalMem, rpal);
	fx::Packed p; fx::pack(*rule, slot, slotPalMem ? rpal : nullptr, p);
	if (rule->kind != fx::Kind::Hsv) {}
	for (size_t i = 0; i + 3 < out.px.size(); i += 4) {
		if (!out.px[i + 3]) continue;
		const fx::Rgb c = fx::applyCpu(p, { out.px[i] / 255.0f, out.px[i + 1] / 255.0f, out.px[i + 2] / 255.0f });
		out.px[i] = (uint8_t)(fx::clamp01(c.r) * 255 + 0.5f); out.px[i + 1] = (uint8_t)(fx::clamp01(c.g) * 255 + 0.5f); out.px[i + 2] = (uint8_t)(fx::clamp01(c.b) * 255 + 0.5f);
	}
	return true;
}

int FindRuleIndex(const fx::CharRules &c, const std::string &id) {
	for (size_t i = 0; i < c.rules.size(); i++) if (c.rules[i].id == id) return (int)i;
	return -1;
}
std::string UniqueRuleId(const fx::CharRules &c, const std::string &base) {
	if (FindRuleIndex(c, base) < 0) return base;
	for (int k = 2;; k++) { std::string s = base + "_" + std::to_string(k); if (FindRuleIndex(c, s) < 0) return s; }
}

void AssignPatterns(fx::CharRules &c, int ruleIdx, const std::vector<int> &patterns) {
	if (ruleIdx < 0 || ruleIdx >= (int)c.rules.size()) return;
	for (size_t i = 0; i < c.rules.size(); i++) {
		auto &l = c.rules[i].patterns;
		if ((int)i == ruleIdx || l.empty()) continue;
		l.erase(std::remove_if(l.begin(), l.end(), [&](int p) { return std::find(patterns.begin(), patterns.end(), p) != patterns.end(); }), l.end());
		if (l.empty()) c.rules[i].enabled = false;   // emptied: an empty list means "any pattern", which it must not become
	}
	auto &dst = c.rules[ruleIdx].patterns;
	for (int p : patterns) if (std::find(dst.begin(), dst.end(), p) == dst.end()) dst.push_back(p);
	std::sort(dst.begin(), dst.end());
}

fx::Rule DefaultRule(const std::string &id) {
	fx::Rule r; r.id = id; r.kind = fx::Kind::LumRamp; r.bank = 1;
	std::string err; fx::parseStops("#000000 accent.dark accent accent.light #ffffff", r.ramp);
	return r;
}

int LoadRules(const std::string &text, fx::CharRules &out, std::string &err) { return fx::parseIni(text, out, err); }
std::string SaveRules(const fx::CharRules &c) { return fx::writeIni(c); }

static std::string RuleBody(const fx::Rule &r) {
	fx::CharRules c; c.rules.push_back(r); std::string s = fx::writeIni(c);
	const size_t p = s.find("[rule"); return p == std::string::npos ? s : s.substr(p);
}

std::string DiffRules(const fx::CharRules &a, const fx::CharRules &b) {
	std::string o;
	for (const fx::Rule &rb : b.rules) {
		const int i = FindRuleIndex(a, rb.id);
		if (i < 0) { o += "+ added rule " + rb.id + "\n"; continue; }
		if (RuleBody(a.rules[i]) != RuleBody(rb)) {
			const fx::Rule &ra = a.rules[i];
			o += "~ changed rule " + rb.id + ":";
			if (ra.patterns != rb.patterns) o += " patterns(" + std::to_string(ra.patterns.size()) + "->" + std::to_string(rb.patterns.size()) + ")";
			if (ra.sprites != rb.sprites) o += " sprites";
			if (ra.kind != rb.kind) o += " kind";
			if (fx::stopsText(ra.ramp) != fx::stopsText(rb.ramp)) o += " ramp";
			if (ra.ov.size() != rb.ov.size()) o += " slot-overrides";
			o += "\n";
		}
	}
	for (const fx::Rule &ra : a.rules) if (FindRuleIndex(b, ra.id) < 0) o += "- removed rule " + ra.id + "\n";
	if (o.empty()) o = "(no changes)\n";
	return o;
}

static const char kDgvAkiha[] =
#include "fxr_dgv_akiha.inc"
;
const char *DgvAkihaIni() { return kDgvAkiha; }

} // namespace cgm
