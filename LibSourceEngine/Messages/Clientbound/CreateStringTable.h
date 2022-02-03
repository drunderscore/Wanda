/*
 * Copyright (c) 2022, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <LibSourceEngine/Message.h>

namespace SourceEngine::Messages::Clientbound
{
class CreateStringTable final : public Message
{
public:
    static constexpr u8 constant_id = 12;

    ALWAYS_INLINE u8 id() const override { return constant_id; }

    static ErrorOr<CreateStringTable> read(ReadableBitStream& stream)
    {
        // TODO: Implement CreateStringTable!
        return Error::from_string_literal("TODO: Implement CreateStringTable!");
    }

    ALWAYS_INLINE ErrorOr<void> write(WritableBitStream& stream) const override
    {
        TRY(Message::write(stream));

        // TODO: I think this needs to be a power of 2 for the log2 operation below to make sense... verify this!
        constexpr u16 max_entries = 1024;

        TRY(stream << m_name);
        // TODO: Implement this properly!
        TRY(stream.write_typed<u16>(max_entries));
        TRY(stream.write_typed<int>(0, AK::log2(max_entries) + 1));
        TRY(stream.write_varint(0));
        TRY(stream.write(false));
        TRY(stream.write(false));

        return {};
    }

    const String& name() const { return m_name; }
    void set_name(String value) { m_name = move(value); }

private:
    String m_name;
};
}