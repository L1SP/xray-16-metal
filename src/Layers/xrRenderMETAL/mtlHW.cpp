#include "stdafx.h"

#include <SDL.h>
#include <SDL_metal.h>
#include <backends/imgui_impl_metal.h>

namespace xray::render::RENDER_NAMESPACE
{
CHW HW;

CHW::CHW()
{
    if (!ThisInstanceIsGlobal())
        return;

    Device.seqAppActivate.Add(this);
    Device.seqAppDeactivate.Add(this);
}

CHW::~CHW()
{
    if (!ThisInstanceIsGlobal())
        return;

    Device.seqAppActivate.Remove(this);
    Device.seqAppDeactivate.Remove(this);
}

void CHW::OnAppActivate()
{
    if (m_window)
        SDL_RestoreWindow(m_window);
}

void CHW::OnAppDeactivate()
{
    if (m_window)
    {
        if (psDeviceMode.WindowStyle == rsFullscreen || psDeviceMode.WindowStyle == rsFullscreenBorderless)
            SDL_MinimizeWindow(m_window);
    }
}

void CHW::CreateDevice(SDL_Window* wnd)
{
    ZoneScoped;
    Msg("* Metal CreateDevice: begin");

    m_window = wnd;
    R_ASSERT(m_window);

    MTL::Device* device = MTL::CreateSystemDefaultDevice();
    if (!device)
    {
        Log("! Metal: MTLCreateSystemDefaultDevice() failed");
        return;
    }

    MTL::CommandQueue* cmdQueue = device->newCommandQueue();
    if (!cmdQueue)
    {
        Log("! Metal: failed to create command queue");
        device->release();
        return;
    }

    Caps.fTarget = D3DFMT_A8R8G8B8;
    Caps.fDepth = D3DFMT_D24S8;
    BackBufferCount = 2; // double buffering
    Caps.Update();

    // Set surface size from window (Metal view not created yet)
    int width, height;
    SDL_GetWindowSize(m_window, &width, &height);
    surf_width = width;
    surf_height = height;

    // Create default sampler state
    {
        MTL::SamplerDescriptor* desc = MTL::SamplerDescriptor::alloc()->init();
        desc->setMinFilter(MTL::SamplerMinMagFilterLinear);
        desc->setMagFilter(MTL::SamplerMinMagFilterLinear);
        desc->setMipFilter(MTL::SamplerMipFilterLinear);
        desc->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
        desc->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
        desc->setRAddressMode(MTL::SamplerAddressModeClampToEdge);
        m_defaultSampler = device->newSamplerState(desc);
        desc->release();
        R_ASSERT2(m_defaultSampler, "Failed to create default Metal sampler state");
    }

    m_device = device;
    m_cmdQueue = cmdQueue;
    m_swapchain = nullptr; // created lazily after window is shown
    m_metalView = nullptr;

    AdapterName = reinterpret_cast<pcstr>(device->name()->utf8String());
    Msg("* GPU Metal device: [%s]", AdapterName);
}

void CHW::CreateMetalView()
{
    if (m_metalView)
        return;
    if (!m_window)
        return;

    SDL_MetalView metalView = SDL_Metal_CreateView(m_window);
    if (!metalView)
    {
        Log("! Metal: SDL_Metal_CreateView failed: %s", SDL_GetError());
        return;
    }

    CA::MetalLayer* layer = static_cast<CA::MetalLayer*>(SDL_Metal_GetLayer(metalView));
    R_ASSERT2(layer, "Failed to get CAMetalLayer from SDL Metal view");

    auto* device = static_cast<MTL::Device*>(m_device);
    layer->setDevice(device);
    layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    layer->setFramebufferOnly(false); // allow compute shader read/write for gamma correction

    int width, height;
    SDL_GetWindowSize(m_window, &width, &height);
    layer->setDrawableSize(CGSizeMake(static_cast<CGFloat>(width), static_cast<CGFloat>(height)));

    m_swapchain = layer;
    m_metalView = metalView;
    surf_width = width;
    surf_height = height;

    // Create persistent RPD for ImGui (needs a texture so ImGui can read sampleCount/pixelFormat)
    if (!m_persistentRPD)
    {
        MTL::RenderPassDescriptor* persistentRPD = MTL::RenderPassDescriptor::renderPassDescriptor();
        auto* caLayer = static_cast<CA::MetalLayer*>(layer);
        MTL::PixelFormat pf = caLayer ? caLayer->pixelFormat() : MTL::PixelFormatBGRA8Unorm;
        if (caLayer)
        {
            // Create a 1x1 texture so ImGui can query pixelFormat and sampleCount
            auto* device = static_cast<MTL::Device*>(m_device);
            MTL::TextureDescriptor* tdesc = MTL::TextureDescriptor::alloc()->init();
            tdesc->setTextureType(MTL::TextureType2D);
            tdesc->setPixelFormat(pf);
            tdesc->setWidth(1);
            tdesc->setHeight(1);
            tdesc->setStorageMode(MTL::StorageModeShared);
            tdesc->setUsage(MTL::TextureUsageShaderRead);
            MTL::Texture* dummyTex = device->newTexture(tdesc);
            tdesc->release();
            if (dummyTex)
            {
                auto* ca = persistentRPD->colorAttachments()->object(0);
                ca->setTexture(dummyTex);
            }
        }
        persistentRPD->retain();
        m_persistentRPD = persistentRPD;
    }

    Msg("* GPU Metal view created: [%dx%d]", surf_width, surf_height);
}

void CHW::DestroyDevice()
{
    if (m_persistentRPD)
    {
        static_cast<MTL::RenderPassDescriptor*>(m_persistentRPD)->release();
        m_persistentRPD = nullptr;
    }

    if (m_currentDrawable)
    {
        static_cast<CA::MetalDrawable*>(m_currentDrawable)->release();
        m_currentDrawable = nullptr;
    }

    if (m_cmdQueue)
    {
        static_cast<MTL::CommandQueue*>(m_cmdQueue)->release();
        m_cmdQueue = nullptr;
    }

    if (m_defaultSampler)
    {
        static_cast<MTL::SamplerState*>(m_defaultSampler)->release();
        m_defaultSampler = nullptr;
    }

    if (m_metalView)
    {
        SDL_Metal_DestroyView(static_cast<SDL_MetalView>(m_metalView));
        m_metalView = nullptr;
    }

    if (m_device)
    {
        static_cast<MTL::Device*>(m_device)->release();
        m_device = nullptr;
    }

    m_swapchain = nullptr;
}

void CHW::Reset()
{
    ZoneScoped;

    if (!m_window || !m_swapchain)
        return;

    auto* layer = static_cast<CA::MetalLayer*>(m_swapchain);

    if (m_currentCmdBuffer)
    {
        static_cast<MTL::CommandBuffer*>(m_currentCmdBuffer)->release();
        m_currentCmdBuffer = nullptr;
    }
    if (m_currentDrawable)
    {
        static_cast<CA::MetalDrawable*>(m_currentDrawable)->release();
        m_currentDrawable = nullptr;
    }

    int width, height;
    SDL_GetWindowSize(m_window, &width, &height);
    layer->setDrawableSize(CGSizeMake(static_cast<CGFloat>(width), static_cast<CGFloat>(height)));

    surf_width = width;
    surf_height = height;
}

void CHW::SetPrimaryAttributes(u32& windowFlags)
{
    windowFlags |= SDL_WINDOW_METAL;
}

void CHW::BeginScene()
{
    auto* pool = static_cast<NS::AutoreleasePool*>(m_autoreleasePool);
    if (pool) pool->drain();
    m_autoreleasePool = NS::AutoreleasePool::alloc()->init();

    CreateMetalView(); // lazy view creation (must happen after window is shown)

    auto* layer = static_cast<CA::MetalLayer*>(m_swapchain);
    auto* cmdQueue = static_cast<MTL::CommandQueue*>(m_cmdQueue);
    if (!layer || !cmdQueue)
    {
        Msg("! BeginScene: no layer(%p) or cmdQueue(%p)", (void*)layer, (void*)cmdQueue);
        return;
    }

    auto* drawable = layer->nextDrawable();
    if (!drawable)
    {
        Msg("! BeginScene: nextDrawable returned null (layer=%p)", (void*)layer);
        return;
    }
    drawable->retain();
    if (m_currentDrawable)
    {
        static_cast<CA::MetalDrawable*>(m_currentDrawable)->release();
        m_currentDrawable = nullptr;
    }
    m_currentDrawable = drawable;

    auto* newCmdBuf = cmdQueue->commandBuffer();
    if (!newCmdBuf)
    {
        static_cast<CA::MetalDrawable*>(m_currentDrawable)->release();
        m_currentDrawable = nullptr;
        return;
    }
    newCmdBuf->retain();
    if (m_currentCmdBuffer)
    {
        static_cast<MTL::CommandBuffer*>(m_currentCmdBuffer)->release();
        m_currentCmdBuffer = nullptr;
    }
    m_currentCmdBuffer = newCmdBuf;

    // Clean up any prior encoding state (e.g. from u_setrt during CRenderTarget construction)
    EndEncoding();

    // Create initial encoder for drawable (used by menu UI rendering which doesn't call u_setrt)
    MTL::RenderPassDescriptor* rpd = MTL::RenderPassDescriptor::alloc()->init();
    auto* ca = rpd->colorAttachments()->object(0);
    auto* drawableTex = static_cast<CA::MetalDrawable*>(m_currentDrawable)->texture();
    if (!drawableTex)
    {
        rpd->release();
        static_cast<MTL::CommandBuffer*>(m_currentCmdBuffer)->release();
        m_currentCmdBuffer = nullptr;
        static_cast<CA::MetalDrawable*>(m_currentDrawable)->release();
        m_currentDrawable = nullptr;
        return;
    }
    ca->setTexture(drawableTex);
    ca->setLoadAction(MTL::LoadActionClear);
    ca->setClearColor(MTL::ClearColor::Make(0.0, 0.0, 0.0, 1.0));
    ca->setStoreAction(MTL::StoreActionStore);

    // Register drawable texture so get_base_rt() can return it
    m_drawableTexHandle = register_mtl_texture(drawableTex);

    // Metal recreates the command encoder each frame, so force rebinding of all
    // GPU state (textures, shaders, blend state, etc.) by invalidating the cache.
    RCache.Invalidate();

    auto* cmdBuf2 = static_cast<MTL::CommandBuffer*>(m_currentCmdBuffer);
    MTL::RenderCommandEncoder* enc = cmdBuf2->renderCommandEncoder(rpd);
    if (!enc)
    {
        rpd->release();
        if (m_drawableTexHandle)
        {
            unregister_mtl_texture(m_drawableTexHandle);
            m_drawableTexHandle = 0;
        }
        static_cast<MTL::CommandBuffer*>(m_currentCmdBuffer)->release();
        m_currentCmdBuffer = nullptr;
        static_cast<CA::MetalDrawable*>(m_currentDrawable)->release();
        m_currentDrawable = nullptr;
        return;
    }
    enc->retain();
    m_currentEncoder = enc;
    m_currentRPD = rpd;
}

void CHW::EndEncoding()
{
    if (m_currentEncoder)
    {
        static_cast<MTL::RenderCommandEncoder*>(m_currentEncoder)->endEncoding();
        static_cast<MTL::RenderCommandEncoder*>(m_currentEncoder)->release();
        m_currentEncoder = nullptr;
    }
    if (m_currentRPD)
    {
        static_cast<MTL::RenderPassDescriptor*>(m_currentRPD)->release();
        m_currentRPD = nullptr;
    }
}

MTL::RenderPassDescriptor* CHW::CreateRPD(MTL::Texture* color0, MTL::Texture* color1,
    MTL::Texture* color2, MTL::Texture* depth)
{
    MTL::RenderPassDescriptor* rpd = MTL::RenderPassDescriptor::alloc()->init();

    u32 colorCount = 0;
    auto set_color = [&](u32 index, MTL::Texture* tex)
    {
        if (!tex) return;
        auto* ca = rpd->colorAttachments()->object(index);
        ca->setTexture(tex);
        ca->setLoadAction(MTL::LoadActionLoad);
        ca->setStoreAction(MTL::StoreActionStore);
        ++colorCount;
    };

    set_color(0, color0);
    set_color(1, color1);
    set_color(2, color2);

    if (depth)
    {
        auto* da = rpd->depthAttachment();
        da->setTexture(depth);
        da->setLoadAction(MTL::LoadActionLoad);
        da->setStoreAction(MTL::StoreActionStore);
    }

    if (colorCount == 0 && !depth)
    {
        rpd->release();
        return nullptr;
    }

    return rpd;
}

void CHW::CreateEncoder(MTL::RenderPassDescriptor* rpd)
{
    EndEncoding();

    rpd->retain();
    m_currentRPD = rpd;

    auto* cmdBuffer = static_cast<MTL::CommandBuffer*>(m_currentCmdBuffer);
    if (!cmdBuffer)
        return;

    MTL::RenderCommandEncoder* enc = cmdBuffer->renderCommandEncoder(rpd);
    if (!enc)
    {
        rpd->release();
        m_currentRPD = nullptr;
        return;
    }
    enc->retain();
    m_currentEncoder = enc;

    // Set initial viewport from the first color attachment size.
    auto* ca = rpd->colorAttachments()->object(0);
    if (ca && ca->texture())
    {
        MTL::Viewport vp{};
        vp.originX = 0.0;
        vp.originY = 0.0;
        vp.width = static_cast<double>(ca->texture()->width());
        vp.height = static_cast<double>(ca->texture()->height());
        vp.znear = 0.0;
        vp.zfar = 1.0;
        enc->setViewport(vp);
    }

    // New encoder starts with clean state; force C++ rebind cache invalidation.
    RCache.InvalidateTextureCache();
}

void CHW::NullifyMetalTextures()
{
    auto* enc = static_cast<MTL::RenderCommandEncoder*>(m_currentEncoder);
    if (!enc)
        return;
    for (u32 i = 0; i < CTexture::mtMaxPixelShaderTextures; ++i)
        enc->setFragmentTexture(nullptr, i);
    for (u32 i = 0; i < CTexture::mtMaxVertexShaderTextures; ++i)
        enc->setVertexTexture(nullptr, i);
}

void CHW::EndScene()
{
    EndEncoding();
}

void CHW::Present()
{
    if (!m_currentCmdBuffer)
    {
        Msg("* Present: no cmd buffer (drawable=%p texHandle=%u)", m_currentDrawable, m_drawableTexHandle);
        if (m_currentDrawable)
        {
            static_cast<CA::MetalDrawable*>(m_currentDrawable)->release();
            m_currentDrawable = nullptr;
        }
        if (m_drawableTexHandle)
        {
            unregister_mtl_texture(m_drawableTexHandle);
            m_drawableTexHandle = 0;
        }
        return;
    }

    EndEncoding(); // ends the last render encoder

    auto* drawable = static_cast<CA::MetalDrawable*>(m_currentDrawable);
    auto* cmdBuffer = static_cast<MTL::CommandBuffer*>(m_currentCmdBuffer);

    // phase_flip: copies get_base_rt() to the drawable.
    // Fast path (identity gamma): plain blit encoder.
    // Slow path (non-identity gamma): commits old cmd buffer → render post-process pass into
    // rt_Generic_0 → blit rt_Generic_0 to drawable (bypasses TBDR sampler lock).
    // When the slow path fires, phase_flip commits the old command buffer and creates a new
    // one — update our local pointer so we present/commit the right buffer.
    if (RImplementation.Target)
    {
        RImplementation.Target->phase_flip();
        EndEncoding(); // End any encoder created by phase_flip (blit or render)
        cmdBuffer = static_cast<MTL::CommandBuffer*>(m_currentCmdBuffer); // may have changed
    }
    else
        Msg("! Present: RImplementation.Target is null");

    if (!cmdBuffer)
    {
        Msg("! Present: cmdBuffer is null after phase_flip, drawable=%p", drawable);
        // phase_flip could not create a new command buffer — nothing to present.
        if (m_currentCmdBuffer)
        {
            static_cast<MTL::CommandBuffer*>(m_currentCmdBuffer)->release();
            m_currentCmdBuffer = nullptr;
        }
        if (m_currentDrawable)
        {
            static_cast<CA::MetalDrawable*>(m_currentDrawable)->release();
            m_currentDrawable = nullptr;
        }
        if (m_drawableTexHandle) { unregister_mtl_texture(m_drawableTexHandle); m_drawableTexHandle = 0; }
        return;
    }

    // Render ImGui overlay on top of gamma-corrected content.
    // ImGui draw data was generated by ImGui::Render() in DoRender().
    {
        ImDrawData* drawData = ImGui::GetDrawData();
        if (drawData && drawData->TotalVtxCount > 0)
        {
            auto* drawableTex = drawable ? drawable->texture() : nullptr;
            if (drawableTex)
            {
                auto* imguiRPD = MTL::RenderPassDescriptor::alloc()->init();
                auto* ca = imguiRPD->colorAttachments()->object(0);
                ca->setTexture(drawableTex);
                ca->setLoadAction(MTL::LoadActionLoad);
                ca->setStoreAction(MTL::StoreActionStore);

                // NewFrame must be called before RenderDrawData to set up pixelFormat/sampleCount.
                // We use the same RPD targeting the drawable.
                ImGui_ImplMetal_NewFrame(imguiRPD);

                auto* enc = cmdBuffer->renderCommandEncoder(imguiRPD);
                if (enc)
                {
                    ImGui_ImplMetal_RenderDrawData(drawData, cmdBuffer, enc);
                    enc->endEncoding();
                    // Don't release enc — renderCommandEncoder returns an autoreleased object.
                }
                imguiRPD->release();
            }
        }
    }

    if (drawable)
        cmdBuffer->presentDrawable(drawable);
    cmdBuffer->commit();

    if (m_currentCmdBuffer)
    {
        static_cast<MTL::CommandBuffer*>(m_currentCmdBuffer)->release();
        m_currentCmdBuffer = nullptr;
    }
    if (m_currentDrawable)
    {
        static_cast<CA::MetalDrawable*>(m_currentDrawable)->release();
        m_currentDrawable = nullptr;
    }
    if (m_drawableTexHandle)
    {
        unregister_mtl_texture(m_drawableTexHandle);
        m_drawableTexHandle = 0;
    }
}

void CHW::ApplyGammaCorrection()
{
    // Gamma correction is applied in phase_flip() via the native Metal gamma PSO
    // (called from CHW::Present()). Nothing to do here.
}

DeviceState CHW::GetDeviceState()
{
    return DeviceState::Normal;
}

bool CHW::ThisInstanceIsGlobal() const
{
    return this == &HW;
}
} // namespace xray::render::RENDER_NAMESPACE
