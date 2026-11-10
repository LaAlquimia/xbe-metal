#pragma once

#include "runtime/gpu/gpu_types.hpp"
#include <memory>
#include <string>
#include <vector>

namespace xbe::gpu {

// ============================================================================
// ITexture Interface
// ============================================================================

class ITexture {
public:
    virtual ~ITexture() = default;
    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;
    virtual PixelFormat GetFormat() const = 0;
    virtual void UpdateData(const void* data, size_t bytesPerRow) = 0;
};

// ============================================================================
// IShader Interface
// ============================================================================

class IShader {
public:
    virtual ~IShader() = default;
    virtual bool IsValid() const = 0;
    virtual const std::string& GetName() const = 0;
};

// ============================================================================
// ICommandBuffer Interface
// ============================================================================

class ICommandBuffer {
public:
    virtual ~ICommandBuffer() = default;
    virtual void Commit() = 0;
    virtual void WaitUntilCompleted() = 0;
};

// ============================================================================
// IRenderDevice Interface
// ============================================================================

class IRenderDevice {
public:
    virtual ~IRenderDevice() = default;

    // Initialization & Lifecycle
    virtual bool Initialize(int width, int height, const std::string& title) = 0;
    virtual void Shutdown() = 0;
    virtual bool ShouldClose() const = 0;
    virtual void PollEvents() = 0;

    // Frame Lifecycle
    virtual bool BeginFrame() = 0;
    virtual void EndFrame() = 0;

    // Commands & Render State
    virtual void Clear(float r, float g, float b, float a) = 0;
    virtual void SetViewport(const Viewport& viewport) = 0;
    virtual void SetScissor(const ScissorRect& scissor) = 0;
    virtual void SetRenderState(const RenderStateDesc& state) = 0;
    virtual void SetFixedFunctionState(const D3D8FixedFunctionState& ffState) = 0;

    // Resource Binding
    virtual void SetVertexBuffer(const void* data, size_t size, uint32_t stride) = 0;
    virtual void SetIndexBuffer(const void* data, size_t size, IndexFormat format) = 0;
    virtual void SetTexture(uint32_t slot, std::shared_ptr<ITexture> texture) = 0;
    virtual void SetShader(std::shared_ptr<IShader> shader) = 0;

    // Drawing
    virtual void DrawPrimitives(PrimitiveTopology topology, uint32_t startVertex, uint32_t vertexCount) = 0;
    virtual void DrawIndexedPrimitives(PrimitiveTopology topology, uint32_t startIndex, uint32_t indexCount, uint32_t baseVertex = 0) = 0;

    // Factory methods
    virtual std::shared_ptr<ITexture> CreateTexture(const TextureDesc& desc, const void* initialData = nullptr) = 0;
    virtual std::shared_ptr<IShader> CreateShader(const ShaderDesc& desc) = 0;

    // Device Information
    virtual int GetWidth() const = 0;
    virtual int GetHeight() const = 0;
    virtual const std::string& GetTitle() const = 0;
    virtual const char* GetBackendName() const = 0;
};

// Factory function implemented in render_device.cpp
std::unique_ptr<IRenderDevice> CreateRenderDevice();

} // namespace xbe::gpu
