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

constexpr int kRampN = 16;     // ramp entries (OKLAB L,a,b) uploaded to the shader, linear between them
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
    int by = 0;                                  // lumramp/rainbow source measure: 0 oklab lightness, 1 max sRGB channel, 2 luma
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
    float mode[4] = {0, 0, 0, 1};   // x: 1 lumramp / 2 rainbow / 3 okhsv, y: by (0 lightness, 1 max, 2 luma), z: vmin, w: 1/(vmax-vmin)
    float hsv[4]  = {0, 1, 1, 0};   // x: hue shift in turns, y: saturation x (chroma x for rainbow), z: value x
    float ramp[kRampN][4] = {};     // OKLAB L, a, b, 1
    bool operator==(const Packed& o) const { return std::memcmp(this, &o, sizeof *this) == 0; }
};

inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline Rgb mix(Rgb a, Rgb b, float t) { return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t }; }

// The CPU reference runs in double (exact round trips); the shader is float32 and is expected to agree to ~1e-3, except on very dark
// fully saturated blues where float32 OkHSV loses a few 1e-2 near s = 1 (a property of the model at the gamut edge, not of the port).
// ---- colour maths: sRGB <-> linear <-> Oklab, OkHSV (Bjorn Ottosson, https://bottosson.github.io/posts/oklab/ and /posts/colorpicker/) ----
// MIRRORED line for line by the HLSL in MbaaccSim_FxRecolor.cpp (same constants, same branch structure). Texels are sRGB-encoded 8-bit
// values read as-is (no sRGB texture read), so every edit decodes with the exact piecewise curve first and encodes on the way out.
// Alpha never enters any of it.
struct Lab { double L = 0, a = 0, b = 0; };

inline double srgbToLinear(double x) { return x >= 0.04045 ? std::pow((x + 0.055) / 1.055, 2.4) : x / 12.92; }
inline double linearToSrgb(double x) { return x >= 0.0031308 ? 1.055 * std::pow(x, 1.0 / 2.4) - 0.055 : 12.92 * x; }

inline Lab linearToOklab(double r, double g, double b) {
    const double l = 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b;
    const double m = 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b;
    const double s = 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b;
    const double l_ = std::cbrt(l), m_ = std::cbrt(m), s_ = std::cbrt(s);
    return { 0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_,
             1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_,
             0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_ };
}
inline void oklabToLinear(Lab c, double& r, double& g, double& b) {
    const double l_ = c.L + 0.3963377774 * c.a + 0.2158037573 * c.b;
    const double m_ = c.L - 0.1055613458 * c.a - 0.0638541728 * c.b;
    const double s_ = c.L - 0.0894841775 * c.a - 1.2914855480 * c.b;
    const double l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
    r = +4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s;
    g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s;
    b = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s;
}
inline Lab srgbToOklab(Rgb c) { return linearToOklab(srgbToLinear(c.r), srgbToLinear(c.g), srgbToLinear(c.b)); }
inline Rgb oklabToSrgb(Lab c) {   // clamped to the sRGB gamut per channel (the shader does the same)
    double r, g, b; oklabToLinear(c, r, g, b);
    auto q = [](double v) { v = linearToSrgb(std::max(v, 0.0)); return (float)(v < 0 ? 0 : (v > 1 ? 1 : v)); };
    return { q(r), q(g), q(b) };
}

// Largest saturation (C/L at L=1 scaling) of hue direction (a, b) inside sRGB: Ottosson's polynomial + one Halley step.
inline double computeMaxSaturation(double a, double b) {
    double k0, k1, k2, k3, k4, wl, wm, ws;
    if (-1.88170328 * a - 0.80936493 * b > 1.0) { k0 = 1.19086277; k1 = 1.76576728; k2 = 0.59662641; k3 = 0.75515197; k4 = 0.56771245; wl = 4.0767416621; wm = -3.3077115913; ws = 0.2309699292; }
    else if (1.81444104 * a - 1.19445276 * b > 1.0) { k0 = 0.73956515; k1 = -0.45954404; k2 = 0.08285427; k3 = 0.12541070; k4 = 0.14503204; wl = -1.2684380046; wm = 2.6097574011; ws = -0.3413193965; }
    else { k0 = 1.35733652; k1 = -0.00915799; k2 = -1.15130210; k3 = -0.50559606; k4 = 0.00692167; wl = -0.0041960863; wm = -0.7034186147; ws = 1.7076147010; }
    double S = k0 + k1 * a + k2 * b + k3 * a * a + k4 * a * b;
    const double kl = 0.3963377774 * a + 0.2158037573 * b, km = -0.1055613458 * a - 0.0638541728 * b, ks = -0.0894841775 * a - 1.2914855480 * b;
    const double l_ = 1.0 + S * kl, m_ = 1.0 + S * km, s_ = 1.0 + S * ks;
    const double l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
    const double l1 = 3.0 * kl * l_ * l_, m1 = 3.0 * km * m_ * m_, s1 = 3.0 * ks * s_ * s_;
    const double l2 = 6.0 * kl * kl * l_, m2 = 6.0 * km * km * m_, s2 = 6.0 * ks * ks * s_;
    const double f = wl * l + wm * m + ws * s, f1 = wl * l1 + wm * m1 + ws * s1, f2 = wl * l2 + wm * m2 + ws * s2;
    return S - f * f1 / (f1 * f1 - 0.5 * f * f2);
}
// (L, C) of the gamut cusp for hue direction (a, b); returned as the ST pair (C/L, C/(1-L)).
inline void cuspST(double a, double b, double& Smax, double& Tmax) {
    const double S = computeMaxSaturation(a, b);
    double r, g, bl; oklabToLinear({ 1.0, S * a, S * b }, r, g, bl);
    const double Lc = std::cbrt(1.0 / std::max(std::max(r, g), bl));
    const double Cc = Lc * S;
    Smax = Cc / Lc; Tmax = Cc / (1.0 - Lc);
}
inline double okToe(double x) { const double k1 = 0.206, k2 = 0.03, k3 = (1.0 + k1) / (1.0 + k2); const double t = k3 * x - k1; return 0.5 * (t + std::sqrt(t * t + 4.0 * k2 * k3 * x)); }
inline double okToeInv(double x) { const double k1 = 0.206, k2 = 0.03, k3 = (1.0 + k1) / (1.0 + k2); return (x * x + k1 * x) / (k3 * (x + k2)); }

struct Hsv { double h = 0, s = 0, v = 0; };   // OkHSV: h in turns 0..1, s and v 0..1
inline Hsv srgbToOkhsv(Rgb c) {
    const Lab lab = srgbToOklab(c);
    if (lab.L < 1e-6) return { 0.0, 0.0, 0.0 };   // black: hue/saturation undefined, and toe(L)/L would be 0/0
    double C = std::sqrt(lab.a * lab.a + lab.b * lab.b);
    const double a_ = C > 1e-6 ? lab.a / C : 1.0, b_ = C > 1e-6 ? lab.b / C : 0.0;
    double L = lab.L;
    const double h = 0.5 + 0.5 * std::atan2(-lab.b, -lab.a) / 3.14159265358979;
    double S_max, T_max; cuspST(a_, b_, S_max, T_max);
    const double S_0 = 0.5, k = 1.0 - S_0 / S_max;
    const double t = T_max / (C + L * T_max);
    const double L_v = t * L, C_v = t * C;
    const double L_vt = okToeInv(L_v), C_vt = C_v * L_vt / L_v;
    double r, g, b; oklabToLinear({ L_vt, a_ * C_vt, b_ * C_vt }, r, g, b);
    const double scaleL = std::cbrt(1.0 / std::max(std::max(r, g), std::max(b, 0.0)));
    L = L / scaleL; C = C / scaleL;
    C = C * okToe(L) / L; L = okToe(L);
    Hsv o;
    o.h = h; o.v = L / L_v;
    o.s = (S_0 + T_max) * C_v / ((T_max * S_0) + T_max * k * C_v);
    return o;
}
inline Rgb okhsvToSrgb(Hsv c) {
    if (c.v < 1e-6) return { 0.0f, 0.0f, 0.0f };
    const double a_ = std::cos(2.0 * 3.14159265358979 * c.h), b_ = std::sin(2.0 * 3.14159265358979 * c.h);
    double S_max, T_max; cuspST(a_, b_, S_max, T_max);
    const double S_0 = 0.5, k = 1.0 - S_0 / S_max;
    const double L_v = 1.0 - c.s * S_0 / (S_0 + T_max - T_max * k * c.s);
    const double C_v = c.s * T_max * S_0 / (S_0 + T_max - T_max * k * c.s);
    double L = c.v * L_v, C = c.v * C_v;
    const double L_vt = okToeInv(L_v), C_vt = C_v * L_vt / L_v;
    const double L_new = okToeInv(L);
    C = C * L_new / L; L = L_new;
    double r, g, b; oklabToLinear({ L_vt, a_ * C_vt, b_ * C_vt }, r, g, b);
    const double scaleL = std::cbrt(1.0 / std::max(std::max(r, g), std::max(b, 0.0)));
    L = L * scaleL; C = C * scaleL;
    return oklabToSrgb({ L, C * a_, C * b_ });
}


inline float luma(Rgb c) { return 0.299f * c.r + 0.587f * c.g + 0.114f * c.b; }

// The ramp as the shader sees it: kRampN OKLAB entries, hat-weighted (== linear interpolation between neighbours, in Oklab).
inline Lab rampAt(const float (*r)[4], float t) {
    const float x = clamp01(t) * (kRampN - 1);
    Lab o;
    for (int k = 0; k < kRampN; ++k) {
        const float w = clamp01(1.0f - std::fabs(x - (float)k));
        o.L += r[k][0] * w; o.a += r[k][1] * w; o.b += r[k][2] * w;
    }
    return o;
}
inline Lab rampCyclic(const float (*r)[4], float h) {     // h = hue in turns; entry k sits at hue k/kRampN (OkLCh hue of the colours)
    const float x = (h - std::floor(h)) * kRampN;
    Lab o;
    for (int k = 0; k < kRampN; ++k) {
        float d = std::fabs(x - (float)k); d = std::min(d, (float)kRampN - d);
        const float w = clamp01(1.0f - d);
        o.L += r[k][0] * w; o.a += r[k][1] * w; o.b += r[k][2] * w;
    }
    return o;
}

// CPU reference of the pixel shader's colour step (alpha and blend are never touched by the recolour).
// mode 1 lumramp: source measure -> Oklab ramp -> sRGB.   mode 2 rainbow: OkLCh hue replaced by the ramp's hue at the source hue, L and
// chroma kept (chroma x sat).   mode 3: OkHSV edit (hue shift, saturation x, value x).
inline Rgb applyCpu(const Packed& p, Rgb src) {
    const int mode = (int)(p.mode[0] + 0.5f);
    const int by = (int)(p.mode[1] + 0.5f);
    const Lab sl = srgbToOklab(src);
    if (mode == 1) {
        const float m = by == 0 ? sl.L : by == 2 ? luma(src) : std::max(src.r, std::max(src.g, src.b));
        return oklabToSrgb(rampAt(p.ramp, clamp01((m - p.mode[2]) * p.mode[3])));
    }
    if (mode == 2) {
        const float C = std::sqrt(sl.a * sl.a + sl.b * sl.b);
        const float h = 0.5f + 0.5f * std::atan2(-sl.b, -sl.a) / 3.14159265358979f;   // same hue turn as OkHSV
        const Lab t = rampCyclic(p.ramp, h);
        const float tc = std::max(std::sqrt(t.a * t.a + t.b * t.b), 1e-4);
        const float k = C * p.hsv[1] / tc;
        return oklabToSrgb({ sl.L, t.a * k, t.b * k });
    }
    Hsv hv = srgbToOkhsv(src);
    hv.h += p.hsv[0]; hv.h -= std::floor(hv.h);
    hv.s = clamp01(hv.s * p.hsv[1]); hv.v = clamp01(hv.v * p.hsv[2]);
    return okhsvToSrgb(hv);
}

// ---- accent: the owner slot's body colour, AUTO from the character palette --------------------------------------
// pal = 256 dwords 0xAABBGGRR (memory R,G,B,A: the layout the game keeps at CG+0x10; Akiha slot 0 entry 1 reads 01D0E0F8 = skin f8e0d0).
// With explicit indices the first valid one wins; with none the most chromatic entry (Oklab chroma, lightness 0.3..0.95) is taken; pure
// #00ff00 / magenta entries are unused-entry markers and are skipped. Deterministic, pure.
inline Rgb dwordRgb(uint32_t d) { return { (d & 255) / 255.0f, ((d >> 8) & 255) / 255.0f, ((d >> 16) & 255) / 255.0f }; }
inline Rgb autoAccent(const uint32_t* pal, const std::vector<int>& idx, bool* found = nullptr) {
    if (found) *found = false;
    if (!pal) return { 0.8f, 0.2f, 0.2f };
    for (int i : idx) if (i >= 0 && i < 256) { if (found) *found = true; return dwordRgb(pal[i]); }
    float best = -1; Rgb bc{ 0.8f, 0.2f, 0.2f };
    for (int i = 1; i < 256; ++i) {
        const Rgb c = dwordRgb(pal[i]);
        if (c.g > 0.98f && c.r < 0.02f && c.b < 0.02f) continue;
        if (c.r > 0.98f && c.g < 0.02f && c.b > 0.98f) continue;
        const Lab l = srgbToOklab(c);
        const float C = std::sqrt(l.a * l.a + l.b * l.b);
        if (l.L < 0.3f || l.L > 0.95f || C < 0.06f) continue;
        if (C > best + 1e-6f) { best = C; bc = c; if (found) *found = true; }
    }
    return bc;
}

// Stops resolve to Oklab (accent tokens are derived in Oklab: dark = L x0.4 and chroma x0.8, light = 60% of the way to white).
inline Lab resolveStop(const Stop& s, Rgb accent) {
    const Lab ac = srgbToOklab(accent);
    switch (s.tok) {
        case ColTok::Accent:      return ac;
        case ColTok::AccentDark:  return { ac.L * 0.4f, ac.a * 0.8f, ac.b * 0.8f };
        case ColTok::AccentLight: return { ac.L + (1.0f - ac.L) * 0.6f, ac.a * 0.4f, ac.b * 0.4f };
        default: return srgbToOklab(s.c);
    }
}

// Evaluate a stop list into kRampN evenly spaced OKLAB entries (interpolated in Oklab; stops without @pos are spread evenly).
inline void evalRamp(const std::vector<Stop>& stops, Rgb accent, float (*out)[4]) {
    std::vector<std::pair<float, Lab>> st;
    const size_t n = stops.size();
    for (size_t i = 0; i < n; ++i) {
        const float pos = stops[i].hasPos ? stops[i].pos : (n > 1 ? (float)i / (float)(n - 1) : 0.0f);
        st.push_back({ clamp01(pos), resolveStop(stops[i], accent) });
    }
    std::stable_sort(st.begin(), st.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (int k = 0; k < kRampN; ++k) {
        const float t = (float)k / (float)(kRampN - 1);
        Lab c;
        if (!st.empty()) {
            if (t <= st.front().first) c = st.front().second;
            else if (t >= st.back().first) c = st.back().second;
            else for (size_t i = 1; i < st.size(); ++i)
                if (t <= st[i].first) {
                    const float span = st[i].first - st[i - 1].first;
                    const float u = span > 1e-6f ? (t - st[i - 1].first) / span : 1.0f;
                    const Lab& A = st[i - 1].second; const Lab& B = st[i].second;
                    c = { A.L + (B.L - A.L) * u, A.a + (B.a - A.a) * u, A.b + (B.b - A.b) * u };
                    break;
                }
        }
        out[k][0] = c.L; out[k][1] = c.a; out[k][2] = c.b; out[k][3] = 1.0f;
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
    p.mode[1] = (float)r.by;
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
        else if (k == "by") { const std::string l = lower(v); cur->by = l == "max" ? 1 : l == "luma" ? 2 : 0; ok = l == "lightness" || l == "max" || l == "luma"; }
        else if (k == "space") { const std::string l = lower(v); ok = l == "okhsv" || l == "oklab"; }   // informative: the maths is always Oklab / OkHSV
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
            s += std::string("by = ") + (r.by == 1 ? "max" : r.by == 2 ? "luma" : "lightness") + "\n";
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
