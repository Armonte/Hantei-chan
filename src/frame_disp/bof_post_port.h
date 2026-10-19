#pragma once
#include "bof_extensions.h"
#include "../bof_authoring_contract.h"

namespace bof {
static const char* const kSubjects[] = { "0: Self", "1: Opponent", "2: Explicit P1", "3: Explicit P2" };
static const char* const kFacing[] = { "0: Native / downstream", "1: Preserve prior facing", "2: Flip prior facing", "3: Same as source", "4: Opposite source", "5: World left", "6: World right" };
static const char* const kAcquire[] = { "0: Require existing grab", "1: Acquire opponent after my confirmed hit" };
template<size_t N> static inline void Choice(const char* label, int* value, const char* const (&items)[N], float width, const std::function<void()>& changed) {
    if (Combo(label, value, items, (int)N, width)) changed();
}
static inline void Flag(const char* label, int* value, int mask, const std::function<void()>& changed) {
    bool set = (*value & mask) != 0;
    if (im::Checkbox(EfRow(LBL(label)), &set)) {
        *value = set ? (*value | mask) : (*value & ~mask);
        changed(); // Preserve unknown bits; merely viewing never rewrites a record.
    }
}
static inline bool DrawPostCondition(int type, int* p, float width, FrameData* fd, const std::function<void()>& changed) {
    if (!ExtendedProfileEnabled() || (type != 158 && type != 159)) return false;
    Caution("Extended Melty / BOF only: requires the matching patched MBAACC executable.");
    if (type == 158) {
        EfSection(TXT("Gameplay predicate"));
        JumpField(&p[0], width, fd, changed);
        static const char* const predicates[] = { "0: Combo participation active", "2: Throw sequence active", "3: Throw context equals" };
        Choice("Predicate", &p[1], predicates, width, changed);
        Choice("Subject", &p[2], kSubjects, width, changed);
        if (p[1] == 3) {
            static const char* const contexts[] = { "1: Raw", "2: Combo", "3: Counter" };
            Choice("Throw context", &p[3], contexts, width, changed);
            Hint("Captured typed-grab context, not a general counter-hit check. Attacker: choose Opponent; defender: choose Self.");
        } else {
            static const char* const expected[] = { "0: Inactive / false", "1: Active / true" };
            Choice("Expected", &p[3], expected, width, changed);
            Hint("Combo participation is broader than the live native throw relationship.");
        }
        ClearReserved(p, 4, 9, changed);
    } else {
        EfSection(TXT("Runtime throw-rule modifier"));
        static const char* const actions[] = { "1: Write slot", "2: Clear owned slot" };
        static const char* const subjects[] = { "0: Self", "1: Opponent" };
        Choice("Action", &p[1], actions, width, changed);
        Choice("Target player", &p[2], subjects, width, changed);
        Int("Modifier slot (0..15)", &p[3], width, changed);
        if (p[1] == 1) {
            static const char* const rules[] = { "1: Throw tech response window", "2: Pre-tech enabled", "3: Pre-tech window", "4: TRM enabled", "5: TRM lockout window", "6: Raw throw techable", "7: Combo throw techable", "8: Counter throw techable", "9: Result route profile" };
            static const char* const domains[] = { "1: Ground", "2: Air", "3: Ground and air" };
            static const char* const operations[] = { "1: Add to inherited value", "2: Absolute override" };
            Choice("Rule", &p[4], rules, width, changed);
            Choice("Domain", &p[5], domains, width, changed);
            Choice("Operation", &p[6], operations, width, changed);
            Int("Value", &p[7], width, changed);
            Hint("Boolean rules require absolute 0/1. Result route requires absolute 0..255.");
        } else if (p[1] == 2) {
            Hint("Clear requires the same target, slot and positive ownership token. Parameters 5..8 must be zero.");
            ClearReserved(p, 4, 8, changed);
        }
        Int("Ownership token", &p[8], width, changed);
        if (p[0]) {
            Warn("Unused destination must be zero.");
            if (im::Button(LBL("Clear unused destination"))) { p[0] = 0; changed(); }
        }
        Hint("Writes rollback-covered runtime state; replacing a slot replaces its prior modifier.");
    }
    if (!bof_authoring::ConditionValid(type, p)) Warn("Invalid or unsupported field combination. Review the fields or use raw mode; values are preserved, not silently corrected.");
    return true;
}
static inline bool DrawPostEffect(int subtype, int* p, float width, const std::function<void()>& changed) {
    if (!ExtendedProfileEnabled() || subtype < 160 || subtype > 162) return false;
    Caution("Extended Melty / BOF only: requires the matching patched MBAACC executable.");
    if (subtype == 160) {
        EfSection(TXT("Scoped interaction"));
        static const char* const operations[] = { "0: Begin scope; replace current; stamp actor", "1: Begin scope; retain current; stamp actor", "2: Adopt owner's current scope", "3: Inherit spawning parent's scope", "4: End this actor's scope" };
        Choice("Operation", &p[0], operations, width, changed);
        Hint("Scopes are opaque engine IDs, not authored move or slot numbers. Retain mode fails closed at 32 active scopes.");
        ClearReserved(p, 1, 12, changed);
    } else if (subtype == 161) {
        EfSection(TXT("Set target pose"));
        Choice("Acquisition", &p[0], kAcquire, width, changed);
        Int("Opponent pattern", &p[1], width, changed);
        Int("Opponent state", &p[2], width, changed);
        Int("X offset", &p[3], width, changed);
        Int("Y offset", &p[4], width, changed);
        Int("Rotation", &p[5], width, changed);
        static const char* const locations[] = { "0: Origin", "1: Grab box 1", "2: Grab box 2", "3: Grab box 3", "4: Grab box 4", "5: Grab box 5" };
        static const char* const positions[] = { "0: Move opponent; ground clamp", "1: Move source; ground clamp", "2: Move opponent; allow below ground", "3: Move source; allow below ground" };
        Choice("Grab location", &p[6], locations, width, changed);
        Choice("Position mode", &p[7], positions, width, changed);
        Choice("Facing", &p[8], kFacing, width, changed);
        static const char* const boolean[] = { "0: No", "1: Yes" };
        Choice("Reset bounce on acquisition", &p[9], boolean, width, changed);
        Flag("Preserve current positions", &p[10], 0x10, changed);
        Hint("Opponent pattern/state IDs belong to the target character, not this file. Native facing leaves downstream effects in control.");
        ClearReserved(p, 11, 12, changed);
    } else {
        EfSection(TXT("Release target + HitVector"));
        Int("Opponent release pattern", &p[0], width, changed);
        Int("X offset", &p[1], width, changed);
        Int("Y offset", &p[2], width, changed);
        Int("HitVector", &p[3], width, changed);
        Int("Untech frames", &p[4], width, changed);
        Flag("Reverse HitVector", &p[5], 0x2, changed);
        Int("Native release flags", &p[5], width, changed);
        Hint("Native Effect 14 flags are preserved. Release bit 0 is forced on by the extension; other native flag edge behavior is not fully verified.");
        Choice("Release facing", &p[6], kFacing, width, changed);
        Choice("Acquisition", &p[7], kAcquire, width, changed);
        static const char* const boolean[] = { "0: No", "1: Yes" };
        Choice("Reset bounce on acquisition", &p[8], boolean, width, changed);
        ClearReserved(p, 9, 12, changed);
    }
    if (!bof_authoring::EffectValid(subtype, p)) Warn("Invalid or unsupported field combination. Values remain unchanged until you edit them.");
    return true;
}
}
