#include "stdafx.h"
#pragma hdrstop

#include "../xrRender/ResourceManager.h"
#include "mtlTextureUtils.h"

namespace xray::render::RENDER_NAMESPACE
{
static MTL::PixelFormat D3DFormatToMetal(D3DFORMAT f)
{
    switch (f)
    {
    case D3DFMT_A8R8G8B8: return MTL::PixelFormatBGRA8Unorm;
    case D3DFMT_X8R8G8B8: return MTL::PixelFormatBGRA8Unorm;
    case D3DFMT_A16B16G16R16F: return MTL::PixelFormatRGBA16Float;
    case D3DFMT_A32B32G32R32F: return MTL::PixelFormatRGBA32Float;
    case D3DFMT_G16R16F: return MTL::PixelFormatRG16Float;
    case D3DFMT_G32R32F: return MTL::PixelFormatRG32Float;
    case D3DFMT_R32F: return MTL::PixelFormatR32Float;
    case D3DFMT_R8G8B8: return MTL::PixelFormatBGRA8Unorm;
    case D3DFMT_D24S8: return MTL::PixelFormatDepth32Float_Stencil8;
    case D3DFMT_D32: return MTL::PixelFormatDepth32Float;
    case D3DFMT_D16: return MTL::PixelFormatDepth16Unorm;
    case D3DFMT_D24X8: return MTL::PixelFormatDepth32Float;
    default: return MTL::PixelFormatBGRA8Unorm;
    }
}

CRT::~CRT()
{
    destroy();
    RImplementation.Resources->_DeleteRT(this);
}

void CRT::set_slice_read(int slice) {}
void CRT::set_slice_write(u32 context_id, int slice) {}

void CRT::create(LPCSTR Name, u32 w, u32 h, D3DFORMAT f, u32 SampleCount, u32 slices_num, Flags32 flags)
{
    if (pRT)
        return;
    R_ASSERT(Name && Name[0] && w && h);
    _order = CPU::QPC();
    dwWidth = w;
    dwHeight = h;
    fmt = f;
    sampleCount = SampleCount;
    RImplementation.Resources->Evict();

    auto* device = static_cast<MTL::Device*>(HW.m_device);
    if (!device)
    {
        pRT = 0;
        pZRT = 0;
        return;
    }

    MTL::PixelFormat pf = D3DFormatToMetal(f);
    bool isDepth = (f == D3DFMT_D24S8 || f == D3DFMT_D32 || f == D3DFMT_D16 || f == D3DFMT_D24X8);

    MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
    desc->setTextureType(MTL::TextureType2D);
    desc->setPixelFormat(pf);
    desc->setWidth(w);
    desc->setHeight(h);
    desc->setStorageMode(MTL::StorageModePrivate);
    desc->setUsage(isDepth ? MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead : MTL::TextureUsageShaderRead | MTL::TextureUsageRenderTarget);

    MTL::Texture* tex = device->newTexture(desc);
    desc->release();

    if (!tex)
    {
        pRT = 0;
        return;
    }

    u32 handle = register_mtl_texture(tex);
    tex->release(); // registry holds a retain

    pTexture = RImplementation.Resources->_CreateTexture(Name);
    if (isDepth)
    {
        pZRT = handle;
        pRT = 0;
    }
    else
    {
        pRT = handle;
        pZRT = 0;
    }
    pTexture->surface_set(0, handle);
}

void CRT::destroy()
{
    if (pRT)
        unregister_mtl_texture(pRT);
    if (pZRT)
        unregister_mtl_texture(pZRT);
    if (pTexture._get())
    {
        pTexture->surface_set(0, 0);
        pTexture = nullptr;
    }
    pRT = 0;
    pZRT = 0;
}

void CRT::reset_begin()
{
    destroy();
}

void CRT::reset_end()
{
    create(cName.c_str(), dwWidth, dwHeight, fmt, sampleCount, {}, {});
}

void CRT::resolve_into(CRT& destination) const
{
}

void resptrcode_crt::create(LPCSTR Name, u32 w, u32 h, D3DFORMAT f, u32 SampleCount, u32 slices_num, Flags32 flags)
{
    _set(RImplementation.Resources->_CreateRT(Name, w, h, f, SampleCount, 1, flags));
}
} // namespace xray::render::RENDER_NAMESPACE
