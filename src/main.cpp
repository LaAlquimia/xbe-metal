#include "runtime/gpu/render_device.hpp"
#include "runtime/core/memory.hpp"
#include "runtime/core/kernel.hpp"
#include "xbe/xbe_parser.hpp"

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cmath>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

// ============================================================================
// 3D Geometry: Cyberpunk / Xbox Gem Cube
// ============================================================================
static void BuildDemoCube(std::vector<xbe::gpu::Vertex3D>& outVertices,
                          std::vector<uint16_t>& outIndices) {
    outVertices.clear();
    outIndices.clear();

    const xbe::gpu::Color4 cFront  = {0.0f, 1.0f, 0.45f, 1.0f}; // Neon Xbox Green
    const xbe::gpu::Color4 cBack   = {0.0f, 0.5f, 0.2f,  1.0f}; // Dark Emerald
    const xbe::gpu::Color4 cTop    = {0.4f, 1.0f, 0.6f,  1.0f}; // Bright Jade
    const xbe::gpu::Color4 cBottom = {0.0f, 0.3f, 0.15f, 1.0f}; // Deep Forest
    const xbe::gpu::Color4 cLeft   = {0.1f, 0.8f, 0.35f, 1.0f}; // Electric Lime
    const xbe::gpu::Color4 cRight  = {0.2f, 0.9f, 0.4f,  1.0f}; // Mint Glow

    // 24 vertices (4 per face for clean normals)
    outVertices = {
        // Front face (+Z)
        {{-0.6f, -0.6f,  0.6f}, {0.0f, 0.0f, 1.0f}, cFront, {0.0f, 1.0f}},
        {{ 0.6f, -0.6f,  0.6f}, {0.0f, 0.0f, 1.0f}, cFront, {1.0f, 1.0f}},
        {{ 0.6f,  0.6f,  0.6f}, {0.0f, 0.0f, 1.0f}, cFront, {1.0f, 0.0f}},
        {{-0.6f,  0.6f,  0.6f}, {0.0f, 0.0f, 1.0f}, cFront, {0.0f, 0.0f}},

        // Back face (-Z)
        {{ 0.6f, -0.6f, -0.6f}, {0.0f, 0.0f, -1.0f}, cBack, {0.0f, 1.0f}},
        {{-0.6f, -0.6f, -0.6f}, {0.0f, 0.0f, -1.0f}, cBack, {1.0f, 1.0f}},
        {{-0.6f,  0.6f, -0.6f}, {0.0f, 0.0f, -1.0f}, cBack, {1.0f, 0.0f}},
        {{ 0.6f,  0.6f, -0.6f}, {0.0f, 0.0f, -1.0f}, cBack, {0.0f, 0.0f}},

        // Top face (+Y)
        {{-0.6f,  0.6f,  0.6f}, {0.0f, 1.0f, 0.0f}, cTop, {0.0f, 1.0f}},
        {{ 0.6f,  0.6f,  0.6f}, {0.0f, 1.0f, 0.0f}, cTop, {1.0f, 1.0f}},
        {{ 0.6f,  0.6f, -0.6f}, {0.0f, 1.0f, 0.0f}, cTop, {1.0f, 0.0f}},
        {{-0.6f,  0.6f, -0.6f}, {0.0f, 1.0f, 0.0f}, cTop, {0.0f, 0.0f}},

        // Bottom face (-Y)
        {{-0.6f, -0.6f, -0.6f}, {0.0f, -1.0f, 0.0f}, cBottom, {0.0f, 1.0f}},
        {{ 0.6f, -0.6f, -0.6f}, {0.0f, -1.0f, 0.0f}, cBottom, {1.0f, 1.0f}},
        {{ 0.6f, -0.6f,  0.6f}, {0.0f, -1.0f, 0.0f}, cBottom, {1.0f, 0.0f}},
        {{-0.6f, -0.6f,  0.6f}, {0.0f, -1.0f, 0.0f}, cBottom, {0.0f, 0.0f}},

        // Left face (-X)
        {{-0.6f, -0.6f, -0.6f}, {-1.0f, 0.0f, 0.0f}, cLeft, {0.0f, 1.0f}},
        {{-0.6f, -0.6f,  0.6f}, {-1.0f, 0.0f, 0.0f}, cLeft, {1.0f, 1.0f}},
        {{-0.6f,  0.6f,  0.6f}, {-1.0f, 0.0f, 0.0f}, cLeft, {1.0f, 0.0f}},
        {{-0.6f,  0.6f, -0.6f}, {-1.0f, 0.0f, 0.0f}, cLeft, {0.0f, 0.0f}},

        // Right face (+X)
        {{ 0.6f, -0.6f,  0.6f}, {1.0f, 0.0f, 0.0f}, cRight, {0.0f, 1.0f}},
        {{ 0.6f, -0.6f, -0.6f}, {1.0f, 0.0f, 0.0f}, cRight, {1.0f, 1.0f}},
        {{ 0.6f,  0.6f, -0.6f}, {1.0f, 0.0f, 0.0f}, cRight, {1.0f, 0.0f}},
        {{ 0.6f,  0.6f,  0.6f}, {1.0f, 0.0f, 0.0f}, cRight, {0.0f, 0.0f}}
    };

    // 12 triangles = 36 indices
    for (uint16_t face = 0; face < 6; ++face) {
        uint16_t base = face * 4;
        outIndices.push_back(base + 0);
        outIndices.push_back(base + 1);
        outIndices.push_back(base + 2);
        outIndices.push_back(base + 2);
        outIndices.push_back(base + 3);
        outIndices.push_back(base + 0);
    }
}

// App Context
struct AppState {
    std::unique_ptr<xbe::gpu::IRenderDevice> device;
    xbe::core::Memory memory;
    std::unique_ptr<xbe::core::Kernel> kernel;
    std::vector<xbe::gpu::Vertex3D> vertices;
    std::vector<uint16_t> indices;
    float rotationAngle{0.0f};
    uint64_t frameCount{0};
    bool isRunning{true};
};

static void RenderFrame(void* userData) {
    AppState* state = static_cast<AppState*>(userData);
    if (!state || !state->device || state->device->ShouldClose()) {
        state->isRunning = false;
        return;
    }

    state->device->PollEvents();

    state->rotationAngle += 0.025f;
    float aspect = static_cast<float>(state->device->GetWidth()) /
                   static_cast<float>(state->device->GetHeight());

    // Compose D3D8 fixed function state
    xbe::gpu::D3D8FixedFunctionState ffState{};
    ffState.projection = xbe::gpu::Matrix4x4::Perspective(45.0f * (3.14159265f / 180.0f), aspect, 0.1f, 100.0f);
    ffState.view = xbe::gpu::Matrix4x4::Translation(0.0f, 0.0f, 2.5f);

    xbe::gpu::Matrix4x4 rotY = xbe::gpu::Matrix4x4::RotationY(state->rotationAngle);
    xbe::gpu::Matrix4x4 rotX = xbe::gpu::Matrix4x4::RotationX(state->rotationAngle * 0.7f);
    ffState.world = xbe::gpu::Matrix4x4::Multiply(rotY, rotX);

    ffState.lightingEnabled = true;
    ffState.material.diffuse = {1.0f, 1.0f, 1.0f, 1.0f};
    ffState.material.ambient = {0.25f, 0.25f, 0.25f, 1.0f};

    ffState.lights[0].enabled = true;
    ffState.lights[0].direction = {0.577f, 0.577f, -0.577f};
    ffState.lights[0].diffuse = {1.0f, 1.0f, 1.0f, 1.0f};

    state->device->SetFixedFunctionState(ffState);

    // Dynamic buffer upload
    state->device->SetVertexBuffer(state->vertices.data(),
                                   state->vertices.size() * sizeof(xbe::gpu::Vertex3D),
                                   sizeof(xbe::gpu::Vertex3D));

    state->device->SetIndexBuffer(state->indices.data(),
                                  state->indices.size() * sizeof(uint16_t),
                                  xbe::gpu::IndexFormat::UInt16);

    // Frame execution
    if (state->device->BeginFrame()) {
        // Dark Xbox futuristic background
        state->device->Clear(0.03f, 0.04f, 0.05f, 1.0f);
        state->device->DrawIndexedPrimitives(xbe::gpu::PrimitiveTopology::TriangleList,
                                            0,
                                            static_cast<uint32_t>(state->indices.size()));
        state->device->EndFrame();
    }

    state->frameCount++;
    if (state->frameCount % 120 == 0) {
        std::cout << "[Runner] Frame " << state->frameCount
                  << " | Backend: " << state->device->GetBackendName() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "====================================================\n";
    std::cout << "         xbe-metal: Xbox Recompiler & Runtime       \n";
    std::cout << "====================================================\n";

    AppState state;

    // 1. Initialize Xbox RAM (64 MB retail default)
    std::cout << "[Memory] Initializing 64 MB Xbox RAM..." << std::endl;

    // 2. Initialize Kernel HLE
    state.kernel = std::make_unique<xbe::core::Kernel>();

    // 3. Inspect / Load .xbe file if provided
    std::string windowTitle = "xbe-metal [Standby Demo]";
    if (argc > 1) {
        std::string xbePath = argv[1];
        xbe::XbeFile xbe;
        if (xbe.LoadFromFile(xbePath)) {
            std::cout << "\n[Loaded XBE Binary]" << std::endl;
            std::cout << "  Title Name   : " << xbe.GetTitleName() << std::endl;
            std::cout << "  Title ID     : 0x" << std::hex << xbe.GetTitleId() << std::dec << std::endl;
            std::cout << "  Sections     : " << xbe.GetSections().size() << std::endl;
            std::cout << "  Entry Point  : 0x" << std::hex << xbe.GetDecodedEntryPoint() << std::dec << std::endl;

            // Load sections into virtual memory
            for (const auto& sec : xbe.GetSections()) {
                if (!sec.data.empty() && sec.header.virtual_address != 0) {
                    state.memory.map_section(sec.header, sec.data);
                    std::cout << "  -> Mapped section '" << sec.name << "' to 0x"
                              << std::hex << sec.header.virtual_address << " (" << std::dec << sec.data.size() << " bytes)" << std::endl;
                }
            }
            windowTitle = "xbe-metal: " + xbe.GetTitleName();
        } else {
            std::cerr << "Could not parse XBE file. Falling back to Standby Demo mode." << std::endl;
        }
    } else {
        std::cout << "\n[Mode] Standby Demo mode (no .xbe file supplied)." << std::endl;
        std::cout << "To load a title: ./xbe-runner /path/to/default.xbe\n" << std::endl;
    }

    // 4. Create and initialize multiplatform Render Device
    state.device = xbe::gpu::CreateRenderDevice();
    if (!state.device || !state.device->Initialize(1280, 720, windowTitle)) {
        std::cerr << "Failed to initialize Graphics Render Device." << std::endl;
        return 1;
    }

    // 5. Generate 3D Demo Geometry
    BuildDemoCube(state.vertices, state.indices);

    std::cout << "[Runner] Graphics Pipeline Active: " << state.device->GetBackendName() << std::endl;
    std::cout << "[Runner] Entering Main Render Loop (Press ESC to exit)...\n" << std::endl;

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop_arg(RenderFrame, &state, 0, 1);
#else
    while (state.isRunning && !state.device->ShouldClose()) {
        RenderFrame(&state);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
#endif

    std::cout << "\n[Runner] Shutting down xbe-metal runtime cleanly." << std::endl;
    state.device->Shutdown();
    state.memory.reset();

    return 0;
}
