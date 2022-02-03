/*
 * Copyright (c) 2022, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <AK/String.h>
#include <LibSourceEngine/Message.h>

namespace SourceEngine::Messages::Serverbound
{
class ClientInfo final : public Message
{
public:
    ALWAYS_INLINE u8 id() const override { return 8; }

    ALWAYS_INLINE ErrorOr<void> write(WritableBitStream& stream) const override
    {
        TRY(Message::write(stream));

        TRY(stream << m_server_count);
        TRY(stream << m_send_table_crc);
        TRY(stream.write(m_is_hltv));
        TRY(stream << m_friends_id);
        TRY(stream << m_friends_name);

        for (auto& custom_file_crc : m_custom_file_crc)
        {
            if (custom_file_crc.has_value())
            {
                TRY(stream.write(true));
                TRY(stream << *custom_file_crc);
            }
            else
            {
                TRY(stream.write(false));
            }
        }

        if (has_replay)
            TRY(stream.write(m_is_replay));

        return {};
    }

    static ErrorOr<ClientInfo> read(ReadableBitStream& stream)
    {
        ClientInfo info;

        TRY(stream >> info.m_server_count);
        TRY(stream >> info.m_send_table_crc);
        info.m_is_hltv = TRY(stream.read());
        TRY(stream >> info.m_friends_id);
        TRY(stream >> info.m_server_count);
        TRY(stream >> info.m_friends_name);

        for (auto& custom_file_crc : info.m_custom_file_crc)
        {
            if (TRY(stream.read()))
            {
                auto crc = TRY(stream.read_typed<u32>());
                custom_file_crc = crc;
            }
        }

        return info;
    }

private:
    // FIXME: Move these constants somewhere more accessible (they will be needed elsewhere)
    static constexpr u32 max_custom_files = 4;
    static constexpr bool has_replay = true;

    i32 m_server_count{};
    i32 m_send_table_crc{};
    bool m_is_hltv{};
    i32 m_friends_id{};
    String m_friends_name;
    Array<Optional<u32>, max_custom_files> m_custom_file_crc;
    // FIXME: Could use Conditional definition... we'd need an Empty type though, could use from Variant or make our
    //        own?
    bool m_is_replay{};
};
}