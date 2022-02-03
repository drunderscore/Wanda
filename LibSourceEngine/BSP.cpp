/*
 * Copyright (c) 2022, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <LibSourceEngine/BSP.h>
#include <LibSourceEngine/CoreStreamOperators.h>

namespace SourceEngine
{
ErrorOr<BSP> BSP::try_parse(Core::Stream::SeekableStream& stream)
{
    u32 signature;
    TRY(stream >> signature);

    if (signature != BSP::signature)
        return Error::from_string_literal("Invalid BSP signature");

    BSP bsp;

    TRY(stream >> bsp.m_version);

    for (auto i = 0; i < number_of_lumps; i++)
    {
        Lump lump;

        u32 offset;
        u32 length;
        TRY(stream >> offset);
        TRY(stream >> length);

        TRY(stream >> lump.m_version);
        TRY(stream >> lump.m_uncompressed_size);

        lump.m_data = TRY(ByteBuffer::create_uninitialized(length));

        // FIXME: Do we need this tell? Does seek return the position _before_ or _after_ seeking?
        auto position_to_return_to = TRY(stream.tell());
        TRY(stream.seek(offset, Core::Stream::SeekMode::SetPosition));
        TRY(stream.read(lump.m_data.bytes()));
        TRY(stream.seek(position_to_return_to, Core::Stream::SeekMode::SetPosition));

        bsp.m_lumps[i] = move(lump);
    }

    TRY(stream >> bsp.m_map_revision);

    return bsp;
}

Crypto::Hash::MD5::DigestType BSP::calculate_md5_hash() const
{
    Crypto::Hash::MD5 md5;

    for (auto i = 0; i < number_of_lumps; i++)
    {
        // Don't hash the Entities lump
        if (static_cast<SourceEngine::BSP::Lump::Type>(i) == SourceEngine::BSP::Lump::Type::Entities)
            continue;

        auto& lump = m_lumps[i];
        md5.update(lump.data());
    }

    return md5.digest();
}
}