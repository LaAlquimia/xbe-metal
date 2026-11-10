#include "runtime/gpu/render_device.hpp"

#if defined(__APPLE__) && !defined(XBE_TARGET_WASM)
#include "runtime/gpu/metal/metal_device.hpp"
#elif defined(XBE_TARGET_WASM) || defined(__EMSCRIPTEN__)
#include "runtime/gpu/webgpu/webgpu_device.hpp"
#else
#include "runtime/gpu/webgpu/webgpu_device.hpp"
#endif

#include <iostream>

namespace xbe::gpu {

std::unique_ptr<IRenderDevice> CreateRenderDevice() {
#if defined(__APPLE__) && !defined(XBE_TARGET_WASM)
    std::cout << "[RenderDevice] Instantiating Native Apple Metal Device." << std::endl;
    return std::make_unique<MetalRenderDevice>();
#elif defined(XBE_TARGET_WASM) || defined(__EMSCRIPTEN__)
    std::cout << "[RenderDevice] Instantiating WebGPU / Canvas Device for WebAssembly." << std::endl;
    return std::make_unique<WebGPURenderDevice>();
#else
    std::cout << "[RenderDevice] Instantiating Fallback WebGPU Device." << std::endl;
    return std::make_unique<WebGPURenderDevice>();
#endif
}

} // namespace xbe::gpu
