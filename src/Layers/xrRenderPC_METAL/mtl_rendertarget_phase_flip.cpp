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

    // Source = get_base_rt() (rt_Base) — where the menu/scene was rendered
    // (with FLIP_VERTEX_Y=true, so content is upside-down).
    // Destination = drawable texture (to be presented on screen).
    //
    const u32 baseHandle = get_base_rt();
    auto* srcTex = lookup_mtl_texture(baseHandle);
    auto* dstTex = lookup_mtl_texture(HW.m_drawableTexHandle);
    if (!srcTex || !dstTex)
    {
        Msg("! phase_flip: tex lookup failed (base=%u drawable=%u)", baseHandle, HW.m_drawableTexHandle);
        return;
    }

    if (!m_gamma_render_pso)
    {
        Msg("! phase_flip: no gamma PSO, falling back to blit");
        // Fallback: raw blit (no gamma, no flip — image will be upside down)
        auto* cmdBuffer = static_cast<MTL::CommandBuffer*>(HW.m_currentCmdBuffer);
        if (!cmdBuffer) return;
        auto* blit = cmdBuffer->blitCommandEncoder();
        if (blit)
        {
            const auto w = std::min(srcTex->width(), dstTex->width());
            const auto h = std::min(srcTex->height(), dstTex->height());
            blit->copyFromTexture(srcTex, 0, 0, MTL::Origin(0, 0, 0),
                MTL::Size(w, h, 1), dstTex, 0, 0, MTL::Origin(0, 0, 0));
            blit->endEncoding();
        }
        return;
    }

    float gamma, brightness, contrast;
    gamma = ps_gamma; brightness = ps_brightness; contrast = ps_contrast;

    // Use the gamma PSO on the SAME command buffer (no commit needed).
    // The render encoder to rt_Base was already ended by Present's EndEncoding(),
    // so tile memory is flushed and we can safely sample rt_Base.
    auto* cmdBuffer = static_cast<MTL::CommandBuffer*>(HW.m_currentCmdBuffer);
    if (!cmdBuffer)
        return;

    MTL::RenderPassDescriptor* rpd = MTL::RenderPassDescriptor::alloc()->init();
    rpd->colorAttachments()->object(0)->setTexture(dstTex);
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

    struct { float gamma, brightness, contrast; } gp = { gamma, brightness, contrast };
    enc->setFragmentBytes(&gp, sizeof(gp), 0);

    // Draw full-screen triangle — 3 vertices, vertex_id generates NDC.
    enc->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));

    enc->endEncoding();
    rpd->release();
}
} // namespace xray::render::RENDER_NAMESPACE
