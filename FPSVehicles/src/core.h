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
inline float dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 cross(Vec3 a,Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline bool unit(Vec3 v,Vec3& result) {
    float n=length(v);if(!finite(v)||!std::isfinite(n)||n<0.00001f)return false;
    result=v*(1/n);return true;
}
struct VehicleFrame {
    Vec3 right{},up{},back{};
    Vec3 world(Vec3 local) const { return right*local.x+up*local.y+back*local.z; }
    Vec3 local(Vec3 worldVector) const { return {dot(worldVector,right),dot(worldVector,up),dot(worldVector,back)}; }
};
inline bool vehicleFrame(const Matrix& vehicle,VehicleFrame& frame) {
    if(!unit({vehicle.m[0],vehicle.m[1],vehicle.m[2]},frame.right))return false;
    Vec3 rawUp={vehicle.m[4],vehicle.m[5],vehicle.m[6]};
    if(!unit(rawUp-frame.right*dot(rawUp,frame.right),frame.up))return false;
    if(!unit(cross(frame.right,frame.up),frame.back))return false;
    Vec3 rawBack={vehicle.m[8],vehicle.m[9],vehicle.m[10]};
    // Reject reflected or collapsed transforms rather than inventing a frame.
    return finite(rawBack)&&dot(frame.back,rawBack)>0.00001f;
}
struct VehicleOrientation {
    Vec3 previousRight={1,0,0}; // vehicle-local; follows chassis motion automatically
    Vec3 previousBack={0,0,1};
    bool valid=false;
    bool apply(Matrix& camera,const Matrix& vehicle) {
        VehicleFrame frame;
        Vec3 back{};
        if(!vehicleFrame(vehicle,frame)||!unit({camera.m[8],camera.m[9],camera.m[10]},back))return false;
        Vec3 localBack=frame.local(back),right{};
        Vec3 expectedRight={1,0,0};
        bool continuous=false;
        if(valid) {
            // Transport the previous right axis with the aim rotation first.
            // Comparing untransported axes misclassifies a >90-degree boarding
            // yaw jump as a pole crossing and permanently inverts the horizon.
            float cosine=std::clamp(dot(previousBack,localBack),-1.0f,1.0f);
            // Boarding can replace the old view with a very different aim pose
            // in one preparation. Re-seed from chassis up for discontinuities;
            // they provide no evidence of a continuous trip through a pole.
            continuous=cosine>0.98f;
            if(cosine<-0.9999f) {
                // Antipodal aim has no unique shortest arc. Treat an abrupt
                // reversal as yaw about previous up, not a pitch somersault.
                expectedRight=previousRight*(-1);
            } else {
                Vec3 axis=cross(previousBack,localBack);
                expectedRight=previousRight+cross(axis,previousRight)+
                    cross(axis,cross(axis,previousRight))*(1/(1+cosine));
            }
            if(!unit(expectedRight-localBack*dot(expectedRight,localBack),expectedRight))return false;
        }
        // Work in the full chassis frame, never Euler roll or a world-up sign
        // test. This remains continuous across 180/360 degrees of chassis roll.
        Vec3 candidate=cross({0,1,0},localBack);
        if(length(candidate)<0.001f) {
            Vec3 prior=expectedRight;
            if(!unit(prior-localBack*dot(prior,localBack),right)) {
                if(!unit(cross({0,0,1},localBack),right))return false;
            }
        } else if(!unit(candidate,right))return false;
        // Looking through a local vertical pole must not invert the horizon.
        // Retain the continuous branch on either side instead of snapping 180.
        if(continuous&&dot(right,expectedRight)<0)right=right*(-1);
        Vec3 up{};if(!unit(cross(localBack,right),up))return false;
        Vec3 worldRight=frame.world(right),worldUp=frame.world(up);
        for(int i=0;i<3;++i) {
            camera.m[i]=i==0?worldRight.x:i==1?worldRight.y:worldRight.z;
            camera.m[4+i]=i==0?worldUp.x:i==1?worldUp.y:worldUp.z;
        }
        camera.m[8]=back.x;camera.m[9]=back.y;camera.m[10]=back.z;
        previousRight=right;previousBack=localBack;valid=true;return true;
    }
};
inline bool vehicleEyeOffset(const Matrix& camera,const Matrix& vehicle,float ahead,float above,Vec3& offset) {
    VehicleFrame frame;if(!vehicleFrame(vehicle,frame))return false;
    Vec3 facing=forward(camera),planar{};
    Vec3 tangent=facing-frame.up*dot(facing,frame.up);
    if(!unit(tangent,planar)) {
        Vec3 right={camera.m[0],camera.m[1],camera.m[2]};
        if(!unit(cross(frame.up,right),planar))return false;
    }
    offset=planar*ahead+frame.up*above;return finite(offset);
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
