// Exercise the real hook helpers with synthetic renderer records, without a
// running game. This catches pointer-chain, argument-forwarding and scope bugs.
#include "../src/plugin.cpp"
#include <cstdlib>

namespace {
Address headPart,bodyPart,actorAddress;
int submitted=0;
bool fault=false;
std::vector<int> vehiclePasses;
void checkRender(bool ok,const char* message);
void __fastcall recordVehicle(Address,Address,int pass) {
    if(pass==9){checkRender(!vehicleFade::scopedContext,"nested NPC has no fade");return;}
    checkRender(vehicleFade::scopedContext==vehicleFade::context,"fade scope uses active context");
    checkRender(drawingLocalActor==0,"vehicle draw never filters shared head materials");
    vehiclePasses.push_back(pass);
    drawUnfiltered(99,0,9);
    checkRender(vehicleFade::scopedContext==vehicleFade::context,"vehicle scope restored after nested NPC");
}
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
void vehicleStateTests() {
    std::vector<unsigned char> image(0xcc8540);
    std::array<unsigned char,0x1800> actor{};
    std::array<unsigned char,0xb00> camera{};
    std::array<unsigned char,0x1100> vehicle{};
    std::array<unsigned char,0x300> seats{};
    std::array<unsigned char,0x440> scene{};
    std::array<unsigned char,0x100> node{};
    std::array<unsigned char,0x40> attachment{};
    Address a=reinterpret_cast<Address>(actor.data()),c=reinterpret_cast<Address>(camera.data());
    Address v=reinterpret_cast<Address>(vehicle.data()),s=reinterpret_cast<Address>(seats.data());
    Address sc=reinterpret_cast<Address>(scene.data()),n=reinterpret_cast<Address>(node.data()),data=reinterpret_cast<Address>(attachment.data());
    std::array<Address,2> camRef{1,c},actorRef{1,a},vehicleRef{1,v};
    base=reinterpret_cast<Address>(image.data());state=State{};cfg=Settings{};
    write(base+0xcc8528,sc);write(sc+0x1d8,reinterpret_cast<Address>(camRef.data()));
    write(c,base+0xa91e18);write(c+0x260,reinterpret_cast<Address>(actorRef.data()));write(c+0x268,a);write(c+0xac0,base+0xe0200);
    write(a+0x16c,100.0f);write(v+0x16c,200.0f);
    write(a+0xf50,v);write(a+0xf58,s);write(a+0xf60,reinterpret_cast<Address>(vehicleRef.data()));
    write(v+0x328,s);write(v+0x338,size_t{2});write(s,v);
    write(v,base+0x1008);write(base+0x1000,base+0x2000);write(base+0x2000,1u);write(base+0x200c,0x3000u);
    auto type=[&](const char* text){memset(image.data()+0x3010,0,96);memcpy(image.data()+0x3010,text,strlen(text));};
    type(".?AVVehicleBike@@");
    Address resolved=0,rv=0,rs=0;VehicleKind kind=VehicleKind::Unknown;
    checkRender(eligible(c,resolved)&&resolved==a,"live bike eligible");
    checkRender(occupiedVehicle(a,rv,rs,kind)&&kind==VehicleKind::Open,"bike stays opaque");
    type(".?AVVehicleTank@@");checkRender(occupiedVehicle(a,rv,rs,kind)&&kind==VehicleKind::Enclosed,"tank fades");
    Matrix m{};m.m[0]=m.m[5]=m.m[10]=m.m[15]=1;setPosition(m,{10,20,30});
    write(n+0xa0,m);write(v+0x60,m);write(a+0x60,m);write(c+0x170,m);write(c+0x14,0.7853981634f);write(c+0x18,0.1f);
    write(s+0x60,data);write(s+0x68,n);write(data+8,0);write(data+0xc,0x20);write(data+0x20,std::array<float,4>{7,3,9,1});
    std::array<unsigned char,0x200> bones{};Address bonesAddress=reinterpret_cast<Address>(bones.data());
    write(bonesAddress,reinterpret_cast<Address>(L"root"));write(bonesAddress+0x10,-1);
    write(bonesAddress+0x100,reinterpret_cast<Address>(L"kubi"));write(bonesAddress+0x110,0);
    Matrix head=m;setPosition(head,{10,21,30});write(bonesAddress+0x1a0,head);
    write(a+0x5d0,bonesAddress);write(a+0x5e0,size_t{2});
    vehicleFade::context=reinterpret_cast<ID3D11DeviceContext*>(0x100000);vehicleFade::attempted=true;
    prepareHook(c,0);
    checkRender(state.active&&state.enclosed&&state.vehicle==v,"enclosed camera activates after renderer ready");
    Matrix changed=value<Matrix>(c+0x170);checkRender(std::abs(position(changed).y-21.15f)<0.001f,"enclosed camera follows animated head, not the exit attachment");
    restoreCamera(c);checkRender(value<Matrix>(c+0x170).m[13]==20,"vehicle camera restored for simulation");
    std::array<unsigned char,16> render{};Address rc=reinterpret_cast<Address>(render.data());write(rc+8,reinterpret_cast<Address>(vehicleFade::context));
    originalDraw=recordVehicle;
    drawHook(v+0xb00,rc,0);drawHook(v+0xb00,rc,1);checkRender(vehiclePasses.empty(),"hull omitted from depth/opaque passes");
    drawHook(v+0xb00,rc,2);checkRender(vehiclePasses==std::vector<int>({1,2}),"opaque hull deferred into translucent stage");
    checkRender(!vehicleFade::scopedContext,"fade scope ends after occupied vehicle");
    // Return from an enclosed seat to a visible bike rider, then on foot.
    type(".?AVVehicleBike@@");prepareHook(c,0);
    checkRender(state.active&&!state.enclosed&&state.head==1,"bike transition resolves animated rider head");
    checkRender(std::abs(value<Matrix>(c+0x170).m[13]-21.15f)<0.001f,"bike eye follows animated head");
    restoreCamera(c);
    type(".?AVVehicleTank@@");prepareHook(c,0);restoreCamera(c);
    checkRender(state.active&&state.enclosed,"re-entering enclosed seat restores vehicle mode");
    type(".?AVVehicleStriker@@");write(s+0x180,v);write(a+0xf58,s+0x180);
    checkRender(occupiedVehicle(a,rv,rs,kind)&&nativePassengerView(rv,rs),"Grape passenger identified independently of live seat validity");
    checkRender(!eligible(c,resolved),"Grape passenger keeps native first-person view");
    prepareHook(c,0);checkRender(!state.active&&!state.overridden,"passenger bypass clears camera and render override");
    checkRender(value<Matrix>(c+0x170).m[13]==20,"native passenger camera untouched");
    write(a+0xf58,s);checkRender(eligible(c,resolved)&&!nativePassengerView(v,s),"Grape driver remains eligible");
    prepareHook(c,0);checkRender(state.active&&state.enclosed,"driver mode resumes after passenger");restoreCamera(c);
    Matrix wall=m;wall.m[0]=0;wall.m[1]=1;wall.m[4]=-1;wall.m[5]=0;
    write(v+0x60,wall);prepareHook(c,0);
    checkRender(std::abs(value<Matrix>(c+0x170).m[1]-1)<0.001f,"vehicle render camera rolls onto wall");
    checkRender(std::abs(value<Matrix>(c+0x170).m[12]-9.85f)<0.001f,"eye height follows vehicle up on wall");
    checkRender(state.originalCamera.m[0]==1&&state.originalCamera.m[5]==1,"saved simulation camera remains unrotated");
    restoreCamera(c);checkRender(value<Matrix>(c+0x170).m[0]==1&&value<Matrix>(c+0x170).m[5]==1,"vehicle roll restored before simulation");
    cfg.vehicleRelativeOrientation=false;prepareHook(c,0);
    checkRender(value<Matrix>(c+0x170).m[0]==1,"vehicle orientation setting restores legacy horizon");restoreCamera(c);
    cfg.vehicleRelativeOrientation=true;write(v+0x60,m);
    // Mech aim/camera attachment is high in the torso; hidden infantry remains
    // at the low rider root. Never use the third-person SGO camera offset.
    std::array<unsigned char,0x200> mechBones{};Address mechNodes=reinterpret_cast<Address>(mechBones.data());
    write(v+0xb80,mechNodes);write(v+0xb90,size_t{2});write(s+0x90,mechNodes+0x100);
    write(mechNodes+0x100,reinterpret_cast<Address>(L"aim_center"));
    Matrix cockpit=m;setPosition(cockpit,{10,24.4f,30});write(mechNodes+0x1a0,cockpit);
    type(".?AVVehicleBegaruta@@");prepareHook(c,0);
    checkRender(state.active&&state.cockpitNode==mechNodes+0x100,"mech uses live camera attachment node");
    checkRender(std::abs(value<Matrix>(c+0x170).m[13]-24.55f)<0.001f,"mech camera raised to cockpit rather than infantry head");
    checkRender(std::abs(value<Matrix>(c+0x170).m[14]-29.8f)<0.001f,"mech cockpit forward adjustment applied");restoreCamera(c);
    write(s+0x90,mechNodes+0x80);prepareHook(c,0);
    checkRender(state.active&&!state.cockpitNode&&std::abs(value<Matrix>(c+0x170).m[13]-21.15f)<0.001f,"invalid mech anchor falls back to known pilot head");restoreCamera(c);
    write(s+0x90,mechNodes+0x100);cfg.mechCockpitCamera=false;prepareHook(c,0);
    checkRender(std::abs(value<Matrix>(c+0x170).m[13]-21.15f)<0.001f,"cockpit camera setting restores pilot head");restoreCamera(c);cfg.mechCockpitCamera=true;
    type(".?AVVehicleTank@@");prepareHook(c,0);
    checkRender(!state.cockpitNode&&std::abs(value<Matrix>(c+0x170).m[13]-21.15f)<0.001f,"ordinary enclosed vehicles retain driver head despite camera attachment");restoreCamera(c);
    type(".?AVUnknownVehicle@@");checkRender(!eligible(c,resolved),"unknown vehicle fails closed");
    type(".?AVVehicleTank@@");write(a+0xf58,s+1);checkRender(!eligible(c,resolved),"misaligned seat rejected");write(a+0xf58,s);
    vehicleRef[1]=v+8;checkRender(!eligible(c,resolved),"stale vehicle reference rejected");vehicleRef[1]=v;
    write(sc+0x430,reinterpret_cast<Address>(camRef.data()));checkRender(!eligible(c,resolved),"split screen rejected");write(sc+0x430,Address{0});
    cfg.enabled=false;checkRender(!eligible(c,resolved)&&eligible(c,resolved,false),"toggle remains available inside vehicle");cfg.enabled=true;
    write(v+0x16c,0.0f);checkRender(!eligible(c,resolved),"destroyed vehicle rejected");write(v+0x16c,200.0f);
    write(a+0xf50,Address{0});write(a+0xf58,Address{0});write(a+0xf60,Address{0});
    checkRender(!eligible(c,resolved),"vehicle callback without rider fails closed");
    write(c+0xac0,base+0xdf5b0);checkRender(eligible(c,resolved),"exit restores on-foot eligibility");
    prepareHook(c,0);checkRender(state.active&&!state.enclosed&&!state.vehicle,"exit restores first-person body mode");restoreCamera(c);
    vehicleFade::context=nullptr;vehicleFade::attempted=false;state=State{};base=0;
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
    vehicleStateTests();
    puts("Render filtering, NPC scope, arguments and exception restoration passed");
}
