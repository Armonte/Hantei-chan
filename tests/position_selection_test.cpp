#include "src/position_selection.h"
#include <cstdio>
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main() {
    using namespace positionpick;
    Key a{0,0,-1,149,0},b{2,0,5,149,0},c{3,0,-1,149,0},other{2,0,6,149,0};
    Selection p; p.sync(149,1,5,10);
    CHECK(p.choose({a,b,c})==a);
    CHECK(p.choose({a,b,c},1)==b);
    CHECK(p.choose({c,a,b})==b); // hover/click preserve selection despite rank changes
    CHECK(p.choose({c,a,b},1)==c);
    CHECK(p.choose({c,a,b},-1)==b);
    CHECK(p.choose({}).kind==-1 && p.candidates.size()==3);
    CHECK(p.select(a) && p.choose({c,a,b})==a);
    CHECK(!p.select(other)); // same source, different displayed instance
    CHECK(p.choose({other})==other);
    CHECK(p.toggleSolo() && p.solo==other);
    CHECK(p.choose({a}).kind==-1 && p.candidates.size()==1); // invisible objects cannot replace Solo
    CHECK(p.toggleSolo() && p.solo.kind==-1);
    p.choose({a,b,c}); CHECK(p.toggleSolo());
    CHECK(p.choose({a},1)==b && p.solo==b && p.candidates.size()==3);
    CHECK(p.select(c) && p.solo==c);
    p.sync(149,1,5,11); CHECK(p.selected.kind==-1 && p.candidates.empty());
    CHECK(p.solo.kind==-1);
    p.choose({a,b}); p.sync(149,1,6,11); CHECK(p.candidates.empty());
    p.choose({a,b}); p.sync(150,1,6,11); CHECK(p.candidates.empty());
    Selection second; second.choose({c}); CHECK(p.selected.kind==-1 && second.selected==c);
    std::puts("POSITION_SELECTION_PASS");
}
