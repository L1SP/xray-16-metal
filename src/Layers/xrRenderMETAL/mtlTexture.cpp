#include "stdafx.h"

#include <gli/gli.hpp>

namespace xray::render::RENDER_NAMESPACE
{
void fix_texture_name(pstr fn)
{
    pstr _ext = strext(fn);
    if (_ext &&
        (0 == xr_stricmp(_ext, ".tga") ||
            0 == xr_stricmp(_ext, ".dds") ||
            0 == xr_stricmp(_ext, ".bmp") ||
            0 == xr_stricmp(_ext, ".ogm")))
        *_ext = 0;
}

int get_texture_load_lod(LPCSTR fn)
{
    return 0;
}

u32 calc_texture_size(int lod, u32 mip_cnt, size_t orig_size)
{
    return orig_size;
}

static MTL::PixelFormat gli_format_to_mtl(gli::format format)
{
    switch (format)
    {
    // BC/DXT compressed (all Apple Silicon)
    case gli::FORMAT_RGB_DXT1_UNORM_BLOCK8:     return MTL::PixelFormatBC1_RGBA;
    case gli::FORMAT_RGB_DXT1_SRGB_BLOCK8:    return MTL::PixelFormatBC1_RGBA_sRGB;
    case gli::FORMAT_RGBA_DXT1_UNORM_BLOCK8:   return MTL::PixelFormatBC1_RGBA;
    case gli::FORMAT_RGBA_DXT1_SRGB_BLOCK8:  return MTL::PixelFormatBC1_RGBA_sRGB;
    case gli::FORMAT_RGBA_DXT3_UNORM_BLOCK16:  return MTL::PixelFormatBC2_RGBA;
    case gli::FORMAT_RGBA_DXT3_SRGB_BLOCK16: return MTL::PixelFormatBC2_RGBA_sRGB;
    case gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16:  return MTL::PixelFormatBC3_RGBA;
    case gli::FORMAT_RGBA_DXT5_SRGB_BLOCK16: return MTL::PixelFormatBC3_RGBA_sRGB;
    case gli::FORMAT_R_ATI1N_UNORM_BLOCK8:     return MTL::PixelFormatBC4_RUnorm;
    case gli::FORMAT_R_ATI1N_SNORM_BLOCK8:     return MTL::PixelFormatBC4_RUnorm;
    case gli::FORMAT_RG_ATI2N_UNORM_BLOCK16:   return MTL::PixelFormatBC5_RGUnorm;
    case gli::FORMAT_RG_ATI2N_SNORM_BLOCK16:   return MTL::PixelFormatBC5_RGUnorm;
    // BPTC (BC6H/BC7)
    case gli::FORMAT_RGB_BP_UFLOAT_BLOCK16:    return MTL::PixelFormatBC6H_RGBFloat;
    case gli::FORMAT_RGB_BP_SFLOAT_BLOCK16:    return MTL::PixelFormatBC6H_RGBFloat;
    case gli::FORMAT_RGBA_BP_UNORM_BLOCK16:    return MTL::PixelFormatBC7_RGBAUnorm;
    case gli::FORMAT_RGBA_BP_SRGB_BLOCK16:    return MTL::PixelFormatBC7_RGBAUnorm_sRGB;
    // ETC2 (Apple Silicon supports via decompression fallback)
    // ASTC (native on Apple Silicon)
    case gli::FORMAT_RGBA_ASTC_4X4_UNORM_BLOCK16:   return MTL::PixelFormatASTC_4x4_LDR;
    case gli::FORMAT_RGBA_ASTC_4X4_SRGB_BLOCK16:  return MTL::PixelFormatASTC_4x4_sRGB;
    case gli::FORMAT_RGBA_ASTC_5X4_UNORM_BLOCK16:   return MTL::PixelFormatASTC_5x4_LDR;
    case gli::FORMAT_RGBA_ASTC_5X4_SRGB_BLOCK16:  return MTL::PixelFormatASTC_5x4_sRGB;
    case gli::FORMAT_RGBA_ASTC_5X5_UNORM_BLOCK16:   return MTL::PixelFormatASTC_5x5_LDR;
    case gli::FORMAT_RGBA_ASTC_5X5_SRGB_BLOCK16:  return MTL::PixelFormatASTC_5x5_sRGB;
    case gli::FORMAT_RGBA_ASTC_6X5_UNORM_BLOCK16:   return MTL::PixelFormatASTC_6x5_LDR;
    case gli::FORMAT_RGBA_ASTC_6X5_SRGB_BLOCK16:  return MTL::PixelFormatASTC_6x5_sRGB;
    case gli::FORMAT_RGBA_ASTC_6X6_UNORM_BLOCK16:   return MTL::PixelFormatASTC_6x6_LDR;
    case gli::FORMAT_RGBA_ASTC_6X6_SRGB_BLOCK16:  return MTL::PixelFormatASTC_6x6_sRGB;
    case gli::FORMAT_RGBA_ASTC_8X5_UNORM_BLOCK16:   return MTL::PixelFormatASTC_8x5_LDR;
    case gli::FORMAT_RGBA_ASTC_8X5_SRGB_BLOCK16:  return MTL::PixelFormatASTC_8x5_sRGB;
    case gli::FORMAT_RGBA_ASTC_8X6_UNORM_BLOCK16:   return MTL::PixelFormatASTC_8x6_LDR;
    case gli::FORMAT_RGBA_ASTC_8X6_SRGB_BLOCK16:  return MTL::PixelFormatASTC_8x6_sRGB;
    case gli::FORMAT_RGBA_ASTC_8X8_UNORM_BLOCK16:   return MTL::PixelFormatASTC_8x8_LDR;
    case gli::FORMAT_RGBA_ASTC_8X8_SRGB_BLOCK16:  return MTL::PixelFormatASTC_8x8_sRGB;
    case gli::FORMAT_RGBA_ASTC_10X5_UNORM_BLOCK16:  return MTL::PixelFormatASTC_10x5_LDR;
    case gli::FORMAT_RGBA_ASTC_10X5_SRGB_BLOCK16: return MTL::PixelFormatASTC_10x5_sRGB;
    case gli::FORMAT_RGBA_ASTC_10X6_UNORM_BLOCK16:  return MTL::PixelFormatASTC_10x6_LDR;
    case gli::FORMAT_RGBA_ASTC_10X6_SRGB_BLOCK16: return MTL::PixelFormatASTC_10x6_sRGB;
    case gli::FORMAT_RGBA_ASTC_10X8_UNORM_BLOCK16:  return MTL::PixelFormatASTC_10x8_LDR;
    case gli::FORMAT_RGBA_ASTC_10X8_SRGB_BLOCK16: return MTL::PixelFormatASTC_10x8_sRGB;
    case gli::FORMAT_RGBA_ASTC_10X10_UNORM_BLOCK16: return MTL::PixelFormatASTC_10x10_LDR;
    case gli::FORMAT_RGBA_ASTC_10X10_SRGB_BLOCK16:return MTL::PixelFormatASTC_10x10_sRGB;
    case gli::FORMAT_RGBA_ASTC_12X10_UNORM_BLOCK16: return MTL::PixelFormatASTC_12x10_LDR;
    case gli::FORMAT_RGBA_ASTC_12X10_SRGB_BLOCK16:return MTL::PixelFormatASTC_12x10_sRGB;
    case gli::FORMAT_RGBA_ASTC_12X12_UNORM_BLOCK16: return MTL::PixelFormatASTC_12x12_LDR;
    case gli::FORMAT_RGBA_ASTC_12X12_SRGB_BLOCK16:return MTL::PixelFormatASTC_12x12_sRGB;
    // Uncompressed 8-bit
    case gli::FORMAT_RGBA8_UNORM_PACK8:         return MTL::PixelFormatRGBA8Unorm;
    case gli::FORMAT_RGBA8_SRGB_PACK8:         return MTL::PixelFormatRGBA8Unorm_sRGB;
    case gli::FORMAT_BGRA8_UNORM_PACK8:         return MTL::PixelFormatBGRA8Unorm;
    case gli::FORMAT_BGRA8_SRGB_PACK8:         return MTL::PixelFormatBGRA8Unorm_sRGB;
    case gli::FORMAT_R8_UNORM_PACK8:            return MTL::PixelFormatR8Unorm;
    case gli::FORMAT_RG8_UNORM_PACK8:           return MTL::PixelFormatRG8Unorm;
    case gli::FORMAT_A8_UNORM_PACK8:            return MTL::PixelFormatR8Unorm;  // A8 → R8
    case gli::FORMAT_L8_UNORM_PACK8:            return MTL::PixelFormatR8Unorm;  // L8 → R8
    case gli::FORMAT_LA8_UNORM_PACK8:           return MTL::PixelFormatRG8Unorm; // LA8 → RG8
    // Uncompressed float
    case gli::FORMAT_R16_SFLOAT_PACK16:         return MTL::PixelFormatR16Float;
    case gli::FORMAT_RG16_SFLOAT_PACK16:        return MTL::PixelFormatRG16Float;
    case gli::FORMAT_RGB16_SFLOAT_PACK16:       return MTL::PixelFormatRGBA16Float;
    case gli::FORMAT_RGBA16_SFLOAT_PACK16:      return MTL::PixelFormatRGBA16Float;
    case gli::FORMAT_R32_SFLOAT_PACK32:         return MTL::PixelFormatR32Float;
    case gli::FORMAT_RG32_SFLOAT_PACK32:        return MTL::PixelFormatRG32Float;
    case gli::FORMAT_RGB32_SFLOAT_PACK32:       return MTL::PixelFormatRGBA32Float;
    case gli::FORMAT_RGBA32_SFLOAT_PACK32:      return MTL::PixelFormatRGBA32Float;
    // Uncompressed unorm packed
    case gli::FORMAT_RGBA4_UNORM_PACK16:        return MTL::PixelFormatRGBA8Unorm;
    case gli::FORMAT_BGRA4_UNORM_PACK16:        return MTL::PixelFormatBGRA8Unorm;
    case gli::FORMAT_R5G6B5_UNORM_PACK16:       return MTL::PixelFormatRGBA8Unorm;
    case gli::FORMAT_B5G6R5_UNORM_PACK16:       return MTL::PixelFormatRGBA8Unorm;
    case gli::FORMAT_RGB5A1_UNORM_PACK16:       return MTL::PixelFormatRGBA8Unorm;
    case gli::FORMAT_BGR5A1_UNORM_PACK16:       return MTL::PixelFormatRGBA8Unorm;
    // Uncompressed unorm 10:10:10:2
    case gli::FORMAT_RGB10A2_UNORM_PACK32:      return MTL::PixelFormatRGB10A2Unorm;
    case gli::FORMAT_BGR10A2_UNORM_PACK32:      return MTL::PixelFormatRGB10A2Unorm;
    default:                                    return MTL::PixelFormatInvalid;
    }
}

u32 CRender::texture_load(pcstr fRName, u32& ret_msize, int& ret_desc)
{
    ret_msize = 0;
    ret_desc = 0;
    if (!fRName || !fRName[0])
        return 0;

    string_path fn;
    string_path fname;
    xr_strcpy(fname, fRName);
    fix_texture_name(fname);

    bool found = false;
    for (cpcstr folder : { "$level$", "$game_saves$", "$game_textures$" })
    {
        found = FS.exist(fn, folder, fname, ".dds");
        if (found)
            break;
    }
    if (!found)
    {
#ifdef DEBUG
        Msg("! texture_load: '%s' not found in level/saves/textures, trying fallback", fRName);
#endif
        if (FS.exist(fn, "$game_textures$", "ed\\ed_not_existing_texture", ".dds"))
            found = true;
    }
    if (!found)
    {
        Msg("! texture_load: '%s' NOT FOUND (no fallback either)", fRName);
        return 0;
    }

    IReader* S = FS.r_open(fn);
    if (!S)
        return 0;

    size_t img_size = S->length();
    if (img_size == 0 || !S->pointer())
    {
        FS.r_close(S);
        return 0;
    }
    gli::texture texture = gli::load((char*)S->pointer(), img_size);
    FS.r_close(S);

    if (texture.empty())
        return 0;

    auto* device = static_cast<MTL::Device*>(HW.m_device);
    if (!device)
        return 0;

    MTL::PixelFormat mtlFormat = gli_format_to_mtl(texture.format());
    if (mtlFormat == MTL::PixelFormatInvalid)
    {
        Msg("! Unsupported gli texture format %d for '%s'", (int)texture.format(), fn);
        return 0;
    }

    // Expand unsupported formats to RGBA8:
    //   A8/L8/LA8 → RGBA8 (Metal R8/RG8 don't replicate channels like GL)
    //   RGB8 → RGBA8 (Metal has no RGB8 format)
    {
        gli::format fmt = texture.format();
        bool needsExpand = fmt == gli::FORMAT_A8_UNORM_PACK8 || fmt == gli::FORMAT_L8_UNORM_PACK8 || fmt == gli::FORMAT_LA8_UNORM_PACK8 || fmt == gli::FORMAT_RGB8_UNORM_PACK8 || fmt == gli::FORMAT_RGB8_SRGB_PACK8;
        if (needsExpand)
        {
            gli::texture expanded(texture.target(), gli::FORMAT_RGBA8_UNORM_PACK8, texture.extent(), texture.layers(), texture.faces(), texture.levels());
            for (size_t level = 0; level < texture.levels(); ++level)
            {
                auto ext = texture.extent(level);
                const uint8_t* src = static_cast<const uint8_t*>(texture.data(0, 0, level));
                uint8_t* dst = static_cast<uint8_t*>(expanded.data(0, 0, level));
                size_t count = static_cast<size_t>(ext.x) * static_cast<size_t>(ext.y);
                if (fmt == gli::FORMAT_A8_UNORM_PACK8)
                {
                    // A8→RGBA8: (255,255,255,A)
                    for (size_t i = 0; i < count; ++i)
                    {
                        dst[i * 4 + 0] = 255;
                        dst[i * 4 + 1] = 255;
                        dst[i * 4 + 2] = 255;
                        dst[i * 4 + 3] = src[i];
                    }
                }
                else if (fmt == gli::FORMAT_L8_UNORM_PACK8)
                {
                    // L8→RGBA8: (L,L,L,255)
                    for (size_t i = 0; i < count; ++i)
                    {
                        dst[i * 4 + 0] = src[i];
                        dst[i * 4 + 1] = src[i];
                        dst[i * 4 + 2] = src[i];
                        dst[i * 4 + 3] = 255;
                    }
                }
                else if (fmt == gli::FORMAT_LA8_UNORM_PACK8)
                {
                    // LA8→RGBA8: (L,L,L,A)
                    for (size_t i = 0; i < count; ++i)
                    {
                        dst[i * 4 + 0] = src[i * 2 + 0];
                        dst[i * 4 + 1] = src[i * 2 + 0];
                        dst[i * 4 + 2] = src[i * 2 + 0];
                        dst[i * 4 + 3] = src[i * 2 + 1];
                    }
                }
                else // RGB8_UNORM_PACK8 or RGB8_SRGB_PACK8 → RGBA8: (R,G,B,255)
                {
                    for (size_t i = 0; i < count; ++i)
                    {
                        dst[i * 4 + 0] = src[i * 3 + 0];
                        dst[i * 4 + 1] = src[i * 3 + 1];
                        dst[i * 4 + 2] = src[i * 3 + 2];
                        dst[i * 4 + 3] = 255;
                    }
                }
            }
            texture = std::move(expanded);
            mtlFormat = fmt == gli::FORMAT_RGB8_SRGB_PACK8 ? MTL::PixelFormatRGBA8Unorm_sRGB : MTL::PixelFormatRGBA8Unorm;
        }
    }

    NS::UInteger levels = static_cast<NS::UInteger>(texture.levels());
    auto extent = texture.extent();

    MTL::TextureDescriptor* tdesc = MTL::TextureDescriptor::alloc()->init();
    tdesc->setPixelFormat(mtlFormat);
    tdesc->setWidth(static_cast<NS::UInteger>(extent.x));
    tdesc->setHeight(static_cast<NS::UInteger>(extent.y));
    tdesc->setMipmapLevelCount(levels);
    tdesc->setStorageMode(MTL::StorageModeShared);
    tdesc->setUsage(MTL::TextureUsageShaderRead);

    // NOTE: BGRA8Unorm does NOT need a swizzle — Metal auto-converts BGRA→RGBA
    // on read (sampler.r = memory byte 2 = actual R, sampler.g = byte 1 = actual G).
    // A BGRA→RGBA swizzle here would INVERT R↔B (red←Blue, blue←Red).

    bool isCube = gli::is_target_cube(texture.target());
    bool is3D = texture.target() == gli::TARGET_3D;
    bool isArray = texture.target() == gli::TARGET_CUBE_ARRAY;

    NS::UInteger depth = 1;

    if (isCube)
    {
        tdesc->setTextureType(isArray ? MTL::TextureTypeCubeArray : MTL::TextureTypeCube);
        if (isArray)
        {
            // For cube arrays: arrayLength = number of cubes = texture.layers()
            tdesc->setArrayLength(static_cast<NS::UInteger>(texture.layers()));
        }
        // else: single cube, arrayLength defaults to 1 — don't set it
    }
    else if (is3D)
    {
        tdesc->setTextureType(MTL::TextureType3D);
        depth = static_cast<NS::UInteger>(texture.extent().z);
        tdesc->setDepth(depth);
    }
    else
    {
        tdesc->setTextureType(MTL::TextureType2D);
        if (texture.layers() > 1)
        {
            tdesc->setTextureType(MTL::TextureType2DArray);
            tdesc->setArrayLength(static_cast<NS::UInteger>(texture.layers()));
        }
    }

    MTL::Texture* mtlTex = device->newTexture(tdesc);
    tdesc->release();
    if (!mtlTex)
    {
        Msg("! Failed to create Metal texture for '%s' (fmt=%d %dx%d)", fn, (int)mtlFormat, extent.x, extent.y);
        return 0;
    }

    // Upload each layer/face/mip
    for (size_t layer = 0; layer < texture.layers(); ++layer)
    {
        for (size_t face = 0; face < texture.faces(); ++face)
        {
            for (size_t level = 0; level < texture.levels(); ++level)
            {
                auto levelExtent = texture.extent(level);
                size_t srcSize = texture.size(level);
                const void* srcData = texture.data(layer, face, level);
                if (!srcData || srcSize == 0)
                    continue;

                NS::UInteger slice = isCube
                    ? static_cast<NS::UInteger>(layer * texture.faces() + face)
                    : static_cast<NS::UInteger>(layer);

                MTL::Region region;
                if (is3D)
                    region = MTL::Region(0, 0, 0,
                        static_cast<NS::UInteger>(levelExtent.x),
                        static_cast<NS::UInteger>(levelExtent.y),
                        static_cast<NS::UInteger>(levelExtent.z));
                else
                    region = MTL::Region(0, 0,
                        static_cast<NS::UInteger>(levelExtent.x),
                        static_cast<NS::UInteger>(levelExtent.y));

                int w = static_cast<int>(levelExtent.x);
                int h = static_cast<int>(levelExtent.y);
                int bs = static_cast<int>(gli::block_size(texture.format()));
                auto blockDim = gli::block_extent(texture.format());
                int bdx = static_cast<int>(blockDim.x);
                int bdy = static_cast<int>(blockDim.y);

                NS::UInteger bytesPerRow;
                NS::UInteger bytesPerImage = 0;
                if (gli::is_compressed(texture.format()))
                {
                    int numBlocksX = (w + bdx - 1) / bdx;
                    int numBlocksY = (h + bdy - 1) / bdy;
                    bytesPerRow = static_cast<NS::UInteger>(numBlocksX) * static_cast<NS::UInteger>(bs);
                    if (is3D)
                        bytesPerImage = bytesPerRow * static_cast<NS::UInteger>(numBlocksY);
                }
                else
                {
                    bytesPerRow = static_cast<NS::UInteger>(w) * static_cast<NS::UInteger>(bs) / static_cast<NS::UInteger>(bdx > 0 ? bdx : 1);
                }

                // Compute per-slice data size (texture.size(level) includes all layers*faces)
                size_t sliceCount = texture.layers() * texture.faces();
                size_t sliceSize = sliceCount > 0 ? srcSize / sliceCount : srcSize;
                size_t numBlockRows = static_cast<size_t>((h + bdy - 1) / bdy);
                size_t expectedSize = static_cast<size_t>(bytesPerRow) * numBlockRows;
                // Fallback: if our calculation doesn't match the actual per-slice size,
                // derive bytesPerRow from the slice data to guarantee correct stride
                if (expectedSize != sliceSize && sliceSize > 0 && numBlockRows > 0)
                {
                    bytesPerRow = static_cast<NS::UInteger>(sliceSize / numBlockRows);
                    if (is3D)
                        bytesPerImage = bytesPerRow * static_cast<NS::UInteger>(numBlockRows);
                }

                mtlTex->replaceRegion(region, static_cast<NS::UInteger>(level), slice, srcData, bytesPerRow, bytesPerImage);
            }
        }
    }


    ret_desc = isCube ? 1 : (is3D ? 2 : 0);
    ret_msize = calc_texture_size(0, (u32)levels, img_size);

    u32 handle = register_mtl_texture(mtlTex);
    return handle;
}
} // namespace xray::render::RENDER_NAMESPACE
