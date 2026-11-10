#include "runtime/gpu/webgpu/webgpu_device.hpp"
#include "runtime/gpu/gpu_types.hpp"

#include <iostream>
#include <vector>
#include <cstring>

#if defined(XBE_TARGET_WASM) || defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/html5.h>

// JavaScript Bridge functions for WebGPU / HTML5 Canvas rendering
EM_JS(int, js_webgpu_init, (const char* canvasId, int width, int height), {
    const selector = UTF8ToString(canvasId);
    let canvas = document.querySelector(selector);
    if (!canvas) {
        canvas = document.getElementById("canvas");
    }
    if (!canvas) {
        console.error("[WebGPU] Target canvas not found: " + selector);
        return 0;
    }

    canvas.width = width;
    canvas.height = height;

    window._xbe_gpu = {
        canvas: canvas,
        ctx2d: null,
        gpuDevice: null,
        gpuContext: null,
        pipeline: null,
        vertexBuffer: null,
        indexBuffer: null,
        uniformBuffer: null,
        vertices: null,
        indices: null,
        clearColor: { r: 0.05, g: 0.05, b: 0.08, a: 1.0 },
        isWebGPU: false
    };

    // Check for WebGPU support
    if (navigator.gpu) {
        console.log("[WebGPU] Browser supports WebGPU. Initializing WebGPU context...");
        navigator.gpu.requestAdapter().then(adapter => {
            if (!adapter) {
                console.warn("[WebGPU] Failed to get GPUAdapter. Falling back to 2D canvas.");
                window._xbe_gpu.ctx2d = canvas.getContext("2d");
                return;
            }
            adapter.requestDevice().then(device => {
                window._xbe_gpu.gpuDevice = device;
                window._xbe_gpu.gpuContext = canvas.getContext("webgpu");
                const format = navigator.gpu.getPreferredCanvasFormat();
                window._xbe_gpu.gpuContext.configure({
                    device: device,
                    format: format,
                    alphaMode: "premultiplied"
                });
                window._xbe_gpu.isWebGPU = true;
                console.log("[WebGPU] Hardware accelerated WebGPU context active!");
            });
        }).catch(err => {
            console.warn("[WebGPU] Error requesting device: " + err + ". Using 2D Canvas fallback.");
            window._xbe_gpu.ctx2d = canvas.getContext("2d");
        });
    } else {
        console.log("[WebGPU] WebGPU not supported on this browser. Initializing Canvas 2D fallback.");
        window._xbe_gpu.ctx2d = canvas.getContext("2d");
    }

    return 1;
});

EM_JS(void, js_webgpu_clear, (float r, float g, float b, float a), {
    if (!window._xbe_gpu) return;
    window._xbe_gpu.clearColor = { r: r, g: g, b: b, a: a };
    if (window._xbe_gpu.ctx2d) {
        const ctx = window._xbe_gpu.ctx2d;
        const w = window._xbe_gpu.canvas.width;
        const h = window._xbe_gpu.canvas.height;
        ctx.fillStyle = `rgba(${Math.round(r*255)}, ${Math.round(g*255)}, ${Math.round(b*255)}, ${a})`;
        ctx.fillRect(0, 0, w, h);
    }
});

EM_JS(void, js_webgpu_begin_frame, (), {
    if (!window._xbe_gpu) return;
    if (window._xbe_gpu.isWebGPU && window._xbe_gpu.gpuDevice && window._xbe_gpu.gpuContext) {
        const device = window._xbe_gpu.gpuDevice;
        const commandEncoder = device.createCommandEncoder();
        const textureView = window._xbe_gpu.gpuContext.getCurrentTexture().createView();
        const cc = window._xbe_gpu.clearColor;

        const renderPass = commandEncoder.beginRenderPass({
            colorAttachments: [{
                view: textureView,
                clearValue: { r: cc.r, g: cc.g, b: cc.b, a: cc.a },
                loadOp: 'clear',
                storeOp: 'store'
            }]
        });

        window._xbe_gpu.currentPass = renderPass;
        window._xbe_gpu.currentEncoder = commandEncoder;
    } else if (window._xbe_gpu.ctx2d) {
        const ctx = window._xbe_gpu.ctx2d;
        const cc = window._xbe_gpu.clearColor;
        ctx.fillStyle = `rgba(${Math.round(cc.r*255)}, ${Math.round(cc.g*255)}, ${Math.round(cc.b*255)}, ${cc.a})`;
        ctx.fillRect(0, 0, window._xbe_gpu.canvas.width, window._xbe_gpu.canvas.height);
    }
});

EM_JS(void, js_webgpu_end_frame, (), {
    if (!window._xbe_gpu) return;
    if (window._xbe_gpu.isWebGPU && window._xbe_gpu.currentEncoder) {
        if (window._xbe_gpu.currentPass) {
            window._xbe_gpu.currentPass.end();
            window._xbe_gpu.currentPass = null;
        }
        window._xbe_gpu.gpuDevice.queue.submit([window._xbe_gpu.currentEncoder.finish()]);
        window._xbe_gpu.currentEncoder = null;
    }
});

EM_JS(void, js_webgpu_upload_vertices, (const void* ptr, int size), {
    if (!window._xbe_gpu) return;
    const data = new Float32Array(HEAPU8.buffer, ptr, size / 4);
    window._xbe_gpu.vertices = new Float32Array(data);
});

EM_JS(void, js_webgpu_upload_indices, (const void* ptr, int count, int is32Bit), {
    if (!window._xbe_gpu) return;
    if (is32Bit) {
        window._xbe_gpu.indices = new Uint32Array(HEAPU8.buffer, ptr, count);
    } else {
        window._xbe_gpu.indices = new Uint16Array(HEAPU8.buffer, ptr, count);
    }
});

EM_JS(void, js_webgpu_draw_indexed, (int topology, int indexCount), {
    if (!window._xbe_gpu) return;
    if (window._xbe_gpu.ctx2d && window._xbe_gpu.vertices && window._xbe_gpu.indices) {
        // Fallback wireframe/point render in 2D canvas
        const ctx = window._xbe_gpu.ctx2d;
        const v = window._xbe_gpu.vertices;
        const idx = window._xbe_gpu.indices;
        const stride = 12; // 12 floats per Vertex3D (pos 3, norm 3, col 4, uv 2)
        const w = window._xbe_gpu.canvas.width;
        const h = window._xbe_gpu.canvas.height;

        ctx.strokeStyle = '#00ff66';
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        for (let i = 0; i < indexCount; i += 3) {
            const i0 = idx[i] * stride;
            const i1 = idx[i+1] * stride;
            const i2 = idx[i+2] * stride;

            const x0 = (v[i0] + 1.0) * 0.5 * w;
            const y0 = (1.0 - v[i0+1]) * 0.5 * h;
            const x1 = (v[i1] + 1.0) * 0.5 * w;
            const y1 = (1.0 - v[i1+1]) * 0.5 * h;
            const x2 = (v[i2] + 1.0) * 0.5 * w;
            const y2 = (1.0 - v[i2+1]) * 0.5 * h;

            ctx.moveTo(x0, y0);
            ctx.lineTo(x1, y1);
            ctx.lineTo(x2, y2);
            ctx.closePath();
        }
        ctx.stroke();
    }
});

#endif // XBE_TARGET_WASM

namespace xbe::gpu {

// ============================================================================
// WebGPU Texture & Shader Implementations
// ============================================================================
class WebGPUTexture final : public ITexture {
public:
    WebGPUTexture(const TextureDesc& desc, const void* initialData)
        : m_width(desc.width), m_height(desc.height), m_format(desc.format)
    {
        if (initialData) {
            size_t size = desc.width * desc.height * 4;
            m_data.assign(static_cast<const uint8_t*>(initialData),
                          static_cast<const uint8_t*>(initialData) + size);
        }
    }

    uint32_t GetWidth() const override { return m_width; }
    uint32_t GetHeight() const override { return m_height; }
    PixelFormat GetFormat() const override { return m_format; }

    void UpdateData(const void* data, size_t bytesPerRow) override {
        if (data) {
            size_t size = m_width * m_height * 4;
            m_data.assign(static_cast<const uint8_t*>(data),
                          static_cast<const uint8_t*>(data) + size);
        }
    }

private:
    uint32_t m_width{0};
    uint32_t m_height{0};
    PixelFormat m_format{PixelFormat::RGBA8_UNorm};
    std::vector<uint8_t> m_data;
};

class WebGPUShader final : public IShader {
public:
    WebGPUShader(const ShaderDesc& desc)
        : m_name("WebGPUShader"), m_desc(desc) {}

    bool IsValid() const override { return true; }
    const std::string& GetName() const override { return m_name; }

private:
    std::string m_name;
    ShaderDesc m_desc;
};

// ============================================================================
// WebGPURenderDevice::Impl
// ============================================================================
struct WebGPURenderDevice::Impl {
    int width{1280};
    int height{720};
    std::string title{"xbe-metal (WebGPU)"};
    bool shouldClose{false};

    Color4 clearColor{0.05f, 0.05f, 0.08f, 1.0f};
    Viewport viewport{0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f};
    ScissorRect scissor{0, 0, 1280, 720};
    RenderStateDesc renderState{};
    D3D8FixedFunctionState fixedFunctionState{};

    std::vector<uint8_t> vertexBuffer;
    std::vector<uint8_t> indexBuffer;
    IndexFormat indexFormat{IndexFormat::UInt16};

    std::shared_ptr<ITexture> boundTexture{nullptr};
    std::shared_ptr<IShader> boundShader{nullptr};
};

WebGPURenderDevice::WebGPURenderDevice()
    : m_impl(std::make_unique<Impl>())
{
}

WebGPURenderDevice::~WebGPURenderDevice() {
    Shutdown();
}

bool WebGPURenderDevice::Initialize(int width, int height, const std::string& title) {
    m_impl->width = width;
    m_impl->height = height;
    m_impl->title = title;
    m_impl->viewport = {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    m_impl->scissor = {0, 0, static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

#if defined(XBE_TARGET_WASM) || defined(__EMSCRIPTEN__)
    std::cout << "[WebGPUDevice] Initializing WebGPU / Emscripten canvas: " << width << "x" << height << std::endl;
    js_webgpu_init("#canvas", width, height);
#else
    std::cout << "[WebGPUDevice] Running desktop mock for WebGPU backend: " << width << "x" << height << std::endl;
#endif

    return true;
}

void WebGPURenderDevice::Shutdown() {
    // Teardown resources
}

bool WebGPURenderDevice::ShouldClose() const {
    return m_impl ? m_impl->shouldClose : true;
}

void WebGPURenderDevice::PollEvents() {
    // In Emscripten, browser events are handled asynchronously via requestAnimationFrame
}

bool WebGPURenderDevice::BeginFrame() {
#if defined(XBE_TARGET_WASM) || defined(__EMSCRIPTEN__)
    js_webgpu_begin_frame();
#endif
    return true;
}

void WebGPURenderDevice::EndFrame() {
#if defined(XBE_TARGET_WASM) || defined(__EMSCRIPTEN__)
    js_webgpu_end_frame();
#endif
}

void WebGPURenderDevice::Clear(float r, float g, float b, float a) {
    if (!m_impl) return;
    m_impl->clearColor = {r, g, b, a};
#if defined(XBE_TARGET_WASM) || defined(__EMSCRIPTEN__)
    js_webgpu_clear(r, g, b, a);
#endif
}

void WebGPURenderDevice::SetViewport(const Viewport& viewport) {
    if (m_impl) m_impl->viewport = viewport;
}

void WebGPURenderDevice::SetScissor(const ScissorRect& scissor) {
    if (m_impl) m_impl->scissor = scissor;
}

void WebGPURenderDevice::SetRenderState(const RenderStateDesc& state) {
    if (m_impl) m_impl->renderState = state;
}

void WebGPURenderDevice::SetFixedFunctionState(const D3D8FixedFunctionState& ffState) {
    if (m_impl) m_impl->fixedFunctionState = ffState;
}

void WebGPURenderDevice::SetVertexBuffer(const void* data, size_t size, uint32_t stride) {
    if (!m_impl || !data || size == 0) return;
    m_impl->vertexBuffer.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
#if defined(XBE_TARGET_WASM) || defined(__EMSCRIPTEN__)
    js_webgpu_upload_vertices(data, static_cast<int>(size));
#endif
}

void WebGPURenderDevice::SetIndexBuffer(const void* data, size_t size, IndexFormat format) {
    if (!m_impl || !data || size == 0) return;
    m_impl->indexBuffer.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
    m_impl->indexFormat = format;
#if defined(XBE_TARGET_WASM) || defined(__EMSCRIPTEN__)
    int count = (format == IndexFormat::UInt16) ? (size / 2) : (size / 4);
    js_webgpu_upload_indices(data, count, (format == IndexFormat::UInt32) ? 1 : 0);
#endif
}

void WebGPURenderDevice::SetTexture(uint32_t slot, std::shared_ptr<ITexture> texture) {
    if (m_impl) m_impl->boundTexture = texture;
}

void WebGPURenderDevice::SetShader(std::shared_ptr<IShader> shader) {
    if (m_impl) m_impl->boundShader = shader;
}

void WebGPURenderDevice::DrawPrimitives(PrimitiveTopology topology, uint32_t startVertex, uint32_t vertexCount) {
    // Draw non-indexed primitives
}

void WebGPURenderDevice::DrawIndexedPrimitives(PrimitiveTopology topology, uint32_t startIndex, uint32_t indexCount, uint32_t baseVertex) {
#if defined(XBE_TARGET_WASM) || defined(__EMSCRIPTEN__)
    js_webgpu_draw_indexed(static_cast<int>(topology), static_cast<int>(indexCount));
#endif
}

std::shared_ptr<ITexture> WebGPURenderDevice::CreateTexture(const TextureDesc& desc, const void* initialData) {
    return std::make_shared<WebGPUTexture>(desc, initialData);
}

std::shared_ptr<IShader> WebGPURenderDevice::CreateShader(const ShaderDesc& desc) {
    return std::make_shared<WebGPUShader>(desc);
}

int WebGPURenderDevice::GetWidth() const {
    return m_impl ? m_impl->width : 0;
}

int WebGPURenderDevice::GetHeight() const {
    return m_impl ? m_impl->height : 0;
}

const std::string& WebGPURenderDevice::GetTitle() const {
    static const std::string empty;
    return m_impl ? m_impl->title : empty;
}

} // namespace xbe::gpu
