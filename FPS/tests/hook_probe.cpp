// Map the executable for inspection only. Never call its entry point or game code.
#include <windows.h>
#include <MinHook.h>
#include <cstdio>
#include <cstdint>
#include <initializer_list>
void placeholder() {}
int wmain(int argc,wchar_t** argv) {
    if(argc!=2){puts("Usage: fps_hook_probe <EDF41.exe>");return 2;}
    auto image=LoadLibraryExW(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    if(!image){printf("Image map failed: %lu\n",GetLastError());return 1;}
    if(MH_Initialize()!=MH_OK)return 1;
    bool ok=true;
    for(uintptr_t rva:{uintptr_t(0xdf230),uintptr_t(0x56f530),uintptr_t(0x322e40),uintptr_t(0x5091b0),uintptr_t(0xdeab0)}) {
        void* target=reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(image)+rva);
        void* trampoline=nullptr;
        auto result=MH_CreateHook(target,reinterpret_cast<void*>(placeholder),&trampoline);
        printf("RVA %llx trampoline: %s\n",static_cast<unsigned long long>(rva),MH_StatusToString(result));
        ok=ok&&result==MH_OK;
        if(result==MH_OK)MH_RemoveHook(target);
    }
    // Hooks are deliberately never enabled; no target instructions are executed.
    MH_Uninitialize();FreeLibrary(image);return ok?0:1;
}
