/*
 * Copyright (c) 2022, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <AK/Stream.h>
#include <AK/Vector.h>
#include <LibCore/File.h>
#include <LibCore/FileStream.h>
#include <LibCrypto/Checksum/CRC32.h>
#include <LibSourceEngine/VPK.h>

namespace SourceEngine
{
ErrorOr<VPK> VPK::try_parse_from_file_path(
    StringView path, Function<ErrorOr<NonnullOwnPtr<Core::Stream::SeekableStream>>(u16)> resolve_external_archive)
{
    auto vpk_file = TRY(Core::File::open(String::formatted("{}_dir.vpk", path), Core::OpenMode::ReadOnly));
    Core::InputFileStream stream(vpk_file);

    u32 signature;
    stream >> signature;

    if (signature != VPK::signature)
        return Error::from_string_literal("Invalid VPK signature");

    VPK vpk;

    u32 version;

    stream >> version;

    if (version != 2)
        return Error::from_string_literal("VPK version must be 2");

    stream >> vpk.m_tree_size;
    stream >> vpk.m_file_data_section_size;
    stream >> vpk.m_archive_md5_section_size;
    stream >> vpk.m_other_md5_section_size;
    stream >> vpk.m_signature_section_size;

    // Files are represented as a tree, as strings, like this
    // > Extension
    //     > Directory Path
    //         > Name
    // A tree end when an empty string is encountered

    // FIXME: This C-style forward-declaration and duplicated read_string is kinda yucky, any way we can improve it?
    String extension;
    String directory_path;
    String name;

    extension = read_string(stream);

    while (!extension.is_empty())
    {
        directory_path = read_string(stream);

        while (!directory_path.is_empty())
        {
            name = read_string(stream);

            while (!name.is_empty())
            {
                Entry entry;

                stream >> entry.m_crc;
                stream >> entry.m_preload_bytes;
                stream >> entry.m_archive_index;
                stream >> entry.m_entry_offset;
                stream >> entry.m_entry_length;

                // TODO: Support archive data inside the directory
                if (entry.m_archive_index == 0x7fff)
                    return Error::from_string_literal("No support for archive data within the directory");

                u16 terminator;
                stream >> terminator;

                // TODO: This might not be 0xFFFF if there is preload data right after, which we can check for...
                if (terminator != 0xFFFF)
                    return Error::from_string_literal("Expected 0xFFFF for a terminator");

                if (!vpk.m_archive_streams.contains(entry.m_archive_index))
                    vpk.m_archive_streams.set(entry.m_archive_index,
                                              TRY(resolve_external_archive(entry.m_archive_index)));

                entry.m_archive_stream = vpk.m_archive_streams.find(entry.m_archive_index)->value;

                // QUIRK: A single space as the directory path represents the root
                // FIXME: Should this use an AK::LexicalPath? What if m_entries stored AK::LexicalPath?
                if(directory_path[0] == ' ')
                    vpk.m_entries.set(String::formatted("{}.{}", name, extension), move(entry));
                else
                    vpk.m_entries.set(String::formatted("{}/{}.{}", directory_path, name, extension), move(entry));

                name = read_string(stream);
            }

            directory_path = read_string(stream);
        }

        extension = read_string(stream);
    }

    return vpk;
}

ErrorOr<VPK> VPK::try_parse_from_file_path(StringView path)
{
    return try_parse_from_file_path(
        path, [&](auto archive_index) -> ErrorOr<NonnullOwnPtr<Core::Stream::SeekableStream>> {
            return TRY(Core::Stream::File::open(String::formatted("{}_{:#03}.vpk", path, archive_index),
                                                Core::Stream::OpenMode::Read));
        });
}

ErrorOr<ByteBuffer> VPK::Entry::read_data_from_archive(bool verify_against_crc) const
{
    auto buffer = TRY(ByteBuffer::create_uninitialized(m_entry_length));

    TRY(m_archive_stream->seek(m_entry_offset, Core::Stream::SeekMode::SetPosition));
    TRY(m_archive_stream->read(buffer));

    if (verify_against_crc)
    {
        Crypto::Checksum::CRC32 crc_checksum;
        crc_checksum.update(buffer.bytes());

        if (crc_checksum.digest() != m_crc)
            return Error::from_string_literal("VPK::Entry CRC did not match");
    }

    return buffer;
}

// FIXME: This doesn't really belong here :^(
String VPK::read_string(InputStream& stream)
{
    Vector<char> characters;

    char c;

    stream >> c;

    while (c != '\0')
    {
        characters.append(c);
        stream >> c;
    }

    return {characters.data(), characters.size()};
}
}