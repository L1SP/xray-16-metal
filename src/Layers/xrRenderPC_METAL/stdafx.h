#pragma once

// metal-cpp headers MUST come before xrEngine/stdafx.h to:
// 1. Define MTL::Event before our Event.hpp (namespace conflict avoidance)
// 2. Let objc/runtime.h define BOOL as bool before PlatformApple.inl
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <MetalFX/MetalFX.hpp>
#include <QuartzCore/QuartzCore.hpp>

// Undefine ObjC macros that conflict with engine/external code
#ifdef nil
#undef nil
#endif

// ObjC headers define BOOL as bool/signed char (1 byte), but the engine
// expects BOOL as int32_t (4 bytes). The shaders.xr file is saved with
// sizeof(BOOL)=4, so we must match. Override BOOL with a preprocessor
// macro so that Common/PlatformApple.inl (#ifndef BOOL) skips its typedef.
#define BOOL int32_t

// Metal system headers spuriously define BSD on Apple platforms, but
// Common/Platform.hpp checks defined(BSD) BEFORE defined(__APPLE__),
// which would misidentify this backend as XR_PLATFORM_BSD. Undefine it
// so Platform.hpp correctly picks the Apple branch.
#ifdef BSD
#undef BSD
#endif

#pragma warning(disable:4995)
#include "xrEngine/stdafx.h"
#pragma warning(default:4995)
#pragma warning(disable:4714)
#pragma warning( 4 : 4018 )
#pragma warning( 4 : 4244 )
#pragma warning(disable:4237)

#include "xrEngine/vis_common.h"
#include "xrEngine/Render.h"
#include "xrEngine/IGame_Level.h"

#include "xrParticles/psystem.h"

// D3D compat types must be included before shared render headers
#include "Common/d3d9compat.hpp"
#include "Common/_d3d_extensions.h"

#define R_METAL 0
#define R_R1 1
#define R_R2 2
#define R_R3 3
#define R_R4 4
#define RENDER R_METAL

#include "Layers/xrRenderMETAL/mtlCommonTypes.h"

#include "Layers/xrRenderMETAL/mtlHW.h"

#include "Layers/xrRender/Debug/dxPixEventWrapper.h"

#include "Layers/xrRender/Shader.h"

#include "Layers/xrRender/R_Backend.h"
#include "Layers/xrRender/R_Backend_Runtime.h"

#include "Layers/xrRender/Blender.h"
#include "Layers/xrRender/Blender_CLSID.h"

#include "Layers/xrRender/ResourceManager.h"
#include "Layers/xrRender/xrRender_console.h"

#include "r2.h"
#include "mtl_rendertarget.h"

namespace xray::render::RENDER_NAMESPACE
{
IC void jitter(CBlender_Compile& C)
{
    C.r_Sampler("jitter0", JITTER(0), true, D3DTADDRESS_WRAP, D3DTEXF_POINT, D3DTEXF_NONE, D3DTEXF_POINT);
    C.r_Sampler("jitter1", JITTER(1), true, D3DTADDRESS_WRAP, D3DTEXF_POINT, D3DTEXF_NONE, D3DTEXF_POINT);
    C.r_Sampler("jitter2", JITTER(2), true, D3DTADDRESS_WRAP, D3DTEXF_POINT, D3DTEXF_NONE, D3DTEXF_POINT);
    C.r_Sampler("jitter3", JITTER(3), true, D3DTADDRESS_WRAP, D3DTEXF_POINT, D3DTEXF_NONE, D3DTEXF_POINT);
}
} // namespace xray::render::RENDER_NAMESPACE
