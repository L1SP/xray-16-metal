#include "stdafx.h"
#include "mtlState.h"
#include "mtlStateUtils.h"

namespace xray::render::RENDER_NAMESPACE
{
glState::glState()
{
    memset(m_samplerArray, 0, sizeof(m_samplerArray));

    rasterizerCullMode = D3DCULL_CCW;

    m_pDepthStencilState.DepthEnable = TRUE;
    m_pDepthStencilState.DepthFunc = D3DCMP_LESSEQUAL;
    m_pDepthStencilState.DepthWriteMask = TRUE;
    m_pDepthStencilState.StencilEnable = TRUE;
    m_pDepthStencilState.StencilFailOp = D3DSTENCILOP_KEEP;
    m_pDepthStencilState.StencilDepthFailOp = D3DSTENCILOP_KEEP;
    m_pDepthStencilState.StencilPassOp = D3DSTENCILOP_KEEP;
    m_pDepthStencilState.StencilFunc = D3DCMP_ALWAYS;
    m_pDepthStencilState.StencilMask = 0xFFFFFFFF;
    m_pDepthStencilState.StencilWriteMask = 0xFFFFFFFF;
    m_pDepthStencilState.StencilRef = 0;

    m_pBlendState.BlendEnable = TRUE;
    m_pBlendState.SrcBlend = D3DBLEND_ONE;
    m_pBlendState.DestBlend = D3DBLEND_ZERO;
    m_pBlendState.SrcBlendAlpha = D3DBLEND_ONE;
    m_pBlendState.DestBlendAlpha = D3DBLEND_ZERO;
    m_pBlendState.BlendOp = D3DBLENDOP_ADD;
    m_pBlendState.BlendOpAlpha = D3DBLENDOP_ADD;
    m_pBlendState.ColorMask = 0xF;

    m_uiMipLODBias = FLT_MAX;
}

glState* glState::Create()
{
    return xr_new<glState>();
}

glState::~glState()
{
    Release();
}

void glState::Release()
{
    auto* device = static_cast<MTL::Device*>(HW.m_device);
    if (device)
    {
        for (auto& sampler : m_samplerArray)
        {
            if (sampler)
            {
                static_cast<MTL::SamplerState*>(sampler)->release();
                sampler = nullptr;
            }
        }
    }
}

void glState::Apply()
{
    auto* enc = static_cast<MTL::RenderCommandEncoder*>(HW.m_currentEncoder);
    if (!enc)
        return;

    // Bind per-stage samplers
    for (size_t stage = 0; stage < MTL_MAX_COMBINED_TEXTURES; stage++)
    {
        if (m_samplerArray[stage])
        {
            auto* sampler = static_cast<MTL::SamplerState*>(m_samplerArray[stage]);
            enc->setFragmentSamplerState(sampler, static_cast<NS::UInteger>(stage));
        }
    }

    // Build combined depth-stencil state (single encoder call avoids clobbering)
    MTL::DepthStencilDescriptor* dsDesc = MTL::DepthStencilDescriptor::alloc()->init();
    if (m_pDepthStencilState.DepthEnable)
    {
        dsDesc->setDepthCompareFunction(
            static_cast<MTL::CompareFunction>(mtlStateUtils::ConvertCmpFunction(m_pDepthStencilState.DepthFunc)));
        dsDesc->setDepthWriteEnabled(m_pDepthStencilState.DepthWriteMask ? true : false);
    }
    else
    {
        dsDesc->setDepthCompareFunction(MTL::CompareFunctionAlways);
        dsDesc->setDepthWriteEnabled(false);
    }
    if (m_pDepthStencilState.StencilEnable)
    {
        auto* stencilDesc = dsDesc->backFaceStencil();
        stencilDesc->setStencilCompareFunction(
            static_cast<MTL::CompareFunction>(mtlStateUtils::ConvertCmpFunction(m_pDepthStencilState.StencilFunc)));
        stencilDesc->setStencilFailureOperation(
            static_cast<MTL::StencilOperation>(mtlStateUtils::ConvertStencilOp(m_pDepthStencilState.StencilFailOp)));
        stencilDesc->setDepthFailureOperation(
            static_cast<MTL::StencilOperation>(mtlStateUtils::ConvertStencilOp(m_pDepthStencilState.StencilDepthFailOp)));
        stencilDesc->setDepthStencilPassOperation(
            static_cast<MTL::StencilOperation>(mtlStateUtils::ConvertStencilOp(m_pDepthStencilState.StencilPassOp)));
        stencilDesc->setReadMask(m_pDepthStencilState.StencilMask);
        stencilDesc->setWriteMask(m_pDepthStencilState.StencilWriteMask);
    }
    auto* device = static_cast<MTL::Device*>(HW.m_device);
    auto* dsState = device->newDepthStencilState(dsDesc);
    if (dsState)
    {
        enc->setDepthStencilState(dsState);
        dsState->release();
    }
    dsDesc->release();

    RCache.set_CullMode(rasterizerCullMode);
    RCache.set_BlendEnable(m_pBlendState.BlendEnable);
    RCache.set_ColorWriteEnable(m_pBlendState.ColorMask);
}

void glState::UpdateRenderState(u32 name, u32 value)
{
    switch (name)
    {
    case D3DRS_CULLMODE:
        rasterizerCullMode = static_cast<D3DCULL>(value);
        break;

    case D3DRS_ZENABLE:
        m_pDepthStencilState.DepthEnable = value ? TRUE : FALSE;
        break;

    case D3DRS_ZWRITEENABLE:
        m_pDepthStencilState.DepthWriteMask = value ? TRUE : FALSE;
        break;

    case D3DRS_ZFUNC:
        m_pDepthStencilState.DepthFunc = static_cast<D3DCMPFUNC>(value);
        break;

    case D3DRS_STENCILENABLE:
        m_pDepthStencilState.StencilEnable = value ? TRUE : FALSE;
        break;

    case D3DRS_STENCILMASK:
        m_pDepthStencilState.StencilMask = value;
        break;

    case D3DRS_STENCILWRITEMASK:
        m_pDepthStencilState.StencilWriteMask = value;
        break;

    case D3DRS_STENCILFAIL:
        m_pDepthStencilState.StencilFailOp = static_cast<D3DSTENCILOP>(value);
        break;

    case D3DRS_STENCILZFAIL:
        m_pDepthStencilState.StencilDepthFailOp = static_cast<D3DSTENCILOP>(value);
        break;

    case D3DRS_STENCILPASS:
        m_pDepthStencilState.StencilPassOp = static_cast<D3DSTENCILOP>(value);
        break;

    case D3DRS_STENCILFUNC:
        m_pDepthStencilState.StencilFunc = static_cast<D3DCMPFUNC>(value);
        break;

    case D3DRS_STENCILREF:
        m_pDepthStencilState.StencilRef = value;
        break;

    case D3DRS_SRCBLEND:
        m_pBlendState.SrcBlend = static_cast<D3DBLEND>(value);
        break;

    case D3DRS_DESTBLEND:
        m_pBlendState.DestBlend = static_cast<D3DBLEND>(value);
        break;

    case D3DRS_BLENDOP:
        m_pBlendState.BlendOp = static_cast<D3DBLENDOP>(value);
        break;

    case D3DRS_SRCBLENDALPHA:
        m_pBlendState.SrcBlendAlpha = static_cast<D3DBLEND>(value);
        break;

    case D3DRS_DESTBLENDALPHA:
        m_pBlendState.DestBlendAlpha = static_cast<D3DBLEND>(value);
        break;

    case D3DRS_BLENDOPALPHA:
        m_pBlendState.BlendOpAlpha = static_cast<D3DBLENDOP>(value);
        break;

    case D3DRS_ALPHABLENDENABLE:
        m_pBlendState.BlendEnable = value ? TRUE : FALSE;
        break;

    case D3DRS_COLORWRITEENABLE:
    case D3DRS_COLORWRITEENABLE1:
    case D3DRS_COLORWRITEENABLE2:
    case D3DRS_COLORWRITEENABLE3:
        m_pBlendState.ColorMask = value;
        break;

    case D3DRS_LIGHTING:
    case D3DRS_FOGENABLE:
    case D3DRS_ALPHATESTENABLE:
    case D3DRS_ALPHAREF:
        // Deprecated / not used in Metal
        break;

    default:
        VERIFY(!"Render state not implemented");
        break;
    }
}

void glState::UpdateSamplerState(u32 stage, u32 name, u32 value)
{
    if (stage >= MTL_MAX_COMBINED_TEXTURES)
        return;

    if (!m_samplerArray[stage])
    {
        // Create default sampler descriptor first
        auto* device = static_cast<MTL::Device*>(HW.m_device);
        if (!device)
            return;

        MTL::SamplerDescriptor* desc = MTL::SamplerDescriptor::alloc()->init();
        desc->setMinFilter(MTL::SamplerMinMagFilterLinear);
        desc->setMagFilter(MTL::SamplerMinMagFilterLinear);
        desc->setMipFilter(MTL::SamplerMipFilterLinear);

        auto* sampler = device->newSamplerState(desc);
        desc->release();
        if (sampler)
        {
            sampler->retain();
            m_samplerArray[stage] = sampler;
        }
    }

    auto* mtlSampler = static_cast<MTL::SamplerState*>(m_samplerArray[stage]);
    if (!mtlSampler)
        return;

    // Re-create sampler if properties change
    auto* device = static_cast<MTL::Device*>(HW.m_device);
    if (!device)
        return;

    MTL::SamplerDescriptor* desc = MTL::SamplerDescriptor::alloc()->init();
    // Read current settings from the existing sampler (we need to rebuild)
    desc->setMinFilter(MTL::SamplerMinMagFilterLinear);
    desc->setMagFilter(MTL::SamplerMinMagFilterLinear);
    desc->setMipFilter(MTL::SamplerMipFilterLinear);
    desc->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    desc->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
    desc->setRAddressMode(MTL::SamplerAddressModeClampToEdge);

    switch (name)
    {
    case D3DSAMP_ADDRESSU:
        desc->setSAddressMode(static_cast<MTL::SamplerAddressMode>(mtlStateUtils::ConvertTextureAddressMode(value)));
        break;
    case D3DSAMP_ADDRESSV:
        desc->setTAddressMode(static_cast<MTL::SamplerAddressMode>(mtlStateUtils::ConvertTextureAddressMode(value)));
        break;
    case D3DSAMP_ADDRESSW:
        desc->setRAddressMode(static_cast<MTL::SamplerAddressMode>(mtlStateUtils::ConvertTextureAddressMode(value)));
        break;
    case D3DSAMP_BORDERCOLOR:
    {
        u32 alpha = (value >> 24) & 0xFF;
        u32 red = (value >> 16) & 0xFF;
        u32 green = (value >> 8) & 0xFF;
        u32 blue = value & 0xFF;
        if (alpha == 0 && red == 0 && green == 0 && blue == 0)
            desc->setBorderColor(MTL::SamplerBorderColorTransparentBlack);
        else if (red == 0 && green == 0 && blue == 0 && alpha == 255)
            desc->setBorderColor(MTL::SamplerBorderColorOpaqueBlack);
        else if (red == 255 && green == 255 && blue == 255 && alpha == 255)
            desc->setBorderColor(MTL::SamplerBorderColorOpaqueWhite);
        else
            desc->setBorderColor(MTL::SamplerBorderColorOpaqueBlack);
        break;
    }
    case D3DSAMP_MAGFILTER:
    {
        u32 filter = mtlStateUtils::ConvertTextureFilter(value);
        desc->setMagFilter((filter & 1) ? MTL::SamplerMinMagFilterLinear : MTL::SamplerMinMagFilterNearest);
        break;
    }
    case D3DSAMP_MINFILTER:
    {
        u32 filter = mtlStateUtils::ConvertTextureFilter(value, 0, false);
        desc->setMinFilter((filter & 1) ? MTL::SamplerMinMagFilterLinear : MTL::SamplerMinMagFilterNearest);
        break;
    }
    case D3DSAMP_MIPFILTER:
    {
        u32 filter = mtlStateUtils::ConvertTextureFilter(value, 0, true);
        desc->setMipFilter((filter & 2) ? MTL::SamplerMipFilterLinear : MTL::SamplerMipFilterNearest);
        if (!(filter & 0x100))
            desc->setMipFilter(MTL::SamplerMipFilterNotMipmapped);
        break;
    }
    case D3DSAMP_MIPMAPLODBIAS:
        // Metal does not support explicit LOD bias; set min/max clamp instead
        desc->setLodMinClamp(0.0f);
        desc->setLodMaxClamp(FLT_MAX);
        break;
    case D3DSAMP_MAXMIPLEVEL:
        desc->setLodMaxClamp(static_cast<float>(value));
        break;
    case D3DSAMP_MAXANISOTROPY:
    {
        NS::UInteger aniso = static_cast<NS::UInteger>(value > 16 ? 16 : (value < 1 ? 1 : value));
        desc->setMaxAnisotropy(aniso);
        break;
    }
    default:
        desc->release();
        return;
    }

    auto* newSampler = device->newSamplerState(desc);
    desc->release();
    if (newSampler)
    {
        static_cast<MTL::SamplerState*>(m_samplerArray[stage])->release();
        m_samplerArray[stage] = newSampler;
    }
}
} // namespace xray::render::RENDER_NAMESPACE
