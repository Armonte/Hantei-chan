#include "src/sprite_pick.h"
#include <cstdio>
#include <cstdlib>
struct Point { float x,y; };
static void require(bool value) { if(!value) { std::puts("sprite picking contract FAILED"); std::exit(1); } }
int main() {
    const unsigned char indices[]={0,4,255,0};
    auto indexed=spritepick::Mask::Build(indices,2,2,true);
    require(indexed && !indexed->sample(.25f,.25f) && indexed->sample(.75f,.25f));
    require(indexed->sample(.25f,.75f) && !indexed->sample(.75f,.75f));
    const unsigned char rgba[]={255,0,0,0, 0,0,0,1, 9,8,7,128, 0,0,0,0};
    auto alpha=spritepick::Mask::Build(rgba,2,2,false);
    require(alpha && alpha->coverage==indexed->coverage);
    require(!alpha->sample(-.01f,0) && !alpha->sample(1,0) && !alpha->sample(0,1));
    const Point uv[]={{0,0},{1,0},{1,1},{0,1}};
    // Affine skew + quarter rotation, then reflected facing.
    for(float facing:{1.f,-1.f}) {
        Point q[]={{100,200},{100,240},{100-20*facing,250},{100-20*facing,210}};
        auto hit=[&](float u,float v) { return spritepick::Hit(q,uv,*alpha,q[0].x+u*(q[1].x-q[0].x)+v*(q[3].x-q[0].x),q[0].y+u*(q[1].y-q[0].y)+v*(q[3].y-q[0].y)); };
        require(!hit(.25f,.25f) && hit(.75f,.25f) && hit(.25f,.75f) && !hit(.75f,.75f));
        require(!hit(1.1f,.3f));
    }
    const Point flat[]={{0,0},{0,0},{0,0},{0,0}};
    require(!spritepick::Hit(flat,uv,*alpha,0,0));
    require(!spritepick::Mask::Build(nullptr,1,1,true));
    const unsigned char halves[]={0,1,0,1}; auto mask=spritepick::Mask::Build(halves,2,2,true);
    const Point tq[]={{0,0},{100,0},{0,100}},tu[]={{0,0},{1,0},{0,1}};
    const float iw[]={1,.25f,1};
    require(!spritepick::TriangleHit(tq,tu,iw,*mask,60,10)); // affine UV would wrongly hit
    require(spritepick::TriangleHit(tq,tu,iw,*mask,90,1));
    require(!spritepick::TriangleHit(tq,tu,iw,*mask,90,90));
    require(spritepick::TriangleHit(tq,tu,iw,*mask,60,10,true)); // PRSP alpha
    const Point reflected[]={{0,0},{-100,0},{0,100}};
    require(spritepick::TriangleHit(reflected,tu,iw,*mask,-90,1));
    require(!spritepick::TriangleHit(flat,tu,iw,*mask,0,0));
    std::puts("SPRITE PICK PASS: index zero, RGBA alpha, skew/rotation/reflection and bounds");
}
