// Exercise the real hook helpers with synthetic renderer records, without a
// running game. This catches pointer-chain, argument-forwarding and scope bugs.
#include "../src/plugin.cpp"
#include <cstdlib>

namespace {
Address headPart,bodyPart,actorAddress;
int submitted=0;
bool fault=false;
void checkRender(bool ok,const char* message) {
    if(!ok){fprintf(stderr,"%s\n",message);std::exit(1);}
}
void __fastcall recordPart(Address part,Address context,int pass,int instances,Address extra,unsigned char flags) {
    checkRender(part==headPart||part==bodyPart,"original part pointer forwarded");
    checkRender(context==123&&pass==2&&instances==7&&extra==456&&flags==9,"all draw arguments forwarded");
    ++submitted;
}
void __fastcall recordModel(Address component,Address,int) {
    if(fault)RaiseException(0xe1234567,0,0,nullptr);
    if(component==1) {
        checkRender(drawingLocalActor==actorAddress,"local draw scope active");
        drawPartHook(headPart,123,2,7,456,9);
        drawPartHook(bodyPart,123,2,7,456,9);
        // Even a nested non-player draw must keep its head.
        drawUnfiltered(2,123,2);
        checkRender(drawingLocalActor==actorAddress,"outer scope restored after nested draw");
    } else {
        checkRender(drawingLocalActor==0,"NPC draw outside local filter scope");
        drawPartHook(headPart,123,2,7,456,9);
    }
}
void __fastcall recordPrepare(Address,Address) {}
bool faultDraw(Address actor,Address original,Address scratch) {
    __try {drawWithPalette(actor,original,scratch,1,123,2);return false;}
    __except(GetExceptionCode()==0xe1234567?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
}
}
int main() {
    std::array<unsigned char,0x600> actor{};
    std::array<unsigned char,0x68> headMaterialRecord{},bodyMaterialRecord{};
    std::array<unsigned char,0x40> headSource{},bodySource{};
    Address headSourceAddress=reinterpret_cast<Address>(headSource.data());
    Address bodySourceAddress=reinterpret_cast<Address>(bodySource.data());
    headPart=reinterpret_cast<Address>(&headSourceAddress);bodyPart=reinterpret_cast<Address>(&bodySourceAddress);
    write(headSourceAddress+0x38,reinterpret_cast<Address>(headMaterialRecord.data()));
    write(bodySourceAddress+0x38,reinterpret_cast<Address>(bodyMaterialRecord.data()));
    write(reinterpret_cast<Address>(headMaterialRecord.data())+0x18,reinterpret_cast<Address>(L"met_Translucent"));
    write(reinterpret_cast<Address>(bodyMaterialRecord.data())+0x18,reinterpret_cast<Address>(L"metal"));
    actorAddress=reinterpret_cast<Address>(actor.data());
    Address original=111,scratch=222;write(actorAddress+0x5f0,original);
    originalDrawPart=recordPart;originalDraw=recordModel;
    drawWithPalette(actorAddress,original,scratch,1,123,2);
    checkRender(submitted==2,"local helmet hidden; body and NPC helmet submitted");
    checkRender(value<Address>(actorAddress+0x5f0)==original&&drawingLocalActor==0,"palette and scope restored");
    cfg.hideHeadMaterials=false;submitted=0;
    drawWithPalette(actorAddress,original,scratch,1,123,2);
    checkRender(submitted==3,"filter setting can disable material suppression");
    fault=true;
    checkRender(faultDraw(actorAddress,original,scratch),"original renderer exception propagates");
    checkRender(value<Address>(actorAddress+0x5f0)==original&&drawingLocalActor==0,"exception restores palette and scope");
    // Regression: the on-screen passes execute after camera restoration. Keep
    // visibility enabled there and when preparing unrelated auxiliary cameras.
    state.camera=actorAddress;state.overridden=true;state.active=true;
    restoreCamera(actorAddress);
    checkRender(state.active&&!state.overridden,"camera restoration preserves visibility for subsequent passes");
    std::array<unsigned char,32> auxiliaryCamera{};
    originalPrepare=recordPrepare;
    prepareHook(reinterpret_cast<Address>(auxiliaryCamera.data()),0);
    checkRender(state.active,"auxiliary camera preparation preserves primary FPS visibility");
    prepareHook(actorAddress,0);
    checkRender(!state.active,"primary preparation failure clears FPS visibility");
    puts("Render filtering, NPC scope, arguments and exception restoration passed");
}
