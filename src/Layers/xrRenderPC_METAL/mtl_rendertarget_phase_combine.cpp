#include "stdafx.h"
#include "xrEngine/IGame_Persistent.h"
#include "xrEngine/Environment.h"
#include "Layers/xrRender/dxEnvironmentRender.h"

#define STENCIL_CULL 0

namespace xray::render::RENDER_NAMESPACE
{
float hclip(float v, float dim) { return 2.f * v / dim - 1.f; }

void CRenderTarget::phase_combine()
{
    PIX_EVENT(phase_combine);
    auto& dsgraph = RImplementation.get_imm_context();
    bool _menu_pp = g_pGamePersistent ? g_pGamePersistent->OnRenderPPUI_query() : false;
    u32 Offset = 0;

    // Clear RTs, set up for the combine pass
    {
        RCache.ClearRT(rt_Generic_0_r, {});
        RCache.ClearRT(rt_Generic_1_r, {});
        u_setrt(RCache, rt_Generic_0_r, rt_Generic_1_r, nullptr, rt_MSAADepth);
    }
    RCache.set_CullMode(CULL_NONE);
    RCache.set_Stencil(FALSE);

    // draw skybox
    if (g_pGamePersistent)
    {
        g_pGamePersistent->Environment().RenderSky();
        g_pGamePersistent->Environment().RenderClouds();
    }

    RCache.set_Stencil(TRUE, D3DCMP_LESSEQUAL, 0x01, 0xff, 0x00);
    if (RImplementation.o.nvstencil)
    {
        u_stencil_optimize(RCache, CRenderTarget::SO_Combine);
        RCache.set_ColorWriteEnable();
    }

    // Combine pass 1 — tonemap HDR scene to rt_Generic_0_r / rt_Generic_1_r
    if (!_menu_pp)
    {
        PIX_EVENT(combine_1);

        const auto& envdesc = g_pGamePersistent->Environment().CurrentEnv;
        const float minamb = 0.001f;
        Fvector4 ambclr =
        {
            std::max(envdesc.ambient.x * 2.f, minamb),
            std::max(envdesc.ambient.y * 2.f, minamb),
            std::max(envdesc.ambient.z * 2.f, minamb),
            0
        };
        ambclr.mul(ps_r2_sun_lumscale_amb);

        Fvector4 envclr = envdesc.env_color;
        envclr.x *= 2 * ps_r2_sun_lumscale_hemi;
        envclr.y *= 2 * ps_r2_sun_lumscale_hemi;
        envclr.z *= 2 * ps_r2_sun_lumscale_hemi;

        Fvector4 fogclr = { envdesc.fog_color.x, envdesc.fog_color.y, envdesc.fog_color.z, 0 };
        Fvector4 sunclr = { 0, 0, 0, 0 };
        Fvector4 sundir = { 0, 0, 1, 0 };

        {
            light* fuckingsun = (light*)RImplementation.Lights.sun._get();
            if (fuckingsun)
            {
                Fvector L_dir, L_clr;
                float L_spec;
                L_clr.set(fuckingsun->color.r, fuckingsun->color.g, fuckingsun->color.b);
                L_spec = u_diffuse2s(L_clr);
                Device.mView.transform_dir(L_dir, fuckingsun->direction);
                L_dir.normalize();
                sunclr.set(L_clr.x, L_clr.y, L_clr.z, L_spec);
                sundir.set(L_dir.x, L_dir.y, L_dir.z, 0);
            }
        }

        float fSSAONoise = 2.0f;
        fSSAONoise *= tan(deg2rad(67.5f / 2.0f));
        fSSAONoise /= tan(deg2rad(Device.fFOV / 2.0f));

        float fSSAOKernelSize = 150.0f;
        fSSAOKernelSize *= tan(deg2rad(67.5f / 2.0f));
        fSSAOKernelSize /= tan(deg2rad(Device.fFOV / 2.0f));

        // Fill VB
        float scale_X = float(Device.dwWidth) / float(TEX_jitter);
        float scale_Y = float(Device.dwHeight) / float(TEX_jitter);

        FVF::TL* pv = (FVF::TL*)RImplementation.Vertex.Lock(4, g_combine->vb_stride, Offset);
        pv->set(-1, 1, 0, 1, 0, 0, scale_Y); pv++;
        pv->set(-1, -1, 0, 0, 0, 0, 0); pv++;
        pv->set(1, 1, 1, 1, 0, scale_X, scale_Y); pv++;
        pv->set(1, -1, 1, 0, 0, scale_X, 0); pv++;
        RImplementation.Vertex.Unlock(4, g_combine->vb_stride);

        // Draw
        RCache.set_Element(s_combine->E[0]);
        RCache.set_Geometry(g_combine);
        RCache.set_c("m_v2w", Device.mInvView);
        RCache.set_c("L_ambient", ambclr);
        RCache.set_c("Ldynamic_color", sunclr);
        RCache.set_c("Ldynamic_dir", sundir);
        RCache.set_c("env_color", envclr);
        RCache.set_c("fog_color", fogclr);
        RCache.set_c("ssao_noise_tile_factor", fSSAONoise);
        RCache.set_c("ssao_kernel_size", fSSAOKernelSize);
        RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);
    }

    // Forward rendering
    {
        PIX_EVENT(Forward_rendering);
        u_setrt(RCache, rt_Generic_0_r, nullptr, nullptr, rt_MSAADepth);
        RCache.set_CullMode(CULL_CCW);
        RCache.set_Stencil(FALSE);
        RCache.set_ColorWriteEnable();
        RImplementation.render_forward();
        if (g_pGamePersistent)
            g_pGamePersistent->OnRenderPPUI_main();
    }

    // Volumetric
    if (m_bHasActiveVolumetric)
        phase_combine_volumetric();

    RCache.set_Stencil(FALSE);

    // Bloom
    if (RImplementation.o.msaa)
    {
        rt_Generic_0_r->resolve_into(*rt_Generic_0);
        rt_Generic_1_r->resolve_into(*rt_Generic_1);
    }
    phase_bloom();

    // Distortion
    BOOL bDistort = RImplementation.o.distortion_enabled;
    {
        if ((0 == dsgraph.mapDistort.size()) && !_menu_pp)
            bDistort = FALSE;
        if (bDistort)
        {
            PIX_EVENT(render_distort_objects);
            u_setrt(RCache, rt_Generic_1_r, nullptr, nullptr, rt_MSAADepth);
            RCache.ClearRT(rt_Generic_1_r, color_rgba(127, 127, 0, 127));
            RCache.set_CullMode(CULL_CCW);
            RCache.set_Stencil(FALSE);
            RCache.set_ColorWriteEnable();
            dsgraph.render_distort();
        }
    }

    RCache.set_Stencil(FALSE);

    // Post-processing enabled?
    BOOL PP_Complex = u_need_PP();
    if (_menu_pp)
        PP_Complex = FALSE;
    PP_Complex = TRUE; // always on for now (SBG, color map)

    // Combine everything + perform AA
    if (PP_Complex)
        u_setrt(RCache, rt_Color, nullptr, nullptr, rt_Base_Depth);
    else
        u_setrt(RCache, Device.dwWidth, Device.dwHeight, get_base_rt(), 0, 0, get_base_zb());
    RCache.set_CullMode(CULL_NONE);
    RCache.set_Stencil(FALSE);

    {
        PIX_EVENT(combine_2);

        // Fill vertex buffer for AA pass
        float _w = float(Device.dwWidth);
        float _h = float(Device.dwHeight);
        float ddw = 1.f / _w;
        float ddh = 1.f / _h;

        Fvector2 p0, p1;
        p0.set(.5f / _w, .5f / _h);
        p1.set((_w + .5f) / _w, (_h + .5f) / _h);

        // Use the same v_aa structure as GL
        struct v_aa
        {
            Fvector4 p;
            Fvector2 uv0;
            Fvector2 uv1;
            Fvector2 uv2;
            Fvector2 uv3;
            Fvector2 uv4;
            Fvector4 uv5;
            Fvector4 uv6;
        };

        v_aa* pv = (v_aa*)RImplementation.Vertex.Lock(4, g_aa_AA->vb_stride, Offset);
        // LT
        pv->p.set(EPS, EPS, EPS, 1.f);
        pv->uv0.set(p0.x, p1.y);
        pv->uv1.set(p0.x - ddw, p1.y - ddh);
        pv->uv2.set(p0.x + ddw, p1.y + ddh);
        pv->uv3.set(p0.x + ddw, p1.y - ddh);
        pv->uv4.set(p0.x - ddw, p1.y + ddh);
        pv->uv5.set(p0.x - ddw, p1.y, p1.y, p0.x + ddw);
        pv->uv6.set(p0.x, p1.y - ddh, p1.y + ddh, p0.x);
        pv++;
        // LB
        pv->p.set(EPS, float(_h + EPS), EPS, 1.f);
        pv->uv0.set(p0.x, p0.y);
        pv->uv1.set(p0.x - ddw, p0.y - ddh);
        pv->uv2.set(p0.x + ddw, p0.y + ddh);
        pv->uv3.set(p0.x + ddw, p0.y - ddh);
        pv->uv4.set(p0.x - ddw, p0.y + ddh);
        pv->uv5.set(p0.x - ddw, p0.y, p0.y, p0.x + ddw);
        pv->uv6.set(p0.x, p0.y - ddh, p0.y + ddh, p0.x);
        pv++;
        // RT
        pv->p.set(float(_w + EPS), EPS, EPS, 1.f);
        pv->uv0.set(p1.x, p1.y);
        pv->uv1.set(p1.x - ddw, p1.y - ddh);
        pv->uv2.set(p1.x + ddw, p1.y + ddh);
        pv->uv3.set(p1.x + ddw, p1.y - ddh);
        pv->uv4.set(p1.x - ddw, p1.y + ddh);
        pv->uv5.set(p1.x - ddw, p1.y, p1.y, p1.x + ddw);
        pv->uv6.set(p1.x, p1.y - ddh, p1.y + ddh, p1.x);
        pv++;
        // RB
        pv->p.set(float(_w + EPS), float(_h + EPS), EPS, 1.f);
        pv->uv0.set(p1.x, p0.y);
        pv->uv1.set(p1.x - ddw, p0.y - ddh);
        pv->uv2.set(p1.x + ddw, p0.y + ddh);
        pv->uv3.set(p1.x + ddw, p0.y - ddh);
        pv->uv4.set(p1.x - ddw, p0.y + ddh);
        pv->uv5.set(p1.x - ddw, p0.y, p0.y, p1.x + ddw);
        pv->uv6.set(p1.x, p0.y - ddh, p0.y + ddh, p1.x);
        pv++;
        RImplementation.Vertex.Unlock(4, g_aa_AA->vb_stride);

        // m-blur matrices
        Fmatrix m_previous, m_current;
        Fvector2 m_blur_scale;
        {
            static Fmatrix m_saved_viewproj;
            m_previous.mul(m_saved_viewproj, Device.mInvView);
            m_current.set(Device.mProject);
            m_saved_viewproj.set(Device.mFullTransform);
            float scale = ps_r2_mblur / 2.f;
            m_blur_scale.set(scale, -scale).div(12.f);
        }

        Fvector2 vDofKernel;
        vDofKernel.set(0.5f / Device.dwWidth, 0.5f / Device.dwHeight);
        vDofKernel.mul(ps_r2_dof_kernel_size);

        // Draw COLOR
        if (!RImplementation.o.msaa)
        {
            if (ps_r2_ls_flags.test(R2FLAG_AA))
                RCache.set_Element(s_combine->E[bDistort ? 3 : 1]);
            else
                RCache.set_Element(s_combine->E[bDistort ? 4 : 2]);
        }
        else
        {
            if (ps_r2_ls_flags.test(R2FLAG_AA))
                RCache.set_Element(s_combine_msaa[0]->E[bDistort ? 3 : 1]);
            else
                RCache.set_Element(s_combine_msaa[0]->E[bDistort ? 4 : 2]);
        }

        RCache.set_c("e_barrier", ps_r2_aa_barier.x, ps_r2_aa_barier.y, ps_r2_aa_barier.z, 0.f);
        RCache.set_c("e_weights", ps_r2_aa_weight.x, ps_r2_aa_weight.y, ps_r2_aa_weight.z, 0.f);
        RCache.set_c("e_kernel", ps_r2_aa_kernel, ps_r2_aa_kernel, ps_r2_aa_kernel, 0.f);
        RCache.set_c("m_current", m_current);
        RCache.set_c("m_previous", m_previous);
        RCache.set_c("m_blur", m_blur_scale.x, m_blur_scale.y, 0.f, 0.f);
        Fvector3 dof;
        g_pGamePersistent->GetCurrentDof(dof);
        RCache.set_c("dof_params", dof.x, dof.y, dof.z, ps_r2_dof_sky);
        RCache.set_c("dof_kernel", vDofKernel.x, vDofKernel.y, ps_r2_dof_kernel_size, 0.f);

        RCache.set_Geometry(g_aa_AA);
        RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);
    }

    RCache.set_Stencil(FALSE);

    // Lens flares
    if (g_pGamePersistent)
        g_pGamePersistent->Environment().RenderFlares();

    // Post-processing (applies SBG — saturation, brightness, gamma)
    if (PP_Complex)
    {
        PIX_EVENT(phase_pp);
        phase_pp();
    }
}

void CRenderTarget::phase_combine_callback()
{
    // TODO: Implement Metal combine phase callback
}

void CRenderTarget::phase_combine_volumetric()
{
    PIX_EVENT(phase_combine_volumetric);
    u32 Offset = 0;

    u_setrt(RCache, rt_Generic_0_r, rt_Generic_1_r, nullptr, rt_MSAADepth);

    RCache.set_ColorWriteEnable(D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
    {
        float scale_X = float(Device.dwWidth) / float(TEX_jitter);
        float scale_Y = float(Device.dwHeight) / float(TEX_jitter);

        FVF::TL* pv = (FVF::TL*)RImplementation.Vertex.Lock(4, g_combine->vb_stride, Offset);
        pv->set(-1, 1, 0, 1, 0, 0, scale_Y);
        pv++;
        pv->set(-1, -1, 0, 0, 0, 0, 0);
        pv++;
        pv->set(1, 1, 1, 1, 0, scale_X, scale_Y);
        pv++;
        pv->set(1, -1, 1, 0, 0, scale_X, 0);
        pv++;
        RImplementation.Vertex.Unlock(4, g_combine->vb_stride);

        RCache.set_Element(s_combine_volumetric->E[0]);
        RCache.set_Geometry(g_combine);
        RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);
    }
    RCache.set_ColorWriteEnable();
}

void CRenderTarget::phase_wallmarks()
{
    // Targets
    RCache.set_RT(0, 2);
    RCache.set_RT(0, 1);
    u_setrt(RCache, rt_Color, nullptr, nullptr, rt_MSAADepth);
    // Stencil - draw only where stencil >= 0x1
    RCache.set_Stencil(TRUE, D3DCMP_LESSEQUAL, 0x01, 0xff, 0x00);
    RCache.set_CullMode(CULL_CCW);
    RCache.set_ColorWriteEnable(D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
}
} // namespace xray::render::RENDER_NAMESPACE
