#include "stdafx.h"
#include "mtlTextureUtils.h"

namespace xray::render::RENDER_NAMESPACE
{
namespace mtlTextureUtils
{
u32 ConvertTextureFormat(D3DFORMAT dx9FMT)
{
    switch (dx9FMT)
    {
    case D3DFMT_UNKNOWN:        return 10; // MTL::PixelFormatInvalid
    case D3DFMT_A8R8G8B8:       return 80; // MTL::PixelFormatBGRA8Unorm
    case D3DFMT_A8B8G8R8:       return 70; // MTL::PixelFormatRGBA8Unorm
    case D3DFMT_G16R16:         return 150; // MTL::PixelFormatRG16Unorm
    case D3DFMT_A16B16G16R16:   return 120; // MTL::PixelFormatRGBA16Unorm
    case D3DFMT_L8:             return 10; // MTL::PixelFormatR8Unorm
    case D3DFMT_V8U8:           return 30; // MTL::PixelFormatRG8Snorm
    case D3DFMT_Q8W8V8U8:       return 70; // MTL::PixelFormatRGBA8Snorm
    case D3DFMT_V16U16:         return 150; // MTL::PixelFormatRG16Snorm
    case D3DFMT_D24S8:          return 260; // MTL::PixelFormatDepth24Unorm_Stencil8
    case D3DFMT_D24X8:          return 260;
    case D3DFMT_G16R16F:        return 155; // MTL::PixelFormatRG16Float
    case D3DFMT_A16B16G16R16F:  return 125; // MTL::PixelFormatRGBA16Float
    case D3DFMT_R32F:           return 55; // MTL::PixelFormatR32Float
    case D3DFMT_R16F:           return 25; // MTL::PixelFormatR16Float
    case D3DFMT_A32B32G32R32F:  return 130; // MTL::PixelFormatRGBA32Float
    default:                    return 80; // MTL::PixelFormatBGRA8Unorm
    }
}
} // namespace mtlTextureUtils
} // namespace xray::render::RENDER_NAMESPACE
