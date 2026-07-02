#include "stdafx.h"

#include "xrCore/Media/Image.hpp"
#include "xrEngine/xrImage_Resampler.h"

namespace xray::render::RENDER_NAMESPACE
{
using namespace XRay::Media;

#define GAMESAVE_SIZE 128

#define SM_FOR_SEND_WIDTH 640
#define SM_FOR_SEND_HEIGHT 480

void CRender::Screenshot(ScreenshotMode mode, pcstr name)
{
    // TODO: Implement Metal screenshot capture
    switch (mode)
    {
    case SM_NORMAL:
    {
        pcstr extension = "jpg";
        string64 time;
        string_path buf;
        xr_sprintf(buf, sizeof(buf), "ss_%s_%s_(%s).%s", Core.UserName, timestamp(time),
            g_pGameLevel ? g_pGameLevel->name().c_str() : "mainmenu", extension);
        IWriter* fs = FS.w_open("$screenshots$", buf);
        R_ASSERT(fs);
        FS.w_close(fs);
        break;
    }
    case SM_FOR_GAMESAVE:
        break;
    default:
        VERIFY(!"CRender::Screenshot. This screenshot type is not supported for Metal.");
    }
}
} // namespace xray::render::RENDER_NAMESPACE
