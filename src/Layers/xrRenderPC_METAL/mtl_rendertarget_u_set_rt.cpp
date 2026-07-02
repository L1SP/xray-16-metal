#include "stdafx.h"

namespace xray::render::RENDER_NAMESPACE
{
void CRenderTarget::u_setrt(CBackend& cmd_list, const ref_rt& _1, const ref_rt& _2, const ref_rt& _3, const ref_rt& _zb)
{
    dwWidth[cmd_list.context_id] = 0;
    dwHeight[cmd_list.context_id] = 0;

    MTL::Texture* mtlColor[3] = {};

    auto set_rt = [&](u32 index, const ref_rt& rt)
    {
        if (!rt) return;

        if (dwWidth[cmd_list.context_id] && dwHeight[cmd_list.context_id])
        {
            VERIFY(rt->dwWidth == dwWidth[cmd_list.context_id]);
            VERIFY(rt->dwHeight == dwHeight[cmd_list.context_id]);
        }
        else
        {
            dwWidth[cmd_list.context_id] = rt->dwWidth;
            dwHeight[cmd_list.context_id] = rt->dwHeight;
        }

        cmd_list.set_RT(rt->pRT, index);
        mtlColor[index] = lookup_mtl_texture(rt->pRT);
    };

    set_rt(0, _1);
    set_rt(1, _2);
    set_rt(2, _3);

    MTL::Texture* mtlDepth = nullptr;
    if (_zb)
    {
        if (dwWidth[cmd_list.context_id] && dwHeight[cmd_list.context_id])
        {
            VERIFY(_zb->dwWidth == dwWidth[cmd_list.context_id]);
            VERIFY(_zb->dwHeight == dwHeight[cmd_list.context_id]);
        }
        else
        {
            dwWidth[cmd_list.context_id] = _zb->dwWidth;
            dwHeight[cmd_list.context_id] = _zb->dwHeight;
        }

        cmd_list.set_ZB(_zb->pZRT);
        mtlDepth = lookup_mtl_texture(_zb->pZRT);
    }
    else
    {
        cmd_list.set_ZB(0);
    }

    VERIFY(dwWidth[cmd_list.context_id] != 0);
    VERIFY(dwHeight[cmd_list.context_id] != 0);

    MTL::RenderPassDescriptor* rpd = HW.CreateRPD(mtlColor[0], mtlColor[1], mtlColor[2], mtlDepth);
    if (rpd)
    {
        HW.CreateEncoder(rpd);
        rpd->release();
    }
}

void CRenderTarget::u_setrt(CBackend& cmd_list, const ref_rt& _1, const ref_rt& _2, const ref_rt& _zb)
{
    dwWidth[cmd_list.context_id] = 0;
    dwHeight[cmd_list.context_id] = 0;

    MTL::Texture* mtlColor[2] = {};

    auto set_rt = [&](u32 index, const ref_rt& rt)
    {
        if (!rt) return;

        if (dwWidth[cmd_list.context_id] && dwHeight[cmd_list.context_id])
        {
            VERIFY(rt->dwWidth == dwWidth[cmd_list.context_id]);
            VERIFY(rt->dwHeight == dwHeight[cmd_list.context_id]);
        }
        else
        {
            dwWidth[cmd_list.context_id] = rt->dwWidth;
            dwHeight[cmd_list.context_id] = rt->dwHeight;
        }

        cmd_list.set_RT(rt->pRT, index);
        mtlColor[index] = lookup_mtl_texture(rt->pRT);
    };

    set_rt(0, _1);
    set_rt(1, _2);

    MTL::Texture* mtlDepth = nullptr;
    if (_zb)
    {
        if (dwWidth[cmd_list.context_id] && dwHeight[cmd_list.context_id])
        {
            VERIFY(_zb->dwWidth == dwWidth[cmd_list.context_id]);
            VERIFY(_zb->dwHeight == dwHeight[cmd_list.context_id]);
        }
        else
        {
            dwWidth[cmd_list.context_id] = _zb->dwWidth;
            dwHeight[cmd_list.context_id] = _zb->dwHeight;
        }

        cmd_list.set_ZB(_zb->pZRT);
        mtlDepth = lookup_mtl_texture(_zb->pZRT);
    }
    else
    {
        cmd_list.set_ZB(0);
    }

    VERIFY(dwWidth[cmd_list.context_id] != 0);
    VERIFY(dwHeight[cmd_list.context_id] != 0);

    MTL::RenderPassDescriptor* rpd = HW.CreateRPD(mtlColor[0], mtlColor[1], nullptr, mtlDepth);
    if (rpd)
    {
        HW.CreateEncoder(rpd);
        rpd->release();
    }
}

void CRenderTarget::u_setrt(CBackend& cmd_list, u32 W, u32 H, u32 _1, u32 _2, u32 _3, u32 zb)
{
    VERIFY(W != 0);
    VERIFY(H != 0);

    dwWidth[cmd_list.context_id] = W;
    dwHeight[cmd_list.context_id] = H;

    cmd_list.set_RT(_1, 0);
    cmd_list.set_RT(_2, 1);
    cmd_list.set_RT(_3, 2);
    cmd_list.set_ZB(zb);

    MTL::Texture* mtlColor[3] = {};
    if (_1) mtlColor[0] = lookup_mtl_texture(_1);
    if (_2) mtlColor[1] = lookup_mtl_texture(_2);
    if (_3) mtlColor[2] = lookup_mtl_texture(_3);

    MTL::Texture* mtlDepth = nullptr;
    if (zb) mtlDepth = lookup_mtl_texture(zb);

    MTL::RenderPassDescriptor* rpd = HW.CreateRPD(mtlColor[0], mtlColor[1], mtlColor[2], mtlDepth);
    if (rpd)
    {
        HW.CreateEncoder(rpd);
        rpd->release();
    }
}
} // namespace xray::render::RENDER_NAMESPACE
