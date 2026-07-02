#pragma once

#include "xrCore/_flags.h"

// Must match CTexture::mtMaxCombinedShaderTextures for OGL/Metal (16+4+16)
static constexpr u32 MTL_MAX_COMBINED_TEXTURES = 36;

namespace xray::render::RENDER_NAMESPACE
{
typedef struct
{
    BOOL DepthEnable;
    BOOL DepthWriteMask;
    D3DCMPFUNC DepthFunc;
    BOOL StencilEnable;
    u32 StencilMask;
    u32 StencilWriteMask;
    D3DSTENCILOP StencilFailOp;
    D3DSTENCILOP StencilDepthFailOp;
    D3DSTENCILOP StencilPassOp;
    D3DCMPFUNC StencilFunc;
    u32 StencilRef;
} D3D_DEPTH_STENCIL_STATE;

typedef struct
{
    BOOL BlendEnable;
    D3DBLEND SrcBlend;
    D3DBLEND DestBlend;
    D3DBLENDOP BlendOp;
    D3DBLEND SrcBlendAlpha;
    D3DBLEND DestBlendAlpha;
    D3DBLENDOP BlendOpAlpha;
    u32 ColorMask;
} D3D_BLEND_STATE;

class glState // reuse name to match shared code expectations
{
private:
    D3DCULL rasterizerCullMode;
    D3D_DEPTH_STENCIL_STATE m_pDepthStencilState;
    D3D_BLEND_STATE m_pBlendState;
    float m_uiMipLODBias;

    void* m_samplerArray[MTL_MAX_COMBINED_TEXTURES]{};

public:
    glState();
    ~glState();

    void Apply();
    void Release();

    void UpdateRenderState(u32 name, u32 value);
    void UpdateSamplerState(u32 stage, u32 name, u32 value);

    static glState* Create();
};
} // namespace xray::render::RENDER_NAMESPACE
