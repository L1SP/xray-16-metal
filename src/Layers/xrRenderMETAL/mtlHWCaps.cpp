#include "stdafx.h"
#pragma hdrstop

#include "Layers/xrRender/HWCaps.h"
#include "mtlHW.h"

namespace xray::render::RENDER_NAMESPACE
{
namespace
{
u32 GetGpuNum()
{
    return 2;
}
}

void CHWCaps::Update()
{
    // TODO: Query Metal device capabilities
    geometry_major = 4;
    geometry_minor = 0;
    geometry_profile = "vs_4_0";
    geometry.bSoftware = FALSE;
    geometry.bPointSprites = FALSE;
    geometry.bNPatches = FALSE;
    u32 cnt = 256;
    clamp<u32>(cnt, 0, 256);
    geometry.dwRegisters = cnt;
    geometry.dwInstructions = 256;
    geometry.dwClipPlanes = _min(6, 15);
    geometry.bVTF = TRUE;

    raster_major = 4;
    raster_minor = 0;
    raster_profile = "ps_4_0";
    raster.dwStages = 15;
    raster.bNonPow2 = TRUE;
    raster.bCubemap = TRUE;
    raster.dwMRT_count = 4;
    raster.b_MRT_mixdepth = TRUE;
    raster.dwInstructions = 256;
    geometry.dwVertexCache = 24;

    if (0 == raster_major)
        geometry_major = 0;

    bTableFog = FALSE;
    bStencil = TRUE;
    bScissor = TRUE;

    soInc = D3DSTENCILOP_INCRSAT;
    soDec = D3DSTENCILOP_DECRSAT;
    dwMaxStencilValue = (1 << 8) - 1;

    max_ffp_lights = 0;

    iGPUNum = GetGpuNum();

    useCombinedSamplers = true;
}
} // namespace xray::render::RENDER_NAMESPACE
