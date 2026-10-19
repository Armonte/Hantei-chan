#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
namespace timeline {
inline float BoundaryX(float origin,float width,int tick) { return origin+tick*width; }
inline float SampleX(float origin,float width,int tick) { return origin+(tick+.5f)*width; }
struct Advance { int tick; bool stopped; };
struct ResizeBinding {
    int button=1; bool requireAlt=true;
    bool enabled(bool alt) const { return !requireAlt || alt; }
};
inline int ResizeEdge(const std::vector<int>& starts,float origin,float width,float mouse,float slop=6.f) {
    int owner=-1;
    for(size_t i=1;i<starts.size();++i) { float d=std::fabs(mouse-(origin+starts[i]*width)); if(d<=slop) { slop=d; owner=int(i)-1; } }
    return owner;
}
inline int ResizedDuration(int original,float delta,float width) {
    if(!(width>0) || !std::isfinite(delta)) return original;
    const double steps=std::round(double(delta)/width);
    if(steps==0) return original; // preserve legacy zero/negative values on no-op
    return int(std::clamp(double(std::max(1,original))+steps,1.,2147483647.));
}
inline Advance Next(int tick,int last,bool loop) {
    last=std::max(0,last);
    if(tick>=last) return {loop?0:last,!loop};
    return {std::max(0,tick+1),false};
}
}
