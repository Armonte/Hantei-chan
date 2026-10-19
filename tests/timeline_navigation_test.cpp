#include "src/timeline_navigation.h"
#include <cstdlib>
#include <cstdio>
static void check(bool v) { if(!v) std::exit(1); }
int main() {
    for(float width:{.25f,1.f,4.f,32.f}) for(int tick=0;tick<100;++tick) {
        check(timeline::BoundaryX(17.f,width,tick)==17.f+width*tick);
        float x=timeline::SampleX(17.f,width,tick);
        check(x>17.f+tick*width && x<17.f+(tick+1)*width);
    }
    check(timeline::Next(9,10,false).tick==10 && !timeline::Next(9,10,false).stopped);
    check(timeline::Next(10,10,false).tick==10 && timeline::Next(10,10,false).stopped);
    check(timeline::Next(10,10,true).tick==0 && !timeline::Next(10,10,true).stopped);
    check(timeline::Next(0,0,false).stopped);
    check(timeline::ResizedDuration(0,0,4)==0);
    check(timeline::ResizedDuration(5,16,4)==9);
    check(timeline::ResizedDuration(5,-400,4)==1);
    check(timeline::ResizedDuration(0,4,4)==2);
    check(timeline::ResizedDuration(5,16,0)==5);
    for(float w:{.25f,1.f,8.f,32.f}) {
        check(timeline::ResizeEdge({0,2,7,11},100,w,100+7*w)==1);
        check(timeline::ResizeEdge({0,2,7,11},100,w,100+11*w)==2);
        check(timeline::ResizeEdge({0,2,7,11},100,w,100+11*w+20)==-1);
    }
    for(float w:{.25f,1.f,8.f,32.f}) check(timeline::ResizedDuration(5,3*w,w)==8);
    check(timeline::Next(1800,1800,false).tick==1800 && timeline::Next(1800,1800,false).stopped);
    std::puts("TIMELINE NAVIGATION PASS");
}
