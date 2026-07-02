#include "stdafx.h"
#include "mtlStateUtils.h"

namespace xray::render::RENDER_NAMESPACE
{
namespace mtlStateUtils
{
u32 ConvertFillMode(u32 Mode)
{
    switch (Mode)
    {
    case D3DFILL_POINT:
        return 0; // MTL::TriangleFillModePoints
    case D3DFILL_WIREFRAME:
        return 1; // MTL::TriangleFillModeLines
    case D3DFILL_SOLID:
        return 2; // MTL::TriangleFillModeFill
    default:
        VERIFY(!"Unexpected fill mode!");
        return 2;
    }
}

u32 ConvertCullMode(u32 Mode)
{
    switch (Mode)
    {
    case D3DCULL_NONE:
        return 0; // MTL::CullModeNone
    case D3DCULL_CW:
        return 1; // MTL::CullModeFront (Metal front=CCW, but D3D uses opposite winding)
    case D3DCULL_CCW:
        return 2; // MTL::CullModeBack
    default:
        VERIFY(!"Unexpected cull mode!");
        return 0;
    }
}

u32 ConvertCmpFunction(u32 Func)
{
    switch (Func)
    {
    case D3DCMP_NEVER:
        return 0; // MTL::CompareFunctionNever
    case D3DCMP_LESS:
        return 1; // MTL::CompareFunctionLess
    case D3DCMP_EQUAL:
        return 2; // MTL::CompareFunctionEqual
    case D3DCMP_LESSEQUAL:
        return 3; // MTL::CompareFunctionLessEqual
    case D3DCMP_GREATER:
        return 4; // MTL::CompareFunctionGreater
    case D3DCMP_NOTEQUAL:
        return 5; // MTL::CompareFunctionNotEqual
    case D3DCMP_GREATEREQUAL:
        return 6; // MTL::CompareFunctionGreaterEqual
    case D3DCMP_ALWAYS:
        return 7; // MTL::CompareFunctionAlways
    default:
        VERIFY(!"ConvertCmpFunction can't convert argument!");
        return 7;
    }
}

u32 ConvertStencilOp(u32 Op)
{
    switch (Op)
    {
    case D3DSTENCILOP_KEEP:
        return 0; // MTL::StencilOperationKeep
    case D3DSTENCILOP_ZERO:
        return 1; // MTL::StencilOperationZero
    case D3DSTENCILOP_REPLACE:
        return 2; // MTL::StencilOperationReplace
    case D3DSTENCILOP_INCRSAT:
        return 3; // MTL::StencilOperationIncrementClamp
    case D3DSTENCILOP_DECRSAT:
        return 4; // MTL::StencilOperationDecrementClamp
    case D3DSTENCILOP_INVERT:
        return 5; // MTL::StencilOperationInvert
    case D3DSTENCILOP_INCR:
        return 6; // MTL::StencilOperationIncrementWrap
    case D3DSTENCILOP_DECR:
        return 7; // MTL::StencilOperationDecrementWrap
    default:
        VERIFY(!"ConvertStencilOp can't convert argument!");
        return 0;
    }
}

u32 ConvertBlendArg(u32 Arg)
{
    switch (Arg)
    {
    case D3DBLEND_ZERO:
        return 0; // MTL::BlendFactorZero
    case D3DBLEND_ONE:
        return 1; // MTL::BlendFactorOne
    case D3DBLEND_SRCCOLOR:
        return 2; // MTL::BlendFactorSourceColor
    case D3DBLEND_INVSRCCOLOR:
        return 3; // MTL::BlendFactorOneMinusSourceColor
    case D3DBLEND_SRCALPHA:
        return 4; // MTL::BlendFactorSourceAlpha
    case D3DBLEND_INVSRCALPHA:
        return 5; // MTL::BlendFactorOneMinusSourceAlpha
    case D3DBLEND_DESTALPHA:
        return 6; // MTL::BlendFactorDestinationAlpha
    case D3DBLEND_INVDESTALPHA:
        return 7; // MTL::BlendFactorOneMinusDestinationAlpha
    case D3DBLEND_DESTCOLOR:
        return 8; // MTL::BlendFactorDestinationColor
    case D3DBLEND_INVDESTCOLOR:
        return 9; // MTL::BlendFactorOneMinusDestinationColor
    case D3DBLEND_SRCALPHASAT:
        return 10; // MTL::BlendFactorSourceAlphaSaturated
    default:
        VERIFY(!"ConvertBlendArg can't convert argument!");
        return 1;
    }
}

u32 ConvertBlendOp(u32 Op)
{
    switch (Op)
    {
    case D3DBLENDOP_ADD:
        return 0; // MTL::BlendOperationAdd
    case D3DBLENDOP_SUBTRACT:
        return 1; // MTL::BlendOperationSubtract
    case D3DBLENDOP_REVSUBTRACT:
        return 2; // MTL::BlendOperationReverseSubtract
    case D3DBLENDOP_MIN:
        return 3; // MTL::BlendOperationMin
    case D3DBLENDOP_MAX:
        return 4; // MTL::BlendOperationMax
    default:
        VERIFY(!"ConvertBlendOp can't convert argument!");
        return 0;
    }
}

u32 ConvertTextureAddressMode(u32 Mode)
{
    switch (Mode)
    {
    case D3DTADDRESS_WRAP:
        return 0; // MTL::SamplerAddressModeRepeat
    case D3DTADDRESS_MIRROR:
        return 1; // MTL::SamplerAddressModeMirrorRepeat
    case D3DTADDRESS_CLAMP:
        return 2; // MTL::SamplerAddressModeClampToEdge
    case D3DTADDRESS_BORDER:
        return 3; // MTL::SamplerAddressModeClampToBorder
    default:
        VERIFY(!"ConvertTextureAddressMode can't convert argument!");
        return 2;
    }
}

u32 ConvertTextureFilter(u32 dxFilter, u32 mtlFilter, bool MipMap)
{
    const u32 FilterLinear = 0x01;
    const u32 MipFilterLinear = 0x02;
    const u32 MipFilterEnable = 0x100;

    switch (dxFilter)
    {
    case D3DTEXF_NONE:
        if (MipMap)
            return mtlFilter & ~MipFilterLinear & ~MipFilterEnable;
        VERIFY(!"D3DTEXF_NONE only supported with D3DSAMP_MIPFILTER");
        return mtlFilter;
    case D3DTEXF_POINT:
    {
        if (MipMap)
            return ((mtlFilter & ~MipFilterLinear) | MipFilterEnable);
        return mtlFilter & ~FilterLinear;
    }
    case D3DTEXF_LINEAR:
    case D3DTEXF_ANISOTROPIC:
    {
        if (MipMap)
            return mtlFilter | MipFilterLinear | MipFilterEnable;
        return mtlFilter | FilterLinear;
    }
    default:
        VERIFY(!"ConvertTextureFilter can't convert argument!");
        return mtlFilter;
    }
}
} // namespace mtlStateUtils
} // namespace xray::render::RENDER_NAMESPACE
