#include "stdafx.h"

#include "dxImGuiRender.h"

#if defined(USE_DX11)
#include <backends/imgui_impl_dx11.h>
#elif defined(USE_OGL)
#include <backends/imgui_impl_opengl3.h>
#elif defined(USE_METAL)
#include <backends/imgui_impl_metal.h>
#endif

namespace xray::render::RENDER_NAMESPACE
{
void dxImGuiRender::Copy(IImGuiRender& _in)
{
    *this = *dynamic_cast<dxImGuiRender*>(&_in);
}

void dxImGuiRender::SetState(ImDrawData* data)
{
    RCache.SetViewport({ 0.f, 0.f, data->DisplaySize.x, data->DisplaySize.y, 0.f, 1.f });
}

void dxImGuiRender::Frame()
{
#if defined(USE_DX11)
    ImGui_ImplDX11_NewFrame();
#elif defined(USE_OGL)
    ImGui_ImplOpenGL3_NewFrame();
#elif defined(USE_METAL)
    // ImGui_ImplMetal_NewFrame is called from CHW::Present() just before rendering,
    // to keep it within the same autorelease pool scope as RenderDrawData.
#endif
}

void dxImGuiRender::Render(ImDrawData* data)
{
#if defined(USE_DX11)
    ImGui_ImplDX11_RenderDrawData(data);
#elif defined(USE_OGL)
    ImGui_ImplOpenGL3_RenderDrawData(data);
#elif defined(USE_METAL)
    // ImGui Metal rendering is deferred to CHW::Present(), after phase_flip()
    // has finished the gamma pass + blit to the drawable. At that point we can
    // create a new encoder targeting the drawable and render ImGui on top.
    (void)data; // unused in Metal — rendered via ImGui::GetDrawData() in Present()
#endif
}

void dxImGuiRender::OnDeviceCreate(ImGuiContext* context)
{
    ImGui::SetAllocatorFunctions(
        [](size_t size, void* /*user_data*/)
        {
            return xr_malloc(size);
        },
        [](void* ptr, void* /*user_data*/)
        {
            xr_free(ptr);
        }
    );
    ImGui::SetCurrentContext(context);

    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "xrRender";

#if defined(USE_DX11)
    ImGui_ImplDX11_Init(HW.pDevice, HW.get_context(CHW::IMM_CTX_ID));
#elif defined(USE_OGL)
    ImGui_ImplOpenGL3_Init();
#elif defined(USE_METAL)
    ImGui_ImplMetal_Init(static_cast<MTL::Device*>(HW.m_device));
#endif
}
void dxImGuiRender::OnDeviceDestroy()
{
#if defined(USE_DX11)
    ImGui_ImplDX11_Shutdown();
#elif defined(USE_OGL)
    ImGui_ImplOpenGL3_Shutdown();
#elif defined(USE_METAL)
    ImGui_ImplMetal_Shutdown();
#endif
}

void dxImGuiRender::OnDeviceResetBegin()
{
#if defined(USE_DX11)
    ImGui_ImplDX11_InvalidateDeviceObjects();
#elif defined(USE_OGL)
    ImGui_ImplOpenGL3_DestroyDeviceObjects();
#elif defined(USE_METAL)
    ImGui_ImplMetal_DestroyDeviceObjects();
#endif
}

void dxImGuiRender::OnDeviceResetEnd()
{
#if defined(USE_DX11)
    ImGui_ImplDX11_CreateDeviceObjects();
#elif defined(USE_OGL)
    ImGui_ImplOpenGL3_CreateDeviceObjects();
#elif defined(USE_METAL)
    ImGui_ImplMetal_CreateDeviceObjects(static_cast<MTL::Device*>(HW.m_device));
#endif
}
} // namespace xray::render::RENDER_NAMESPACE
