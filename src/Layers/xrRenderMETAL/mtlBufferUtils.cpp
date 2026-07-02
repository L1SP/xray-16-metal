#include "stdafx.h"
#include "Layers/xrRender/BufferUtils.h"

#include <FlexibleVertexFormat.h>

namespace xray::render::RENDER_NAMESPACE
{
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
        auto* buffer = device->newBuffer(size, MTL::ResourceStorageModeShared);
        m_DeviceBuffer = buffer;
    }
    AddRef();
}

void VertexStreamBuffer::Destroy()
{
    if (m_DeviceBuffer)
    {
        auto* buffer = static_cast<MTL::Buffer*>(m_DeviceBuffer);
        buffer->release();
        m_DeviceBuffer = nullptr;
    }
}

void* VertexStreamBuffer::Map(size_t offset, size_t size, bool flush)
{
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
        auto* buffer = device->newBuffer(size, MTL::ResourceStorageModeShared);
        m_DeviceBuffer = buffer;
    }
    AddRef();
}

void IndexStreamBuffer::Destroy()
{
    if (m_DeviceBuffer)
    {
        auto* buffer = static_cast<MTL::Buffer*>(m_DeviceBuffer);
        buffer->release();
        m_DeviceBuffer = nullptr;
    }
}

void* IndexStreamBuffer::Map(size_t offset, size_t size, bool flush)
{
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
