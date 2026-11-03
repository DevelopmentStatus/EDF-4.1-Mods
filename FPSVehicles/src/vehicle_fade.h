#pragma once
#include <d3d11.h>

// Called only under the plugin's recursive render lock. D3D draw detours use
// thread-local scope so other contexts, NPCs and nested draws remain unchanged.
namespace vehicleFade {
using IndexedFn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,INT);
using InstancedFn=void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*,UINT,UINT,UINT,INT,UINT);
inline IndexedFn indexed=nullptr;
inline InstancedFn instanced=nullptr;
inline ID3D11DeviceContext* context=nullptr;
inline ID3D11BlendState* blend=nullptr;
inline thread_local ID3D11DeviceContext* scopedContext=nullptr;
inline thread_local float opacity=1;
struct DepthEntry { ID3D11DepthStencilState* source; ID3D11DepthStencilState* faded; };
inline std::vector<DepthEntry> depths;
inline bool attempted=false;

// Preserve the game's comparison/stencil settings and disable only depth writes.
inline ID3D11DepthStencilState* fadedDepth(ID3D11DeviceContext* ctx,ID3D11DepthStencilState* source) {
    for(auto& entry:depths)if(entry.source==source)return entry.faded;
    if(depths.size()>=32)return nullptr;
    D3D11_DEPTH_STENCIL_DESC desc{};
    if(source)source->GetDesc(&desc);
    else {
        desc.DepthEnable=TRUE;desc.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;desc.DepthFunc=D3D11_COMPARISON_LESS;
        desc.StencilReadMask=desc.StencilWriteMask=0xff;
        desc.FrontFace={D3D11_STENCIL_OP_KEEP,D3D11_STENCIL_OP_KEEP,D3D11_STENCIL_OP_KEEP,D3D11_COMPARISON_ALWAYS};
        desc.BackFace=desc.FrontFace;
    }
    desc.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
    // The hull is omitted from the prepass, so an equality-only comparison
    // cannot be retained for its late color draw.
    if(desc.DepthFunc==D3D11_COMPARISON_EQUAL)desc.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
    ID3D11Device* device=nullptr;ctx->GetDevice(&device);
    ID3D11DepthStencilState* faded=nullptr;
    HRESULT result=device->CreateDepthStencilState(&desc,&faded);device->Release();
    if(FAILED(result))return nullptr;
    if(source)source->AddRef();
    depths.push_back({source,faded});return faded;
}
struct SavedState {
    ID3D11BlendState* blend=nullptr;
    ID3D11DepthStencilState* depth=nullptr;
    FLOAT factors[4]{};UINT mask=0,stencil=0;
    bool changed=false;
};
inline SavedState begin(ID3D11DeviceContext* ctx) {
    SavedState saved;
    if(ctx!=scopedContext||!blend||opacity>=1)return saved;
    ctx->OMGetBlendState(&saved.blend,saved.factors,&saved.mask);
    ctx->OMGetDepthStencilState(&saved.depth,&saved.stencil);
    auto faded=fadedDepth(ctx,saved.depth);
    if(!faded) {
        if(saved.blend)saved.blend->Release();if(saved.depth)saved.depth->Release();
        saved.blend=nullptr;saved.depth=nullptr;return saved;
    }
    FLOAT factor[4]={opacity,opacity,opacity,opacity};
    ctx->OMSetBlendState(blend,factor,saved.mask);
    ctx->OMSetDepthStencilState(faded,saved.stencil);
    saved.changed=true;return saved;
}
inline void end(ID3D11DeviceContext* ctx,SavedState& saved) {
    if(!saved.changed)return;
    ctx->OMSetBlendState(saved.blend,saved.factors,saved.mask);
    ctx->OMSetDepthStencilState(saved.depth,saved.stencil);
    if(saved.blend)saved.blend->Release();if(saved.depth)saved.depth->Release();
}
inline void STDMETHODCALLTYPE indexedHook(ID3D11DeviceContext* ctx,UINT count,UINT start,INT vertex) {
    auto saved=begin(ctx);
    __try {indexed(ctx,count,start,vertex);}
    __finally {end(ctx,saved);}
}
inline void STDMETHODCALLTYPE instancedHook(ID3D11DeviceContext* ctx,UINT count,UINT instances,UINT start,INT vertex,UINT first) {
    auto saved=begin(ctx);
    __try {instanced(ctx,count,instances,start,vertex,first);}
    __finally {end(ctx,saved);}
}
inline bool initialize(ID3D11DeviceContext* ctx) {
    if(context)return context==ctx;
    if(attempted||!ctx)return false;
    attempted=true;
    ID3D11Device* device=nullptr;ctx->GetDevice(&device);
    D3D11_BLEND_DESC desc{};
    auto& rt=desc.RenderTarget[0];rt.BlendEnable=TRUE;
    rt.SrcBlend=D3D11_BLEND_BLEND_FACTOR;rt.DestBlend=D3D11_BLEND_INV_BLEND_FACTOR;rt.BlendOp=D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha=D3D11_BLEND_ZERO;rt.DestBlendAlpha=D3D11_BLEND_ONE;rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    HRESULT result=device->CreateBlendState(&desc,&blend);device->Release();
    if(FAILED(result))return false;
    void** vt=*reinterpret_cast<void***>(ctx);
    bool first=false,second=false;
    auto status=MH_CreateHook(vt[12],reinterpret_cast<void*>(indexedHook),reinterpret_cast<void**>(&indexed));
    if(status==MH_OK) {
        first=true;
        status=MH_CreateHook(vt[20],reinterpret_cast<void*>(instancedHook),reinterpret_cast<void**>(&instanced));
        second=status==MH_OK;
    }
    if(first&&second) {
        status=MH_EnableHook(vt[12]);
        if(status==MH_OK)status=MH_EnableHook(vt[20]);
        if(status==MH_OK){ctx->AddRef();context=ctx;return true;}
    }
    if(first){MH_DisableHook(vt[12]);MH_RemoveHook(vt[12]);}
    if(second){MH_DisableHook(vt[20]);MH_RemoveHook(vt[20]);}
    indexed=nullptr;instanced=nullptr;blend->Release();blend=nullptr;
    return false;
}
}
