#include "stdafx.h"
#include "xrEngine/IGame_Persistent.h"
#include "xrEngine/Environment.h"

namespace xray::render::RENDER_NAMESPACE
{
void CRenderTarget::accum_direct(CBackend& cmd_list, u32 sub_phase)
{
    // TODO: Implement Metal accum direct phase
}

void CRenderTarget::accum_direct_cascade(CBackend& cmd_list, u32 sub_phase, Fmatrix& xform, Fmatrix& xform_prev, float fBias)
{
    // TODO: Implement Metal accum direct cascade
}

void CRenderTarget::accum_direct_blend(CBackend& cmd_list)
{
    // TODO: Implement Metal accum direct blend
}

void CRenderTarget::accum_direct_f(CBackend& cmd_list, u32 sub_phase)
{
    // TODO: Implement Metal accum direct filter
}

void CRenderTarget::accum_direct_lum(CBackend& cmd_list)
{
    // TODO: Implement Metal accum direct luminance
}

void CRenderTarget::accum_direct_volumetric(u32 sub_phase, const u32 Offset, const Fmatrix& mShadow)
{
    // TODO: Implement Metal accum direct volumetric
}
} // namespace xray::render::RENDER_NAMESPACE
