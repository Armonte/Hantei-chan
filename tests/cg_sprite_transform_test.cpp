#include "src/cg_sprite_transform.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
static void near(float a,float b) { if(std::abs(a-b)>.001f) { std::printf("Mismatch %.9g != %.9g\n",a,b); std::exit(1); } }
int main() {
    const glm::vec2 corners[]={{0,0},{353,0},{353,97},{0,97}};
    // Daily MBAACC capture, owner 149/0 child 400/0 angle 2197. Origin (221,418), 2x CG scale removed.
    const glm::vec2 expected[]={{-11.229645f,-42.0043335f},{-7.972412f,25.416168f},{17.760269f,44.933716f},{14.5030365f,-22.486786f}};
    auto m=cgtransform::Sprite(glm::mat4(1),cgtransform::Actor(.2197f,false),.4f,-.3f,.59f,.33f,.47f,false,-54,188);
    for(int i=0;i<4;++i) { auto v=m*glm::vec4(corners[i],0,1); near(v.x,expected[i].x); near(v.y,expected[i].y); }
    const glm::vec2 zeroExpected[]={{-43.3704376f,3.0781555f},{23.448311f,12.637924f},{47.482689f,-8.9364625f},{-19.3360595f,-18.496231f}};
    auto zero=cgtransform::Sprite(glm::mat4(1),cgtransform::Actor(0,false),.4f,-.3f,.59f,.33f,.47f,false,-54,188);
    for(int i=0;i<4;++i) { auto v=zero*glm::vec4(corners[i],0,1); near(v.x,zeroExpected[i].x); near(v.y,zeroExpected[i].y); }
    // Facing reflection remains outside the full layer; never mirror its origin twice.
    auto flipped=cgtransform::Sprite(glm::mat4(1),cgtransform::Actor(.2197f,true),.4f,-.3f,.59f,.33f,.47f,false,-54,188);
    for(auto p:corners) { auto a=m*glm::vec4(p,0,1), b=flipped*glm::vec4(p,0,1); near(a.x,-b.x); near(a.y,b.y); }
    // Historical flipped pattern-412 game fixture, including authored Y flip.
    auto golden=cgtransform::Sprite(glm::mat4(1),cgtransform::Actor(.2288f,true),.45f,.45f,0,-.5f,0,false,-246,-328);
    auto v=golden*glm::vec4(320,416,0,1); near(v.x,-63.885147f); near(v.y,15.956772f);
    // Actual MBAACC PAT84 capture: effect173/state1, AFRT enabled.
    // Remove the recorded draw anchor and randomized uniform actor scale .6989.
    const glm::vec2 patCorners[]={{-96,-96},{96,-96},{-96,96},{96,96}};
    const glm::vec2 patExpected[]={{32.3795035f,-172.799979f},{32.3795035f,172.800011f},{-32.379482f,-172.799995f},{-32.3795035f,172.799967f}};
    for(bool mirror:{false,true}) {
        auto pat=cgtransform::MbaaPat(glm::mat4(1),cgtransform::Actor(0,mirror),1.8f,1.8f,0,.22f,.25f,true,0,0);
        for(int i=0;i<4;++i) {
            const auto p=pat*glm::vec4(patCorners[i],0,1);
            near(p.x,mirror?-patExpected[i].x:patExpected[i].x); near(p.y,patExpected[i].y); near(p.z,0);
        }
    }
    std::puts("CG/PAT transform golden contracts PASS");
    for(bool mirror:{false,true}) {
        auto pivot=glm::vec4(-2,-72,0,1);
        auto anchored=cgtransform::Actor(.2135f,mirror,pivot.x,pivot.y)*pivot;
        near(anchored.x,mirror?2.f:-2.f); near(anchored.y,-72.f);
    }
}
