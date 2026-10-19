#pragma once
#include "undo_manager.h"
#include <array>
#include <string>

namespace statetransfer {
enum Group : unsigned { Duration=1, Animation=2, State=4, Attack=8, Boxes=16, All=31 };
inline const char* Name(unsigned group) {
    switch(group) {case Duration:return "Duration"; case Animation:return "Animation layers (all)";
    case State:return "State data (AS)"; case Attack:return "Attack data (AT)"; case Boxes:return "Boxes"; default:return "";}
}
struct Request {
    int sourcePattern=0, sourceFirst=0, sourceLast=0;
    int targetPattern=0, targetFirst=0, targetLast=0;
    bool repeat=true;
    unsigned groups=Duration;
};
struct Row {int source=0,target=0; unsigned changed=0; Frame sourceSnapshot,before,after;};
struct Plan {Request request; std::vector<Row> rows; int changed=0; std::string error;};
inline bool Equal(const Frame& a,const Frame& b) {
    Sequence x,y; x.frames.push_back(a); y.frames.push_back(b);
    return UndoManager::SequenceContentEquals(x,y);
}
inline void Copy(Frame& to,const Frame& from,unsigned group) {
    switch(group) {
    case Duration: to.AF.duration=from.AF.duration; break;
    case Animation: to.AF.layers=from.AF.layers; to.AF.AFRT=from.AF.AFRT; break;
    case State: to.AS=from.AS; break;
    case Attack: to.AT=from.AT; break;
    case Boxes: to.hitboxes=from.hitboxes; break;
    }
}
inline Plan Preview(const std::vector<Sequence>& doc,const Request& q) {
    Plan p; p.request=q;
    auto reject=[&](const char* e) {p.error=e; return p;};
    if(q.groups==0 || (q.groups&~All)) return reject("Select at least one supported property group.");
    if(q.sourcePattern<0 || q.targetPattern<0 || size_t(q.sourcePattern)>=doc.size() || size_t(q.targetPattern)>=doc.size())
        return reject("Pattern does not exist.");
    if(!doc[q.sourcePattern].initialized || !doc[q.targetPattern].initialized) return reject("Source or destination pattern is uninitialized.");
    const auto& src=doc[q.sourcePattern].frames; const auto& dst=doc[q.targetPattern].frames;
    if(q.sourceFirst<0 || q.sourceLast<q.sourceFirst || size_t(q.sourceLast)>=src.size() ||
       q.targetFirst<0 || q.targetLast<q.targetFirst || size_t(q.targetLast)>=dst.size())
        return reject("State range is invalid. No states will be inserted or truncated.");
    const int count=q.targetLast-q.targetFirst+1;
    if(count>4096) return reject("Limit: 4096 destination states per operation.");
    if(!q.repeat && q.sourceLast-q.sourceFirst+1!=count) return reject("Source and destination ranges must have equal lengths.");
    p.rows.reserve(count);
    // Snapshot every source before applying anything: overlapping ranges never cascade.
    for(int i=0;i<count;++i) {
        Row row; row.source=q.sourceFirst+(q.repeat?0:i); row.target=q.targetFirst+i;
        row.sourceSnapshot=src[row.source]; row.before=dst[row.target]; row.after=row.before;
        for(unsigned g=1;g<=Boxes;g<<=1) if(q.groups&g) {
            Frame a,b; Copy(a,row.before,g); Copy(b,row.sourceSnapshot,g);
            if(!Equal(a,b)) {row.changed|=g; Copy(row.after,row.sourceSnapshot,g);}
        }
        if(row.changed) ++p.changed;
        p.rows.push_back(std::move(row));
    }
    return p;
}
inline bool Apply(std::vector<Sequence>& doc,const Plan& p,std::string& error) {
    error.clear();
    if(!p.error.empty() || p.rows.empty() || !p.changed) {error=p.error.empty()?"Nothing to change.":p.error; return false;}
    const auto fresh=Preview(doc,p.request);
    if(!fresh.error.empty() || fresh.rows.size()!=p.rows.size()) {error="Ranges changed; preview again."; return false;}
    for(size_t i=0;i<p.rows.size();++i) if(!Equal(fresh.rows[i].before,p.rows[i].before) ||
        !Equal(fresh.rows[i].sourceSnapshot,p.rows[i].sourceSnapshot)) {
        error="Source or destination changed; preview again."; return false;
    }
    // Allocate/copy before the only mutation; swapping frames cannot partly apply.
    auto staged=doc[p.request.targetPattern].frames;
    for(const auto& row:p.rows) if(row.changed) staged[row.target]=row.after;
    doc[p.request.targetPattern].frames.swap(staged);
    return true;
}
}
