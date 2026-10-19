#pragma once
#include <algorithm>
#include <cstdint>

namespace opponent {
struct Flight { int x=0,y=0,landing=-1; };
// Primary HitVector phase only: 1/256 fixed point, acceleration before motion.
// Flat viewport ground is an editor approximation, not a stage/collision model.
inline Flight ReleaseFlight(int x,int y,int ticks,int vx,int vy,int ax,int ay,bool ground) {
    std::int64_t px=std::int64_t(x)*256,py=std::int64_t(y)*256;
    std::int64_t sx=vx,sy=vy;
    if(ground && py>0) py=0;
    Flight r{x,int(py/256),-1};
    for(int t=0;t<std::clamp(ticks,0,1800);++t) {
        sx+=ax; sy+=ay; px+=sx; py+=sy;
        if(ground && py>0) { py=0; r.landing=t+1; break; }
    }
    r.x=int(px/256); r.y=int(py/256); return r;
}
struct Reaction { bool valid=false; int pattern=0,state=0; };
// AF-only reaction flow. No IF/EF execution or invented collision outcomes.
template<class Data> Reaction ReactionAt(Data& data,int pattern,int state,int ticks) {
    Reaction r{false,pattern,state};
    int remaining=std::clamp(ticks,0,1800),loops=0;
    auto* seq=data.get_sequence(pattern);
    if(!seq || state<0 || state>=int(seq->frames.size())) return r;
    loops=std::max(0,seq->frames[state].AF.loopCount);
    for(int guard=0;guard<=1800;++guard) {
        seq=data.get_sequence(r.pattern);
        if(!seq || r.state<0 || r.state>=int(seq->frames.size())) { r.valid=false; return r; }
        r.valid=true;
        const auto& af=seq->frames[r.state].AF;
        const int duration=std::max(1,af.duration);
        if(remaining<duration) return r;
        remaining-=duration;
        if(af.aniType==0) { r.pattern=af.jump+((af.aniFlag&4)?r.pattern:0); r.state=0; loops=0; }
        else if(af.aniType==1) ++r.state;
        else if(af.aniType==2) {
            if(loops) --loops;
            r.state=(!(af.aniFlag&2)||loops) ? af.jump+((af.aniFlag&4)?r.state:0) : af.loopEnd+((af.aniFlag&8)?r.state:0);
        } else return r;
        seq=data.get_sequence(r.pattern);
        if(seq && r.state>=0 && r.state<int(seq->frames.size()) && seq->frames[r.state].AF.loopCount>0) loops=seq->frames[r.state].AF.loopCount;
    }
    r.valid=false; return r;
}
template<class Data> Reaction ReleasedReaction(Data& data,int pattern,int state,int elapsed,int landing) {
    if(landing<0) return ReactionAt(data,pattern,state,elapsed);
    auto contact=ReactionAt(data,pattern,state,landing);
    if(!contact.valid) return contact;
    auto* seq=data.get_sequence(contact.pattern);
    const int destination=seq->frames[contact.state].AF.landJump;
    return ReactionAt(data,contact.pattern,destination,std::max(0,elapsed-landing));
}
}
