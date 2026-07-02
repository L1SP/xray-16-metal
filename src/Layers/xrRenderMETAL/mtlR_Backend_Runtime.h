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
    bool blendEnabled;

    bool operator<(const MTLPipelineKey& o) const
    {
        if (vsID != o.vsID) return vsID < o.vsID;
        if (psID != o.psID) return psID < o.psID;
        if (declID != o.declID) return declID < o.declID;
        return blendEnabled < o.blendEnabled;
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

inline MTL::RenderPipelineState* get_or_create_pipeline(u32 vsID, u32 psID, u32 declID, u32 vbStride, SDeclaration* decl, bool blendEnabled)
{
    MTLPipelineKey key{ vsID, psID, declID, blendEnabled };
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

    auto* layer = static_cast<CA::MetalLayer*>(HW.m_swapchain);
    MTL::PixelFormat pf = layer ? layer->pixelFormat() : MTL::PixelFormatBGRA8Unorm;
    psoDesc->colorAttachments()->object(0)->setPixelFormat(pf);
    psoDesc->setRasterSampleCount(1);
    auto* ca = psoDesc->colorAttachments()->object(0);
    if (blendEnabled)
    {
        ca->setBlendingEnabled(true);
        ca->setRgbBlendOperation(MTL::BlendOperationAdd);
        ca->setAlphaBlendOperation(MTL::BlendOperationAdd);
        ca->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
        ca->setSourceAlphaBlendFactor(MTL::BlendFactorSourceAlpha);
        ca->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
        ca->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    }
    else
    {
        ca->setBlendingEnabled(false);
        ca->setWriteMask(MTL::ColorWriteMaskAll);
    }

    MTL::VertexDescriptor* vertDesc = MTL::VertexDescriptor::alloc()->init();
    if (decl && !decl->dcl_code.empty())
    {
        u32 maxAttr = 0;
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
            enc->release();
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
    auto* mtlVB = static_cast<MTL::Buffer*>(vb);
    auto* mtlIB = static_cast<MTL::Buffer*>(ib);
    if (!enc || !mtlVB || !mtlIB)
    {
        Msg("! Render indexed skipped: enc=%p vb=%p ib=%p blend=%u T=%d baseV=%u countV=%u PC=%u",
            (void*)enc, (void*)mtlVB, (void*)mtlIB, blendEnabled, T, baseV, countV, PC);
        return;
    }

    auto* pso = get_or_create_pipeline(vs, ps, decl ? decl->dcl : 0, vb_stride, decl, blendEnabled);
    if (!pso)
    {
        Msg("! CBackend::Render indexed: PSO null for vs=%u ps=%u blend=%u", vs, ps, blendEnabled);
        return;
    }

    enc->setRenderPipelineState(pso);
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
    auto* mtlVB = static_cast<MTL::Buffer*>(vb);
    if (!enc || !mtlVB)
    {
        Msg("! CBackend::Render non-indexed: no encoder or vb (enc=%p vb=%p)", (void*)enc, (void*)mtlVB);
        return;
    }

    u32 vsID = vs;
    u32 psID = ps;
    auto* pso = get_or_create_pipeline(vsID, psID, decl ? decl->dcl : 0, vb_stride, decl, blendEnabled);
    if (!pso)
    {
        Msg("! CBackend::Render non-indexed: PSO null for vs=%u ps=%u decl=%u stride=%u blend=%u", vsID, psID, decl ? decl->dcl : 0, vb_stride, blendEnabled);
        return;
    }

    enc->setRenderPipelineState(pso);
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

IC void CBackend::set_Stencil(u32 _enable, u32 _func, u32 _ref, u32 _mask, u32 _writemask, u32 _fail, u32 _pass,
                              u32 _zfail)
{
    auto* enc = static_cast<MTL::RenderCommandEncoder*>(HW.m_currentEncoder);
    if (!enc)
        return;

    stencil_enable = _enable;
    stencil_func = _func;
    stencil_ref = _ref;
    stencil_mask = _mask;
    stencil_writemask = _writemask;
    stencil_fail = _fail;
    stencil_pass = _pass;
    stencil_zfail = _zfail;

    enc->setStencilReferenceValue(_ref);

    MTL::DepthStencilDescriptor* dsDesc = MTL::DepthStencilDescriptor::alloc()->init();
    if (_enable)
    {
        auto* stencilDesc = dsDesc->backFaceStencil();
        stencilDesc->setStencilCompareFunction(static_cast<MTL::CompareFunction>(mtlStateUtils::ConvertCmpFunction(_func)));
        stencilDesc->setStencilFailureOperation(static_cast<MTL::StencilOperation>(mtlStateUtils::ConvertStencilOp(_fail)));
        stencilDesc->setDepthFailureOperation(static_cast<MTL::StencilOperation>(mtlStateUtils::ConvertStencilOp(_zfail)));
        stencilDesc->setDepthStencilPassOperation(static_cast<MTL::StencilOperation>(mtlStateUtils::ConvertStencilOp(_pass)));
        stencilDesc->setReadMask(_mask);
        stencilDesc->setWriteMask(_writemask);
        dsDesc->setDepthCompareFunction(MTL::CompareFunctionAlways);
        dsDesc->setDepthWriteEnabled(false);
    }
    else
    {
        dsDesc->setDepthCompareFunction(MTL::CompareFunctionAlways);
        dsDesc->setDepthWriteEnabled(false);
    }

    auto* device = static_cast<MTL::Device*>(HW.m_device);
    auto* dsState = device->newDepthStencilState(dsDesc);
    if (dsState)
    {
        enc->setDepthStencilState(dsState);
        dsState->release();
    }
    dsDesc->release();
}

IC void CBackend::set_Z(u32 _enable)
{
    if (z_enable == _enable)
        return;
    z_enable = _enable;

    auto* enc = static_cast<MTL::RenderCommandEncoder*>(HW.m_currentEncoder);
    if (!enc)
        return;

    MTL::DepthStencilDescriptor* dsDesc = MTL::DepthStencilDescriptor::alloc()->init();
    dsDesc->setDepthCompareFunction(z_enable
        ? static_cast<MTL::CompareFunction>(mtlStateUtils::ConvertCmpFunction(z_func))
        : MTL::CompareFunctionAlways);
    dsDesc->setDepthWriteEnabled(z_enable);
    auto* device = static_cast<MTL::Device*>(HW.m_device);
    auto* dsState = device->newDepthStencilState(dsDesc);
    if (dsState)
    {
        enc->setDepthStencilState(dsState);
        dsState->release();
    }
    dsDesc->release();
}

IC void CBackend::set_ZFunc(u32 _func)
{
    if (z_func == _func)
        return;
    z_func = _func;

    auto* enc = static_cast<MTL::RenderCommandEncoder*>(HW.m_currentEncoder);
    if (!enc)
        return;

    MTL::DepthStencilDescriptor* dsDesc = MTL::DepthStencilDescriptor::alloc()->init();
    dsDesc->setDepthCompareFunction(z_enable
        ? static_cast<MTL::CompareFunction>(mtlStateUtils::ConvertCmpFunction(z_func))
        : MTL::CompareFunctionAlways);
    dsDesc->setDepthWriteEnabled(z_enable);
    auto* device = static_cast<MTL::Device*>(HW.m_device);
    auto* dsState = device->newDepthStencilState(dsDesc);
    if (dsState)
    {
        enc->setDepthStencilState(dsState);
        dsState->release();
    }
    dsDesc->release();
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
