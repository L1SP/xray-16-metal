#include "stdafx.h"
#include "Layers/xrRender/BufferUtils.h"

#include <FlexibleVertexFormat.h>

namespace xray::render::RENDER_NAMESPACE
{
namespace
{
static constexpr u32 kStreamRingSize = 3;

struct VertexRingState
{
    MTL::Buffer* buffers[kStreamRingSize] = {};
    MTL::Buffer* initialHandle = nullptr;
    u32 current = 0;
};

struct IndexRingState
{
    MTL::Buffer* buffers[kStreamRingSize] = {};
    MTL::Buffer* initialHandle = nullptr;
    u32 current = 0;
};

static xr_map<VertexStreamBuffer*, VertexRingState> s_vertexRings;
static xr_map<IndexStreamBuffer*, IndexRingState> s_indexRings;

// Handle redirect: maps the initial (load-time) handle to the current ring buffer slot.
// Geometry objects cache SGeometry::vb at load time; this table redirects stale handles
// to the active ring buffer at resolve time (set_Vertices).
static xr_map<MTL::Buffer*, MTL::Buffer*> s_streamRedirect;
} // anonymous namespace

// --- public redirect resolver ---
MTL::Buffer* resolve_stream_buffer(MTL::Buffer* handle)
{
    auto it = s_streamRedirect.find(handle);
    return (it != s_streamRedirect.end()) ? it->second : handle;
}
u32 GetFVFVertexSize(u32 FVF)
{
    // TODO: Implement FVF vertex size calculation
    return static_cast<u32>(::FVF::ComputeVertexSize(FVF));
}

u32 GetDeclVertexSize(const VertexElement* decl, u32 Stream)
{
    // TODO: Implement decl vertex size calculation
    return static_cast<u32>(::FVF::ComputeVertexSize(decl, Stream));
}

u32 GetDeclLength(const VertexElement* decl)
{
    // TODO: Implement decl length calculation
    return static_cast<u32>(::FVF::GetDeclLength(decl));
}

void SetVertexDeclaration(const VertexElement* dxdecl)
{
    // TODO: Implement Metal vertex declaration setup
}

void ConvertVertexDeclaration(const VertexElement* dxdecl, SDeclaration* decl)
{
    // TODO: Implement Metal vertex declaration conversion
}

void SetGLVertexPointer(SDeclaration* decl)
{
    // TODO: Implement Metal vertex pointer setup
}

VertexStagingBuffer::~VertexStagingBuffer()
{
    Destroy();
}

void VertexStagingBuffer::Create(size_t size, bool allowReadBack)
{
    // TODO: Implement Metal staging buffer creation
    m_Size = size;
    m_AllowReadBack = allowReadBack;
    m_HostBuffer = xr_alloc<u8>(size);
    AddRef();
}

bool VertexStagingBuffer::IsValid() const
{
    return !!m_DeviceBuffer;
}

void* VertexStagingBuffer::Map(size_t offset, size_t size, bool read)
{
    VERIFY2(m_HostBuffer, "Buffer wasn't created or already discarded");
    VERIFY2(!read || m_AllowReadBack, "Can't read from write only buffer");
    VERIFY2((size + offset) <= m_Size, "Map region is too large");
    return static_cast<u8*>(m_HostBuffer) + offset;
}

void VertexStagingBuffer::Unmap(bool doFlush)
{
    if (!doFlush)
        return;
    // Create Metal buffer and upload host data
    auto* device = static_cast<MTL::Device*>(HW.m_device);
    if (device && m_HostBuffer)
    {
        auto* buffer = device->newBuffer(m_HostBuffer, m_Size, MTL::ResourceStorageModeShared);
        if (m_DeviceBuffer)
            static_cast<MTL::Buffer*>(m_DeviceBuffer)->release();
        m_DeviceBuffer = buffer;
    }
    if (!m_AllowReadBack)
        DiscardHostBuffer();
}

VertexBufferHandle VertexStagingBuffer::GetBufferHandle() const
{
    return m_DeviceBuffer;
}

void VertexStagingBuffer::Destroy()
{
    DiscardHostBuffer();
    m_Size = 0;
    if (m_DeviceBuffer)
    {
        static_cast<MTL::Buffer*>(m_DeviceBuffer)->release();
        m_DeviceBuffer = nullptr;
    }
}

void VertexStagingBuffer::DiscardHostBuffer()
{
    if (m_HostBuffer)
        xr_free(m_HostBuffer);
}

size_t VertexStagingBuffer::GetSystemMemoryUsage() const
{
    return m_HostBuffer ? m_Size : 0;
}

size_t VertexStagingBuffer::GetVideoMemoryUsage() const
{
    // TODO: Query Metal buffer size
    return 0;
}

IndexStagingBuffer::~IndexStagingBuffer()
{
    Destroy();
}

void IndexStagingBuffer::Create(size_t size, bool allowReadBack, bool managed)
{
    m_Size = size;
    m_AllowReadBack = allowReadBack;
    m_HostBuffer = xr_alloc<u8>(size);
    AddRef();
}

bool IndexStagingBuffer::IsValid() const
{
    return !!m_DeviceBuffer;
}

void* IndexStagingBuffer::Map(size_t offset, size_t size, bool read)
{
    VERIFY2(m_HostBuffer, "Buffer wasn't created or already discarded");
    VERIFY2(!read || m_AllowReadBack, "Can't read from write only buffer");
    VERIFY2((size + offset) <= m_Size, "Map region is too large");
    return static_cast<u8*>(m_HostBuffer) + offset;
}

void IndexStagingBuffer::Unmap(bool doFlush)
{
    if (!doFlush)
        return;
    // Create Metal buffer and upload host data
    auto* device = static_cast<MTL::Device*>(HW.m_device);
    if (device && m_HostBuffer)
    {
        auto* buffer = device->newBuffer(m_HostBuffer, m_Size, MTL::ResourceStorageModeShared);
        if (m_DeviceBuffer)
            static_cast<MTL::Buffer*>(m_DeviceBuffer)->release();
        m_DeviceBuffer = buffer;
    }
    if (!m_AllowReadBack)
        DiscardHostBuffer();
}

IndexBufferHandle IndexStagingBuffer::GetBufferHandle() const
{
    return m_DeviceBuffer;
}

void IndexStagingBuffer::Destroy()
{
    DiscardHostBuffer();
    m_Size = 0;
    if (m_DeviceBuffer)
    {
        static_cast<MTL::Buffer*>(m_DeviceBuffer)->release();
        m_DeviceBuffer = nullptr;
    }
}

void IndexStagingBuffer::DiscardHostBuffer()
{
    if (m_HostBuffer)
        xr_free(m_HostBuffer);
}

size_t IndexStagingBuffer::GetSystemMemoryUsage() const
{
    return m_HostBuffer ? m_Size : 0;
}

size_t IndexStagingBuffer::GetVideoMemoryUsage() const
{
    // TODO: Query Metal buffer size
    return 0;
}

VertexStreamBuffer::~VertexStreamBuffer()
{
    Destroy();
}

void VertexStreamBuffer::Create(size_t size)
{
    auto* device = static_cast<MTL::Device*>(HW.m_device);
    if (device)
    {
        auto& ring = s_vertexRings[this];
        for (u32 i = 0; i < kStreamRingSize; i++)
        {
            ring.buffers[i] = device->newBuffer(size, MTL::ResourceStorageModeShared);
        }
        ring.current = 0;
        ring.initialHandle = ring.buffers[0];
        m_DeviceBuffer = ring.buffers[0];
        s_streamRedirect[ring.initialHandle] = ring.buffers[0];
    }
    AddRef();
}

void VertexStreamBuffer::Destroy()
{
    auto it = s_vertexRings.find(this);
    if (it != s_vertexRings.end())
    {
        for (u32 i = 0; i < kStreamRingSize; i++)
        {
            if (it->second.buffers[i])
            {
                it->second.buffers[i]->release();
                it->second.buffers[i] = nullptr;
            }
        }
        s_vertexRings.erase(it);
    }
    m_DeviceBuffer = nullptr;
}

void* VertexStreamBuffer::Map(size_t offset, size_t size, bool flush)
{
    if (flush)
    {
        auto it = s_vertexRings.find(this);
        if (it != s_vertexRings.end())
        {
            it->second.current = (it->second.current + 1) % kStreamRingSize;
            m_DeviceBuffer = it->second.buffers[it->second.current];
            // Redirect all geometries with the initial handle to the current buffer
            s_streamRedirect[it->second.initialHandle] = static_cast<MTL::Buffer*>(m_DeviceBuffer);
        }
    }
    auto* buffer = static_cast<MTL::Buffer*>(m_DeviceBuffer);
    if (!buffer)
        return nullptr;
    return static_cast<u8*>(buffer->contents()) + offset;
}

void VertexStreamBuffer::Unmap()
{
}

bool VertexStreamBuffer::IsValid() const
{
    return !!m_DeviceBuffer;
}

IndexStreamBuffer::~IndexStreamBuffer()
{
    Destroy();
}

void IndexStreamBuffer::Create(size_t size)
{
    auto* device = static_cast<MTL::Device*>(HW.m_device);
    if (device)
    {
        auto& ring = s_indexRings[this];
        for (u32 i = 0; i < kStreamRingSize; i++)
        {
            ring.buffers[i] = device->newBuffer(size, MTL::ResourceStorageModeShared);
        }
        ring.current = 0;
        ring.initialHandle = ring.buffers[0];
        m_DeviceBuffer = ring.buffers[0];
        s_streamRedirect[ring.initialHandle] = ring.buffers[0];
    }
    AddRef();
}

void IndexStreamBuffer::Destroy()
{
    auto it = s_indexRings.find(this);
    if (it != s_indexRings.end())
    {
        for (u32 i = 0; i < kStreamRingSize; i++)
        {
            if (it->second.buffers[i])
            {
                it->second.buffers[i]->release();
                it->second.buffers[i] = nullptr;
            }
        }
        s_indexRings.erase(it);
    }
    m_DeviceBuffer = nullptr;
}

void* IndexStreamBuffer::Map(size_t offset, size_t size, bool flush)
{
    if (flush)
    {
        auto it = s_indexRings.find(this);
        if (it != s_indexRings.end())
        {
            it->second.current = (it->second.current + 1) % kStreamRingSize;
            m_DeviceBuffer = it->second.buffers[it->second.current];
            s_streamRedirect[it->second.initialHandle] = static_cast<MTL::Buffer*>(m_DeviceBuffer);
        }
    }
    auto* buffer = static_cast<MTL::Buffer*>(m_DeviceBuffer);
    if (!buffer)
        return nullptr;
    return static_cast<u8*>(buffer->contents()) + offset;
}

void IndexStreamBuffer::Unmap()
{
}

bool IndexStreamBuffer::IsValid() const
{
    return !!m_DeviceBuffer;
}
} // namespace xray::render::RENDER_NAMESPACE
