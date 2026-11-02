#include "core.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cwctype>
using namespace fps;
void check(bool ok,const char* message) { if(!ok) { std::cerr<<message<<'\n'; std::exit(1); } }
bool near(float a,float b) { return std::fabs(a-b)<0.0001f; }
int main(int argc,char** argv) {
    HoldToggle hold;
    check(!hold.update(true,true,100,700),"stick press starts hold");
    check(!hold.update(true,true,799,700),"short press does not toggle");
    check(hold.update(true,true,800,700),"hold threshold toggles once");
    check(!hold.update(true,true,5000,700),"held stick does not repeat toggle");
    hold.update(false,true,5001,700);
    check(!hold.update(true,true,5100,700)&&hold.update(true,true,5800,700),"release rearms hold");
    hold.update(true,false,5900,700);
    check(!hold.update(true,true,10000,700),"focus gain while held does not toggle");
    hold.update(false,true,10001,700);
    check(!hold.update(true,true,10100,700)&&hold.update(true,true,10800,700),"release rearms after focus loss");
    if(argc==2) {
        // Fixture is drawn from all 12 shipped class/LOD models. In particular,
        // the body material 'metal' must not be confused with helmet 'met'.
        std::ifstream file(argv[1]);check(file.good(),"material fixture opens");
        std::string line;int cases=0;
        while(std::getline(file,line)) {
            if(line.empty()||line[0]=='#')continue;
            std::istringstream row(line);std::string model,name;int hidden=-1;
            std::getline(row,model,'\t');std::getline(row,name,'\t');row>>hidden;
            check(!row.fail(),"material fixture parses");
            std::wstring normalized(name.begin(),name.end());
            for(auto& c:normalized)c=static_cast<wchar_t>(towlower(c));
            if(headMaterial(normalized)!=(hidden==1)) {
                std::cerr<<model<<" material "<<name<<" classified incorrectly\n";return 1;
            }
            ++cases;
        }
        check(cases>=60,"material fixture covers all class LODs");
        std::cout<<cases<<" shipped-material checks passed\n";
    }
    int parents[]={-1,0,1,2,2,1,6,99};
    check(descendant(parents,8,2,2),"head includes itself");
    check(descendant(parents,8,4,2),"head accessory included");
    check(!descendant(parents,8,5,2),"arm branch preserved");
    check(!descendant(parents,8,6,2),"cycle bounded");
    check(!descendant(parents,8,7,2),"invalid parent bounded");
    Matrix m{};m.m[0]=m.m[5]=m.m[10]=m.m[15]=1;setPosition(m,{10,20,30});
    auto small=shrink(m,{10,18,30},0.001f);
    check(near(small.m[0],0.001f)&&near(small.m[13],18.002f),"collapse around head pivot");
    check(near(m.m[13],20),"source palette unchanged");
    check(near(small.m[15],1),"homogeneous coordinate preserved");
    check(near(smoothstep(0.3f,0.9f,0),0),"no look-down extension looking level");
    check(near(smoothstep(0.3f,0.9f,1),1),"full look-down extension looking down");
    float a=0,b=0;for(int i=0;i<60;++i)a=follow(a,1,12,1.0f/60);for(int i=0;i<120;++i)b=follow(b,1,12,1.0f/120);
    check(near(a,b),"look-down smoothing independent of frame rate");
    m.m[9]=1;m.m[10]=0;
    check(near(forward(m).y,-1),"right-handed camera pitch points down");
    check(finite(horizontalForward(m,Matrix{})),"vertical look has finite fallback");
    check(near(zoomedFov(1.1f,0.785398f,0.785398f),1.1f),"configured FOV at normal zoom");
    check(zoomedFov(1.1f,0.15f,0.785398f)<0.3f,"weapon zoom retained");
    std::cout<<"FPS core checks passed\n";
}
