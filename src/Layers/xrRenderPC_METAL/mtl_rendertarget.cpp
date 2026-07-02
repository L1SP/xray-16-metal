#include "stdafx.h"

namespace xray::render::RENDER_NAMESPACE
{
void CRenderTarget::_create_gamma_pso()
{
    Msg("* _create_gamma_pso: enter");
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    auto* device = static_cast<MTL::Device*>(HW.m_device);
    if (!device)
    {
        Msg("! _create_gamma_pso: device is null");
        pool->release();
        return;
    }

    // Release old PSO if any (e.g. from a prior device state before ResetDevice).
    if (m_gamma_render_pso)
    {
        m_gamma_render_pso->release();
        m_gamma_render_pso = nullptr;
    }

    // ── Vertex shader: full-screen triangle via vertex_id ────────────────────────
    // Guarantees correct NDC [-1, 1] without relying on engine uniforms or F_TL.
    // SPIRV-Cross FLIP_VERTEX_Y=true automatically negates position.y so the
    // triangle covers the Metal NDC space.  UVs pass through unchanged.
    const char* vsSrc = "using namespace metal;\n"
        "struct VertexOut { float4 position [[position]]; float2 uv; };\n"
        "vertex VertexOut vertex_gamma(uint vid [[vertex_id]]) {\n"
        "    VertexOut out;\n"
        "    out.uv = float2((vid << 1) & 2, vid & 2);\n"
        "    out.position = float4(out.uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);\n"
        "    return out;\n"
        "}\n";

    // ── Fragment shader: safe Stalker [0.5 … 1.5] formula with saturate ──────
    // FLIP_VERTEX_Y flips the rasterised position, which inverts the
    // interpolated UV (uv.y=0 → screen bottom, uv.y=1 → screen top).
    // We flip uv.y back here so the final image reads the source texture
    // right-side up and displays upright on screen.
    const char* fsSrc = "using namespace metal;\n"
        "struct VertexOut { float4 position [[position]]; float2 uv; };\n"
        "struct GammaParams { float gamma; float brightness; float contrast; };\n"
        "fragment float4 fragment_gamma(VertexOut in [[stage_in]],\n"
        "    texture2d<float, access::sample> srcTex [[texture(0)]],\n"
        "    constant GammaParams& p [[buffer(0)]])\n"
        "{\n"
        "    constexpr sampler s(address::clamp_to_edge, filter::linear);\n"
        "    float2 flipped_uv = in.uv;\n"
        "    flipped_uv.y = 1.0 - flipped_uv.y;\n"
        "    float4 originalColor = srcTex.sample(s, flipped_uv);\n"
        "    float3 color = originalColor.rgb;\n"
        "    color = color * p.contrast;\n"
        "    color = color + (p.brightness - 1.0);\n"
        "    color = pow(max(color, 0.0), float3(1.0 / max(p.gamma, 0.01)));\n"
        "    return float4(saturate(color), originalColor.a);\n"
        "}\n";

    NS::Error* error = nullptr;

    // Compile vertex library
    auto* vsLib = device->newLibrary(NS::String::string(vsSrc, NS::UTF8StringEncoding), nullptr, &error);
    if (!vsLib)
    {
        if (error)
        {
            Msg("! _create_gamma_pso: VS compile error: %s", error->localizedDescription()->utf8String());
            error->release();
        }
        pool->release();
        return;
    }

    auto* vsFn = vsLib->newFunction(NS::String::string("vertex_gamma", NS::UTF8StringEncoding));
    vsLib->release();
    if (!vsFn)
    {
        if (error)
            error->release();
        pool->release();
        return;
    }

    // Compile fragment library
    error = nullptr;
    auto* fsLib = device->newLibrary(NS::String::string(fsSrc, NS::UTF8StringEncoding), nullptr, &error);
    if (!fsLib)
    {
        if (error)
        {
            Msg("! _create_gamma_pso: FS compile error: %s", error->localizedDescription()->utf8String());
            error->release();
        }
        vsFn->release();
        pool->release();
        return;
    }

    auto* fsFn = fsLib->newFunction(NS::String::string("fragment_gamma", NS::UTF8StringEncoding));
    fsLib->release();
    if (!fsFn)
    {
        vsFn->release();
        if (error)
            error->release();
        pool->release();
        return;
    }

    // Build render pipeline descriptor
    auto* desc = MTL::RenderPipelineDescriptor::alloc()->init();
    desc->setVertexFunction(vsFn);
    desc->setFragmentFunction(fsFn);
    desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

    error = nullptr;
    m_gamma_render_pso = device->newRenderPipelineState(desc, &error);
    if (!m_gamma_render_pso)
    {
        if (error)
        {
            Msg("! _create_gamma_pso: PSO creation failed: %s", error->localizedDescription()->utf8String());
            error->release();
        }
    }
    else
    {
        Msg("* _create_gamma_pso: PSO created successfully");
    }

    desc->release();
    fsFn->release();
    vsFn->release();
    pool->release();
}

} // namespace xray::render::RENDER_NAMESPACE
