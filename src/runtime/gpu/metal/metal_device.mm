#ifdef __APPLE__

#import "runtime/gpu/metal/metal_device.hpp"
#import "runtime/gpu/gpu_types.hpp"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>
#import <simd/simd.h>

#include <iostream>
#include <unordered_map>
#include <vector>
#include <cstring>

// ============================================================================
// Metal Uniform Buffer Layout (Aligned with MSL)
// ============================================================================
struct UniformBufferData {
    simd::float4x4 mvpMatrix;
    simd::float4x4 modelMatrix;
    simd::float4   tintColor;
    simd::float4   ambientColor;
    simd::float4   lightDirection;
    simd::float4   lightColor;
    uint32_t       hasTexture;
    uint32_t       lightingEnabled;
    float          alphaRef;
    uint32_t       alphaFunc; // 0=None, 1=Greater, 2=Less, 3=Equal
};

// ============================================================================
// Built-in MSL Shader Source
// ============================================================================
static const char* kDefaultMSLSource = R"metal(
#include <metal_stdlib>
using namespace metal;

struct VertexInput {
    float3 position  [[attribute(0)]];
    float3 normal    [[attribute(1)]];
    float4 color     [[attribute(2)]];
    float2 texCoord  [[attribute(3)]];
};

struct RasterizerData {
    float4 position [[position]];
    float4 color;
    float2 texCoord;
    float3 normal;
    float3 worldPos;
};

struct Uniforms {
    float4x4 mvpMatrix;
    float4x4 modelMatrix;
    float4   tintColor;
    float4   ambientColor;
    float4   lightDirection;
    float4   lightColor;
    uint     hasTexture;
    uint     lightingEnabled;
    float    alphaRef;
    uint     alphaFunc;
};

vertex RasterizerData vertex_main(VertexInput in [[stage_in]],
                                  constant Uniforms& uniforms [[buffer(1)]]) {
    RasterizerData out;
    float4 pos4 = float4(in.position, 1.0);
    out.position = uniforms.mvpMatrix * pos4;
    out.color = in.color * uniforms.tintColor;
    out.texCoord = in.texCoord;
    out.normal = (uniforms.modelMatrix * float4(in.normal, 0.0)).xyz;
    out.worldPos = (uniforms.modelMatrix * pos4).xyz;
    return out;
}

fragment float4 fragment_main(RasterizerData in [[stage_in]],
                              constant Uniforms& uniforms [[buffer(1)]],
                              texture2d<float> colorTexture [[texture(0)]],
                              sampler textureSampler [[sampler(0)]]) {
    float4 texColor = float4(1.0, 1.0, 1.0, 1.0);
    if (uniforms.hasTexture != 0) {
        texColor = colorTexture.sample(textureSampler, in.texCoord);
    }
    
    float4 finalColor = in.color * texColor;
    
    // Direct3D 8 Alpha Test translation
    if (uniforms.alphaFunc == 1) { // Greater
        if (finalColor.a <= uniforms.alphaRef) discard_fragment();
    } else if (uniforms.alphaFunc == 2) { // Less
        if (finalColor.a >= uniforms.alphaRef) discard_fragment();
    } else if (uniforms.alphaFunc == 3) { // Equal
        if (abs(finalColor.a - uniforms.alphaRef) > 0.001) discard_fragment();
    }
    
    // Direct3D 8 Lighting translation
    if (uniforms.lightingEnabled != 0) {
        float3 N = normalize(in.normal);
        float3 L = normalize(uniforms.lightDirection.xyz);
        float nDotL = max(dot(N, L), 0.0);
        float3 diffuse = uniforms.lightColor.rgb * nDotL;
        float3 ambient = uniforms.ambientColor.rgb;
        finalColor.rgb = finalColor.rgb * (ambient + diffuse);
    }
    
    return finalColor;
}
)metal";

// ============================================================================
// Metal Window Delegate
// ============================================================================
@interface XbeWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) bool* shouldCloseFlag;
@end

@implementation XbeWindowDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender {
    if (_shouldCloseFlag) {
        *_shouldCloseFlag = true;
    }
    return YES;
}
- (void)windowWillClose:(NSNotification *)notification {
    if (_shouldCloseFlag) {
        *_shouldCloseFlag = true;
    }
}
@end

namespace xbe::gpu {

// ============================================================================
// MetalTexture Implementation
// ============================================================================
class MetalTexture final : public ITexture {
public:
    MetalTexture(id<MTLDevice> device, const TextureDesc& desc, const void* initialData)
        : m_width(desc.width), m_height(desc.height), m_format(desc.format)
    {
        MTLTextureDescriptor* texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                          width:desc.width
                                                                                         height:desc.height
                                                                                      mipmapped:(desc.mipLevels > 1)];
        texDesc.usage = MTLTextureUsageShaderRead;
        if (desc.isRenderTarget) {
            texDesc.usage |= MTLTextureUsageRenderTarget;
        }
        
        m_texture = [device newTextureWithDescriptor:texDesc];
        if (initialData && m_texture) {
            NSUInteger bytesPerRow = desc.width * 4;
            [m_texture replaceRegion:MTLRegionMake2D(0, 0, desc.width, desc.height)
                         mipmapLevel:0
                           withBytes:initialData
                         bytesPerRow:bytesPerRow];
        }
    }

    ~MetalTexture() override {
        m_texture = nil;
    }

    uint32_t GetWidth() const override { return m_width; }
    uint32_t GetHeight() const override { return m_height; }
    PixelFormat GetFormat() const override { return m_format; }

    void UpdateData(const void* data, size_t bytesPerRow) override {
        if (m_texture && data) {
            [m_texture replaceRegion:MTLRegionMake2D(0, 0, m_width, m_height)
                         mipmapLevel:0
                           withBytes:data
                         bytesPerRow:bytesPerRow];
        }
    }

    id<MTLTexture> GetNativeTexture() const { return m_texture; }

private:
    id<MTLTexture> m_texture{nil};
    uint32_t m_width{0};
    uint32_t m_height{0};
    PixelFormat m_format{PixelFormat::RGBA8_UNorm};
};

// ============================================================================
// MetalShader Implementation
// ============================================================================
class MetalShader final : public IShader {
public:
    MetalShader(id<MTLDevice> device, const ShaderDesc& desc)
        : m_name("CustomMetalShader")
    {
        NSError* error = nil;
        NSString* src = [NSString stringWithUTF8String:desc.vertexSource.c_str()];
        id<MTLLibrary> library = [device newLibraryWithSource:src options:nil error:&error];
        if (library) {
            m_vertexFunc = [library newFunctionWithName:[NSString stringWithUTF8String:desc.vertexEntryPoint.c_str()]];
            m_fragmentFunc = [library newFunctionWithName:[NSString stringWithUTF8String:desc.fragmentEntryPoint.c_str()]];
        } else if (error) {
            std::cerr << "[MetalShader] Compilation Error: " << [[error localizedDescription] UTF8String] << std::endl;
        }
    }

    ~MetalShader() override {
        m_vertexFunc = nil;
        m_fragmentFunc = nil;
    }

    bool IsValid() const override {
        return m_vertexFunc != nil && m_fragmentFunc != nil;
    }

    const std::string& GetName() const override {
        return m_name;
    }

    id<MTLFunction> GetVertexFunction() const { return m_vertexFunc; }
    id<MTLFunction> GetFragmentFunction() const { return m_fragmentFunc; }

private:
    std::string m_name;
    id<MTLFunction> m_vertexFunc{nil};
    id<MTLFunction> m_fragmentFunc{nil};
};

// ============================================================================
// MetalRenderDevice::Impl
// ============================================================================
struct MetalRenderDevice::Impl {
    int width{1280};
    int height{720};
    std::string title{"xbe-metal"};
    bool shouldClose{false};
    bool isHeadless{false};

    // Cocoa window & UI
    NSWindow* window{nil};
    XbeWindowDelegate* windowDelegate{nil};
    CAMetalLayer* metalLayer{nil};

    // Metal core objects
    id<MTLDevice> device{nil};
    id<MTLCommandQueue> commandQueue{nil};
    id<MTLLibrary> defaultLibrary{nil};
    id<MTLFunction> defaultVertexFunc{nil};
    id<MTLFunction> defaultFragmentFunc{nil};
    id<MTLRenderPipelineState> pipelineState{nil};
    id<MTLDepthStencilState> depthStencilState{nil};
    id<MTLSamplerState> defaultSampler{nil};

    // Frame transient objects
    NSAutoreleasePool* framePool{nil};
    id<CAMetalDrawable> currentDrawable{nil};
    id<MTLCommandBuffer> currentCommandBuffer{nil};
    id<MTLRenderCommandEncoder> currentEncoder{nil};
    id<MTLTexture> offscreenTexture{nil};
    id<MTLTexture> depthTexture{nil};

    // Dynamic buffers
    id<MTLBuffer> vertexBuffer{nil};
    id<MTLBuffer> indexBuffer{nil};
    id<MTLBuffer> uniformBuffer{nil};

    // State tracking
    Color4 clearColor{0.05f, 0.05f, 0.08f, 1.0f};
    Viewport currentViewport{0, 0, 1280, 720, 0.0f, 1.0f};
    ScissorRect currentScissor{0, 0, 1280, 720};
    RenderStateDesc currentRenderState{};
    D3D8FixedFunctionState currentFixedFunctionState{};
    std::shared_ptr<ITexture> boundTexture{nullptr};
    std::shared_ptr<IShader> boundShader{nullptr};

    void BuildDefaultPipeline();
    void UpdateUniforms();
};

void MetalRenderDevice::Impl::BuildDefaultPipeline() {
    NSError* error = nil;
    NSString* source = [NSString stringWithUTF8String:kDefaultMSLSource];
    MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
    defaultLibrary = [device newLibraryWithSource:source options:options error:&error];
    if (!defaultLibrary || error) {
        std::cerr << "[MetalDevice] Default library compilation error: "
                  << (error ? [[error localizedDescription] UTF8String] : "Unknown") << std::endl;
        return;
    }

    defaultVertexFunc = [defaultLibrary newFunctionWithName:@"vertex_main"];
    defaultFragmentFunc = [defaultLibrary newFunctionWithName:@"fragment_main"];

    MTLVertexDescriptor* vtxDesc = [MTLVertexDescriptor vertexDescriptor];
    // Position (Float3)
    vtxDesc.attributes[0].format = MTLVertexFormatFloat3;
    vtxDesc.attributes[0].offset = offsetof(Vertex3D, position);
    vtxDesc.attributes[0].bufferIndex = 0;
    // Normal (Float3)
    vtxDesc.attributes[1].format = MTLVertexFormatFloat3;
    vtxDesc.attributes[1].offset = offsetof(Vertex3D, normal);
    vtxDesc.attributes[1].bufferIndex = 0;
    // Color (Float4)
    vtxDesc.attributes[2].format = MTLVertexFormatFloat4;
    vtxDesc.attributes[2].offset = offsetof(Vertex3D, color);
    vtxDesc.attributes[2].bufferIndex = 0;
    // TexCoord (Float2)
    vtxDesc.attributes[3].format = MTLVertexFormatFloat2;
    vtxDesc.attributes[3].offset = offsetof(Vertex3D, texCoord);
    vtxDesc.attributes[3].bufferIndex = 0;

    vtxDesc.layouts[0].stride = sizeof(Vertex3D);
    vtxDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

    MTLRenderPipelineDescriptor* pDesc = [[MTLRenderPipelineDescriptor alloc] init];
    pDesc.vertexFunction = defaultVertexFunc;
    pDesc.fragmentFunction = defaultFragmentFunc;
    pDesc.vertexDescriptor = vtxDesc;
    pDesc.colorAttachments[0].pixelFormat = isHeadless ? MTLPixelFormatRGBA8Unorm : metalLayer.pixelFormat;
    
    // Blending setup
    pDesc.colorAttachments[0].blendingEnabled = YES;
    pDesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pDesc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    pDesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pDesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pDesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    pDesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

    pDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

    pipelineState = [device newRenderPipelineStateWithDescriptor:pDesc error:&error];
    if (!pipelineState || error) {
        std::cerr << "[MetalDevice] Pipeline state error: "
                  << (error ? [[error localizedDescription] UTF8String] : "Unknown") << std::endl;
    }

    MTLDepthStencilDescriptor* dsDesc = [[MTLDepthStencilDescriptor alloc] init];
    dsDesc.depthCompareFunction = MTLCompareFunctionLessEqual;
    dsDesc.depthWriteEnabled = YES;
    depthStencilState = [device newDepthStencilStateWithDescriptor:dsDesc];

    MTLSamplerDescriptor* sDesc = [[MTLSamplerDescriptor alloc] init];
    sDesc.minFilter = MTLSamplerMinMagFilterLinear;
    sDesc.magFilter = MTLSamplerMinMagFilterLinear;
    sDesc.mipFilter = MTLSamplerMipFilterLinear;
    sDesc.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sDesc.tAddressMode = MTLSamplerAddressModeClampToEdge;
    defaultSampler = [device newSamplerStateWithDescriptor:sDesc];

    // Uniform buffer allocation
    uniformBuffer = [device newBufferWithLength:sizeof(UniformBufferData)
                                        options:MTLResourceStorageModeShared];
}

void MetalRenderDevice::Impl::UpdateUniforms() {
    if (!uniformBuffer) return;

    UniformBufferData data{};
    // Compute MVP = Projection * View * World
    Matrix4x4 viewProj = Matrix4x4::Multiply(currentFixedFunctionState.projection, currentFixedFunctionState.view);
    Matrix4x4 mvp = Matrix4x4::Multiply(viewProj, currentFixedFunctionState.world);

    std::memcpy(&data.mvpMatrix, mvp.m.data(), sizeof(simd::float4x4));
    std::memcpy(&data.modelMatrix, currentFixedFunctionState.world.m.data(), sizeof(simd::float4x4));

    data.tintColor = simd::make_float4(
        currentFixedFunctionState.material.diffuse.r,
        currentFixedFunctionState.material.diffuse.g,
        currentFixedFunctionState.material.diffuse.b,
        currentFixedFunctionState.material.diffuse.a
    );

    data.ambientColor = simd::make_float4(
        currentFixedFunctionState.material.ambient.r,
        currentFixedFunctionState.material.ambient.g,
        currentFixedFunctionState.material.ambient.b,
        currentFixedFunctionState.material.ambient.a
    );

    // Light 0
    if (currentFixedFunctionState.lights[0].enabled) {
        data.lightDirection = simd::make_float4(
            currentFixedFunctionState.lights[0].direction.x,
            currentFixedFunctionState.lights[0].direction.y,
            currentFixedFunctionState.lights[0].direction.z,
            0.0f
        );
        data.lightColor = simd::make_float4(
            currentFixedFunctionState.lights[0].diffuse.r,
            currentFixedFunctionState.lights[0].diffuse.g,
            currentFixedFunctionState.lights[0].diffuse.b,
            currentFixedFunctionState.lights[0].diffuse.a
        );
    } else {
        data.lightDirection = simd::make_float4(0.0f, 0.0f, 1.0f, 0.0f);
        data.lightColor = simd::make_float4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    data.hasTexture = (boundTexture != nullptr) ? 1 : 0;
    data.lightingEnabled = currentFixedFunctionState.lightingEnabled ? 1 : 0;
    data.alphaRef = currentFixedFunctionState.alphaRef;
    if (currentFixedFunctionState.alphaTestEnable) {
        if (currentFixedFunctionState.alphaFunc == CompareFunction::Greater) data.alphaFunc = 1;
        else if (currentFixedFunctionState.alphaFunc == CompareFunction::Less) data.alphaFunc = 2;
        else if (currentFixedFunctionState.alphaFunc == CompareFunction::Equal) data.alphaFunc = 3;
        else data.alphaFunc = 0;
    } else {
        data.alphaFunc = 0;
    }

    std::memcpy([uniformBuffer contents], &data, sizeof(UniformBufferData));
}

// ============================================================================
// MetalRenderDevice Public Interface
// ============================================================================

MetalRenderDevice::MetalRenderDevice()
    : m_impl(std::make_unique<Impl>())
{
}

MetalRenderDevice::~MetalRenderDevice() {
    Shutdown();
}

bool MetalRenderDevice::Initialize(int width, int height, const std::string& title) {
    m_impl->width = width;
    m_impl->height = height;
    m_impl->title = title;

    @autoreleasepool {
        m_impl->device = MTLCreateSystemDefaultDevice();
        if (!m_impl->device) {
            std::cerr << "[MetalDevice] Error: No compatible Apple Metal GPU found." << std::endl;
            return false;
        }

        std::cout << "[MetalDevice] Initialized Metal backend on device: "
                  << [[m_impl->device name] UTF8String] << std::endl;

        m_impl->commandQueue = [m_impl->device newCommandQueue];

        // Attempt Cocoa window initialization
        bool hasDisplay = (NSApp != nil) || (getenv("CI") == nullptr && getenv("SSH_CONNECTION") == nullptr);
        if (hasDisplay) {
            [NSApplication sharedApplication];
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

            NSRect frame = NSMakeRect(0, 0, width, height);
            NSWindowStyleMask style = NSWindowStyleMaskTitled |
                                      NSWindowStyleMaskClosable |
                                      NSWindowStyleMaskMiniaturizable |
                                      NSWindowStyleMaskResizable;

            m_impl->window = [[NSWindow alloc] initWithContentRect:frame
                                                         styleMask:style
                                                           backing:NSBackingStoreBuffered
                                                             defer:NO];

            if (m_impl->window) {
                [m_impl->window setTitle:[NSString stringWithUTF8String:title.c_str()]];
                [m_impl->window center];

                m_impl->windowDelegate = [[XbeWindowDelegate alloc] init];
                m_impl->windowDelegate.shouldCloseFlag = &m_impl->shouldClose;
                [m_impl->window setDelegate:m_impl->windowDelegate];

                NSView* contentView = [m_impl->window contentView];
                contentView.wantsLayer = YES;

                m_impl->metalLayer = [CAMetalLayer layer];
                m_impl->metalLayer.device = m_impl->device;
                m_impl->metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
                m_impl->metalLayer.framebufferOnly = YES;
                m_impl->metalLayer.drawableSize = CGSizeMake(width, height);
                contentView.layer = m_impl->metalLayer;

                [m_impl->window makeKeyAndOrderFront:nil];
                [NSApp activateIgnoringOtherApps:YES];
            } else {
                m_impl->isHeadless = true;
            }
        } else {
            m_impl->isHeadless = true;
        }

        if (m_impl->isHeadless) {
            std::cout << "[MetalDevice] Operating in Headless / Offscreen Metal mode." << std::endl;
            MTLTextureDescriptor* offscreenDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                                     width:width
                                                                                                    height:height
                                                                                                 mipmapped:NO];
            offscreenDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
            m_impl->offscreenTexture = [m_impl->device newTextureWithDescriptor:offscreenDesc];
        }

        // Depth texture setup
        MTLTextureDescriptor* depthDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                             width:width
                                                                                            height:height
                                                                                         mipmapped:NO];
        depthDesc.usage = MTLTextureUsageRenderTarget;
        depthDesc.storageMode = MTLStorageModePrivate;
        m_impl->depthTexture = [m_impl->device newTextureWithDescriptor:depthDesc];

        m_impl->BuildDefaultPipeline();
        m_impl->currentViewport = {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
        m_impl->currentScissor = {0, 0, static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    }

    return true;
}

void MetalRenderDevice::Shutdown() {
    if (!m_impl) return;

    @autoreleasepool {
        if (m_impl->window) {
            [m_impl->window close];
            m_impl->window = nil;
        }
        m_impl->metalLayer = nil;
        m_impl->commandQueue = nil;
        m_impl->device = nil;
    }
}

bool MetalRenderDevice::ShouldClose() const {
    return m_impl ? m_impl->shouldClose : true;
}

void MetalRenderDevice::PollEvents() {
    if (!m_impl || m_impl->isHeadless) return;

    @autoreleasepool {
        NSEvent* event = nil;
        while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                           untilDate:nil
                                              inMode:NSDefaultRunLoopMode
                                             dequeue:YES])) {
            if ([event type] == NSEventTypeKeyDown) {
                // Escape key code = 53
                if ([event keyCode] == 53) {
                    m_impl->shouldClose = true;
                }
            }
            [NSApp sendEvent:event];
        }
    }
}

bool MetalRenderDevice::BeginFrame() {
    if (!m_impl) return false;

    // Allocate frame-scoped autorelease pool drained in EndFrame
    m_impl->framePool = [[NSAutoreleasePool alloc] init];

    m_impl->currentCommandBuffer = [m_impl->commandQueue commandBuffer];

    MTLRenderPassDescriptor* rpd = [MTLRenderPassDescriptor renderPassDescriptor];

    if (!m_impl->isHeadless && m_impl->metalLayer) {
        m_impl->currentDrawable = [m_impl->metalLayer nextDrawable];
        if (!m_impl->currentDrawable) {
            [m_impl->framePool drain];
            m_impl->framePool = nil;
            return false;
        }
        rpd.colorAttachments[0].texture = m_impl->currentDrawable.texture;
    } else {
        rpd.colorAttachments[0].texture = m_impl->offscreenTexture;
    }

    rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
    rpd.colorAttachments[0].clearColor = MTLClearColorMake(
        m_impl->clearColor.r,
        m_impl->clearColor.g,
        m_impl->clearColor.b,
        m_impl->clearColor.a
    );
    rpd.colorAttachments[0].storeAction = MTLStoreActionStore;

    if (m_impl->depthTexture) {
        rpd.depthAttachment.texture = m_impl->depthTexture;
        rpd.depthAttachment.loadAction = MTLLoadActionClear;
        rpd.depthAttachment.clearDepth = 1.0;
        rpd.depthAttachment.storeAction = MTLStoreActionDontCare;
    }

    m_impl->currentEncoder = [m_impl->currentCommandBuffer renderCommandEncoderWithDescriptor:rpd];
    [m_impl->currentEncoder setRenderPipelineState:m_impl->pipelineState];
    [m_impl->currentEncoder setDepthStencilState:m_impl->depthStencilState];
    [m_impl->currentEncoder setFragmentSamplerState:m_impl->defaultSampler atIndex:0];

    // Apply Viewport & Scissor
    MTLViewport vp{
        m_impl->currentViewport.x,
        m_impl->currentViewport.y,
        m_impl->currentViewport.width,
        m_impl->currentViewport.height,
        m_impl->currentViewport.minDepth,
        m_impl->currentViewport.maxDepth
    };
    [m_impl->currentEncoder setViewport:vp];

    MTLScissorRect sc{
        static_cast<NSUInteger>(m_impl->currentScissor.x),
        static_cast<NSUInteger>(m_impl->currentScissor.y),
        m_impl->currentScissor.width,
        m_impl->currentScissor.height
    };
    [m_impl->currentEncoder setScissorRect:sc];

    m_impl->UpdateUniforms();
    [m_impl->currentEncoder setVertexBuffer:m_impl->uniformBuffer offset:0 atIndex:1];
    [m_impl->currentEncoder setFragmentBuffer:m_impl->uniformBuffer offset:0 atIndex:1];

    if (m_impl->vertexBuffer) {
        [m_impl->currentEncoder setVertexBuffer:m_impl->vertexBuffer offset:0 atIndex:0];
    }
    if (m_impl->boundTexture) {
        auto metalTex = std::static_pointer_cast<MetalTexture>(m_impl->boundTexture);
        [m_impl->currentEncoder setFragmentTexture:metalTex->GetNativeTexture() atIndex:0];
    }

    return true;
}

void MetalRenderDevice::EndFrame() {
    if (!m_impl) return;

    if (m_impl->currentEncoder) {
        [m_impl->currentEncoder endEncoding];
        m_impl->currentEncoder = nil;
    }

    if (!m_impl->isHeadless && m_impl->currentDrawable) {
        [m_impl->currentCommandBuffer presentDrawable:m_impl->currentDrawable];
        m_impl->currentDrawable = nil;
    }

    if (m_impl->currentCommandBuffer) {
        [m_impl->currentCommandBuffer commit];
        m_impl->currentCommandBuffer = nil;
    }

    if (m_impl->framePool) {
        [m_impl->framePool drain];
        m_impl->framePool = nil;
    }
}

void MetalRenderDevice::Clear(float r, float g, float b, float a) {
    if (!m_impl) return;
    m_impl->clearColor = {r, g, b, a};
}

void MetalRenderDevice::SetViewport(const Viewport& viewport) {
    if (!m_impl) return;
    m_impl->currentViewport = viewport;
    if (m_impl->currentEncoder) {
        MTLViewport vp{
            viewport.x, viewport.y, viewport.width, viewport.height,
            viewport.minDepth, viewport.maxDepth
        };
        [m_impl->currentEncoder setViewport:vp];
    }
}

void MetalRenderDevice::SetScissor(const ScissorRect& scissor) {
    if (!m_impl) return;
    m_impl->currentScissor = scissor;
    if (m_impl->currentEncoder) {
        MTLScissorRect sc{
            static_cast<NSUInteger>(scissor.x),
            static_cast<NSUInteger>(scissor.y),
            scissor.width,
            scissor.height
        };
        [m_impl->currentEncoder setScissorRect:sc];
    }
}

void MetalRenderDevice::SetRenderState(const RenderStateDesc& state) {
    if (!m_impl) return;
    m_impl->currentRenderState = state;
    if (m_impl->currentEncoder) {
        MTLCullMode cm = MTLCullModeNone;
        if (state.cullMode == CullMode::Front) cm = MTLCullModeFront;
        else if (state.cullMode == CullMode::Back) cm = MTLCullModeBack;
        [m_impl->currentEncoder setCullMode:cm];

        MTLTriangleFillMode tfm = (state.fillMode == FillMode::Wireframe)
            ? MTLTriangleFillModeLines : MTLTriangleFillModeFill;
        [m_impl->currentEncoder setTriangleFillMode:tfm];
    }
}

void MetalRenderDevice::SetFixedFunctionState(const D3D8FixedFunctionState& ffState) {
    if (!m_impl) return;
    m_impl->currentFixedFunctionState = ffState;
    m_impl->UpdateUniforms();
}

void MetalRenderDevice::SetVertexBuffer(const void* data, size_t size, uint32_t stride) {
    if (!m_impl || !m_impl->device || !data || size == 0) return;

    if (!m_impl->vertexBuffer || [m_impl->vertexBuffer length] < size) {
        m_impl->vertexBuffer = [m_impl->device newBufferWithLength:size options:MTLResourceStorageModeShared];
    }

    std::memcpy([m_impl->vertexBuffer contents], data, size);
    if (m_impl->currentEncoder) {
        [m_impl->currentEncoder setVertexBuffer:m_impl->vertexBuffer offset:0 atIndex:0];
    }
}

void MetalRenderDevice::SetIndexBuffer(const void* data, size_t size, IndexFormat format) {
    if (!m_impl || !m_impl->device || !data || size == 0) return;

    if (!m_impl->indexBuffer || [m_impl->indexBuffer length] < size) {
        m_impl->indexBuffer = [m_impl->device newBufferWithLength:size options:MTLResourceStorageModeShared];
    }

    std::memcpy([m_impl->indexBuffer contents], data, size);
}

void MetalRenderDevice::SetTexture(uint32_t slot, std::shared_ptr<ITexture> texture) {
    if (!m_impl) return;
    m_impl->boundTexture = texture;
    m_impl->UpdateUniforms();

    if (m_impl->currentEncoder) {
        if (texture) {
            auto metalTex = std::static_pointer_cast<MetalTexture>(texture);
            [m_impl->currentEncoder setFragmentTexture:metalTex->GetNativeTexture() atIndex:slot];
        } else {
            [m_impl->currentEncoder setFragmentTexture:nil atIndex:slot];
        }
    }
}

void MetalRenderDevice::SetShader(std::shared_ptr<IShader> shader) {
    if (!m_impl) return;
    m_impl->boundShader = shader;
    if (shader && shader->IsValid() && m_impl->currentEncoder) {
        auto metalShader = std::static_pointer_cast<MetalShader>(shader);
        // Note: Can re-bind custom vertex/fragment functions
    }
}

void MetalRenderDevice::DrawPrimitives(PrimitiveTopology topology, uint32_t startVertex, uint32_t vertexCount) {
    if (!m_impl || !m_impl->currentEncoder) return;

    MTLPrimitiveType primType = MTLPrimitiveTypeTriangle;
    switch (topology) {
        case PrimitiveTopology::PointList:     primType = MTLPrimitiveTypePoint; break;
        case PrimitiveTopology::LineList:      primType = MTLPrimitiveTypeLine; break;
        case PrimitiveTopology::LineStrip:     primType = MTLPrimitiveTypeLineStrip; break;
        case PrimitiveTopology::TriangleList:  primType = MTLPrimitiveTypeTriangle; break;
        case PrimitiveTopology::TriangleStrip: primType = MTLPrimitiveTypeTriangleStrip; break;
        default: break;
    }

    [m_impl->currentEncoder drawPrimitives:primType vertexStart:startVertex vertexCount:vertexCount];
}

void MetalRenderDevice::DrawIndexedPrimitives(PrimitiveTopology topology, uint32_t startIndex, uint32_t indexCount, uint32_t baseVertex) {
    if (!m_impl || !m_impl->currentEncoder || !m_impl->indexBuffer) return;

    MTLPrimitiveType primType = MTLPrimitiveTypeTriangle;
    switch (topology) {
        case PrimitiveTopology::PointList:     primType = MTLPrimitiveTypePoint; break;
        case PrimitiveTopology::LineList:      primType = MTLPrimitiveTypeLine; break;
        case PrimitiveTopology::LineStrip:     primType = MTLPrimitiveTypeLineStrip; break;
        case PrimitiveTopology::TriangleList:  primType = MTLPrimitiveTypeTriangle; break;
        case PrimitiveTopology::TriangleStrip: primType = MTLPrimitiveTypeTriangleStrip; break;
        default: break;
    }

    MTLIndexType indexType = MTLIndexTypeUInt16;
    size_t indexSize = (indexType == MTLIndexTypeUInt16) ? 2 : 4;
    NSUInteger offset = startIndex * indexSize;

    [m_impl->currentEncoder drawIndexedPrimitives:primType
                                      indexCount:indexCount
                                       indexType:indexType
                                     indexBuffer:m_impl->indexBuffer
                               indexBufferOffset:offset
                                   instanceCount:1
                                      baseVertex:baseVertex
                                    baseInstance:0];
}

std::shared_ptr<ITexture> MetalRenderDevice::CreateTexture(const TextureDesc& desc, const void* initialData) {
    if (!m_impl || !m_impl->device) return nullptr;
    return std::make_shared<MetalTexture>(m_impl->device, desc, initialData);
}

std::shared_ptr<IShader> MetalRenderDevice::CreateShader(const ShaderDesc& desc) {
    if (!m_impl || !m_impl->device) return nullptr;
    return std::make_shared<MetalShader>(m_impl->device, desc);
}

int MetalRenderDevice::GetWidth() const {
    return m_impl ? m_impl->width : 0;
}

int MetalRenderDevice::GetHeight() const {
    return m_impl ? m_impl->height : 0;
}

const std::string& MetalRenderDevice::GetTitle() const {
    static const std::string empty;
    return m_impl ? m_impl->title : empty;
}

} // namespace xbe::gpu

#endif // __APPLE__
