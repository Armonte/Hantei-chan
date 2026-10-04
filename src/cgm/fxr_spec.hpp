#pragma once
// FxRecolor.hpp — [mbaacc/fxrecolor] the portable model of the MBAACC effect-sprite runtime recolour.
//
// WHAT IT IS. A per-pattern colour remap applied to effect quads at DRAW time, by a pixel shader. It never touches
// simulation memory: the rule lookup reads (owner character, owner palette slot, pattern id, sprite id, blend preset)
// and the draw reads the quad's own texel colour. Peers may run different rule sets; nothing here is hashed.
// Design + data: hantei-chan docs/cg/effect_recolor_design.md (section 6), docs/cg/effect_recolor_data/.
//
// THIS HEADER holds everything that can be tested without a game: the ini grammar, the rule match, the ramp/accent
// maths, the packing of a rule into the shader constants, and a CPU reference of the shader (applyCpu) that the unit
// test and the F3 preview share. The live half (hooks, D3DX effect, palette read) is MbaaccSim_FxRecolor.cpp.
//
// INI (one file per character: <game>\fxrecolor\<char>.ini, char = lowercase roster name without dots: arc, akiha, aoko ...)
//   [rule arc_blades]            ; one section per rule, first match wins (file order)
//   patterns = 396,397,404       ; pattern ids (HA6 pattern index). empty/absent = any
//   sprites  = 804,805           ; sprite (image) ids. empty = any
//   slots    = 0,1,2             ; owner palette slots 0..35. empty/* = any
//   bank     = any|char|effect   ; effect = the object belongs to effect.ha6 (obj char id != owner's)
//   blend    = any|1|2|3|4       ; the game's blend preset of the quad (1 normal, 2 additive, 3 subtract ...)
//   kind     = lumramp|rainbow|hsv
//   by       = max|luma          ; lumramp/rainbow: which channel measure drives the ramp
//   vrange   = 0.0,1.0           ; lumramp: source range mapped onto the ramp ends
//   ramp     = #000000 accent.dark accent accent.light #ffffff     ; stops, evenly spaced, or colour@pos
//              colour tokens: #rrggbb | accent | accent.dark | accent.light  (accent = the owner slot's body colour)
//   accent_idx = 12,13           ; palette indices the AUTO accent is taken from (default: most saturated vivid entry)
//   hsv      = 40,1.0,1.0        ; hue shift in degrees, saturation x, value x   (kind = hsv)
//   slot.3.accent = #ff8800      ; per-slot OVERRIDE of the accent colour
//   slot.3.ramp   = #000 #f80 #fff   ; per-slot OVERRIDE of the whole ramp
//   enabled  = 1
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace mbaacc::fxr {

constexpr int kRampN = 8;      // ramp entries uploaded to the shader (linear between them)
constexpr int kMaxSlots = 36;

struct Rgb { float r = 0, g = 0, b = 0; };

enum class Kind : int { LumRamp = 1, Rainbow = 2, Hsv = 3 };
enum class ColTok : int { Literal = 0, Accent = 1, AccentDark = 2, AccentLight = 3 };

struct Stop { float pos = 0; ColTok tok = ColTok::Literal; Rgb c; bool hasPos = false; };

struct SlotOv {
    int slot = -1;
    bool hasAccent = false; Rgb accent;
    bool hasRamp = false;   std::vector<Stop> ramp;
};

struct Rule {
    std::string id;
    bool enabled = true;
    std::vector<int> patterns, sprites, slots;   // empty = any
    int bank = 0;                                // 0 any, 1 character bank, 2 effect.ha6
    int blend = -1;                              // -1 any
    Kind kind = Kind::LumRamp;
    bool byLuma = false;
    float vmin = 0.0f, vmax = 1.0f;
    std::vector<Stop> ramp;
    std::vector<int> accentIdx;
    float hueDeg = 0.0f, sat = 1.0f, val = 1.0f;
    std::vector<SlotOv> ov;
};

struct CharRules {
    std::string name;
    std::vector<Rule> rules;
};

// ---- the shader constants of one resolved rule + slot -------------------------------------------------------
struct Packed {
    float mode[4] = {0, 0, 0, 1};   // x: 1 lumramp / 2 rainbow / 3 hsv, y: by (0 max, 1 luma), z: vmin, w: 1/(vmax-vmin)
    float hsv[4]  = {0, 1, 1, 0};   // x: hue shift in turns, y: saturation x, z: value x
    float ramp[kRampN][4] = {};     // rgb + 1
    bool operator==(const Packed& o) const { return std::memcmp(this, &o, sizeof *this) == 0; }
};

inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline Rgb mix(Rgb a, Rgb b, float t) { return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; }

// ---- colour maths ----------------------------------------------------------------------------------------------
inline void rgb2hsv(Rgb c, float& h, float& s, float& v) {
    const float mx = std::max(c.r, std::max(c.g, c.b)), mn = std::min(c.r, std::min(c.g, c.b)), d = mx - mn;
    v = mx; s = mx > 1e-6f ? d / mx : 0.0f; h = 0.0f;
    if (d > 1e-6f) {
        if (mx == c.r) h = std::fmod((c.g - c.b) / d, 6.0f);
        else if (mx == c.g) h = (c.b - c.r) / d + 2.0f;
        else h = (c.r - c.g) / d + 4.0f;
        h /= 6.0f; if (h < 0) h += 1.0f;
    }
}
inline Rgb hsv2rgb(float h, float s, float v) {
    h = h - std::floor(h);
    const float r = clamp01(std::fabs(h * 6.0f - 3.0f) - 1.0f), g = clamp01(2.0f - std::fabs(h * 6.0f - 2.0f)),
                b = clamp01(2.0f - std::fabs(h * 6.0f - 4.0f));
    return { v * (1 + s * (r - 1)), v * (1 + s * (g - 1)), v * (1 + s * (b - 1)) };
}
inline float luma(Rgb c) { return 0.299f * c.r + 0.587f * c.g + 0.114f * c.b; }

// The ramp as the shader sees it: kRampN entries, hat-weighted (== linear interpolation between neighbours).
inline Rgb rampAt(const float (*r)[4], float t) {          // t in 0..1 over the entries (clamped ends)
    const float x = clamp01(t) * (kRampN - 1);
    Rgb o;
    for (int k = 0; k < kRampN; ++k) {
        const float w = clamp01(1.0f - std::fabs(x - (float)k));
        o.r += r[k][0] * w; o.g += r[k][1] * w; o.b += r[k][2] * w;
    }
    return o;
}
inline Rgb rampCyclic(const float (*r)[4], float h) {     // h = hue in turns; entry k sits at hue k/kRampN
    const float x = (h - std::floor(h)) * kRampN;
    Rgb o;
    for (int k = 0; k < kRampN; ++k) {
        float d = std::fabs(x - (float)k); d = std::min(d, (float)kRampN - d);
        const float w = clamp01(1.0f - d);
        o.r += r[k][0] * w; o.g += r[k][1] * w; o.b += r[k][2] * w;
    }
    return o;
}

// CPU reference of the pixel shader's colour step (alpha and blend are never touched by the recolour).
inline Rgb applyCpu(const Packed& p, Rgb src) {
    const int mode = (int)(p.mode[0] + 0.5f);
    if (mode == 1) {
        const float m = p.mode[1] > 0.5f ? luma(src) : std::max(src.r, std::max(src.g, src.b));
        return rampAt(p.ramp, clamp01((m - p.mode[2]) * p.mode[3]));
    }
    float h, s, v; rgb2hsv(src, h, s, v);
    if (mode == 2) {
        Rgb t = rampCyclic(p.ramp, h);
        const float tm = std::max(1e-4f, std::max(t.r, std::max(t.g, t.b)));
        const float k = v / tm;
        Rgb col{ t.r * k, t.g * k, t.b * k };
        const float sat = clamp01(s * p.hsv[1]);   // near-white pixels stay near-white
        return { v + (col.r - v) * sat, v + (col.g - v) * sat, v + (col.b - v) * sat };
    }
    return hsv2rgb(h + p.hsv[0], clamp01(s * p.hsv[1]), clamp01(v * p.hsv[2]));
}

// ---- accent: the owner slot's body colour, AUTO from the character palette --------------------------------------
// pal = 256 dwords 0xAARRGGBB (the layout the game keeps at CG+0x10). With explicit indices the first valid one wins; with none the most
// vivid entry (saturation x value, ignoring near-grey and near-black) is taken. Deterministic, pure.
inline Rgb dwordRgb(uint32_t d) { return { ((d >> 16) & 255) / 255.0f, ((d >> 8) & 255) / 255.0f, (d & 255) / 255.0f }; }
inline Rgb autoAccent(const uint32_t* pal, const std::vector<int>& idx, bool* found = nullptr) {
    if (found) *found = false;
    if (!pal) return { 0.8f, 0.2f, 0.2f };
    if (!idx.empty()) {
        for (int i : idx) if (i >= 0 && i < 256) { if (found) *found = true; return dwordRgb(pal[i]); }
    }
    float best = -1; Rgb bc{ 0.8f, 0.2f, 0.2f };
    for (int i = 1; i < 256; ++i) {
        const Rgb c = dwordRgb(pal[i]);
        float h, s, v; rgb2hsv(c, h, s, v);
        if (v < 0.35f || s < 0.35f) continue;
        const float sc = s * v;
        if (sc > best + 1e-6f) { best = sc; bc = c; if (found) *found = true; }
    }
    return bc;
}

inline Rgb resolveStop(const Stop& s, Rgb accent) {
    switch (s.tok) {
        case ColTok::Accent:      return accent;
        case ColTok::AccentDark:  return { accent.r * 0.35f, accent.g * 0.35f, accent.b * 0.35f };
        case ColTok::AccentLight: return mix(accent, Rgb{ 1, 1, 1 }, 0.6f);
        default: return s.c;
    }
}

// Evaluate a stop list into kRampN evenly spaced entries (stops without @pos are spread evenly).
inline void evalRamp(const std::vector<Stop>& stops, Rgb accent, float (*out)[4]) {
    std::vector<std::pair<float, Rgb>> st;
    const size_t n = stops.size();
    for (size_t i = 0; i < n; ++i) {
        const float pos = stops[i].hasPos ? stops[i].pos : (n > 1 ? (float)i / (float)(n - 1) : 0.0f);
        st.push_back({ clamp01(pos), resolveStop(stops[i], accent) });
    }
    std::stable_sort(st.begin(), st.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (int k = 0; k < kRampN; ++k) {
        const float t = (float)k / (float)(kRampN - 1);
        Rgb c{ 0, 0, 0 };
        if (!st.empty()) {
            if (t <= st.front().first) c = st.front().second;
            else if (t >= st.back().first) c = st.back().second;
            else for (size_t i = 1; i < st.size(); ++i)
                if (t <= st[i].first) {
                    const float span = st[i].first - st[i - 1].first;
                    c = mix(st[i - 1].second, st[i].second, span > 1e-6f ? (t - st[i - 1].first) / span : 1.0f);
                    break;
                }
        }
        out[k][0] = c.r; out[k][1] = c.g; out[k][2] = c.b; out[k][3] = 1.0f;
    }
}

inline const SlotOv* findOv(const Rule& r, int slot) {
    for (const SlotOv& o : r.ov) if (o.slot == slot) return &o;
    return nullptr;
}

// Resolve a rule for an owner slot + its palette into the shader constants.
inline void pack(const Rule& r, int slot, const uint32_t* pal, Packed& p) {
    p = Packed{};
    const SlotOv* ov = findOv(r, slot);
    Rgb accent = (ov && ov->hasAccent) ? ov->accent : autoAccent(pal, r.accentIdx);
    p.mode[0] = (float)(int)r.kind;
    p.mode[1] = r.byLuma ? 1.0f : 0.0f;
    p.mode[2] = r.vmin;
    p.mode[3] = 1.0f / std::max(1e-4f, r.vmax - r.vmin);
    p.hsv[0] = r.hueDeg / 360.0f; p.hsv[1] = r.sat; p.hsv[2] = r.val;
    const std::vector<Stop>& stops = (ov && ov->hasRamp) ? ov->ramp : r.ramp;
    evalRamp(stops, accent, p.ramp);
}

// ---- match -------------------------------------------------------------------------------------------------------
inline bool inList(const std::vector<int>& l, int v) { return l.empty() || std::find(l.begin(), l.end(), v) != l.end(); }

inline const Rule* findRule(const CharRules& c, int pattern, int sprite, int slot, bool effectBank, int blend) {
    for (const Rule& r : c.rules) {
        if (!r.enabled) continue;
        if (!inList(r.patterns, pattern) || !inList(r.sprites, sprite) || !inList(r.slots, slot)) continue;
        if (r.bank == 1 && effectBank) continue;
        if (r.bank == 2 && !effectBank) continue;
        if (r.blend >= 0 && r.blend != blend) continue;
        return &r;
    }
    return nullptr;
}

// ---- ini ---------------------------------------------------------------------------------------------------------
inline std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}
inline std::string lower(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }

inline bool parseHex(const std::string& t, Rgb& out) {
    if (t.size() < 2 || t[0] != '#') return false;
    std::string h = t.substr(1);
    if (h.size() == 3) h = std::string{ h[0], h[0], h[1], h[1], h[2], h[2] };
    if (h.size() != 6) return false;
    char* end = nullptr;
    const unsigned long v = std::strtoul(h.c_str(), &end, 16);
    if (*end) return false;
    out = { ((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f };
    return true;
}
inline bool parseStops(const std::string& v, std::vector<Stop>& out) {
    out.clear();
    size_t i = 0;
    while (i < v.size()) {
        while (i < v.size() && (v[i] == ' ' || v[i] == '\t' || v[i] == ',')) ++i;
        size_t j = i;
        while (j < v.size() && v[j] != ' ' && v[j] != '\t' && v[j] != ',') ++j;
        if (j == i) break;
        std::string tok = lower(v.substr(i, j - i));
        i = j;
        Stop s;
        const size_t at = tok.find('@');
        if (at != std::string::npos) { s.hasPos = true; s.pos = (float)std::atof(tok.c_str() + at + 1); tok = tok.substr(0, at); }
        if (tok == "accent") s.tok = ColTok::Accent;
        else if (tok == "accent.dark") s.tok = ColTok::AccentDark;
        else if (tok == "accent.light") s.tok = ColTok::AccentLight;
        else if (!parseHex(tok, s.c)) return false;
        out.push_back(s);
    }
    return !out.empty();
}
inline std::vector<int> parseInts(const std::string& v) {
    std::vector<int> o;
    if (trim(v) == "*") return o;
    size_t i = 0;
    while (i < v.size()) {
        while (i < v.size() && (v[i] == ' ' || v[i] == ',' || v[i] == '\t')) ++i;
        size_t j = i;
        while (j < v.size() && v[j] != ' ' && v[j] != ',' && v[j] != '\t') ++j;
        if (j == i) break;
        const std::string t = v.substr(i, j - i);
        const size_t dash = t.find('-', 1);
        if (dash != std::string::npos) {   // a-b range
            const int a = std::atoi(t.c_str()), b = std::atoi(t.c_str() + dash + 1);
            for (int k = a; k <= b && k - a < 4096; ++k) o.push_back(k);
        } else o.push_back(std::atoi(t.c_str()));
        i = j;
    }
    return o;
}
inline std::string joinInts(const std::vector<int>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) { if (i) s += ','; s += std::to_string(v[i]); }
    return s;
}
inline std::string hexOf(Rgb c) {
    char b[16];
    std::snprintf(b, sizeof b, "#%02x%02x%02x", (int)(clamp01(c.r) * 255 + 0.5f), (int)(clamp01(c.g) * 255 + 0.5f), (int)(clamp01(c.b) * 255 + 0.5f));
    return b;
}
inline std::string stopsText(const std::vector<Stop>& st) {
    std::string s;
    for (const Stop& x : st) {
        if (!s.empty()) s += ' ';
        s += x.tok == ColTok::Accent ? "accent" : x.tok == ColTok::AccentDark ? "accent.dark" : x.tok == ColTok::AccentLight ? "accent.light" : hexOf(x.c);
        if (x.hasPos) { char b[16]; std::snprintf(b, sizeof b, "@%.3f", x.pos); s += b; }
    }
    return s;
}

// Parse one character file. Unknown keys and malformed values are reported in `err` (one line each) and skipped; a rule
// with no ramp (lumramp/rainbow) is dropped. Returns the number of rules kept.
inline int parseIni(const std::string& text, CharRules& out, std::string& err) {
    out.rules.clear();
    Rule* cur = nullptr;
    size_t pos = 0; int lineNo = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(pos, nl - pos);
        pos = nl + 1; ++lineNo;
        const size_t sc = line.find(';');
        if (sc != std::string::npos) line = line.substr(0, sc);
        line = trim(line);
        if (line.empty()) continue;
        char eb[160];
        if (line[0] == '[') {
            const size_t e = line.find(']');
            const std::string h = e == std::string::npos ? "" : lower(trim(line.substr(1, e - 1)));
            if (h.rfind("rule", 0) == 0) { out.rules.emplace_back(); cur = &out.rules.back(); cur->id = trim(h.substr(4)); if (cur->id.empty()) cur->id = "rule" + std::to_string(out.rules.size()); }
            else { cur = nullptr; std::snprintf(eb, sizeof eb, "line %d: unknown section '%s'\n", lineNo, line.c_str()); err += eb; }
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos || !cur) { std::snprintf(eb, sizeof eb, "line %d: ignored '%s'\n", lineNo, line.c_str()); err += eb; continue; }
        const std::string k = lower(trim(line.substr(0, eq))), v = trim(line.substr(eq + 1));
        bool ok = true;
        if (k == "patterns") cur->patterns = parseInts(v);
        else if (k == "sprites") cur->sprites = parseInts(v);
        else if (k == "slots") cur->slots = parseInts(v);
        else if (k == "accent_idx") cur->accentIdx = parseInts(v);
        else if (k == "enabled") cur->enabled = std::atoi(v.c_str()) != 0;
        else if (k == "bank") { const std::string l = lower(v); cur->bank = l == "char" ? 1 : l == "effect" ? 2 : 0; ok = l == "char" || l == "effect" || l == "any"; }
        else if (k == "blend") cur->blend = lower(v) == "any" ? -1 : std::atoi(v.c_str());
        else if (k == "kind") { const std::string l = lower(v); cur->kind = l == "rainbow" ? Kind::Rainbow : l == "hsv" ? Kind::Hsv : Kind::LumRamp; ok = l == "lumramp" || l == "rainbow" || l == "hsv"; }
        else if (k == "by") cur->byLuma = lower(v) == "luma";
        else if (k == "vrange") { float a = 0, b = 1; ok = std::sscanf(v.c_str(), "%f%*[, ]%f", &a, &b) == 2; if (ok) { cur->vmin = a; cur->vmax = b; } }
        else if (k == "hsv") { float a = 0, s = 1, w = 1; ok = std::sscanf(v.c_str(), "%f%*[, ]%f%*[, ]%f", &a, &s, &w) == 3; if (ok) { cur->hueDeg = a; cur->sat = s; cur->val = w; } }
        else if (k == "ramp") ok = parseStops(v, cur->ramp);
        else if (k.rfind("slot.", 0) == 0) {
            const size_t d2 = k.find('.', 5);
            const int sl = std::atoi(k.c_str() + 5);
            const std::string sub = d2 == std::string::npos ? "" : k.substr(d2 + 1);
            SlotOv* o = nullptr;
            for (SlotOv& x : cur->ov) if (x.slot == sl) o = &x;
            if (!o) { cur->ov.emplace_back(); o = &cur->ov.back(); o->slot = sl; }
            if (sub == "accent") { ok = parseHex(lower(v), o->accent); o->hasAccent = ok; }
            else if (sub == "ramp") { ok = parseStops(v, o->ramp); o->hasRamp = ok; }
            else ok = false;
        } else ok = false;
        if (!ok) { std::snprintf(eb, sizeof eb, "line %d: bad or unknown key/value '%s'\n", lineNo, line.c_str()); err += eb; }
    }
    // drop rules that cannot run
    for (size_t i = out.rules.size(); i-- > 0;) {
        const Rule& r = out.rules[i];
        if (r.kind != Kind::Hsv && r.ramp.empty()) { err += "rule '" + r.id + "': no ramp, dropped\n"; out.rules.erase(out.rules.begin() + (long)i); }
    }
    return (int)out.rules.size();
}

// The same rules back as ini text (the F3 Save). Comments of a hand-edited file are NOT kept; Save writes a fresh file.
inline std::string writeIni(const CharRules& c) {
    std::string s = "; PovertyCaster MBAACC effect recolour (presentation only, never in the simulation).\n; See pc-adapters/mbaacc/include/mbaacc/FxRecolor.hpp for the grammar.\n";
    for (const Rule& r : c.rules) {
        s += "\n[rule " + r.id + "]\n";
        if (!r.enabled) s += "enabled = 0\n";
        if (!r.patterns.empty()) s += "patterns = " + joinInts(r.patterns) + "\n";
        if (!r.sprites.empty()) s += "sprites = " + joinInts(r.sprites) + "\n";
        if (!r.slots.empty()) s += "slots = " + joinInts(r.slots) + "\n";
        if (r.bank) s += std::string("bank = ") + (r.bank == 1 ? "char" : "effect") + "\n";
        if (r.blend >= 0) s += "blend = " + std::to_string(r.blend) + "\n";
        s += std::string("kind = ") + (r.kind == Kind::Rainbow ? "rainbow" : r.kind == Kind::Hsv ? "hsv" : "lumramp") + "\n";
        char b[96];
        if (r.kind != Kind::Hsv) {
            s += std::string("by = ") + (r.byLuma ? "luma" : "max") + "\n";
            std::snprintf(b, sizeof b, "vrange = %.3f,%.3f\n", r.vmin, r.vmax); s += b;
            s += "ramp = " + stopsText(r.ramp) + "\n";
        } else { std::snprintf(b, sizeof b, "hsv = %.1f,%.3f,%.3f\n", r.hueDeg, r.sat, r.val); s += b; }
        if (!r.accentIdx.empty()) s += "accent_idx = " + joinInts(r.accentIdx) + "\n";
        for (const SlotOv& o : r.ov) {
            if (o.hasAccent) s += "slot." + std::to_string(o.slot) + ".accent = " + hexOf(o.accent) + "\n";
            if (o.hasRamp) s += "slot." + std::to_string(o.slot) + ".ramp = " + stopsText(o.ramp) + "\n";
        }
    }
    return s;
}

} // namespace mbaacc::fxr
