#pragma once

#ifdef __APPLE__

#include "runtime/gpu/render_device.hpp"
#include <memory>
#include <string>

namespace xbe::gpu {

class MetalRenderDevice final : public IRenderDevice {
public:
    MetalRenderDevice();
    ~MetalRenderDevice() override;

    bool Initialize(int width, int height, const std::string& title) override;
    void Shutdown() override;
    bool ShouldClose() const override;
    void PollEvents() override;

    bool BeginFrame() override;
    void EndFrame() override;

    void Clear(float r, float g, float b, float a) override;
    void SetViewport(const Viewport& viewport) override;
    void SetScissor(const ScissorRect& scissor) override;
    void SetRenderState(const RenderStateDesc& state) override;
    void SetFixedFunctionState(const D3D8FixedFunctionState& ffState) override;

    void SetVertexBuffer(const void* data, size_t size, uint32_t stride) override;
    void SetIndexBuffer(const void* data, size_t size, IndexFormat format) override;
    void SetTexture(uint32_t slot, std::shared_ptr<ITexture> texture) override;
    void SetShader(std::shared_ptr<IShader> shader) override;

    void DrawPrimitives(PrimitiveTopology topology, uint32_t startVertex, uint32_t vertexCount) override;
    void DrawIndexedPrimitives(PrimitiveTopology topology, uint32_t startIndex, uint32_t indexCount, uint32_t baseVertex = 0) override;

    std::shared_ptr<ITexture> CreateTexture(const TextureDesc& desc, const void* initialData = nullptr) override;
    std::shared_ptr<IShader> CreateShader(const ShaderDesc& desc) override;

    int GetWidth() const override;
    int GetHeight() const override;
    const std::string& GetTitle() const override;
    const char* GetBackendName() const override { return "Apple Metal (Apple Silicon)"; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace xbe::gpu

#endif // __APPLE__
