#include "stdafx.h"

namespace xray::render::RENDER_NAMESPACE
{
BOOL xrRender_test_hw()
{
    // TODO: Verify Metal availability using MTLCreateSystemDefaultDevice()
    // For now, assume Metal is available on Apple Silicon
    return true;
}
} // namespace xray::render::RENDER_NAMESPACE
