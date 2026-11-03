#include <windows.h>
#include <MinHook.h>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <d3dcompiler.h>
#include "../src/vehicle_fade.h"

void check(bool ok,const char* message) { if(!ok){fprintf(stderr,"%s\n",message);exit(1);} }
int main() {
    ID3D11Device* device=nullptr;ID3D11DeviceContext* ctx=nullptr;
    check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&ctx)),"WARP device");
    check(MH_Initialize()==MH_OK,"MinHook initialization");
    check(vehicleFade::initialize(ctx),"real D3D11 indexed draw hooks install");
    const char* shader="float4 vs(uint id:SV_VertexID):SV_Position {float2 p[3]={float2(-1,-1),float2(-1,3),float2(3,-1)};return float4(p[id],0.5,1);} float4 ps():SV_Target {return float4(1,0,0,1);}";
    ID3DBlob *vs=nullptr,*ps=nullptr;
    check(SUCCEEDED(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"vs","vs_4_0",0,0,&vs,nullptr)),"vertex shader compiles");
    check(SUCCEEDED(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"ps","ps_4_0",0,0,&ps,nullptr)),"pixel shader compiles");
    ID3D11VertexShader* vertex=nullptr;ID3D11PixelShader* pixel=nullptr;
    check(SUCCEEDED(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex)),"vertex shader created");
    check(SUCCEEDED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&pixel)),"pixel shader created");
    ctx->VSSetShader(vertex,nullptr,0);ctx->PSSetShader(pixel,nullptr,0);
    UINT indices[]={0,1,2};D3D11_BUFFER_DESC ib{};ib.ByteWidth=sizeof(indices);ib.Usage=D3D11_USAGE_IMMUTABLE;ib.BindFlags=D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA initial{indices,0,0};ID3D11Buffer* index=nullptr;
    check(SUCCEEDED(device->CreateBuffer(&ib,&initial,&index)),"index buffer created");
    ctx->IASetIndexBuffer(index,DXGI_FORMAT_R32_UINT,0);ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=8;td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.BindFlags=D3D11_BIND_RENDER_TARGET;
    ID3D11Texture2D *target=nullptr,*staging=nullptr;ID3D11RenderTargetView* view=nullptr;
    check(SUCCEEDED(device->CreateTexture2D(&td,nullptr,&target)),"render target created");
    check(SUCCEEDED(device->CreateRenderTargetView(target,nullptr,&view)),"target view created");
    td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    check(SUCCEEDED(device->CreateTexture2D(&td,nullptr,&staging)),"staging texture created");
    ctx->OMSetRenderTargets(1,&view,nullptr);D3D11_VIEWPORT viewport{0,0,8,8,0,1};ctx->RSSetViewports(1,&viewport);
    D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
    ID3D11RasterizerState* raster=nullptr;check(SUCCEEDED(device->CreateRasterizerState(&rd,&raster)),"rasterizer created");ctx->RSSetState(raster);
    auto drawAndCheck=[&](bool faded,bool instance) {
        FLOAT blue[4]={0,0,1,1};ctx->ClearRenderTargetView(view,blue);
        vehicleFade::scopedContext=faded?ctx:nullptr;vehicleFade::opacity=0.25f;
        if(instance)ctx->DrawIndexedInstanced(3,1,0,0,0);else ctx->DrawIndexed(3,0,0);
        ctx->CopyResource(staging,target);D3D11_MAPPED_SUBRESOURCE map{};
        check(SUCCEEDED(ctx->Map(staging,0,D3D11_MAP_READ,0,&map)),"readback");
        auto rgba=static_cast<unsigned char*>(map.pData)+4*map.RowPitch+4*4;
        check(faded?(rgba[0]>=63&&rgba[0]<=65&&rgba[2]>=190&&rgba[2]<=192):(rgba[0]==255&&rgba[2]==0),"pixel color proves actual hull alpha blend and ordinary opaque draw");
        ctx->Unmap(staging,0);
        ID3D11BlendState* restored=nullptr;FLOAT factors[4]{};UINT mask=0;
        ctx->OMGetBlendState(&restored,factors,&mask);check(restored==nullptr,"blend state restored after draw");
        ID3D11DepthStencilState* depth=nullptr;UINT stencil=0;
        ctx->OMGetDepthStencilState(&depth,&stencil);check(depth==nullptr,"depth state restored after draw");
    };
    drawAndCheck(true,false);drawAndCheck(false,false);drawAndCheck(true,true);drawAndCheck(false,true);
    vehicleFade::scopedContext=nullptr;
    MH_DisableHook(MH_ALL_HOOKS);MH_Uninitialize();
    ctx->ClearState();ctx->Flush();
    for(auto& entry:vehicleFade::depths){if(entry.source)entry.source->Release();entry.faded->Release();}
    vehicleFade::blend->Release();vehicleFade::context->Release();
    raster->Release();view->Release();target->Release();staging->Release();index->Release();vertex->Release();pixel->Release();vs->Release();ps->Release();ctx->Release();device->Release();
    puts("WARP pixel readback: 25% red hull over blue; indexed/instanced hooks and state restoration passed");
}
