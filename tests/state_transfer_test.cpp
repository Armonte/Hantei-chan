#include "state_transfer.h"
#include <cstdio>
#include <cstdlib>
std::set<int> numberSet;
int maxCount;
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main() {
    using namespace statetransfer;
    std::vector<Sequence> doc(2);
    for(int p=0;p<2;++p) {
        doc[p].initialized=true; doc[p].name=std::to_string(p); doc[p].frames.resize(5);
        for(int i=0;i<5;++i) {
            auto& f=doc[p].frames[i]; f.AF.duration=1+i+p*10; f.AF.jump=4;
            f.AF.layers.resize(1); f.AF.layers[0].spriteId=p*10+i;
            f.AS.speed[0]=p*100+i; f.AT.damage=p*1000+i;
            Frame_EF ef{};ef.type=6;ef.number=154; f.EF.push_back(ef);
            Frame_IF cond{};cond.type=159;f.IF.push_back(cond);
            f.hitboxes[1]=Hitbox{{p,i,3,4}};
        }
    }
    const auto original=doc;
    Request q; q.sourcePattern=0;q.sourceFirst=q.sourceLast=1;
    q.targetPattern=1;q.targetFirst=1;q.targetLast=3;q.groups=All;
    auto p=Preview(doc,q); CHECK(p.error.empty() && p.changed==3);
    CHECK(UndoManager::SequenceContentEquals(doc[1],original[1])); // dry run
    UndoManager undo;undo.attach(&doc);undo.ensureBaseline();undo.beginTransaction("transfer");
    std::string error;CHECK(Apply(doc,p,error));undo.markModified(1);CHECK(undo.commitTransaction());
    CHECK(undo.undoCount()==1 && doc[1].frames.size()==5 && doc[1].name=="1");
    for(int i=1;i<=3;++i) {
        auto expected=original[1].frames[i];
        for(unsigned g=1;g<=Boxes;g<<=1) Copy(expected,original[0].frames[1],g);
        CHECK(Equal(expected,doc[1].frames[i]));
        CHECK(doc[1].frames[i].AF.jump==4 && doc[1].frames[i].EF[0].number==154 && doc[1].frames[i].IF[0].type==159);
    }
    CHECK(undo.undo());CHECK(UndoManager::SequenceContentEquals(doc[1],original[1]));
    CHECK(undo.redo()); CHECK(doc[1].frames[3].AT.damage==1);
    auto noop=Preview(doc,q);CHECK(noop.changed==0);CHECK(!Apply(doc,noop,error));
    doc=original;
    // Overlapping mapped range must read immutable source snapshots.
    q.sourcePattern=q.targetPattern=0;q.sourceFirst=0;q.sourceLast=2;
    q.targetFirst=1;q.targetLast=3;q.repeat=false;q.groups=Duration;
    p=Preview(doc,q);CHECK(p.error.empty());CHECK(Apply(doc,p,error));
    for(int i=1;i<=3;++i) CHECK(doc[0].frames[i].AF.duration==i);
    doc=original;p=Preview(doc,q);
    doc[0].frames[0].AF.duration=99;CHECK(!Apply(doc,p,error)); // stale source
    CHECK(doc[0].frames[3].AF.duration==4);
    doc=original;p=Preview(doc,q);doc[0].frames[3].AT.damage=99;
    CHECK(!Apply(doc,p,error));CHECK(doc[0].frames[1].AF.duration==2); // stale destination
    q.targetLast=99;CHECK(!Preview(doc,q).error.empty());
    q.targetLast=2;CHECK(!Preview(doc,q).error.empty()); // mismatched range lengths
    q.targetLast=3;q.groups=0;CHECK(!Preview(doc,q).error.empty());
    q.groups=32;CHECK(!Preview(doc,q).error.empty());
    // Every selective group leaves all other frame records intact.
    for(unsigned g=1;g<=Boxes;g<<=1) {
        doc=original;q={};q.sourceFirst=q.sourceLast=2;q.targetPattern=1;q.groups=g;
        p=Preview(doc,q);CHECK(p.changed==1);CHECK(Apply(doc,p,error));
        auto expected=original[1].frames[0];Copy(expected,original[0].frames[2],g);
        CHECK(Equal(doc[1].frames[0],expected));
    }
    std::puts("STATE TRANSFER PASS: dry-run, selective groups, overlap, stale rejection, one-step undo/redo");
}
