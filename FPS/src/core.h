#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string_view>
#include <cstdint>

namespace fps {
struct HoldToggle {
    uint64_t started=0;
    bool tracking=false,fired=false,blocked=false;
    bool update(bool down,bool allowed,uint64_t now,uint64_t holdMs) {
        if(!allowed){tracking=false;fired=false;blocked=down;return false;}
        if(!down){tracking=false;fired=false;blocked=false;return false;}
        if(blocked)return false;
        if(!tracking){tracking=true;started=now;}
        if(!fired&&now-started>=holdMs){fired=true;return true;}
        return false;
    }
};
// Exact material names verified in the shipped models for all four classes/LODs.
// In particular, 'met' is the helmet, while 'metal' also covers body armor.
inline bool headMaterial(std::wstring_view name) {
    return name==L"met" || name==L"met_translucent" || name==L"rikusen-face" ||
           name==L"head3" || name==L"eye" || name==L"head-rev" ||
           name==L"hair" || name==L"head";
}
struct Vec3 { float x, y, z; };
struct Matrix { float m[16]; };
static_assert(sizeof(Matrix)==64);
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec3 operator*(Vec3 a,float s) { return {a.x*s,a.y*s,a.z*s}; }
inline float length(Vec3 a) { return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z); }
inline bool finite(Vec3 a) { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z); }
inline Vec3 position(const Matrix& a) { return {a.m[12],a.m[13],a.m[14]}; }
inline void setPosition(Matrix& a, Vec3 p) { a.m[12]=p.x;a.m[13]=p.y;a.m[14]=p.z; }
inline float smoothstep(float a,float b,float value) {
    float t=std::clamp((value-a)/(b-a),0.0f,1.0f);
    return t*t*(3-2*t);
}
inline float follow(float oldValue,float target,float speed,float dt) {
    return oldValue+(target-oldValue)*(1-std::exp(-speed*std::clamp(dt,0.0f,0.1f)));
}
inline float zoomedFov(float configured,float gameFov,float nominal) {
    return 2*std::atan(std::tan(configured*0.5f)*std::tan(gameFov*0.5f)/std::tan(nominal*0.5f));
}
// EDF uses a right-handed camera; its third matrix row points backwards.
inline Vec3 forward(const Matrix& a) {
    Vec3 v={-a.m[8],-a.m[9],-a.m[10]};
    float n=length(v); return n>0.00001f?v*(1/n):Vec3{0,0,-1};
}
inline Vec3 horizontalForward(const Matrix& camera,const Matrix& actor) {
    Vec3 v=forward(camera); v.y=0;
    if(length(v)<0.01f) { v={actor.m[8],0,actor.m[10]}; }
    float n=length(v); return n>0.00001f?v*(1/n):Vec3{0,0,1};
}
inline bool descendant(const int* parents,int count,int node,int head) {
    // Bound the walk to reject cycles and invalid model data.
    for(int depth=0; depth<count && node>=0 && node<count; ++depth) {
        if(node==head) return true;
        node=parents[node];
    }
    return false;
}
inline Matrix shrink(Matrix a,Vec3 pivot,float scale) {
    for(int row=0;row<3;++row)
        for(int col=0;col<3;++col) a.m[row*4+col]*=scale;
    setPosition(a,pivot+(position(a)-pivot)*scale);
    return a;
}
}
