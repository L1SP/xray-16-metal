#include "stdafx.h"

namespace xray::render::RENDER_NAMESPACE
{
static void generate_jitter(u32* dest, u32 elem_count)
{
    const int cmax = 8;
    svector<Ivector2, cmax> samples;
    while (samples.size() < elem_count * 2)
    {
        Ivector2 test;
        test.set(Random.randI(0, 256), Random.randI(0, 256));
        BOOL valid = TRUE;
        for (auto& sample : samples)
        {
            int dist = _abs(test.x - sample.x) + _abs(test.y - sample.y);
            if (dist < 32)
            {
                valid = FALSE;
                break;
            }
        }
        if (valid)
            samples.push_back(test);
    }
    for (u32 it = 0; it < elem_count; it++, dest++)
        *dest = color_rgba(samples[2 * it].x, samples[2 * it].y, samples[2 * it + 1].y, samples[2 * it + 1].x);
}

void CRenderTarget::build_textures()
{
    auto* device = static_cast<MTL::Device*>(HW.m_device);

    // Build material(s)
    {
        MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
        desc->setTextureType(MTL::TextureType3D);
        desc->setPixelFormat(MTL::PixelFormatRG8Unorm);
        desc->setWidth(TEX_material_LdotN);
        desc->setHeight(TEX_material_LdotH);
        desc->setDepth(TEX_material_Count);
        desc->setStorageMode(MTL::StorageModeShared);
        desc->setUsage(MTL::TextureUsageShaderRead);

        MTL::Texture* tex = device->newTexture(desc);
        desc->release();

        // Fill it (addr: x=dot(L,N), y=dot(L,H))
        static constexpr u32 RowPitch = TEX_material_LdotN * 2;
        static constexpr u32 SlicePitch = TEX_material_LdotH * RowPitch;
        u16 pBits[TEX_material_LdotN * TEX_material_LdotH * TEX_material_Count];
        for (u32 slice = 0; slice < TEX_material_Count; slice++)
        {
            for (u32 y = 0; y < TEX_material_LdotH; y++)
            {
                for (u32 x = 0; x < TEX_material_LdotN; x++)
                {
                    u16* p = (u16*)((u8*)(pBits)+slice * SlicePitch +
                        y * RowPitch + x * 2);
                    float ld = float(x) / float(TEX_material_LdotN - 1);
                    float ls = float(y) / float(TEX_material_LdotH - 1) + EPS_S;
                    ls *= powf(ld, 1 / 32.f);
                    float fd, fs;

                    switch (slice)
                    {
                    case 0:
                    {
                        fd = powf(ld, 0.75f);
                        fs = powf(ls, 16.f) * .5f;
                    }
                    break;
                    case 1:
                    {
                        fd = powf(ld, 0.90f);
                        fs = powf(ls, 24.f);
                    }
                    break;
                    case 2:
                    {
                        fd = ld;
                        fs = powf(ls * 1.01f, 128.f);
                    }
                    break;
                    case 3:
                    {
                        float s0 = _abs(1 - _abs(0.05f * _sin(33.f * ld) + ld - ls));
                        float s1 = _abs(1 - _abs(0.05f * _cos(33.f * ld * ls) + ld - ls));
                        float s2 = _abs(1 - _abs(ld - ls));
                        fd = ld;
                        fs = powf(_max(_max(s0, s1), s2), 24.f);
                        fs *= powf(ld, 1 / 7.f);
                    }
                    break;
                    default: fd = fs = 0;
                    }
                    s32 _d = clampr(iFloor(fd * 255.5f), 0, 255);
                    s32 _s = clampr(iFloor(fs * 255.5f), 0, 255);
                    if (y == (TEX_material_LdotH - 1) && x == (TEX_material_LdotN - 1))
                    {
                        _d = 255;
                        _s = 255;
                    }
                    *p = u16(_s * 256 + _d);
                }
            }
        }

        MTL::Region region = MTL::Region(0, 0, 0,
            static_cast<NS::UInteger>(TEX_material_LdotN),
            static_cast<NS::UInteger>(TEX_material_LdotH),
            static_cast<NS::UInteger>(TEX_material_Count));
        tex->replaceRegion(region, 0, 0, pBits, RowPitch, SlicePitch);

        u32 handle = register_mtl_texture(tex);
        tex->release();
        t_material_surf = handle;
        t_material = RImplementation.Resources->_CreateTexture(r2_material);
        t_material->surface_set(0, handle);
    }

    // Build noise table
    if (true)
    {
        static const int sampleSize = 4;
        u32 tempData[TEX_jitter_count][TEX_jitter * TEX_jitter];

        // Surfaces
        for (u32 it1 = 0; it1 < TEX_jitter_count - 1; it1++)
        {
            string_path name;
            xr_sprintf(name, "%s%d", r2_jitter, it1);

            MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
            desc->setTextureType(MTL::TextureType2D);
            desc->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
            desc->setWidth(TEX_jitter);
            desc->setHeight(TEX_jitter);
            desc->setStorageMode(MTL::StorageModeShared);
            desc->setUsage(MTL::TextureUsageShaderRead);

            MTL::Texture* tex = device->newTexture(desc);
            desc->release();

            u32 handle = register_mtl_texture(tex);
            tex->release();
            t_noise_surf[it1] = handle;
            t_noise[it1] = RImplementation.Resources->_CreateTexture(name);
            t_noise[it1]->surface_set(0, handle);
        }

        // Fill it,
        static const u32 Pitch = TEX_jitter * sampleSize;
        for (u32 y = 0; y < TEX_jitter; y++)
        {
            for (u32 x = 0; x < TEX_jitter; x++)
            {
                u32 data[TEX_jitter_count - 1];
                generate_jitter(data, TEX_jitter_count - 1);
                for (u32 it2 = 0; it2 < TEX_jitter_count - 1; it2++)
                {
                    u32* p = (u32*)((u8*)(tempData[it2]) + y * Pitch + x * 4);
                    *p = data[it2];
                }
            }
        }
        for (u32 it3 = 0; it3 < TEX_jitter_count - 1; it3++)
        {
            auto* mtlTex = lookup_mtl_texture(t_noise_surf[it3]);
            if (mtlTex)
            {
                MTL::Region region = MTL::Region(0, 0,
                    static_cast<NS::UInteger>(TEX_jitter),
                    static_cast<NS::UInteger>(TEX_jitter));
                mtlTex->replaceRegion(region, 0, tempData[it3], Pitch);
            }
        }

        float tempDataHBAO[TEX_jitter * TEX_jitter * 4];

        // generate HBAO jitter texture (last)
        int it = TEX_jitter_count - 1;
        string_path name;
        xr_sprintf(name, "%s%d", r2_jitter, it);

        {
            MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
            desc->setTextureType(MTL::TextureType2D);
            desc->setPixelFormat(MTL::PixelFormatRGBA32Float);
            desc->setWidth(TEX_jitter);
            desc->setHeight(TEX_jitter);
            desc->setStorageMode(MTL::StorageModeShared);
            desc->setUsage(MTL::TextureUsageShaderRead);

            MTL::Texture* tex = device->newTexture(desc);
            desc->release();

            u32 handle = register_mtl_texture(tex);
            tex->release();
            t_noise_surf[it] = handle;
            t_noise[it] = RImplementation.Resources->_CreateTexture(name);
            t_noise[it]->surface_set(0, handle);
        }

        // Fill it,
        static const int HBAOPitch = TEX_jitter * sampleSize * static_cast<int>(sizeof(float));
        for (u32 y = 0; y < TEX_jitter; y++)
        {
            for (u32 x = 0; x < TEX_jitter; x++)
            {
                float numDir = 1.0f;
                switch (ps_r_ssao)
                {
                case 1: numDir = 4.0f; break;
                case 2: numDir = 6.0f; break;
                case 3: numDir = 8.0f; break;
                }
                float angle = 2 * PI * Random.randF(0.0f, 1.0f) / numDir;
                float dist = Random.randF(0.0f, 1.0f);

                float* p =
                    (float*)((u8*)(tempDataHBAO)+y * HBAOPitch + x * 4 * sizeof(float));
                *p = (float)_cos(angle);
                *(p + 1) = (float)_sin(angle);
                *(p + 2) = (float)dist;
                *(p + 3) = 0;
            }
        }

        {
            auto* mtlTex = lookup_mtl_texture(t_noise_surf[it]);
            if (mtlTex)
            {
                MTL::Region region = MTL::Region(0, 0,
                    static_cast<NS::UInteger>(TEX_jitter),
                    static_cast<NS::UInteger>(TEX_jitter));
                mtlTex->replaceRegion(region, 0, tempDataHBAO, HBAOPitch);
            }
        }

        // Create noise mipped
        {
            MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
            desc->setTextureType(MTL::TextureType2D);
            desc->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
            desc->setWidth(TEX_jitter);
            desc->setHeight(TEX_jitter);
            desc->setMipmapLevelCount(7);
            desc->setStorageMode(MTL::StorageModeShared);
            desc->setUsage(MTL::TextureUsageShaderRead);

            MTL::Texture* tex = device->newTexture(desc);
            desc->release();

            // Upload base level
            MTL::Region region = MTL::Region(0, 0,
                static_cast<NS::UInteger>(TEX_jitter),
                static_cast<NS::UInteger>(TEX_jitter));
            tex->replaceRegion(region, 0, tempData[0], Pitch);

            // Generate mipmaps via blit encoder
            auto* cmdQueue = static_cast<MTL::CommandQueue*>(HW.m_cmdQueue);
            if (cmdQueue)
            {
                MTL::CommandBuffer* cmdBuf = cmdQueue->commandBuffer();
                MTL::BlitCommandEncoder* blitEnc = cmdBuf->blitCommandEncoder();
                blitEnc->generateMipmaps(tex);
                blitEnc->endEncoding();
                cmdBuf->commit();
                cmdBuf->waitUntilCompleted();
            }

            u32 handle = register_mtl_texture(tex);
            tex->release();
            t_noise_surf_mipped = handle;
            t_noise_mipped = RImplementation.Resources->_CreateTexture(r2_jitter_mipped);
            t_noise_mipped->surface_set(0, handle);
        }
    }
}
} // namespace xray::render::RENDER_NAMESPACE
