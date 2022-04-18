/*
 * Copyright (c) 2022, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <LibSourceEngine/Message.h>

namespace SourceEngine::Messages::Clientbound
{
class GetConVarValue final : public Message
{
public:
    static constexpr u8 constant_id = 31;

    ALWAYS_INLINE u8 id() const override { return constant_id; }

    static ErrorOr<GetConVarValue> read(ReadableBitStream& stream)
    {
        GetConVarValue get_convar_value;

        TRY(stream >> get_convar_value.m_cookie);
        TRY(stream >> get_convar_value.m_convar_name);

        return get_convar_value;
    }

    ALWAYS_INLINE ErrorOr<void> write(WritableBitStream& stream) const override
    {
        TRY(Message::write(stream));

        TRY(stream << m_cookie);
        TRY(stream << m_convar_name);

        return {};
    }

    int cookie() const { return m_cookie; }
    void set_cookie(int value) { m_cookie = value; }
    const String& convar_name() const { return m_convar_name; }
    void set_convar_name(String value) { m_convar_name = move(value); }

private:
    int m_cookie{};
    String m_convar_name;
};
}