#pragma once

#include "mtlStateUtils.h"

namespace xray::render::RENDER_NAMESPACE
{
// ── Vertex attribute location mapping (matches GL backend VertexUsageList) ──
static const u32 c_VertexUsageToLocation[] = {
    3,    // D3DDECLUSAGE_POSITION = 0
    ~0u,  // D3DDECLUSAGE_BLENDWEIGHT = 1
    ~0u,  // D3DDECLUSAGE_BLENDINDICES = 2
    5,    // D3DDECLUSAGE_NORMAL = 3
    ~0u,  // D3DDECLUSAGE_PSIZE = 4
    8,    // D3DDECLUSAGE_TEXCOORD = 5
    4,    // D3DDECLUSAGE_TANGENT = 6
    6,    // D3DDECLUSAGE_BINORMAL = 7
    ~0u,  // D3DDECLUSAGE_TESSFACTOR = 8
    3,    // D3DDECLUSAGE_POSITIONT = 9
    0,    // D3DDECLUSAGE_COLOR = 10
    7,    // D3DDECLUSAGE_FOG = 11
    ~0u,  // D3DDECLUSAGE_DEPTH = 12
    ~0u,  // D3DDECLUSAGE_SAMPLE = 13
};

// ── Pipeline state cache ─────────────────────────────────────────────────
struct MTLPipelineKey
{
    u32 vsID;
    u32 psID;
    u32 declID;
    u32 blendEnabled;  // 0/1
    u32 srcBlend;
    u32 destBlend;
    u32 blendOp;
    u32 srcBlendAlpha;
    u32 destBlendAlpha;
    u32 blendOpAlpha;
    u32 colorWriteMask;
    u32 colorPF[3];    // MTL::PixelFormat for attachments 0/1/2
    u32 depthPF;       // MTL::PixelFormat for depth
    u32 stencilPF;     // MTL::PixelFormat for stencil
    u32 sampleCount;   // raster sample count

    bool operator<(const MTLPipelineKey& o) const
    {
        if (vsID != o.vsID) return vsID < o.vsID;
        if (psID != o.psID) return psID < o.psID;
        if (declID != o.declID) return declID < o.declID;
        if (blendEnabled != o.blendEnabled) return blendEnabled < o.blendEnabled;
        if (srcBlend != o.srcBlend) return srcBlend < o.srcBlend;
        if (destBlend != o.destBlend) return destBlend < o.destBlend;
        if (blendOp != o.blendOp) return blendOp < o.blendOp;
        if (srcBlendAlpha != o.srcBlendAlpha) return srcBlendAlpha < o.srcBlendAlpha;
        if (destBlendAlpha != o.destBlendAlpha) return destBlendAlpha < o.destBlendAlpha;
        if (blendOpAlpha != o.blendOpAlpha) return blendOpAlpha < o.blendOpAlpha;
        if (colorWriteMask != o.colorWriteMask) return colorWriteMask < o.colorWriteMask;
        for (int i = 0; i < 3; i++)
            if (colorPF[i] != o.colorPF[i]) return colorPF[i] < o.colorPF[i];
        if (depthPF != o.depthPF) return depthPF < o.depthPF;
        if (stencilPF != o.stencilPF) return stencilPF < o.stencilPF;
        return sampleCount < o.sampleCount;
    }
};

static xr_map<MTLPipelineKey, void*> s_pipelineCache;

inline MTL::PrimitiveType TranslateTopology(D3DPRIMITIVETYPE T)
{
    switch (T)
    {
    case D3DPT_POINTLIST:     return MTL::PrimitiveTypePoint;
    case D3DPT_LINELIST:      return MTL::PrimitiveTypeLine;
    case D3DPT_LINESTRIP:     return MTL::PrimitiveTypeLineStrip;
    case D3DPT_TRIANGLELIST:  return MTL::PrimitiveTypeTriangle;
    case D3DPT_TRIANGLESTRIP: return MTL::PrimitiveTypeTriangleStrip;
    case D3DPT_TRIANGLEFAN:   return MTL::PrimitiveTypeTriangleStrip;
    default:                  return MTL::PrimitiveTypeTriangle;
    }
}

static pcstr PixelFormatName(u32 fmt)
{
    switch (fmt)
    {
    case MTL::PixelFormatInvalid: return "Invalid";
    case MTL::PixelFormatBGRA8Unorm: return "BGRA8Unorm";
    case MTL::PixelFormatBGRA8Unorm_sRGB: return "BGRA8Unorm_sRGB";
    case MTL::PixelFormatRGBA8Unorm: return "RGBA8Unorm";
    case MTL::PixelFormatRGBA8Unorm_sRGB: return "RGBA8Unorm_sRGB";
    case MTL::PixelFormatRGBA16Float: return "RGBA16Float";
    case MTL::PixelFormatRGBA32Float: return "RGBA32Float";
    case MTL::PixelFormatR32Float: return "R32Float";
    case MTL::PixelFormatDepth32Float: return "Depth32Float";
    case MTL::PixelFormatDepth32Float_Stencil8: return "Depth32Float_Stencil8";
    case MTL::PixelFormatBC1_RGBA: return "BC1_RGBA";
    case MTL::PixelFormatBC3_RGBA: return "BC3_RGBA";
    case MTL::PixelFormatBC4_RUnorm: return "BC4_RUnorm";
    case MTL::PixelFormatBC5_RGUnorm: return "BC5_RGUnorm";
    case MTL::PixelFormatBC7_RGBAUnorm: return "BC7_RGBAUnorm";
    default: return "?";
    }
}

// Convert D3D color write mask bits → Metal color write mask bits.
// D3D: RED=1, GREEN=2, BLUE=4, ALPHA=8
// Metal: RED=8, GREEN=4, BLUE=2, ALPHA=1
static MTL::ColorWriteMask ConvertWriteMask(u32 d3dMask)
{
    MTL::ColorWriteMask mtl = MTL::ColorWriteMask(0);
    if (d3dMask & 1) mtl |= MTL::ColorWriteMaskRed;
    if (d3dMask & 2) mtl |= MTL::ColorWriteMaskGreen;
    if (d3dMask & 4) mtl |= MTL::ColorWriteMaskBlue;
    if (d3dMask & 8) mtl |= MTL::ColorWriteMaskAlpha;
    return mtl;
}

inline MTL::RenderPipelineState* get_or_create_pipeline(u32 vsID, u32 psID, u32 declID, u32 vbStride, SDeclaration* decl,
    bool blendEnabled, u32 srcBlend, u32 destBlend, u32 blendOp,
    u32 srcBlendAlpha, u32 destBlendAlpha, u32 blendOpAlpha,
    u32 colorWriteMask)
{
    // Build key with pixel formats from current render pass descriptor
    MTLPipelineKey key{};
    key.vsID = vsID;
    key.psID = psID;
    key.declID = declID;
    key.blendEnabled = blendEnabled ? 1u : 0u;
    key.srcBlend = (srcBlend != u32(-1)) ? srcBlend : D3DBLEND_ONE;
    key.destBlend = (destBlend != u32(-1)) ? destBlend : D3DBLEND_ZERO;
    key.blendOp = (blendOp != u32(-1)) ? blendOp : D3DBLENDOP_ADD;
    key.srcBlendAlpha = (srcBlendAlpha != u32(-1)) ? srcBlendAlpha : D3DBLEND_ONE;
    key.destBlendAlpha = (destBlendAlpha != u32(-1)) ? destBlendAlpha : D3DBLEND_ZERO;
    key.blendOpAlpha = (blendOpAlpha != u32(-1)) ? blendOpAlpha : D3DBLENDOP_ADD;
    key.colorWriteMask = (colorWriteMask != u32(-1)) ? colorWriteMask : 0xF;

    auto* rpd = static_cast<MTL::RenderPassDescriptor*>(HW.m_currentRPD);
    if (rpd)
    {
        for (int i = 0; i < 3; i++)
        {
            auto* ca = rpd->colorAttachments()->object(i);
            if (ca && ca->texture())
                key.colorPF[i] = static_cast<u32>(ca->texture()->pixelFormat());
        }
        auto* da = rpd->depthAttachment();
        if (da && da->texture())
            key.depthPF = static_cast<u32>(da->texture()->pixelFormat());
        auto* sa = rpd->stencilAttachment();
        if (sa && sa->texture())
            key.stencilPF = static_cast<u32>(sa->texture()->pixelFormat());
        if (auto* tex = rpd->colorAttachments()->object(0)->texture())
            key.sampleCount = static_cast<u32>(tex->sampleCount());
    }
    if (key.sampleCount == 0)
        key.sampleCount = 1;

    auto it = s_pipelineCache.find(key);
    if (it != s_pipelineCache.end())
        return static_cast<MTL::RenderPipelineState*>(it->second);

    auto* device = static_cast<MTL::Device*>(HW.m_device);
    auto* vsFunc = static_cast<MTL::Function*>(lookup_shader_func(vsID));
    auto* psFunc = static_cast<MTL::Function*>(lookup_shader_func(psID));
    if (!vsFunc || !psFunc)
        return nullptr;

    MTL::RenderPipelineDescriptor* psoDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    psoDesc->setVertexFunction(vsFunc);
    psoDesc->setFragmentFunction(psFunc);
    psoDesc->setRasterSampleCount(key.sampleCount);

    // Set pixel formats from the current render pass descriptor (must match exactly)
    if (rpd)
    {
        for (int i = 0; i < 3; i++)
        {
            auto* ca = rpd->colorAttachments()->object(i);
            if (ca && ca->texture())
                psoDesc->colorAttachments()->object(i)->setPixelFormat(ca->texture()->pixelFormat());
        }
        auto* da = rpd->depthAttachment();
        if (da && da->texture())
            psoDesc->setDepthAttachmentPixelFormat(da->texture()->pixelFormat());
        auto* sa = rpd->stencilAttachment();
        if (sa && sa->texture())
            psoDesc->setStencilAttachmentPixelFormat(sa->texture()->pixelFormat());
    }

    // Configure blend state and write mask for all color attachments
    for (int i = 0; i < 3; i++)
    {
        auto* ca = psoDesc->colorAttachments()->object(i);
        ca->setWriteMask(ConvertWriteMask(key.colorWriteMask));
        if (key.blendEnabled && i == 0)
        {
            ca->setBlendingEnabled(true);
            ca->setRgbBlendOperation(
                static_cast<MTL::BlendOperation>(mtlStateUtils::ConvertBlendOp(key.blendOp)));
            ca->setAlphaBlendOperation(
                static_cast<MTL::BlendOperation>(mtlStateUtils::ConvertBlendOp(key.blendOpAlpha)));
            ca->setSourceRGBBlendFactor(
                static_cast<MTL::BlendFactor>(mtlStateUtils::ConvertBlendArg(key.srcBlend)));
            ca->setSourceAlphaBlendFactor(
                static_cast<MTL::BlendFactor>(mtlStateUtils::ConvertBlendArg(key.srcBlendAlpha)));
            ca->setDestinationRGBBlendFactor(
                static_cast<MTL::BlendFactor>(mtlStateUtils::ConvertBlendArg(key.destBlend)));
            ca->setDestinationAlphaBlendFactor(
                static_cast<MTL::BlendFactor>(mtlStateUtils::ConvertBlendArg(key.destBlendAlpha)));
        }
        else
        {
            ca->setBlendingEnabled(false);
        }
    }

    MTL::VertexDescriptor* vertDesc = MTL::VertexDescriptor::alloc()->init();
    u32 maxAttr = 0;
    if (decl && !decl->dcl_code.empty())
    {
        for (const auto& elem : decl->dcl_code)
        {
            if (elem.Stream != 0)
                continue;
            if (elem.Usage >= sizeof(c_VertexUsageToLocation) / sizeof(c_VertexUsageToLocation[0]))
                continue;
            u32 baseLocation = c_VertexUsageToLocation[elem.Usage];
            if (baseLocation == ~0u)
                continue;
            u32 location = baseLocation + elem.UsageIndex;
            if (location > maxAttr) maxAttr = location;

            MTL::VertexFormat fmt = MTL::VertexFormatInvalid;
            switch (elem.Type)
            {
            case D3DDECLTYPE_FLOAT1:   fmt = MTL::VertexFormatFloat; break;
            case D3DDECLTYPE_FLOAT2:   fmt = MTL::VertexFormatFloat2; break;
            case D3DDECLTYPE_FLOAT3:   fmt = MTL::VertexFormatFloat3; break;
            case D3DDECLTYPE_FLOAT4:   fmt = MTL::VertexFormatFloat4; break;
            case D3DDECLTYPE_D3DCOLOR: fmt = MTL::VertexFormatUChar4Normalized; break;
            case D3DDECLTYPE_UBYTE4:   fmt = MTL::VertexFormatUChar4; break;
            case D3DDECLTYPE_UBYTE4N:  fmt = MTL::VertexFormatUChar4Normalized; break;
            case D3DDECLTYPE_SHORT2:   fmt = MTL::VertexFormatShort2; break;
            case D3DDECLTYPE_SHORT4:   fmt = MTL::VertexFormatShort4; break;
            case D3DDECLTYPE_SHORT2N:  fmt = MTL::VertexFormatShort2Normalized; break;
            case D3DDECLTYPE_SHORT4N:  fmt = MTL::VertexFormatShort4Normalized; break;
            default: break;
            }
            if (fmt != MTL::VertexFormatInvalid)
            {
                vertDesc->attributes()->object(location)->setFormat(fmt);
                vertDesc->attributes()->object(location)->setOffset(elem.Offset);
                vertDesc->attributes()->object(location)->setBufferIndex(30);
            }
        }
    }
    vertDesc->layouts()->object(30)->setStride(vbStride);
    psoDesc->setVertexDescriptor(vertDesc);
    vertDesc->release();

    NS::Error* error = nullptr;
    // Capture write mask values BEFORE releasing psoDesc
    u32 wm0 = MTL::ColorWriteMaskAll, wm1 = MTL::ColorWriteMaskAll, wm2 = MTL::ColorWriteMaskAll;
    if (psoDesc)
    {
        wm0 = (u32)psoDesc->colorAttachments()->object(0)->writeMask();
        wm1 = (u32)psoDesc->colorAttachments()->object(1)->writeMask();
        wm2 = (u32)psoDesc->colorAttachments()->object(2)->writeMask();
    }

    auto* pso = device->newRenderPipelineState(psoDesc, &error);
    if (!pso)
    {
        if (error)
            Msg("! Metal PSO creation failed: %s", error->localizedDescription()->utf8String());
        psoDesc->release();
        return nullptr;
    }
    psoDesc->release();

    s_pipelineCache[key] = pso;
    return pso;
}

inline MTL::SamplerState* get_default_sampler()
{
    return static_cast<MTL::SamplerState*>(HW.m_defaultSampler);
}

// ── CBackend inline methods ──────────────────────────────────────────────

IC void CBackend::set_xform(u32 ID, const Fmatrix& M)
{
    stat.xforms++;
    switch (ID)
    {
    case 0: xforms.m_wvp = M; break;
    case 1: xforms.m_wv = M; break;
    case 2: xforms.m_vp = M; break;
    case 3: xforms.m_p = M; break;
    }
}

IC u32 CBackend::get_FB()
{
    return pFB;
}

IC void CBackend::set_FB(u32 FB)
{
    if (FB != pFB)
    {
        PGO(Msg("PGO:set_FB"));
        pFB = FB;
        // TODO: Bind Metal framebuffer
    }
}

IC void CBackend::set_RT(u32 RT, u32 ID)
{
    if (RT != pRT[ID])
    {
        PGO(Msg("PGO:setRT"));
        stat.target_rt++;
        pRT[ID] = RT;
        // TODO: Set Metal render target texture
    }
}

IC void CBackend::set_ZB(u32 ZB)
{
    if (ZB != pZB)
    {
        PGO(Msg("PGO:setZB"));
        stat.target_zb++;
        pZB = ZB;
        // TODO: Set Metal depth stencil texture
    }
}

IC void CBackend::ClearRT(u32 rt, const Fcolor& color)
{
    if (!rt)
        return;
    MTL::Texture* tex = lookup_mtl_texture(rt);
    if (!tex)
        return;

    // End current encoder
    HW.EndEncoding();

    // Create a temporary encoder with LoadActionClear for this RT
    MTL::RenderPassDescriptor* clearRPD = MTL::RenderPassDescriptor::alloc()->init();
    auto* ca = clearRPD->colorAttachments()->object(0);
    ca->setTexture(tex);
    ca->setLoadAction(MTL::LoadActionClear);
    ca->setClearColor(MTL::ClearColor::Make(color.r, color.g, color.b, color.a));
    ca->setStoreAction(MTL::StoreActionStore);

    auto* cmdBuffer = static_cast<MTL::CommandBuffer*>(HW.m_currentCmdBuffer);
    if (cmdBuffer)
    {
        MTL::RenderCommandEncoder* enc = cmdBuffer->renderCommandEncoder(clearRPD);
        if (enc)
        {
            enc->endEncoding();
        }
    }
    clearRPD->release();

    // Recreate the original encoder from current bound RT state
    if (pRT[0] || pRT[1] || pRT[2] || pZB)
    {
        MTL::Texture* mtlColor[3] = {};
        if (pRT[0]) mtlColor[0] = lookup_mtl_texture(pRT[0]);
        if (pRT[1]) mtlColor[1] = lookup_mtl_texture(pRT[1]);
        if (pRT[2]) mtlColor[2] = lookup_mtl_texture(pRT[2]);
        MTL::Texture* mtlDepth = pZB ? lookup_mtl_texture(pZB) : nullptr;

        MTL::RenderPassDescriptor* rpd = HW.CreateRPD(mtlColor[0], mtlColor[1], mtlColor[2], mtlDepth);
        if (rpd)
        {
            HW.CreateEncoder(rpd);
            rpd->release();
        }
    }
}

IC void CBackend::ClearZB(u32 zb, float depth)
{
    if (!zb)
        return;
    MTL::Texture* tex = lookup_mtl_texture(zb);
    if (!tex)
        return;

    HW.EndEncoding();

    MTL::RenderPassDescriptor* clearRPD = MTL::RenderPassDescriptor::alloc()->init();
    auto* da = clearRPD->depthAttachment();
    da->setTexture(tex);
    da->setLoadAction(MTL::LoadActionClear);
    da->setClearDepth(depth);
    da->setStoreAction(MTL::StoreActionStore);

    auto* cmdBuffer = static_cast<MTL::CommandBuffer*>(HW.m_currentCmdBuffer);
    if (cmdBuffer)
    {
        MTL::RenderCommandEncoder* enc = cmdBuffer->renderCommandEncoder(clearRPD);
        if (enc)
        {
            enc->endEncoding();
            // NOTE: enc is autoreleased — NO release() call
        }
    }
    clearRPD->release();

    // Recreate the original encoder
    MTL::Texture* mtlColor[3] = {};
    if (pRT[0]) mtlColor[0] = lookup_mtl_texture(pRT[0]);
    if (pRT[1]) mtlColor[1] = lookup_mtl_texture(pRT[1]);
    if (pRT[2]) mtlColor[2] = lookup_mtl_texture(pRT[2]);
    MTL::Texture* mtlDepth = pZB ? lookup_mtl_texture(pZB) : nullptr;

    MTL::RenderPassDescriptor* rpd = HW.CreateRPD(mtlColor[0], mtlColor[1], mtlColor[2], mtlDepth);
    if (rpd)
    {
        HW.CreateEncoder(rpd);
        rpd->release();
    }
}

IC void CBackend::ClearZB(u32 zb, float depth, u8 stencil)
{
    if (!zb)
        return;
    MTL::Texture* tex = lookup_mtl_texture(zb);
    if (!tex)
        return;

    HW.EndEncoding();

    MTL::RenderPassDescriptor* clearRPD = MTL::RenderPassDescriptor::alloc()->init();
    auto* da = clearRPD->depthAttachment();
    da->setTexture(tex);
    da->setLoadAction(MTL::LoadActionClear);
    da->setClearDepth(depth);
    da->setStoreAction(MTL::StoreActionStore);
    // Metal does not have a separate stencil clear action in the depth attachment;
    // the stencil is cleared together with depth when LoadActionClear is set on the
    // depth attachment. The stencil clear value defaults to 0. For custom stencil
    // values, we'd need a separate stencil attachment descriptor, but most backends
    // don't use this overload with a non-zero stencil value in practice.

    auto* cmdBuffer = static_cast<MTL::CommandBuffer*>(HW.m_currentCmdBuffer);
    if (cmdBuffer)
    {
        MTL::RenderCommandEncoder* enc = cmdBuffer->renderCommandEncoder(clearRPD);
        if (enc)
        {
            enc->endEncoding();
        }
    }
    clearRPD->release();

    // Recreate the original encoder
    MTL::Texture* mtlColor[3] = {};
    if (pRT[0]) mtlColor[0] = lookup_mtl_texture(pRT[0]);
    if (pRT[1]) mtlColor[1] = lookup_mtl_texture(pRT[1]);
    if (pRT[2]) mtlColor[2] = lookup_mtl_texture(pRT[2]);
    MTL::Texture* mtlDepth = pZB ? lookup_mtl_texture(pZB) : nullptr;

    MTL::RenderPassDescriptor* rpd = HW.CreateRPD(mtlColor[0], mtlColor[1], mtlColor[2], mtlDepth);
    if (rpd)
    {
        HW.CreateEncoder(rpd);
        rpd->release();
    }
}

IC bool CBackend::ClearRTRect(u32 rt, const Fcolor& color, size_t numRects, const Irect* rects)
{
    // TODO: Implement Metal scissored RT clear
    return false;
}

IC bool CBackend::ClearZBRect(u32 zb, float depth, size_t numRects, const Irect* rects)
{
    // TODO: Implement Metal scissored ZB clear
    return false;
}

ICF void CBackend::set_Format(SDeclaration* _decl)
{
    if (decl != _decl)
    {
        PGO(Msg("PGO:v_format:%x", _decl));
#ifdef DEBUG
        stat.decl++;
#endif
        decl = _decl;
        ib = 0;
    }
}

ICF void CBackend::set_PS(u32 _ps, LPCSTR _n)
{
    if (ps != _ps)
    {
        PGO(Msg("PGO:Pshader:%d,%s", _ps, _n ? _n : ""));
        stat.ps++;
        ps = _ps;
#ifdef DEBUG
        ps_name = _n;
#endif
    }
}

ICF void CBackend::set_GS(u32 _gs, LPCSTR _n)
{
    if (gs != _gs)
    {
        PGO(Msg("PGO:Gshader:%d,%s", _gs, _n ? _n : ""));
        stat.gs++;
        gs = _gs;
#ifdef DEBUG
        gs_name = _n;
#endif
    }
}

ICF void CBackend::set_VS(u32 _vs, LPCSTR _n)
{
    if (vs != _vs)
    {
        PGO(Msg("PGO:Vshader:%d,%s", _vs, _n ? _n : ""));
        stat.vs++;
        vs = _vs;
#ifdef DEBUG
        vs_name = _n;
#endif
    }
}

ICF void CBackend::set_PP(u32 _pp, pcstr _n)
{
    if (pp != _pp)
    {
        PGO(Msg("PGO:PPshader:%d,%s", _pp, _n ? _n : ""));
        stat.pp++;
        pp = _pp;
#ifdef DEBUG
        pp_name = _n;
#endif
    }
}

ICF void CBackend::set_Vertices(VertexBufferHandle _vb, u32 _vb_stride)
{
    // Resolve stale SGeometry::vb handles (cached at load time) to the current
    // dynamic stream ring buffer slot. The ring advances on every FLUSH-LOCK.
    auto* resolved = resolve_stream_buffer(static_cast<MTL::Buffer*>(_vb));
    _vb = resolved;

    if (vb != _vb || vb_stride != _vb_stride)
    {
        PGO(Msg("PGO:VB:%x,%d", _vb, _vb_stride));
#ifdef DEBUG
        stat.vb++;
#endif
        vb = _vb;
        vb_stride = _vb_stride;
    }
}

ICF void CBackend::set_Indices(IndexBufferHandle _ib)
{
    if (ib != _ib)
    {
        PGO(Msg("PGO:IB:%x", _ib));
#ifdef DEBUG
        stat.ib++;
#endif
        ib = _ib;
    }
}

ICF void CBackend::Render(D3DPRIMITIVETYPE T, u32 baseV, u32 startV, u32 countV, u32 startI, u32 PC)
{
    stat.render.calls++;
    stat.render.verts += countV;
    stat.render.polys += PC;

    auto* enc = static_cast<MTL::RenderCommandEncoder*>(HW.m_currentEncoder);
    auto* mtlIB = static_cast<MTL::Buffer*>(ib);
    auto* mtlVB = resolve_stream_buffer(static_cast<MTL::Buffer*>(vb));
    if (!enc || !mtlVB || !mtlIB)
    {
        // Safety net: recreate encoder from current RT bindings if it was
        // prematurely ended (e.g. by a sub-pass calling EndEncoding).
        if (!enc && HW.m_currentCmdBuffer && (pRT[0] || pRT[1] || pRT[2] || pZB))
        {
            MTL::Texture* mtlColor[3] = {};
            if (pRT[0]) mtlColor[0] = lookup_mtl_texture(pRT[0]);
            if (pRT[1]) mtlColor[1] = lookup_mtl_texture(pRT[1]);
            if (pRT[2]) mtlColor[2] = lookup_mtl_texture(pRT[2]);
            MTL::Texture* mtlDepth = pZB ? lookup_mtl_texture(pZB) : nullptr;

            MTL::RenderPassDescriptor* rpd = HW.CreateRPD(mtlColor[0], mtlColor[1], mtlColor[2], mtlDepth);
            if (rpd)
            {
                HW.CreateEncoder(rpd);
                rpd->release();
                enc = static_cast<MTL::RenderCommandEncoder*>(HW.m_currentEncoder);
                Msg("[Metal Safety] Recreated encoder from pRT[%u %u %u] ZB=%u",
                    pRT[0], pRT[1], pRT[2], pZB);
            }
        }
        if (!enc || !mtlVB || !mtlIB)
        {
            #ifdef DEBUG
                Msg("! Render indexed skipped: enc=%p vb=%p ib=%p blend=%u ps=%u T=%d baseV=%u countV=%u PC=%u",
                (void*)enc, (void*)mtlVB, (void*)mtlIB, blendEnabled, ps, T, baseV, countV, PC);
            #endif

            return;
        }
    }

    auto* pso = get_or_create_pipeline(
        vs, ps, decl ? decl->dcl : 0, vb_stride, decl,
        blendEnabled, srcBlend, destBlend, blendOp,
        srcBlendAlpha, destBlendAlpha, blendOpAlpha,
        colorwrite_mask);
    if (!pso)
    {
        Msg("! CBackend::Render indexed: PSO null for vs=%u ps=%u blend=%u", vs, ps, blendEnabled);
        return;
    }

    enc->setRenderPipelineState(pso);
    ApplyDS();
    enc->setVertexBuffer(mtlVB, 0, 30);
    constants.flush();

    MTL::PrimitiveType mtlPrim = TranslateTopology(T);
    u32 indexCount = 0;
    switch (T)
    {
    case D3DPT_TRIANGLELIST:  indexCount = PC * 3; break;
    case D3DPT_TRIANGLESTRIP: indexCount = PC + 2; break;
    case D3DPT_TRIANGLEFAN:   indexCount = PC + 2; break;
    case D3DPT_LINELIST:      indexCount = PC * 2; break;
    case D3DPT_LINESTRIP:     indexCount = PC + 1; break;
    case D3DPT_POINTLIST:     indexCount = PC; break;
    default: return;
    }

    enc->drawIndexedPrimitives(mtlPrim, indexCount, MTL::IndexTypeUInt16, mtlIB, startI * sizeof(u16), 1, baseV, 0);

    PGO(Msg("PGO:DIP:%dv/%df", countV, PC));
}

ICF void CBackend::Render(D3DPRIMITIVETYPE T, u32 startV, u32 PC)
{
    stat.render.calls++;
    stat.render.verts += PC;
    stat.render.polys += PC;

    auto* enc = static_cast<MTL::RenderCommandEncoder*>(HW.m_currentEncoder);
    auto* mtlVB = resolve_stream_buffer(static_cast<MTL::Buffer*>(vb));
    if (!enc || !mtlVB)
    {
        // Safety net: recreate encoder from current RT bindings.
        if (!enc && HW.m_currentCmdBuffer && (pRT[0] || pRT[1] || pRT[2] || pZB))
        {
            MTL::Texture* mtlColor[3] = {};
            if (pRT[0]) mtlColor[0] = lookup_mtl_texture(pRT[0]);
            if (pRT[1]) mtlColor[1] = lookup_mtl_texture(pRT[1]);
            if (pRT[2]) mtlColor[2] = lookup_mtl_texture(pRT[2]);
            MTL::Texture* mtlDepth = pZB ? lookup_mtl_texture(pZB) : nullptr;

            MTL::RenderPassDescriptor* rpd = HW.CreateRPD(mtlColor[0], mtlColor[1], mtlColor[2], mtlDepth);
            if (rpd)
            {
                HW.CreateEncoder(rpd);
                rpd->release();
                enc = static_cast<MTL::RenderCommandEncoder*>(HW.m_currentEncoder);
                Msg("[Metal Safety] Recreated encoder (non-indexed) from pRT[%u %u %u] ZB=%u",
                    pRT[0], pRT[1], pRT[2], pZB);
            }
        }
        if (!enc || !mtlVB)
        {
            #ifdef DEBUG
                Msg("! CBackend::Render non-indexed: no encoder or vb (enc=%p vb=%p)", (void*)enc, (void*)mtlVB);
            #endif
            
            return;
        }
    }

    u32 vsID = vs;
    u32 psID = ps;
    auto* pso = get_or_create_pipeline(
        vsID, psID, decl ? decl->dcl : 0, vb_stride, decl,
        blendEnabled, srcBlend, destBlend, blendOp,
        srcBlendAlpha, destBlendAlpha, blendOpAlpha,
        colorwrite_mask);
    if (!pso)
    {
        Msg("! CBackend::Render non-indexed: PSO null for vs=%u ps=%u decl=%u stride=%u blend=%u", vsID, psID, decl ? decl->dcl : 0, vb_stride, blendEnabled);
        return;
    }

    enc->setRenderPipelineState(pso);
    ApplyDS();
    enc->setVertexBuffer(mtlVB, 0, 30);
    constants.flush();

    MTL::PrimitiveType mtlPrim = TranslateTopology(T);
    u32 vertexCount = 0;
    switch (T)
    {
    case D3DPT_TRIANGLELIST:  vertexCount = PC * 3; break;
    case D3DPT_TRIANGLESTRIP: vertexCount = PC + 2; break;
    case D3DPT_TRIANGLEFAN:   vertexCount = PC + 2; break;
    case D3DPT_LINELIST:      vertexCount = PC * 2; break;
    case D3DPT_LINESTRIP:     vertexCount = PC + 1; break;
    case D3DPT_POINTLIST:     vertexCount = PC; break;
    default: return;
    }

    enc->drawPrimitives(mtlPrim, startV, vertexCount);
}

IC void CBackend::set_Geometry(SGeometry* _geom)
{
    set_Format(&*_geom->dcl);
    set_Vertices(_geom->vb, _geom->vb_stride);
    set_Indices(_geom->ib);
}

IC void CBackend::set_Scissor(const Irect* R)
{
    auto* enc = static_cast<MTL::RenderCommandEncoder*>(HW.m_currentEncoder);
    if (!enc)
        return;
    if (R)
    {
        MTL::ScissorRect r;
        r.x = R->x1;
        r.y = Device.dwHeight - R->y2; // D3D Y-flip
        r.width = R->x2 - R->x1;
        r.height = R->y2 - R->y1;
        enc->setScissorRect(r);
    }
    else
    {
        MTL::ScissorRect r{ 0, 0, Device.dwWidth, Device.dwHeight };
        enc->setScissorRect(r);
    }
}

IC void CBackend::SetViewport(const D3D_VIEWPORT& viewport) const
{
    auto* enc = static_cast<MTL::RenderCommandEncoder*>(HW.m_currentEncoder);
    if (!enc)
        return;
    MTL::Viewport vp;
    vp.originX = viewport.TopLeftX;
    vp.originY = viewport.TopLeftY;
    vp.width = viewport.Width;
    vp.height = (float)viewport.Height;
    vp.znear = viewport.MinDepth;
    vp.zfar = viewport.MaxDepth;
    enc->setViewport(vp);
}

IC void CBackend::ApplyDS()
{
    auto* enc = static_cast<MTL::RenderCommandEncoder*>(HW.m_currentEncoder);
    if (!enc)
        return;

    MTL::DepthStencilDescriptor* dsDesc = MTL::DepthStencilDescriptor::alloc()->init();

    bool depthTest = (z_enable == 1); // explicit set_Z(TRUE) only; u32(-1) defaults to no depth
    bool depthWrite = depthTest && (z_write == 1); // explicit set_ZWritable(TRUE); u32(-1) defaults to no write
    dsDesc->setDepthCompareFunction(depthTest && z_func != u32(-1)
        ? static_cast<MTL::CompareFunction>(mtlStateUtils::ConvertCmpFunction(z_func))
        : MTL::CompareFunctionAlways);
    dsDesc->setDepthWriteEnabled(depthWrite);

    auto* stencilDesc = dsDesc->backFaceStencil();
    bool stlEnable = (stencil_enable == 1);
    stencilDesc->setStencilCompareFunction(stlEnable
        ? static_cast<MTL::CompareFunction>(mtlStateUtils::ConvertCmpFunction(stencil_func))
        : MTL::CompareFunctionAlways);
    stencilDesc->setStencilFailureOperation(
        static_cast<MTL::StencilOperation>(mtlStateUtils::ConvertStencilOp(stlEnable ? stencil_fail : D3DSTENCILOP_KEEP)));
    stencilDesc->setDepthFailureOperation(
        static_cast<MTL::StencilOperation>(mtlStateUtils::ConvertStencilOp(stlEnable ? stencil_zfail : D3DSTENCILOP_KEEP)));
    stencilDesc->setDepthStencilPassOperation(
        static_cast<MTL::StencilOperation>(mtlStateUtils::ConvertStencilOp(stlEnable ? stencil_pass : D3DSTENCILOP_KEEP)));
    stencilDesc->setReadMask(stlEnable ? stencil_mask : 0);
    stencilDesc->setWriteMask(stlEnable ? stencil_writemask : 0);

    auto* device = static_cast<MTL::Device*>(HW.m_device);
    auto* dsState = device->newDepthStencilState(dsDesc);
    if (dsState)
    {
        enc->setDepthStencilState(dsState);
        enc->setStencilReferenceValue(stencil_ref);
        dsState->release();
    }
    dsDesc->release();
}

IC void CBackend::set_Stencil(u32 _enable, u32 _func, u32 _ref, u32 _mask, u32 _writemask, u32 _fail, u32 _pass,
                              u32 _zfail)
{
    stencil_enable = _enable;
    stencil_func = _func;
    stencil_ref = _ref;
    stencil_mask = _mask;
    stencil_writemask = _writemask;
    stencil_fail = _fail;
    stencil_pass = _pass;
    stencil_zfail = _zfail;

    ApplyDS();
}

IC void CBackend::set_Z(u32 _enable)
{
    if (z_enable == _enable)
        return;
    z_enable = _enable;
    ApplyDS();
}

IC void CBackend::set_ZFunc(u32 _func)
{
    if (z_func == _func)
        return;
    z_func = _func;
    ApplyDS();
}

IC void CBackend::set_ZWritable(u32 _enable)
{
    if (z_write == _enable)
        return;
    z_write = _enable;
    ApplyDS();
}

IC void CBackend::set_AlphaRef(u32 _value)
{
    alpha_ref = _value;
}

IC void CBackend::set_ColorWriteEnable(u32 _mask)
{
    if (colorwrite_mask == _mask)
        return;
    colorwrite_mask = _mask;
}

ICF void CBackend::set_CullMode(u32 _mode)
{
    if (cull_mode == _mode)
        return;
    cull_mode = _mode;

    auto* enc = static_cast<MTL::RenderCommandEncoder*>(HW.m_currentEncoder);
    if (!enc)
        return;

    if (_mode == D3DCULL_NONE)
        enc->setCullMode(MTL::CullModeNone);
    else
        enc->setCullMode(static_cast<MTL::CullMode>(mtlStateUtils::ConvertCullMode(_mode)));
}

ICF void CBackend::set_BlendEnable(bool _enable)
{
    blendEnabled = _enable;
}

ICF void CBackend::set_SrcBlend(u32 v) { srcBlend = v; }
ICF void CBackend::set_DestBlend(u32 v) { destBlend = v; }
ICF void CBackend::set_BlendOp(u32 v) { blendOp = v; }
ICF void CBackend::set_SrcBlendAlpha(u32 v) { srcBlendAlpha = v; }
ICF void CBackend::set_DestBlendAlpha(u32 v) { destBlendAlpha = v; }
ICF void CBackend::set_BlendOpAlpha(u32 v) { blendOpAlpha = v; }

ICF void CBackend::set_FillMode(u32 _mode)
{
    if (fill_mode == _mode)
        return;
    fill_mode = _mode;

    auto* enc = static_cast<MTL::RenderCommandEncoder*>(HW.m_currentEncoder);
    if (!enc)
        return;

    enc->setTriangleFillMode(static_cast<MTL::TriangleFillMode>(mtlStateUtils::ConvertFillMode(_mode)));
}

ICF void CBackend::SetTextureFactor(u32 factor) const
{
    if (auto* enc = static_cast<MTL::RenderCommandEncoder*>(HW.m_currentEncoder))
    {
        float r = ((factor >> 16) & 0xFF) / 255.f;
        float g = ((factor >> 8) & 0xFF) / 255.f;
        float b = (factor & 0xFF) / 255.f;
        float a = ((factor >> 24) & 0xFF) / 255.f;
        enc->setBlendColor(r, g, b, a);
    }
}

ICF void CBackend::SetAmbient(u32) const {}

ICF void CBackend::set_VS(ref_vs& _vs)
{
    set_VS(_vs->sh, _vs->cName.c_str());
}

IC void CBackend::set_Constants(R_constant_table* C)
{
    if (ctable == C)
        return;
    ctable = C;
    constants.reset();
    xforms.unmap();
    hemi.unmap();
    tree.unmap();
    if (nullptr == C)
        return;
    PGO(Msg("PGO:c-table"));
    for (auto& Cs : C->table)
        if (Cs->handler)
            Cs->handler->setup(*this, &*Cs);
}

void CBackend::set_pass_targets(const ref_rt& _1, const ref_rt& _2, const ref_rt& _3, const ref_rt& zb)
{
    if (_1)
    {
        curr_rt_width  = _1->dwWidth;
        curr_rt_height = _1->dwHeight;
    }
    else
    {
        VERIFY(zb);
        curr_rt_width  = zb->dwWidth;
        curr_rt_height = zb->dwHeight;
    }
    const D3D_VIEWPORT viewport = { 0, 0, static_cast<float>(curr_rt_width), static_cast<float>(curr_rt_height), 0.f, 1.f };
    SetViewport(viewport);
}
IC u32 GetIndexCount(D3DPRIMITIVETYPE T, u32 iPrimitiveCount)
{
    return 0;
}

} // namespace xray::render::RENDER_NAMESPACE
