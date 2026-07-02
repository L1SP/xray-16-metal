#include "stdafx.h"

#include "r2_R_sun_support.h"

#include "xrEngine/IGame_Persistent.h"
#include "xrEngine/IRenderable.h"
#include "Layers/xrRender/FBasicVisual.h"
#include "xrCore/Threading/ParallelFor.hpp"

#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/matrix_access.hpp"

namespace xray::render::RENDER_NAMESPACE
{
void render_sun_old::init()
{
    // TODO: Implement Metal sun cascades initialization
    u32 cascade_count = R__NUM_SUN_CASCADES;
    m_sun_cascades.resize(cascade_count);
    float fBias = -0.0000025f;
    m_sun_cascades[0].reset_chain = true;
    m_sun_cascades[0].size = 20;
    m_sun_cascades[0].bias = m_sun_cascades[0].size * fBias;
    m_sun_cascades[1].size = 40;
    m_sun_cascades[1].bias = m_sun_cascades[1].size * fBias;
}

void render_sun_old::render()
{
    // TODO: Implement Metal sun rendering
}

void render_sun_old::flush()
{
    // TODO: Implement Metal sun flush
}
} // namespace xray::render::RENDER_NAMESPACE
