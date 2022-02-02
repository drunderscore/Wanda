#pragma once

#include <AK/Stream.h>
#include <AK/Types.h>

namespace SourceEngine
{
class VTF
{
public:
    // FIXME: These enum names are not very standard with what I normally would write...
    enum class ImageFormat
    {
        None = -1,
        RGBA8888 = 0,
        ABGR8888,
        RGB888,
        BGR888,
        RGB565,
        I8,
        IA88,
        P8,
        A8,
        RGB888_BlueScreen,
        BGR888_BlueScreen,
        ARGB8888,
        BGRA8888,
        DXT1,
        DXT3,
        DXT5,
        BGRX8888,
        BGR565,
        BGRX5551,
        BGRA4444,
        DXT1_OneBitAlpha,
        BGRA5551,
        UV88,
        UVWQ8888,
        RGBA16161616F,
        RGBA16161616,
        UVLX8888
    };

    enum class Flags
    {
        PointSample = 1 << 0,
        Trilinear = 1 << 1,
        ClampS = 1 << 2,
        ClampT = 1 << 3,
        Anisotropic = 1 << 4,
        HintDXT = 1 << 5,
        PWLCorrected = 1 << 6,
        Normal = 1 << 7,
        NoMip = 1 << 8,
        NoLOD = 1 << 9,
        AllMips = 1 << 10,
        Procedural = 1 << 11,
        OneBitAlpha = 1 << 12,
        EightBitAlpha = 1 << 13,
        EnvMap = 1 << 14,
        RenderTarget = 1 << 15,
        DepthRenderTarget = 1 << 16,
        NoDebugOverride = 1 << 17,
        SinlgeCopy = 1 << 18,
        PreSRGB = 1 << 19,
        NoDepthBuffer = 1 << 23,
        ClampU = 1 << 25,
        VertexTexture = 1 << 26,
        SSBump = 1 << 27,
        Border = 1 << 29
    };

    static ErrorOr<VTF> try_parse(InputStream&);

    u32 major_version() const { return m_major_version; }
    u32 minor_version() const { return m_minor_version; }
    u16 width() const { return m_width; }
    u16 height() const { return m_height; }
    Flags flags() const { return m_flags; }
    u16 frames() const { return m_frames; }
    u8 number_of_mipmaps() const { return m_number_of_mipmaps; }

private:
    static constexpr u32 signature = 0x00465456;

    u32 m_major_version{};
    u32 m_minor_version{};
    u16 m_width{};
    u16 m_height{};
    Flags m_flags{};
    u16 m_frames{};
    u16 m_first_frame{};
    Array<float, 3> m_reflectivity{};
    float m_bumpmap_scale{};
    ImageFormat m_high_resolution_format{};
    u8 m_number_of_mipmaps{};
    ImageFormat m_low_resolution_format{};
    u8 m_low_resolution_width{};
    u8 m_low_resolution_height{};

    Optional<u16> m_depth;
};
}