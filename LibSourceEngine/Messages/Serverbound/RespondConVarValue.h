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
class RespondConVarValue final : public Message
{
public:
    enum class Response : u8
    {
        Success,
        NotFound,
        NotAConVar,
        CannotQuery
    };

    static constexpr u8 constant_id = 13;

    ALWAYS_INLINE u8 id() const override { return constant_id; }

    ALWAYS_INLINE ErrorOr<void> write(WritableBitStream& stream) const override
    {
        TRY(Message::write(stream));

        TRY(stream << m_cookie);
        TRY(stream.write_typed(static_cast<u8>(m_response), 4));
        TRY(stream << m_convar_name);
        TRY(stream << m_convar_value);

        return {};
    }

    static ErrorOr<RespondConVarValue> read(ReadableBitStream& stream)
    {
        RespondConVarValue info;

        TRY(stream >> info.m_cookie);
        auto response = TRY(stream.read_typed<u8>(4));
        info.m_response = static_cast<Response>(response);
        TRY(stream >> info.m_convar_name);
        TRY(stream >> info.m_convar_value);

        return info;
    }

    int cookie() const { return m_cookie; }
    void set_cookie(int value) { m_cookie = value; }
    Response response() const { return m_response; }
    void set_response(Response value) { m_response = value; }
    const String& convar_name() const { return m_convar_name; }
    void set_convar_name(String value) { m_convar_name = move(value); }
    const String& convar_value() const { return m_convar_value; }
    void set_convar_value(String value) { m_convar_value = move(value); }

private:
    int m_cookie{};
    Response m_response{Response::NotFound};
    String m_convar_name;
    String m_convar_value;
};
}