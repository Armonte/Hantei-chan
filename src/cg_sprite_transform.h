#pragma once
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace cgtransform {
// Actor facing/angle acts on the completed authored layer, not on its Z slot.
inline glm::mat4 Actor(float turns, bool mirrored, float pivotX=0.f, float pivotY=0.f) {
    glm::mat4 m(1.f);
    if (mirrored) m = glm::scale(m, glm::vec3(-1.f,1.f,1.f));
    m = glm::translate(m,glm::vec3(pivotX,pivotY,0));
    m = glm::rotate(m, turns * glm::two_pi<float>(), glm::vec3(0,0,1));
    return glm::translate(m,glm::vec3(-pivotX,-pivotY,0));
}
inline glm::mat4 Sprite(glm::mat4 view, const glm::mat4& actor,
    float sx, float sy, float rx, float ry, float rz, bool afrt, int ox, int oy) {
    view *= actor;
    view = glm::scale(view, glm::vec3(sx,sy,0));
    const float tau = glm::two_pi<float>();
    if (afrt) {
        view = glm::rotate(view,rx*tau,glm::vec3(1,0,0));
        view = glm::rotate(view,ry*tau,glm::vec3(0,1,0));
        view = glm::rotate(view,rz*tau,glm::vec3(0,0,1));
    } else {
        view = glm::rotate(view,rz*tau,glm::vec3(0,0,1));
        view = glm::rotate(view,ry*tau,glm::vec3(0,1,0));
        view = glm::rotate(view,rx*tau,glm::vec3(1,0,0));
    }
    return glm::translate(view,glm::vec3(-128+ox,-224+oy,0));
}
// PAT local coordinates already use the part pivot: unlike CG, there is no
// cropped-sheet (128,224) origin. Same actor/layer order, no perspective.
inline glm::mat4 MbaaPat(glm::mat4 view,const glm::mat4& actor,
    float sx,float sy,float rx,float ry,float rz,bool afrt,int ox,int oy) {
    return Sprite(view,actor,sx,sy,rx,ry,rz,afrt,ox+128,oy+224);
}
}
