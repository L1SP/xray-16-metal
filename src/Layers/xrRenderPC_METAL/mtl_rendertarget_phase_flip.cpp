#include "stdafx.h"

extern float ps_gamma;
extern float ps_brightness;
extern float ps_contrast;

namespace xray::render::RENDER_NAMESPACE
{
void CRenderTarget::phase_flip()
{
    if (!HW.m_drawableTexHandle || !HW.m_currentCmdBuffer)
    {
        Msg("! phase_flip: no drawable or cmd buffer");
        return;
    }

    float gamma, brightness, contrast;
    gamma = ps_gamma; brightness = ps_brightness; contrast = ps_contrast;

    const u32 baseHandle = get_base_rt();
    auto* dstTex = lookup_mtl_texture(HW.m_drawableTexHandle);
    if (!baseHandle || !dstTex)
    {
        Msg("! phase_flip: baseHandle=%u dstTex=%p", baseHandle, (void*)dstTex);
        return;
    }

    if (!m_gamma_render_pso)
    {
        Msg("! phase_flip: no gamma PSO");
        return;
    }

    auto* oldCmdBuffer = static_cast<MTL::CommandBuffer*>(HW.m_currentCmdBuffer);
    if (oldCmdBuffer)
    {
        oldCmdBuffer->commit();
        oldCmdBuffer->release(); // balance BeginScene's retain
    }
    HW.m_currentCmdBuffer = nullptr;
    HW.m_currentEncoder = nullptr;
    HW.m_currentRPD = nullptr;

    auto* cmdQueue = static_cast<MTL::CommandQueue*>(HW.m_cmdQueue);
    auto* cmdBuffer = cmdQueue->commandBuffer();
    if (!cmdBuffer)
    {
        Msg("! phase_flip: new cmd buffer creation failed");
        return;
    }
    cmdBuffer->retain();
    HW.m_currentCmdBuffer = cmdBuffer;

    auto* srcTex = lookup_mtl_texture(baseHandle);
    auto* interTex = lookup_mtl_texture(rt_Generic_0->pRT);
    if (!srcTex || !interTex)
    {
        Msg("! phase_flip: tex lookup failed (base=%u gen0=%u)", baseHandle, rt_Generic_0->pRT);
        return;
    }

    // ── Render gamma pass into rt_Generic_0 ─────────────────────────────────

    MTL::RenderPassDescriptor* rpd = MTL::RenderPassDescriptor::alloc()->init();
    rpd->colorAttachments()->object(0)->setTexture(interTex);
    rpd->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionDontCare);
    rpd->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);

    auto* enc = cmdBuffer->renderCommandEncoder(rpd);
    if (!enc)
    {
        rpd->release();
        return;
    }

    enc->setRenderPipelineState(m_gamma_render_pso);
    enc->setFragmentTexture(srcTex, 0);
    enc->setFragmentSamplerState(get_default_sampler(), 0);

    // Uniforms: Stalker GammaParams struct (gamma, brightness, contrast)
    struct { float gamma, brightness, contrast; } gp = { gamma, brightness, contrast };
    enc->setFragmentBytes(&gp, sizeof(gp), 0);

    // Draw full-screen triangle — 3 vertices, vertex_id generates NDC.
    enc->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));

    enc->endEncoding();
    rpd->release();

    // ── Blit gamma-corrected rt_Generic_0 to the Metal drawable ─────────────

    {
        auto* blit = cmdBuffer->blitCommandEncoder();
        if (blit)
        {
            const auto w = std::min(interTex->width(), dstTex->width());
            const auto h = std::min(interTex->height(), dstTex->height());
            blit->copyFromTexture(interTex, 0, 0, MTL::Origin(0, 0, 0),
                MTL::Size(w, h, 1), dstTex, 0, 0, MTL::Origin(0, 0, 0));
            blit->endEncoding();
        }
    }
}
} // namespace xray::render::RENDER_NAMESPACE
