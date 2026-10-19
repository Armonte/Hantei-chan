#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace spritepick {
// Immutable, palette-independent coverage shared by cache entries and live snapshots.
struct Mask {
    int width=0,height=0;
    std::vector<uint8_t> coverage;
    static std::shared_ptr<const Mask> Build(const unsigned char* pixels,int w,int h,bool indexed) {
        if(!pixels || w<=0 || h<=0 || size_t(w)>SIZE_MAX/size_t(h)) return {};
        auto m=std::make_shared<Mask>(); m->width=w; m->height=h;
        const size_t count=size_t(w)*size_t(h);
        if(!indexed && count>SIZE_MAX/4) return {};
        m->coverage.resize(count);
        for(size_t i=0;i<count;++i) m->coverage[i]=indexed?(pixels[i]!=0):(pixels[i*4+3]!=0);
        return m;
    }
    bool sample(float u,float v) const {
        if(!std::isfinite(u) || !std::isfinite(v) || u<0 || v<0 || u>=1 || v>=1 || width<=0 || height<=0) return false;
        const size_t offset=size_t(std::floor(v*height))*size_t(width)+size_t(std::floor(u*width));
        return offset<coverage.size() && coverage[offset]!=0;
    }
};
// CG quads are affine (including XYZ projection, skew and facing). Invert the
// submitted quad, then interpolate its actual UV endpoints, not guessed bounds.
template<class Point> bool Hit(const Point* quad,const Point* uv,const Mask& mask,float x,float y) {
    const float ax=quad[1].x-quad[0].x,ay=quad[1].y-quad[0].y;
    const float bx=quad[3].x-quad[0].x,by=quad[3].y-quad[0].y;
    const float det=ax*by-ay*bx;
    if(!std::isfinite(det) || std::fabs(det)<1e-6f) return false;
    const float dx=x-quad[0].x,dy=y-quad[0].y;
    const float u=(dx*by-dy*bx)/det,v=(ax*dy-ay*dx)/det;
    if(u<0 || v<0 || u>=1 || v>=1) return false;
    return mask.sample(uv[0].x+u*(uv[1].x-uv[0].x)+v*(uv[3].x-uv[0].x),
                       uv[0].y+u*(uv[1].y-uv[0].y)+v*(uv[3].y-uv[0].y));
}
// PAT parts are submitted triangles, sometimes curved/3D, not affine quads.
// Screen barycentrics must be corrected by the submitted clip-space W.
template<class Point> bool TriangleHit(const Point* q,const Point* uv,const float* invW,const Mask& mask,float x,float y,bool solid=false) {
 const float ax=q[1].x-q[0].x,ay=q[1].y-q[0].y,bx=q[2].x-q[0].x,by=q[2].y-q[0].y;
 const float det=ax*by-ay*bx;
 if(!std::isfinite(det) || std::fabs(det)<1e-6f) return false;
 const float b=((x-q[0].x)*by-(y-q[0].y)*bx)/det,c=(ax*(y-q[0].y)-ay*(x-q[0].x))/det,a=1-b-c;
 if(a<0 || b<0 || c<0 || !std::isfinite(a+b+c)) return false;
 const float w=a*invW[0]+b*invW[1]+c*invW[2];
 if(!std::isfinite(w) || w<=0) return false;
 if(solid) return true;
 const float u=(a*uv[0].x*invW[0]+b*uv[1].x*invW[1]+c*uv[2].x*invW[2])/w;
 const float v=(a*uv[0].y*invW[0]+b*uv[1].y*invW[1]+c*uv[2].y*invW[2])/w;
 if(!std::isfinite(u) || !std::isfinite(v)) return false;
 // PAT textures use CLAMP_TO_EDGE; CG's strict source-bounds rule is unchanged.
 return mask.sample(std::clamp(u,0.f,std::nextafter(1.f,0.f)),std::clamp(v,0.f,std::nextafter(1.f,0.f)));
}
}
