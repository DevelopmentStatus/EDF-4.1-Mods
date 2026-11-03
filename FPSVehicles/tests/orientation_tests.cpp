#include "../src/core.h"
#include <cstdio>
#include <cstdlib>
using namespace fps;
void check(bool ok,const char* message) { if(!ok){fprintf(stderr,"%s\n",message);exit(1);} }
Matrix identity() { Matrix m{};m.m[0]=m.m[5]=m.m[10]=m.m[15]=1;return m; }
Vec3 row(const Matrix& m,int i) { return {m.m[i*4],m.m[i*4+1],m.m[i*4+2]}; }
bool close(Vec3 a,Vec3 b) { return length(a-b)<0.0001f; }
Matrix rotation(int axis,float angle) {
    Matrix m=identity();float c=std::cos(angle),s=std::sin(angle);
    if(axis==0){m.m[5]=c;m.m[6]=s;m.m[9]=-s;m.m[10]=c;}
    if(axis==1){m.m[0]=c;m.m[2]=-s;m.m[8]=s;m.m[10]=c;}
    if(axis==2){m.m[0]=c;m.m[1]=s;m.m[4]=-s;m.m[5]=c;}
    return m;
}
void orthonormal(const Matrix& m) {
    check(std::abs(length(row(m,0))-1)<0.0001f&&std::abs(length(row(m,1))-1)<0.0001f&&std::abs(length(row(m,2))-1)<0.0001f,"unit camera axes");
    check(std::abs(dot(row(m,0),row(m,1)))<0.0001f&&std::abs(dot(row(m,0),row(m,2)))<0.0001f&&std::abs(dot(row(m,1),row(m,2)))<0.0001f,"perpendicular camera axes");
    check(close(cross(row(m,0),row(m,1)),row(m,2)),"right-handed camera basis");
}
int main() {
    constexpr float rad=3.14159265358979323846f/180;
    // Full loops in both directions, including walls, ceilings, and Euler wrap
    // boundaries. The incoming game camera supplies only its aim direction.
    for(int axis=0;axis<3;++axis) {
        VehicleOrientation orientation;
        for(int degree=-720;degree<=720;++degree) {
            Matrix chassis=rotation(axis,degree*rad),camera=identity();
            setPosition(camera,{11,22,33});
            for(int i=0;i<3;++i)camera.m[8+i]=chassis.m[8+i];
            Vec3 aim=forward(camera);
            check(orientation.apply(camera,chassis),"continuous chassis rotation accepted");
            check(close(forward(camera),aim),"game aim direction preserved");
            for(int i=0;i<3;++i)check(close(row(camera,i),row(chassis,i)),"camera follows full chassis frame through 360 degrees");
            check(close(position(camera),{11,22,33})&&camera.m[15]==1,"rotation preserves camera position");
            orthonormal(camera);
            Vec3 offset{};check(vehicleEyeOffset(camera,chassis,0.025f,0.15f,offset),"vehicle-relative eye offset accepted");
            check(close(offset,row(chassis,1)*0.15f-row(chassis,2)*0.025f),"eye offset rotates onto walls and ceilings");
        }
    }
    for(int axis:{0,1}) {
        VehicleOrientation orientation;Matrix previous=identity();
        for(int degree=0;degree<=720;++degree) {
            Matrix look=rotation(axis,degree*rad),camera=identity();
            for(int i=0;i<3;++i)camera.m[8+i]=look.m[8+i];
            check(orientation.apply(camera,identity()),"local look accepted through vertical poles");
            check(close(forward(camera),forward(look)),"local aim preserved");orthonormal(camera);
            if(degree)check(dot(row(previous,0),row(camera,0))>0.99f&&dot(row(previous,1),row(camera,1))>0.99f,"no horizon inversion or snap at local vertical");
            Vec3 offset{};check(vehicleEyeOffset(camera,identity(),0.025f,0.15f,offset)&&finite(offset),"vertical look offset stays finite");
            previous=camera;
        }
    }
    // The first prepared entry pose may still face where the player looked on
    // foot; the next pose jumps to the vehicle's aim. This is yaw, not a pole
    // crossing, and must not lock the camera onto the inverted horizon branch.
    for(float initialYaw:{-180.0f,-170.0f,-100.0f,100.0f,170.0f,180.0f}) {
        VehicleOrientation entry;
        Matrix oldLook=rotation(1,initialYaw*rad),newLook=identity();
        check(entry.apply(oldLook,identity())&&entry.apply(newLook,identity()),"entry yaw transition accepted");
        check(dot(row(newLook,1),{0,1,0})>0.999f,"large entry yaw change must not latch an upside-down camera");
        for(float initialPitch:{-80.0f,-45.0f,45.0f,80.0f}) {
            VehicleOrientation pitchedEntry;oldLook=identity();newLook=identity();
            oldLook.m[8]=std::sin(initialYaw*rad)*std::cos(initialPitch*rad);
            oldLook.m[9]=std::sin(initialPitch*rad);
            oldLook.m[10]=std::cos(initialYaw*rad)*std::cos(initialPitch*rad);
            check(pitchedEntry.apply(oldLook,identity())&&pitchedEntry.apply(newLook,identity()),"pitched entry transition accepted");
            check(dot(row(newLook,1),{0,1,0})>0.999f,"pitched entry yaw jump must settle upright");
        }
    }
    Matrix collapsed{},camera=identity();VehicleOrientation orientation;
    check(!orientation.apply(camera,collapsed),"collapsed vehicle transform rejected");
    Matrix scaled=rotation(2,0.7f);for(int i=0;i<12;++i)scaled.m[i]*=2;
    check(orientation.apply(camera,scaled),"scaled chassis rotation normalized");orthonormal(camera);
    puts("Full chassis roll/pitch/yaw, local vertical poles, aim preservation and rotated eye offsets passed");
}
