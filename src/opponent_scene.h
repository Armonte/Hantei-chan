#pragma once
#include "framedata.h"
#include "preview_sim.h"
#include <algorithm>

namespace opponent {
struct EditSlots { int x=-1,y=-1,rotation=-1; bool writable() const { return x>=0 && y>=0; } };
inline EditSlots ReleaseEditSlots(const Frame_EF& e,bool extended) {
    const int* v=e.parameters;
    if(e.type==4 && (v[3]&17)==1 && v[2]==0) return {0,1,-1};
    if(e.type==14 && (v[3]&17)==1 && v[6]==0) return {0,1,-1};
    if(extended && e.type==6 && e.number==162) return {1,2,-1};
    return {};
}
inline EditSlots HeldEditSlots(const Frame_EF& e,bool extended) {
    const int* v=e.parameters;
    if(e.type==4 && !(v[3]&17) && v[2]==0) return {0,1,-1};
    if(e.type==14 && !(v[3]&17) && v[6]==0) return {0,1,2};
    if(extended && e.type==6 && e.number==161) return {3,4,5};
    return {}; // release/relationship-relative/unsupported modes are read-only
}
// Authored placement descriptor, deliberately separate from render matrices.
struct Pose {
    bool supported = false, extended = false, release = false;
    int pattern = -1, state = 0, x = 0, y = 0, rotation = 0, grabLocation = 0;
    int vector = -1, flags = 0, facingMode = 0;
};
inline Pose Describe(const Frame_EF& e, bool extended) {
    Pose p; const int* v = e.parameters;
    if (e.type == 4 || e.type == 14) {
        p.supported = true; p.pattern = e.number; p.x = v[0]; p.y = v[1];
        p.release = (v[3] & 1) != 0; p.flags = v[3];
        if (e.type == 14) {
            p.rotation = v[2]; p.state = p.release ? 0 : v[5];
            p.grabLocation = p.release ? 0 : v[4]; p.vector = p.release ? v[4] : -1;
        } else { p.state = v[7]; p.vector = p.release ? v[4] : -1; }
    } else if (extended && e.type == 6 && e.number == 161) {
        p.supported = p.extended = true;
        p.pattern = v[1]; p.state = v[2]; p.x = v[3]; p.y = v[4]; p.rotation = v[5];
        p.grabLocation = v[6]; p.facingMode = v[8];
    } else if (extended && e.type == 6 && e.number == 162) {
        p.supported = p.extended = p.release = true;
        p.pattern = v[0]; p.x = v[1]; p.y = v[2]; p.vector = v[3]; p.flags = v[5]; p.facingMode = v[6];
    }
    return p;
}
struct Scene {
    bool found = false, facingLeft = false;
    Pose pose;
    int sourcePattern = -1, sourceState = -1, effect = -1, entryTick = -1, elapsed = 0;
    uint32_t visit = 0;
    bool authored = false; // inspection only; entryTick stays -1, elapsed stays 0
};
inline bool Facing(int mode, bool prior, bool source) {
    switch (mode) {
    case 1: return prior;
    case 2: return !prior;
    case 3: return source;
    case 4: return !source;
    case 5: return true;
    case 6: return false;
    default: return !source; // native baseline; downstream native effects are not modeled
    }
}
inline Scene InspectAuthored(FrameData& data, int pattern, int state, bool extended, bool sourceFacingLeft = false) {
    Scene result;
    const auto* sequence = data.get_sequence(pattern);
    if (!sequence || state < 0 || state >= (int)sequence->frames.size()) return result;
    const auto& effects = sequence->frames[state].EF;
    bool facing = !sourceFacingLeft;
    for (int i = 0; i < (int)effects.size(); ++i) {
        const Pose p = Describe(effects[i], extended);
        if (!p.supported) continue;
        facing = Facing(p.facingMode, facing, sourceFacingLeft);
        result = {true, facing, p, pattern, state, i, -1, 0, 0, true};
    }
    return result;
}
// Pure read-only resolution over the view's actual cached simulator history.
// Entry serial distinguishes self-loop re-entry from a held state. EF order is
// preserved; an unvisited authored record can never become the current event.
inline Scene Resolve(FrameData& data, const std::vector<preview::RootVisitSample>& track, int tick, bool extended) {
    Scene result;
    if (tick < 0 || track.empty()) return result;
    bool targetFacing = !track.front().facingLeft;
    for (int t = 0; t < (int)track.size(); ++t) {
        const auto& sample = track[t];
        if (sample.entryTick > tick) break;
        const auto* sequence = data.get_sequence(sample.pattern);
        if (!sequence || sample.frame < 0 || sample.frame >= (int)sequence->frames.size()) continue;
        const auto& effects = sequence->frames[sample.frame].EF;
        for (int i = 0; i < (int)effects.size(); ++i) {
            const Pose p = Describe(effects[i], extended);
            if (!p.supported) continue;
            targetFacing = Facing(p.facingMode, targetFacing, sample.facingLeft);
            result = {true, targetFacing, p, sample.pattern, sample.frame, i, sample.entryTick, 0, sample.serial};
        }
    }
    if (result.found) result.elapsed = std::max(0, tick - result.entryTick);
    return result;
}
}
