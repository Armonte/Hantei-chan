#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace positionpick {
// Displayed instance and authored source are deliberately distinct identities.
struct Key {
    int kind=-1,index=-1,instance=-1,pattern=-1,state=-1;
    bool operator==(const Key& b) const { return kind==b.kind && index==b.index && instance==b.instance && pattern==b.pattern && state==b.state; }
};
struct Selection {
    int pattern=-1,state=-1,tick=-1;
    uint64_t version=0;
    Key selected;
    Key solo;
    std::vector<Key> candidates;
    void clear() { selected={}; solo={}; candidates.clear(); }
    bool toggleSolo() { if(solo.kind>=0) { solo={}; return true; } if(selected.kind<0) return false; solo=selected; return true; }
    void sync(int p,int s,int t,uint64_t v) {
        if(pattern!=p || state!=s || tick!=t || version!=v) clear();
        pattern=p; state=s; tick=t; version=v;
    }
    Key choose(const std::vector<Key>& hits,int direction=0) {
        if(solo.kind>=0) {
            if(direction && !candidates.empty()) {
                auto list=candidates; solo={}; choose(list,direction); solo=selected; return selected;
            }
            return std::find(hits.begin(),hits.end(),solo)!=hits.end()?solo:Key{};
        }
        if(hits.empty()) return {}; // keep the last overlap list reachable by mouse
        candidates=hits;
        auto it=std::find(hits.begin(),hits.end(),selected);
        int i=it==hits.end()?0:int(it-hits.begin());
        if(direction && it!=hits.end()) i=(i+(direction>0?1:-1)+int(hits.size()))%int(hits.size());
        selected=hits[i]; return selected;
    }
    bool select(Key key) {
        if(std::find(candidates.begin(),candidates.end(),key)==candidates.end()) return false;
        selected=key; if(solo.kind>=0) solo=key; return true;
    }
};
}
