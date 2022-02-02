#include <LibSourceEngine/VTF.h>

template<typename T, size_t Size>
InputStream& operator>>(InputStream& stream, Array<T, Size>& value)
{
    for (auto i = 0; i < Size; i++)
        stream >> value[i];

    return stream;
}

namespace SourceEngine
{
ErrorOr<VTF> VTF::try_parse(InputStream& stream)
{
    u32 signature;

    stream >> signature;

    if (signature != VTF::signature)
        return Error::from_string_literal("Invalid VTF signature");

    VTF vtf;

    stream >> vtf.m_major_version;
    stream >> vtf.m_minor_version;

    // 7 is the only version, ever.
    if (vtf.m_major_version != 7)
        return Error::from_string_literal("Invalid major version");

    // We don't support anything past 7.2
    // FIXME: Support VTF 7.3+
    if (vtf.m_minor_version >= 3)
        return Error::from_string_literal("Invalid minor version");

    u32 header_size;
    stream >> header_size;
    stream >> vtf.m_width;
    stream >> vtf.m_height;
    u32 flags;
    stream >> flags;
    vtf.m_flags = static_cast<Flags>(flags);
    stream >> vtf.m_frames;
    stream >> vtf.m_first_frame;

    // FIXME: Is there no nice way to skip bytes with an InputStream?
    [[maybe_unused]] u32 padding_0;
    stream >> padding_0;

    stream >> vtf.m_reflectivity;

    [[maybe_unused]] u32 padding_1;
    stream >> padding_1;

    stream >> vtf.m_bumpmap_scale;

    u32 high_resolution_format;
    stream >> high_resolution_format;
    vtf.m_high_resolution_format = static_cast<ImageFormat>(high_resolution_format);
    stream >> vtf.m_number_of_mipmaps;
    u32 low_resolution_format;
    stream >> low_resolution_format;
    vtf.m_low_resolution_format = static_cast<ImageFormat>(low_resolution_format);
    stream >> vtf.m_low_resolution_width;
    stream >> vtf.m_low_resolution_height;

    // We only check the minor version, because we've already checked cif the major version is 7
    if (vtf.m_minor_version >= 2)
        stream >> vtf.m_depth;

    return vtf;
}
}