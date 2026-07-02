#pragma once

#include "Layers/xrRender/HWCaps.h"

namespace xray::render::RENDER_NAMESPACE
{
class CHW
    : public pureAppActivate,
      public pureAppDeactivate
{
public:
    CHW();
    ~CHW();

    void CreateDevice(SDL_Window* wnd);
    void CreateMetalView();
    void DestroyDevice();
    void Reset();

    void SetPrimaryAttributes(u32& windowFlags);

    void BeginScene();
    void EndScene();
    void Present();
    void ApplyGammaCorrection(); // Gamma applied in phase_flip() via Metal PSO (Present path)

    // Encoder management for u_setrt / ClearRT / ClearZB
    void EndEncoding();
    void CreateEncoder(MTL::RenderPassDescriptor* rpd);
    MTL::RenderPassDescriptor* CreateRPD(MTL::Texture* color0, MTL::Texture* color1,
        MTL::Texture* color2, MTL::Texture* depth);

    std::pair<u32, u32> GetSurfaceSize() const { return { surf_width, surf_height }; }
    DeviceState GetDeviceState();

    void OnAppActivate() override;
    void OnAppDeactivate() override;

    bool ThisInstanceIsGlobal() const;

public:
    static constexpr auto IMM_CTX_ID = 0;

    CHWCaps Caps;

    SDL_Window* m_window = nullptr;

    u32 CurrentBackBuffer{};
    u32 BackBufferCount{};

    // Metal-specific (stored as void* to avoid exposing metal-cpp in header)
    void* m_device = nullptr;         // MTL::Device*
    void* m_cmdQueue = nullptr;       // MTL::CommandQueue*
    void* m_swapchain = nullptr;      // CA::MetalLayer*
    void* m_currentDrawable = nullptr; // CA::MetalDrawable*
    void* m_currentCmdBuffer = nullptr; // MTL::CommandBuffer* (per-frame)
    void* m_currentEncoder = nullptr;  // MTL::RenderCommandEncoder* (per-frame)
    void* m_currentRPD = nullptr;      // MTL::RenderPassDescriptor* (per-frame)
    void* m_metalView = nullptr;      // SDL_MetalView
    void* m_persistentRPD = nullptr;  // MTL::RenderPassDescriptor* (persistent, for ImGui)
    void* m_defaultSampler = nullptr; // MTL::SamplerState* (persistent, default sampler)
    void* m_autoreleasePool = nullptr; // NS::AutoreleasePool* (per-frame)

    // Adapter info (matches GL backend)
    pcstr AdapterName = nullptr;

    u32 surf_width = 0;
    u32 surf_height = 0;

    // Per-frame drawable texture handle (registered each frame in BeginScene)
    u32 m_drawableTexHandle = 0;
};

extern ECORE_API CHW HW;
} // namespace xray::render::RENDER_NAMESPACE
