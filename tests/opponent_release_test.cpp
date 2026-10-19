#include "src/opponent_release.h"
#include <map>
#include <vector>
#include <cstdio>
#include <cstdlib>
static void check(bool ok) { if(!ok) std::exit(1); }
struct AF { int duration=1,loopCount=0,aniType=2,aniFlag=0,jump=0,loopEnd=0,landJump=2; };
struct Frame { struct AF AF; };
struct Seq { std::vector<Frame> frames; };
struct Data { std::map<int,Seq> patterns; Seq* get_sequence(int p) { auto it=patterns.find(p); return it==patterns.end()?nullptr:&it->second; } };
int main() {
    auto rising=opponent::ReleaseFlight(0,0,1,300,-1800,0,150,true);
    check(rising.x==1 && rising.y==-6 && rising.landing==-1);
    auto landed=opponent::ReleaseFlight(0,10,60,300,-1800,0,150,true);
    check(landed.y==0 && landed.landing==24);
    auto reverse=opponent::ReleaseFlight(0,0,1,-300,-1800,0,150,true);
    check(reverse.x==-1 && reverse.y==-6);
    Data data; data.patterns[26].frames.resize(3); data.patterns[33].frames.resize(1);
    data.patterns[26].frames[0].AF.duration=2;
    data.patterns[26].frames[2].AF.aniType=0; data.patterns[26].frames[2].AF.jump=33;
    auto contact=opponent::ReleasedReaction(data,26,0,24,24); check(contact.valid && contact.pattern==26 && contact.state==2);
    auto wake=opponent::ReleasedReaction(data,26,0,25,24); check(wake.valid && wake.pattern==33 && wake.state==0);
    data.patterns[20].frames.resize(2); data.patterns[20].frames[0].AF.aniType=1;
    auto boundary=opponent::ReactionAt(data,20,0,1); check(boundary.valid && boundary.state==1);
    check(!opponent::ReactionAt(data,26,99,0).valid);
    data.patterns[26].frames[2].AF.jump=999; check(!opponent::ReactionAt(data,26,2,1).valid);
    std::puts("OPPONENT RELEASE CONTRACTS PASS");
}
