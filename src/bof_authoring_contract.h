#pragma once

// Editor-only schema checks, not a simulator or a promise of vanilla support.
// Slot meanings match the maintained EX panels and accepted BOF runtime sources.
namespace bof_authoring {
inline bool Range(int v, int lo, int hi) { return v >= lo && v <= hi; }
inline bool Zero(const int* p, int from, int to) {
    for (int i = from; i < to; ++i) if (p[i]) return false;
    return true;
}
inline bool ConditionValid(int type, const int* p) {
    if (type == 158) {
        return Range(p[2], 0, 3) && Zero(p, 4, 9) &&
            ((p[1] == 3 && Range(p[3], 1, 3)) ||
             ((p[1] == 0 || p[1] == 2) && Range(p[3], 0, 1)));
    }
    if (type != 159 || p[0] || !Range(p[2], 0, 1) || !Range(p[3], 0, 15) || p[8] <= 0) return false;
    if (p[1] == 2) return Zero(p, 4, 8);
    if (p[1] != 1 || !Range(p[4], 1, 9) || !Range(p[5], 1, 3) || !Range(p[6], 1, 2)) return false;
    const bool boolean = p[4] == 2 || p[4] == 4 || Range(p[4], 6, 8);
    if (boolean) return p[6] == 2 && Range(p[7], 0, 1);
    if (p[4] == 9) return p[6] == 2 && Range(p[7], 0, 255);
    return true;
}
inline bool EffectValid(int subtype, const int* p) {
    if (subtype == 160) return Range(p[0], 0, 4) && Zero(p, 1, 12);
    if (subtype == 161) return Range(p[0], 0, 1) && p[1] >= 0 && p[2] >= 0 &&
        Range(p[6], 0, 5) && Range(p[7], 0, 3) && Range(p[8], 0, 6) &&
        Range(p[9], 0, 1) && (p[10] & ~0x10) == 0 && p[11] == 0;
    if (subtype == 162) return p[0] >= 0 && p[3] >= 0 && p[4] >= 0 &&
        Range(p[6], 0, 6) && Range(p[7], 0, 1) && Range(p[8], 0, 1) && Zero(p, 9, 12);
    return false;
}
}
