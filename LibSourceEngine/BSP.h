#pragma once

#include <AK/ByteBuffer.h>
#include <AK/Error.h>
#include <AK/Stream.h>
#include <AK/Types.h>
#include <LibCore/Stream.h>
#include <LibCrypto/Hash/MD5.h>

namespace SourceEngine
{
class BSP
{
public:
    static constexpr u32 number_of_lumps = 64;

    class Lump
    {
    public:
        friend BSP;

        // FIXME: Some missing lump types are actually used in other games, and some games use different lumps
        //        entirely for the same type. Most of this is from public/bspfile.h
        enum class Type
        {
            Entities,
            Planes,
            TextureData,
            // The Engine actually refers to this type as "Vertexes", but we can take the liberty of renaming this to
            // the proper pluralization :^)
            Vertices,
            Visibility,
            Nodes,
            TextureInfo,
            Faces,
            Lighting,
            Occlusion,
            Leafs,
            FaceIDs,
            Edges,
            SurfaceEdges,
            Models,
            WorldLights,
            LeafFaces,
            LeafBrushes,
            Brushes,
            BrushSides,
            Areas,
            AreaPortals,
            DisplacementInfo = 26,
            OriginalFaces,
            // TODO: Properly name this (taken from the Engine)
            PhysDisp,
            PhysCollide,
            VertexNormals,
            VertexNormalIndices,
            DisplacementLightmapAlphas,
            DisplacementVertices,
            DisplacementLightmapSamplePositions,
            Game,
            LeafWaterData,
            Primitive,
            PrimitiveVertices,
            PrimitiveIndices,
            PakFile,
            ClipPortalVertices,
            Cubemaps,
            TextureDataStringData,
            TextureDataStringTable,
            Overlays,
            LeafMinimumDistanceToWater,
            FaceMacroTextureInfo,
            DisplacementTris,
            PhysCollideSurface,
            WaterOverlays,
            LeafAmbientIndexHDR,
            LeafAmbientIndex,
            LightingHDR,
            WorldLightsHDR,
            LeafAmbientLightingHDR,
            LeafAmbientLighting,
            XZipPackFile,
            FacesHDR,
            MapFlags,
            OverlayFades
        };

        const ByteBuffer& data() const { return m_data; }
        u32 version() const { return m_version; }

    private:
        ByteBuffer m_data;
        u32 m_version{};
        u32 m_uncompressed_size{};
    };

    static ErrorOr<BSP> try_parse(Core::Stream::SeekableStream&);

    u32 version() const { return m_version; }
    u32 map_revision() const { return m_map_revision; }
    const Array<Lump, number_of_lumps>& lumps() const { return m_lumps; }
    const Lump& lump(Lump::Type type) const { return m_lumps[static_cast<size_t>(type)]; }

    // Calculate an MD5 hash for this BSP, the same way the Engine would.
    Crypto::Hash::MD5::DigestType calculate_md5_hash() const;

private:
    static constexpr u32 signature = 0x50534256;

    u32 m_version{};
    Array<Lump, number_of_lumps> m_lumps{};
    u32 m_map_revision{};
};
}