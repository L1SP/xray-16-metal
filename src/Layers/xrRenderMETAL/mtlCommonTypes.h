#pragma once

#include "xrCore/_vector3d.h"

// Metal type aliases matching the GL backend's CommonTypes.h pattern
// D3D types are provided by Common/d3d9compat.hpp (included via stdafx.h)

struct XR_GL_VIEWPORT
{
    float TopLeftX;
    float TopLeftY;
    float Width;
    float Height;
    float MinDepth;
    float MaxDepth;
};

struct D3D_VIEWPORT : XR_GL_VIEWPORT
{
    using TopLeftCoords = float;
    using Dimensions = float;
    D3D_VIEWPORT() = default;
    D3D_VIEWPORT(TopLeftCoords x, TopLeftCoords y, Dimensions w, Dimensions h, float minZ, float maxZ)
        : XR_GL_VIEWPORT{ x, y, w, h, minZ, maxZ }
    {}
};

using VertexBufferHandle = void*;   // MTL::Buffer*
using IndexBufferHandle = void*;    // MTL::Buffer*
using TextureHandle = void*;        // MTL::Texture*
using ShaderHandle = void*;         // MTL::Function*
using PipelineStateHandle = void*;  // MTL::RenderPipelineState*
using DepthStencilHandle = void*;   // MTL::DepthStencilState*
using SamplerStateHandle = void*;   // MTL::SamplerState*

using D3D_QUERY = enum XR_MTL_QUERY
{
    D3D_QUERY_EVENT,
    D3D_QUERY_OCCLUSION
};

struct D3D_DRIVER_TYPE {};
namespace xray::render::RENDER_NAMESPACE
{
class glState; // defined in mtlState.h
}
using ID3DState = xray::render::RENDER_NAMESPACE::glState;

struct ID3DBaseTexture {};
struct ID3DTexture2D : ID3DBaseTexture {};

using InputElementDesc = int;

typedef enum D3D_COMPARISON_FUNC {
    D3D_COMPARISON_NEVER = 0,
    D3D_COMPARISON_LESS = 1,
    D3D_COMPARISON_EQUAL = 2,
    D3D_COMPARISON_LESS_EQUAL = 3,
    D3D_COMPARISON_GREATER = 4,
    D3D_COMPARISON_NOT_EQUAL = 5,
    D3D_COMPARISON_GREATER_EQUAL = 6,
    D3D_COMPARISON_ALWAYS = 7
} D3D_COMPARISON_FUNC;

using VertexElement = _D3DVERTEXELEMENT9;

template <typename T, u32 N>
using FixedStorage = void;

/// All other type aliases (matching GL)
using ConstantBufferHandle = void*;
using HostBufferHandle = void*;

#define DX11_ONLY(expr) do {} while (0)

namespace xray::render::RENDER_NAMESPACE
{
// Lookup Metal shader function by u32 ID (stored in SVS/SPS::sh)
void* lookup_shader_func(u32 id);
void register_shader_func(void* func, u32& outID);
extern xr_map<u32, void*> s_shaderFuncs;
extern u32 s_nextShaderID;

// Texture handle registry (u32 → MTL::Texture*)
MTL::Texture* lookup_mtl_texture(u32 handle);
u32 register_mtl_texture(MTL::Texture* tex);
void unregister_mtl_texture(u32 handle);

// Dynamic stream buffer redirect: resolves stale SGeometry::vb handles
// (cached at load time) to the current ring buffer slot.
MTL::Buffer* resolve_stream_buffer(MTL::Buffer* handle);
} // namespace xray::render::RENDER_NAMESPACE
