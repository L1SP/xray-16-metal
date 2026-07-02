#pragma once

namespace xray::render::RENDER_NAMESPACE
{
namespace mtlStateUtils
{
u32 ConvertFillMode(u32 Mode);
u32 ConvertCullMode(u32 Mode);
u32 ConvertCmpFunction(u32 Func);
u32 ConvertStencilOp(u32 Op);
u32 ConvertBlendArg(u32 Arg);
u32 ConvertBlendOp(u32 Op);
u32 ConvertTextureAddressMode(u32 Mode);
u32 ConvertTextureFilter(u32 dxFilter, u32 mtlFilter = 0, bool MipMap = false);
} // namespace mtlStateUtils
} // namespace xray::render::RENDER_NAMESPACE
