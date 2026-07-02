#include "stdafx.h"

#include <gli/gli.hpp>

#include "Layers/xrRender/SH_Texture.h"
#include "xrEngine/xrTheora_Surface.h"

namespace xray::render::RENDER_NAMESPACE
{
void resptrcode_texture::create(LPCSTR _name)
{
    if (_name && _name[0])
        _set(RImplementation.Resources->_CreateTexture(_name));
}
CTexture::CTexture()
{
    pAVI = nullptr;
    pTheora = nullptr;
    pSurface = 0;
    pBuffer = 0;
    desc_cache = 0;
    desc = 0;
    m_width = 0;
    m_height = 0;
    seqMSPF = 0;
    flags.bLoaded = false;
    flags.bUser = false;
    flags.seqCycles = false;
    flags.MemoryUsage = 0;
    m_material = 1.0f;
    bind = fastdelegate::FastDelegate2<CBackend&, u32>(this, &CTexture::apply_load);
}

CTexture::~CTexture()
{
    Unload();
    RImplementation.Resources->_DeleteTexture(this);
}

xr_map<u32, MTL::Texture*> s_mtlTextures;
xr_map<MTL::Texture*, u32> s_mtlTextureReverse;
static u32 s_nextTextureHandle = 1;

u32 register_mtl_texture(MTL::Texture* tex)
{
    if (!tex) return 0;
    auto it = s_mtlTextureReverse.find(tex);
    if (it != s_mtlTextureReverse.end())
        return it->second;
    u32 handle = s_nextTextureHandle++;
    tex->retain();
    s_mtlTextures[handle] = tex;
    s_mtlTextureReverse[tex] = handle;
    return handle;
}

void unregister_mtl_texture(MTL::Texture* tex)
{
    auto it = s_mtlTextureReverse.find(tex);
    if (it != s_mtlTextureReverse.end())
    {
        s_mtlTextures.erase(it->second);
        s_mtlTextureReverse.erase(it);
        tex->release();
    }
}

void unregister_mtl_texture(u32 handle)
{
    auto it = s_mtlTextures.find(handle);
    if (it != s_mtlTextures.end())
    {
        auto* tex = it->second;
        s_mtlTextureReverse.erase(tex);
        s_mtlTextures.erase(it);
        tex->release();
    }
}

MTL::Texture* lookup_mtl_texture(u32 handle)
{
    auto it = s_mtlTextures.find(handle);
    return it != s_mtlTextures.end() ? it->second : nullptr;
}

void CTexture::surface_set(int target, u32 surf)
{
    desc = target;
    pSurface = surf;
}

u32 CTexture::surface_get() const
{
    return pSurface;
}

void CTexture::PostLoad()
{
    if (pTheora)
    {
        bind = fastdelegate::FastDelegate2<CBackend&, u32>(this, &CTexture::apply_theora);
    }
    else if (!seqDATA.empty())
    {
        bind = fastdelegate::FastDelegate2<CBackend&, u32>(this, &CTexture::apply_seq);
    }
    else
    {
        bind = fastdelegate::FastDelegate2<CBackend&, u32>(this, &CTexture::apply_normal);
    }
}

void CTexture::apply_load(CBackend& cmd_list, u32 dwStage)
{
    if (!flags.bLoaded)
        Load();
    PostLoad();
    bind(cmd_list, dwStage);
}

void CTexture::apply_seq(CBackend& cmd_list, u32 dwStage)
{
    // SEQ
    u32 frame = Device.dwTimeContinual / seqMSPF;
    u32 frame_data = seqDATA.size();
    if (flags.seqCycles)
    {
        u32 frame_id = frame % (frame_data * 2);
        if (frame_id >= frame_data)
            frame_id = (frame_data - 1) - (frame_id % frame_data);
        pSurface = seqDATA[frame_id];
    }
    else
    {
        u32 frame_id = frame % frame_data;
        pSurface = seqDATA[frame_id];
    }
    apply_normal(cmd_list, dwStage);
}

void CTexture::apply_theora(CBackend& cmd_list, u32 dwStage)
{
    auto* enc = static_cast<MTL::RenderCommandEncoder*>(HW.m_currentEncoder);
    if (!enc)
        return;

    auto* mtlTex = lookup_mtl_texture(pSurface);
    if (!mtlTex)
        return;

    enc->setFragmentTexture(mtlTex, dwStage);
    if (auto* sampler = static_cast<MTL::SamplerState*>(HW.m_defaultSampler))
        enc->setFragmentSamplerState(sampler, dwStage);

    if (pTheora->Update(m_play_time != 0xFFFFFFFF ? m_play_time : Device.dwTimeContinual))
    {
        u32 _w = pTheora->Width(true);
        u32 _h = pTheora->Height(true);

        size_t bufSize = _w * _h * 4;
        u32* pBits = xr_alloc<u32>(bufSize / sizeof(u32));
        int _pos = 0;
        pTheora->DecompressFrame(pBits, 0, _pos);

        MTL::Region region = MTL::Region(0, 0, _w, _h);
        mtlTex->replaceRegion(region, 0, pBits, _w * 4);

        xr_free(pBits);
    }
}

extern xr_map<u32, MTL::Texture*> s_mtlTextures;
void CTexture::apply_normal(CBackend& cmd_list, u32 dwStage) const
{
    auto* mtlTex = lookup_mtl_texture(pSurface);
    if (!mtlTex)
    {
        Msg("! apply_normal FAIL: pSurface=%u '%s' stage=%d", pSurface, cName.c_str(), dwStage);
        return;
    }
    if (auto* enc = static_cast<MTL::RenderCommandEncoder*>(HW.m_currentEncoder))
    {
        if (mtlTex->pixelFormat() != MTL::PixelFormatBGRA8Unorm && mtlTex->pixelFormat() != MTL::PixelFormatBGRA8Unorm_sRGB
            && mtlTex->pixelFormat() != MTL::PixelFormatRGBA8Unorm && mtlTex->pixelFormat() != MTL::PixelFormatRGBA8Unorm_sRGB
            && mtlTex->pixelFormat() != MTL::PixelFormatBC1_RGBA && mtlTex->pixelFormat() != MTL::PixelFormatBC3_RGBA
            && mtlTex->pixelFormat() != MTL::PixelFormatBC4_RUnorm && mtlTex->pixelFormat() != MTL::PixelFormatBC5_RGUnorm
            && mtlTex->pixelFormat() != MTL::PixelFormatBC7_RGBAUnorm)
            Msg("! apply_normal: '%s' has unusual pixelFormat=%d", cName.c_str(), (int)mtlTex->pixelFormat());
        enc->setFragmentTexture(mtlTex, dwStage);
        if (auto* sampler = static_cast<MTL::SamplerState*>(HW.m_defaultSampler))
            enc->setFragmentSamplerState(sampler, dwStage);
    }
}

void CTexture::Preload()
{
    m_bumpmap = RImplementation.Resources->m_textures_description.GetBumpName(cName);
    m_material = RImplementation.Resources->m_textures_description.GetMaterial(cName);
}

void CTexture::Load()
{
    flags.bLoaded = true;
    desc_cache = 0;
    if (pSurface)
        return;
    flags.bUser = false;
    flags.MemoryUsage = 0;
    if (nullptr == cName.c_str())
        return;
    if (0 == xr_stricmp(cName.c_str(), "$null"))
        return;
    if (0 == strncmp(cName.c_str(), "$user$", sizeof("$user$") - 1))
    {
        flags.bUser = true;
        return;
    }
    Preload();

    bool bTheora = false;
    string_path fn;
    if (FS.exist(fn, "$game_textures$", cName.c_str(), ".ogm"))
    {
        pTheora = xr_new<CTheoraSurface>();
        m_play_time = 0xFFFFFFFF;
        if (!pTheora->Load(fn))
        {
            xr_delete(pTheora);
            FATAL("Can't open video stream");
        }
        else
        {
            flags.MemoryUsage = pTheora->Width(true) * pTheora->Height(true) * 4;
            pTheora->Play(TRUE, Device.dwTimeContinual);
            u32 _w = pTheora->Width(true);
            u32 _h = pTheora->Height(true);
            auto* device = static_cast<MTL::Device*>(HW.m_device);
            if (device)
            {
                MTL::TextureDescriptor* tdesc = MTL::TextureDescriptor::alloc()->init();
                tdesc->setTextureType(MTL::TextureType2D);
                tdesc->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
                tdesc->setWidth(_w);
                tdesc->setHeight(_h);
                tdesc->setStorageMode(MTL::StorageModeShared);
                tdesc->setUsage(MTL::TextureUsageShaderRead);
                MTL::Texture* tex = device->newTexture(tdesc);
                tdesc->release();
                if (tex)
                {
                    pSurface = register_mtl_texture(tex);
                    tex->release();
                }
            }
            bTheora = true;
        }
    }
    if (!bTheora)
    {
        string_path fn;
        if (FS.exist(fn, "$game_textures$", cName.c_str(), ".seq"))
        {
            // Sequence
            string256 buffer;
            IReader* _fs = FS.r_open(fn);

            flags.seqCycles = FALSE;
            _fs->r_string(buffer, sizeof(buffer));
            if (0 == xr_stricmp(buffer, "cycled"))
            {
                flags.seqCycles = TRUE;
                _fs->r_string(buffer, sizeof(buffer));
            }
            u32 fps = atoi(buffer);
            seqMSPF = 1000 / fps;

            while (!_fs->eof())
            {
                _fs->r_string(buffer, sizeof(buffer));
                _Trim(buffer);
                if (buffer[0])
                {
                    u32 mem = 0;
                    int seq_desc = 0;
                    u32 tex_handle = RImplementation.texture_load(buffer, mem, seq_desc);
                    if (tex_handle)
                    {
                        seqDATA.push_back(tex_handle);
                        flags.MemoryUsage += mem;
                    }
                }
            }
            pSurface = 0;
            FS.r_close(_fs);
        }
        else
        {
            // Normal texture
            u32 mem = 0;
            int desc_type = 0;
            pSurface = RImplementation.texture_load(cName.c_str(), mem, desc_type);
            desc = desc_type;
            flags.MemoryUsage = mem;
        }
    }

    PostLoad();
}

void CTexture::Unload()
{
    flags.bLoaded = FALSE;
    if (!seqDATA.empty())
    {
        for (auto& handle : seqDATA)
        {
            if (auto* tex = lookup_mtl_texture(handle))
                unregister_mtl_texture(tex);
        }
        seqDATA.clear();
    }
    if (pSurface)
    {
        if (auto* tex = lookup_mtl_texture(pSurface))
            unregister_mtl_texture(tex);
        pSurface = 0;
    }
    pBuffer = 0;
    desc_cache = 0;
    m_width = 0;
    m_height = 0;
    xr_delete(pTheora);
    bind = fastdelegate::FastDelegate2<CBackend&, u32>(this, &CTexture::apply_load);
}

void CTexture::desc_update()
{
    desc_cache = pSurface;
    if (auto* tex = lookup_mtl_texture(pSurface))
    {
        m_width = static_cast<u32>(tex->width());
        m_height = static_cast<u32>(tex->height());
    }
}

void CTexture::video_Play(BOOL looped, u32 _time)
{
    if (pTheora)
        pTheora->Play(looped, _time != 0xFFFFFFFF ? (m_play_time = _time) : Device.dwTimeContinual);
}

void CTexture::video_Pause(BOOL state) const
{
    if (pTheora)
        pTheora->Pause(state);
}

void CTexture::video_Stop() const
{
    if (pTheora)
        pTheora->Stop();
}

BOOL CTexture::video_IsPlaying() const
{
    return pTheora ? pTheora->IsPlaying() : FALSE;
}
} // namespace xray::render::RENDER_NAMESPACE
