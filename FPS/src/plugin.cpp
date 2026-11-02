#include <windows.h>
#include <bcrypt.h>
#include <xinput.h>
#include <MinHook.h>
#include "PluginAPI.h"
#include "core.h"
#include <array>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cwctype>
#include <mutex>
#include <string>
#include <vector>

namespace {
using namespace fps;
using Address=uintptr_t;
constexpr size_t MaxBones=1024;
HMODULE self;
Address base;
FILE* logfile;
std::recursive_mutex gate;
std::wstring iniPath;
struct Settings {
    bool enabled=true, dump=true,hideHeadMaterials=true;
    float eyeForward=0.025f,eyeUp=0.15f,lookDownForward=0.20f;
    float startDegrees=20,endDegrees=75,smoothing=12,headScale=0.001f;
    float nearClip=0.035f,verticalFov=65;
    std::wstring headBone;
    WORD controllerMask=XINPUT_GAMEPAD_LEFT_THUMB;
    DWORD controllerIndex=0,controllerHoldMs=700;
} cfg;
struct State {
    Address camera=0,actor=0,nodes=0;
    size_t count=0;
    int head=-1;
    std::array<bool,MaxBones> hidden{};
    Matrix originalCamera{};
    float originalNear=0,originalFov=0,extension=0,dt=1.0f/60;
    bool overridden=false,active=false;
} state;
// Only the synchronous local body draw may suppress head materials. Other
// characters can share these model resources and must keep their heads.
thread_local Address drawingLocalActor=0;
std::vector<std::wstring> suppressedMaterials;
using UpdateFn=Address(__fastcall*)(Address,const float*);
using PrepareFn=void(__fastcall*)(Address,Address);
using DrawFn=void(__fastcall*)(Address,Address,int);
using DestroyFn=void(__fastcall*)(Address);
using DrawPartFn=void(__fastcall*)(Address,Address,int,int,Address,unsigned char);
UpdateFn originalUpdate;
PrepareFn originalPrepare;
DrawFn originalDraw;
DestroyFn originalDestroy;
DrawPartFn originalDrawPart;

void log(const char* fmt,...) {
    if(!logfile)return;
    va_list args;va_start(args,fmt);vfprintf(logfile,fmt,args);va_end(args);
    fputc('\n',logfile);fflush(logfile);
}
std::wstring sibling(const wchar_t* name) {
    wchar_t path[32768];GetModuleFileNameW(self,path,32768);
    std::wstring s(path);s.resize(s.find_last_of(L"\\/")+1);return s+name;
}
// SEH is isolated from C++ objects. Failed reads disable this frame's effect.
bool readBytes(Address p,void* out,size_t bytes) {
    if(p<0x10000 || bytes>0x100000 || p+bytes<p)return false;
    __try { memcpy(out,reinterpret_cast<void*>(p),bytes);return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template<class T> bool read(Address p,T& out) { return readBytes(p,&out,sizeof(out)); }
template<class T> T value(Address p) { T out{};read(p,out);return out; }
template<class T> void write(Address p,const T& v) { memcpy(reinterpret_cast<void*>(p),&v,sizeof(v)); }
std::wstring readWideString(Address p) {
    wchar_t s[160]{};
    for(size_t i=0;i<159;++i) {
        if(!read(p+i*sizeof(wchar_t),s[i]))return {};
        if(!s[i])return s;
    }
    return {};
}
std::wstring boneName(Address node) { return readWideString(value<Address>(node)); }
std::wstring lower(std::wstring s) { for(auto& c:s)c=static_cast<wchar_t>(towlower(c));return s; }
std::string utf8(const std::wstring& s) {
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
    std::string out(n,0);
    if(n)WideCharToMultiByte(CP_UTF8,0,s.data(),static_cast<int>(s.size()),out.data(),n,nullptr,nullptr);
    return out;
}
float option(const wchar_t* key,float fallback,float minimum,float maximum) {
    wchar_t def[40],buf[80];swprintf_s(def,L"%.6g",fallback);
    GetPrivateProfileStringW(L"FPS",key,def,buf,80,iniPath.c_str());
    wchar_t* end=nullptr;float v=wcstof(buf,&end);
    return end==buf || *end || !std::isfinite(v)?fallback:std::clamp(v,minimum,maximum);
}
void loadConfig() {
    iniPath=sibling(L"EDF41FPS.ini");
    cfg.enabled=GetPrivateProfileIntW(L"FPS",L"Enabled",1,iniPath.c_str())!=0;
    cfg.dump=GetPrivateProfileIntW(L"FPS",L"DumpBones",1,iniPath.c_str())!=0;
    cfg.hideHeadMaterials=GetPrivateProfileIntW(L"FPS",L"HideHeadMaterials",1,iniPath.c_str())!=0;
    cfg.eyeForward=option(L"EyeForward",0.025f,-0.1f,0.3f);
    cfg.eyeUp=option(L"EyeUp",0.15f,-0.1f,0.3f);
    cfg.lookDownForward=option(L"LookDownForward",0.20f,0,0.4f);
    cfg.startDegrees=option(L"LookDownStartDegrees",20,0,80);
    cfg.endDegrees=option(L"LookDownEndDegrees",75,cfg.startDegrees+1,90);
    cfg.smoothing=option(L"LookDownSmoothing",12,1,40);
    cfg.headScale=option(L"HeadScale",0.001f,0.0001f,0.05f);
    cfg.nearClip=option(L"NearClip",0.035f,0.01f,0.1f);
    cfg.verticalFov=option(L"VerticalFov",65,0,100);
    wchar_t name[160];GetPrivateProfileStringW(L"FPS",L"HeadBone",L"",name,160,iniPath.c_str());
    cfg.headBone=name;
    GetPrivateProfileStringW(L"FPS",L"ControllerToggle",L"LeftStick",name,160,iniPath.c_str());
    auto button=lower(name);
    cfg.controllerMask=button==L"off"?0:button==L"rightstick"?XINPUT_GAMEPAD_RIGHT_THUMB:
        button==L"bothsticks"?(XINPUT_GAMEPAD_LEFT_THUMB|XINPUT_GAMEPAD_RIGHT_THUMB):XINPUT_GAMEPAD_LEFT_THUMB;
    cfg.controllerIndex=std::clamp(GetPrivateProfileIntW(L"FPS",L"ControllerIndex",0,iniPath.c_str()),0u,3u);
    cfg.controllerHoldMs=std::clamp(GetPrivateProfileIntW(L"FPS",L"ControllerHoldMs",700,iniPath.c_str()),250u,3000u);
}
bool supportedExecutable() {
    wchar_t path[32768];GetModuleFileNameW(nullptr,path,32768);
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    bool ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
    DWORD objectSize=0,result=0;
    if(ok)ok=BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectSize),sizeof(objectSize),&result,0)>=0;
    std::vector<UCHAR> object(objectSize);
    if(ok)ok=BCryptCreateHash(alg,&hash,object.data(),objectSize,nullptr,0,0)>=0;
    UCHAR buffer[65536];DWORD count=0;
    while(ok) {
        if(!ReadFile(file,buffer,sizeof(buffer),&count,nullptr)){ok=false;break;}
        if(!count)break;
        ok=BCryptHashData(hash,buffer,count,0)>=0;
    }
    UCHAR digest[32]{};
    if(ok)ok=BCryptFinishHash(hash,digest,sizeof(digest),0)>=0;
    if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);CloseHandle(file);
    constexpr UCHAR expected[]={0x39,0xb4,0xab,0xf7,0xfe,0x72,0x21,0x75,0x74,0x1a,0x5e,0x0a,0x15,0x40,0x63,0xb2,0x7e,0xf8,0xc5,0x89,0xa1,0xaa,0x5f,0x86,0xc8,0x48,0x53,0x57,0x1a,0xc7,0xbd,0xa8};
    return ok&&memcmp(digest,expected,32)==0;
}
bool liveReference(Address handle,Address expected=0) {
    Address ctrl=value<Address>(handle);
    return ctrl&&value<int>(ctrl)>0&&(!expected||value<Address>(ctrl+8)==expected);
}
bool eligible(Address camera,Address& actor,bool requireEnabled=true) {
    if((requireEnabled&&!cfg.enabled) || value<Address>(camera)!=base+0xa91e18)return false;
    Address scene=value<Address>(base+0xcc8528);
    // Only the primary viewport; an active second camera conservatively disables FPS.
    if(!scene || !liveReference(scene+0x1d8,camera) || liveReference(scene+0x430))return false;
    actor=value<Address>(camera+0x268);
    if(!actor || !liveReference(camera+0x260,actor))return false;
    if(value<Address>(camera+0xac0)!=base+0xdf5b0)return false;
    if(value<Address>(actor+0xf50)||value<Address>(actor+0xf58)||liveReference(actor+0xf60))return false;
    float hp=value<float>(actor+0x16c);
    return std::isfinite(hp)&&hp>0;
}
bool resolveHead(Address actor) {
    Address nodes=0;size_t count=0;
    if(!read(actor+0x5d0,nodes)||!read(actor+0x5e0,count)||!nodes||!count||count>MaxBones)return false;
    if(state.actor==actor && state.nodes==nodes && state.count==count)return state.head>=0;
    state.actor=actor;state.nodes=nodes;state.count=count;state.head=-1;state.hidden.fill(false);
    suppressedMaterials.clear();
    std::array<int,MaxBones> parents{};
    const auto configured=lower(cfg.headBone);
    int matches=0,bestPriority=0;
    for(size_t i=0;i<count;++i) {
        auto name=boneName(nodes+i*0x100);auto n=lower(name);
        if(!read(nodes+i*0x100+0x10,parents[i]))return false;
        if(cfg.dump)log("bone[%zu] parent=%d name=%s",i,parents[i],utf8(name).c_str());
        bool match=!configured.empty()?n==configured:
            n==L"head"||n==L"bip01 head"||n==L"bip001 head"||n==L"bip01_head"||n==L"bip001_head"||
            n==L"j_head"||n==L"bn_head"||n==L"head_jnt"||n==L"mixamorig:head"||n==L"頭"||n==L"頭部";
        // All four shipped classes use kubi. WingDiver also has a non-skeletal
        // mesh named Head, so the verified joint must take precedence.
        int priority=configured.empty()&&n==L"kubi"?2:match?1:0;
        if(priority>bestPriority){bestPriority=priority;matches=0;}
        if(priority&&priority==bestPriority){state.head=static_cast<int>(i);++matches;}
    }
    if(matches!=1) {
        state.head=-1;log("Head resolution failed (%d matches). Set HeadBone to the exact name from this log and restart.",matches);return false;
    }
    size_t hiddenCount=0;
    for(size_t i=0;i<count;++i) {
        state.hidden[i]=descendant(parents.data(),static_cast<int>(count),static_cast<int>(i),state.head);
        hiddenCount+=state.hidden[i]?1:0;
    }
    // A root named 'head' must not cause the entire body to vanish.
    if(hiddenCount==count){state.head=-1;log("Refused head subtree covering the entire skeleton");return false;}
    state.extension=0;
    log("Head resolved: %s; shrinking %zu of %zu bones",utf8(boneName(nodes+state.head*0x100)).c_str(),hiddenCount,count);
    return true;
}
void restoreCamera(Address camera) {
    if(state.overridden&&state.camera==camera) {
        write(camera+0x170,state.originalCamera);
        write(camera+0x18,state.originalNear);write(camera+0x14,state.originalFov);
        state.overridden=false;
    }
    // Restoring the simulation camera must not end first-person visibility.
    // EDF draws passes 0/1/2 between update and the next preparation; only pass
    // 3 occurs while the render-camera override is installed.
}
Address __fastcall updateHook(Address camera,const float* dt) {
    {
    std::lock_guard<std::recursive_mutex> lock(gate);
    if(state.camera==camera)restoreCamera(camera);
    float elapsed=0;
    if(read(reinterpret_cast<Address>(dt),elapsed)&&std::isfinite(elapsed)&&elapsed>0)state.dt=std::min(elapsed,0.1f);
    Address scene=value<Address>(base+0xcc8528);
    if(scene&&liveReference(scene+0x1d8,camera)) {
        static bool keyWasDown=false;
        static HoldToggle stick;
        static ULONGLONG nextControllerPoll=0;
        static bool controllerPresent=false;
        bool down=(GetAsyncKeyState('C')&0x8000)!=0||(GetAsyncKeyState(VK_F6)&0x8000)!=0;
        DWORD foregroundPid=0;GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);
        Address actor=0;
        bool allowed=foregroundPid==GetCurrentProcessId()&&eligible(camera,actor,false);
        bool keyboard=allowed&&down&&!keyWasDown;
        keyWasDown=down;
        ULONGLONG now=GetTickCount64();
        bool controllerDown=false;
        if(cfg.controllerMask&&(controllerPresent||now>=nextControllerPoll)) {
            XINPUT_STATE pad{};
            controllerPresent=XInputGetState(cfg.controllerIndex,&pad)==ERROR_SUCCESS;
            if(!controllerPresent)nextControllerPoll=now+1000;
            controllerDown=controllerPresent&&(pad.Gamepad.wButtons&cfg.controllerMask)==cfg.controllerMask;
        }
        bool controller=stick.update(controllerDown,allowed,now,cfg.controllerHoldMs);
        if(keyboard||controller) {
            cfg.enabled=!cfg.enabled;
            log("%s toggle: first person %s",controller?"Controller hold":"C/F6",cfg.enabled?"enabled":"disabled");
        }
    }
    }
    return originalUpdate(camera,dt);
}
void __fastcall destroyHook(Address camera) {
    {
        std::lock_guard<std::recursive_mutex> lock(gate);
        if(state.camera==camera){state=State{};suppressedMaterials.clear();}
    }
    originalDestroy(camera);
}
void __fastcall prepareHook(Address camera,Address viewport) {
    {
    std::lock_guard<std::recursive_mutex> lock(gate);
    // Keep visibility across camera updates and auxiliary camera preparations.
    // Each draw independently checks eligible(), so vehicles, death, toggles and
    // split screen immediately use the ordinary body render path.
    if(state.camera==camera&&state.overridden)restoreCamera(camera);
    Address actor=0;
    bool ready=false;
    if(eligible(camera,actor)&&resolveHead(actor)) {
        Matrix cam{},head{},actorMatrix{};
        if(read(camera+0x170,cam)&&read(state.nodes+state.head*0x100+0xa0,head)&&read(actor+0x60,actorMatrix)) {
            Vec3 eye=position(head),feet=position(actorMatrix);
            float distance=length(eye-feet);
            // Reject missing/uninitialized poses; never silently use a guessed eye height.
            if(finite(eye)&&finite(feet)&&distance>0.25f&&distance<8.0f) {
                state.originalCamera=cam;state.originalNear=value<float>(camera+0x18);state.originalFov=value<float>(camera+0x14);
                Vec3 f=forward(cam),horizontal=horizontalForward(cam,actorMatrix);
                constexpr float rad=3.14159265358979323846f/180;
                float target=smoothstep(std::sin(cfg.startDegrees*rad),std::sin(cfg.endDegrees*rad),-f.y);
                state.extension=follow(state.extension,target,cfg.smoothing,state.dt);
                eye=eye+horizontal*(cfg.eyeForward+cfg.lookDownForward*state.extension)+Vec3{0,cfg.eyeUp,0};
                setPosition(cam,eye);write(camera+0x170,cam);write(camera+0x18,cfg.nearClip);
                if(cfg.verticalFov>0)write(camera+0x14,zoomedFov(cfg.verticalFov*rad,state.originalFov,0.7853981634f));
                state.camera=camera;state.overridden=true;state.active=true;
                ready=true;
            }
        }
    }
    if(state.camera==camera&&!ready)state.active=false;
    }
    originalPrepare(camera,viewport);
}
void drawWithPalette(Address actor,Address palette,Address temporary,Address component,Address renderContext,int pass) {
    Address previousActor=drawingLocalActor;
    drawingLocalActor=actor;
    write(actor+0x5f0,temporary);
    __try { originalDraw(component,renderContext,pass); }
    __finally { write(actor+0x5f0,palette);drawingLocalActor=previousActor; }
}
void drawUnfiltered(Address component,Address renderContext,int pass) {
    Address previousActor=drawingLocalActor;
    drawingLocalActor=0;
    __try { originalDraw(component,renderContext,pass); }
    __finally { drawingLocalActor=previousActor; }
}
void __fastcall drawPartHook(Address part,Address renderContext,int pass,int instances,Address extra,unsigned char flags) {
    if(drawingLocalActor&&cfg.hideHeadMaterials) {
        Address source=value<Address>(part);
        Address material=source?value<Address>(source+0x38):0;
        Address namePointer=material?value<Address>(material+0x18):0;
        auto name=lower(readWideString(namePointer));
        if(headMaterial(name)) {
            if(std::find(suppressedMaterials.begin(),suppressedMaterials.end(),name)==suppressedMaterials.end()) {
                suppressedMaterials.push_back(name);
                log("First-person head material suppressed: %s",utf8(name).c_str());
            }
            return;
        }
    }
    originalDrawPart(part,renderContext,pass,instances,extra,flags);
}
void __fastcall drawHook(Address component,Address renderContext,int pass) {
    std::lock_guard<std::recursive_mutex> lock(gate);
    Address actor=0;
    if(!state.active||component!=state.actor+0x550||!eligible(state.camera,actor)||actor!=state.actor) {
        drawUnfiltered(component,renderContext,pass);return;
    }
    Address palette=value<Address>(actor+0x5f0);
    // Separate per-call storage also keeps nested draws from corrupting a palette.
    alignas(16) std::array<Matrix,MaxBones> scratch;
    if(!palette||!readBytes(palette,scratch.data(),state.count*sizeof(Matrix))) {
        drawUnfiltered(component,renderContext,pass);return;
    }
    Vec3 pivot=position(scratch[state.head]);
    if(!finite(pivot)){drawUnfiltered(component,renderContext,pass);return;}
    for(size_t i=0;i<state.count;++i)
        if(state.hidden[i])scratch[i]=shrink(scratch[i],pivot,cfg.headScale);
    Address temporary=reinterpret_cast<Address>(scratch.data());
    // Restore even if the game raises an SEH exception; do not swallow the exception.
    drawWithPalette(actor,palette,temporary,component,renderContext,pass);
}
bool installHooks() {
    struct Hook {Address rva;const char* bytes;size_t size;void* replacement;void** original;};
    Hook hooks[]={
        {0xdf230,"\x40\x57\x48\x83\xec\x40\x48\xc7",8,reinterpret_cast<void*>(updateHook),reinterpret_cast<void**>(&originalUpdate)},
        {0x56f530,"\x4c\x8b\xdc\x55\x53\x56\x41\x57",8,reinterpret_cast<void*>(prepareHook),reinterpret_cast<void**>(&originalPrepare)},
        {0x322e40,"\x48\x89\x6c\x24\x18\x48\x89\x74",8,reinterpret_cast<void*>(drawHook),reinterpret_cast<void**>(&originalDraw)},
        {0x5091b0,"\x40\x55\x57\x41\x56\x41\x57\x48",8,reinterpret_cast<void*>(drawPartHook),reinterpret_cast<void**>(&originalDrawPart)},
        {0xdeab0,"\x48\x89\x4c\x24\x08\x57\x48\x83",8,reinterpret_cast<void*>(destroyHook),reinterpret_cast<void**>(&originalDestroy)}
    };
    for(auto& h:hooks)if(h.bytes&&memcmp(reinterpret_cast<void*>(base+h.rva),h.bytes,h.size))return false;
    auto status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED)return false;
    for(auto& h:hooks) {
        status=MH_CreateHook(reinterpret_cast<void*>(base+h.rva),h.replacement,h.original);
        if(status!=MH_OK) {
            log("CreateHook %llx: %s",static_cast<unsigned long long>(h.rva),MH_StatusToString(status));
            for(auto& cleanup:hooks)if(*cleanup.original)MH_RemoveHook(reinterpret_cast<void*>(base+cleanup.rva));
            return false;
        }
    }
    for(auto& h:hooks)MH_QueueEnableHook(reinterpret_cast<void*>(base+h.rva));
    status=MH_ApplyQueued();
    if(status!=MH_OK) {
        for(auto& h:hooks){MH_DisableHook(reinterpret_cast<void*>(base+h.rva));MH_RemoveHook(reinterpret_cast<void*>(base+h.rva));}
        return false;
    }
    return true;
}
}
extern "C" __declspec(dllexport) bool EDFMLAPI EML4_Load(PluginInfo* info) {
    info->infoVersion=PluginInfo::MaxInfoVer;info->name="EDF41FPS";info->version=PLUG_VER(0,1,3,0);
    logfile=_wfopen(sibling(L"EDF41FPS.log").c_str(),L"w");
    base=reinterpret_cast<Address>(GetModuleHandleW(nullptr));loadConfig();
    if(!supportedExecutable()){log("Unsupported EDF41.exe SHA-256; no hooks installed");return false;}
    if(!installHooks()){log("Hook installation failed; FPS unavailable");return false;}
    log("EDF41FPS 0.1.3 ready. C/F6 or controller hold toggles. Persistent head filtering %s.",cfg.hideHeadMaterials?"enabled":"disabled");
    return true;
}
BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){self=module;DisableThreadLibraryCalls(module);}return TRUE;
}
