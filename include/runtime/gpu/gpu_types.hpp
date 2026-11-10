#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <array>
#include <cmath>

namespace xbe::gpu {

// ============================================================================
// Math Primitives
// ============================================================================

struct Vector2 {
    float x{0.0f}, y{0.0f};
};

struct Vector3 {
    float x{0.0f}, y{0.0f}, z{0.0f};
};

struct Vector4 {
    float x{0.0f}, y{0.0f}, z{0.0f}, w{1.0f};
};

struct Color4 {
    float r{1.0f}, g{1.0f}, b{1.0f}, a{1.0f};
};

struct Matrix4x4 {
    // Column-major 4x4 matrix for Metal / WebGPU compatibility
    std::array<float, 16> m{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    static Matrix4x4 Identity() {
        return Matrix4x4();
    }

    static Matrix4x4 Perspective(float fovRad, float aspect, float zNear, float zFar) {
        Matrix4x4 result{};
        float tanHalfFov = std::tan(fovRad * 0.5f);
        result.m[0] = 1.0f / (aspect * tanHalfFov);
        result.m[5] = 1.0f / tanHalfFov;
        // Metal/WebGPU depth range [0, 1]
        result.m[10] = zFar / (zFar - zNear);
        result.m[11] = 1.0f;
        result.m[14] = -(zNear * zFar) / (zFar - zNear);
        result.m[15] = 0.0f;
        return result;
    }

    static Matrix4x4 Ortho(float left, float right, float bottom, float top, float zNear, float zFar) {
        Matrix4x4 result{};
        result.m[0] = 2.0f / (right - left);
        result.m[5] = 2.0f / (top - bottom);
        result.m[10] = 1.0f / (zFar - zNear);
        result.m[12] = -(right + left) / (right - left);
        result.m[13] = -(top + bottom) / (top - bottom);
        result.m[14] = -zNear / (zFar - zNear);
        result.m[15] = 1.0f;
        return result;
    }

    static Matrix4x4 RotationY(float angleRad) {
        Matrix4x4 result = Identity();
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        result.m[0] = c;
        result.m[2] = -s;
        result.m[8] = s;
        result.m[10] = c;
        return result;
    }

    static Matrix4x4 RotationX(float angleRad) {
        Matrix4x4 result = Identity();
        float c = std::cos(angleRad);
        float s = std::sin(angleRad);
        result.m[5] = c;
        result.m[6] = s;
        result.m[9] = -s;
        result.m[10] = c;
        return result;
    }

    static Matrix4x4 Translation(float tx, float ty, float tz) {
        Matrix4x4 result = Identity();
        result.m[12] = tx;
        result.m[13] = ty;
        result.m[14] = tz;
        return result;
    }

    static Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b) {
        Matrix4x4 r{};
        for (int c = 0; c < 4; ++c) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += a.m[k * 4 + row] * b.m[c * 4 + k];
                }
                r.m[c * 4 + row] = sum;
            }
        }
        return r;
    }
};

// ============================================================================
// Vertex Formats
// ============================================================================

struct Vertex3D {
    Vector3 position;
    Vector3 normal;
    Color4  color;
    Vector2 texCoord;
};

// ============================================================================
// GPU Enums
// ============================================================================

enum class PrimitiveTopology : uint8_t {
    PointList = 0,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    TriangleFan
};

enum class IndexFormat : uint8_t {
    UInt16 = 0,
    UInt32
};

enum class PixelFormat : uint8_t {
    RGBA8_UNorm = 0,
    BGRA8_UNorm,
    RGBA16_Float,
    Depth32_Float,
    Depth24_Stencil8
};

enum class CullMode : uint8_t {
    None = 0,
    Front,
    Back
};

enum class FillMode : uint8_t {
    Solid = 0,
    Wireframe
};

enum class CompareFunction : uint8_t {
    Never = 0,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always
};

enum class BlendFactor : uint8_t {
    Zero = 0,
    One,
    SrcColor,
    InvSrcColor,
    SrcAlpha,
    InvSrcAlpha,
    DestAlpha,
    InvDestAlpha,
    DestColor,
    InvDestColor
};

enum class BlendOp : uint8_t {
    Add = 0,
    Subtract,
    RevSubtract,
    Min,
    Max
};

// ============================================================================
// Direct3D 8 Fixed-Function State Representation
// ============================================================================

struct D3D8Material {
    Color4 diffuse{1.0f, 1.0f, 1.0f, 1.0f};
    Color4 ambient{0.2f, 0.2f, 0.2f, 1.0f};
    Color4 specular{0.0f, 0.0f, 0.0f, 1.0f};
    Color4 emissive{0.0f, 0.0f, 0.0f, 1.0f};
    float  power{0.0f};
};

struct D3D8Light {
    bool    enabled{false};
    uint32_t type{0}; // 1 = Point, 2 = Spot, 3 = Directional
    Color4  diffuse{1.0f, 1.0f, 1.0f, 1.0f};
    Color4  specular{1.0f, 1.0f, 1.0f, 1.0f};
    Color4  ambient{0.0f, 0.0f, 0.0f, 1.0f};
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 direction{0.0f, 0.0f, 1.0f};
    float   range{1000.0f};
    float   attenuation0{1.0f};
    float   attenuation1{0.0f};
    float   attenuation2{0.0f};
};

struct D3D8TextureStage {
    uint32_t colorOp{4};     // D3DTOP_MODULATE default
    uint32_t colorArg1{2};   // D3DTA_TEXTURE
    uint32_t colorArg2{1};   // D3DTA_CURRENT
    uint32_t alphaOp{4};     // D3DTOP_MODULATE default
    uint32_t alphaArg1{2};   // D3DTA_TEXTURE
    uint32_t alphaArg2{1};   // D3DTA_CURRENT
};

struct D3D8FixedFunctionState {
    Matrix4x4 world{Matrix4x4::Identity()};
    Matrix4x4 view{Matrix4x4::Identity()};
    Matrix4x4 projection{Matrix4x4::Identity()};

    D3D8Material material{};
    std::array<D3D8Light, 8> lights{};
    std::array<D3D8TextureStage, 4> stages{};

    bool lightingEnabled{false};
    bool zEnable{true};
    bool zWriteEnable{true};
    CompareFunction zFunc{CompareFunction::LessEqual};

    bool alphaTestEnable{false};
    float alphaRef{0.0f};
    CompareFunction alphaFunc{CompareFunction::Greater};

    bool blendEnable{false};
    BlendFactor srcBlend{BlendFactor::SrcAlpha};
    BlendFactor destBlend{BlendFactor::InvSrcAlpha};
    BlendOp blendOp{BlendOp::Add};

    CullMode cullMode{CullMode::Back};
    FillMode fillMode{FillMode::Solid};
};

// ============================================================================
// Viewport & Scissor
// ============================================================================

struct Viewport {
    float x{0.0f};
    float y{0.0f};
    float width{1280.0f};
    float height{720.0f};
    float minDepth{0.0f};
    float maxDepth{1.0f};
};

struct ScissorRect {
    int32_t x{0};
    int32_t y{0};
    uint32_t width{1280};
    uint32_t height{720};
};

// ============================================================================
// Resource Descriptions
// ============================================================================

struct TextureDesc {
    uint32_t width{1};
    uint32_t height{1};
    uint32_t depth{1};
    uint32_t mipLevels{1};
    PixelFormat format{PixelFormat::RGBA8_UNorm};
    bool isRenderTarget{false};
};

struct BufferDesc {
    size_t size{0};
    bool isIndexBuffer{false};
    bool isDynamic{true};
};

struct ShaderDesc {
    std::string vertexSource;
    std::string fragmentSource;
    std::string vertexEntryPoint{"vertex_main"};
    std::string fragmentEntryPoint{"fragment_main"};
};

struct RenderStateDesc {
    CullMode cullMode{CullMode::Back};
    FillMode fillMode{FillMode::Solid};
    bool depthTestEnable{true};
    bool depthWriteEnable{true};
    CompareFunction depthFunc{CompareFunction::LessEqual};
    bool blendEnable{false};
    BlendFactor srcColorBlend{BlendFactor::SrcAlpha};
    BlendFactor dstColorBlend{BlendFactor::InvSrcAlpha};
    BlendOp colorBlendOp{BlendOp::Add};
    BlendFactor srcAlphaBlend{BlendFactor::One};
    BlendFactor dstAlphaBlend{BlendFactor::InvSrcAlpha};
    BlendOp alphaBlendOp{BlendOp::Add};
};

} // namespace xbe::gpu
