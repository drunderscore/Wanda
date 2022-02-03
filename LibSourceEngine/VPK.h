/*
 * Copyright (c) 2022, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <AK/Error.h>
#include <AK/Function.h>
#include <AK/StdLibExtraDetails.h>
#include <AK/String.h>
#include <AK/Types.h>
#include <LibCore/Stream.h>

namespace SourceEngine
{

// There are two versions of the VPK format still in use. The very original VPK format does not have a signature, and
// has not been used since 2009, so we do not support it.

// This implementation covers version 2 of VPK, as it is used by games I am more interested in supporting
// (TF2 and CS:GO, plus many others). We should sill have a version 1 implementation in the future, as it's used by
// games that are worthwhile to support too (L4D2, SFM)
// Much of this implementation is based on https://developer.valvesoftware.com/wiki/VPK_File_Format
// TODO: Support Version 1, for games like L4D2 and SFM.
// TODO: Check CRC of entries
// TODO: What is preload_bytes inside of VPK::Entry? What is preload?
class VPK
{
public:
    class Entry
    {
    public:
        friend VPK;

        u32 crc() const { return m_crc; }
        const ByteBuffer& data() const { return m_data; }

    private:
        u32 m_crc{};
        u16 m_preload_bytes{};
        u16 m_archive_index{};
        u32 m_entry_offset{};
        u32 m_entry_length{};
        ByteBuffer m_data;
    };

    static ErrorOr<VPK> try_parse_from_file_path(
        StringView path,
        Function<ErrorOr<NonnullOwnPtr<Core::Stream::SeekableStream>>(u16 archive_index)> resolve_external_archive);
    static ErrorOr<VPK> try_parse_from_file_path(StringView path);

    const HashMap<String, Entry>& entries() const { return m_entries; }

private:
    static constexpr u32 signature = 0x55AA1234;

    u32 m_tree_size{};
    u32 m_file_data_section_size{};
    u32 m_archive_md5_section_size{};
    u32 m_other_md5_section_size{};
    u32 m_signature_section_size{};

    HashMap<String, Entry> m_entries;

    static String read_string(InputStream&);
};
}