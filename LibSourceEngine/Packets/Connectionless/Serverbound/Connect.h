/*
 * Copyright (c) 2022, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <LibSourceEngine/AuthProtocol.h>
#include <LibSourceEngine/Packet.h>

namespace SourceEngine::Packets::Connectionless::Serverbound
{
class Connect final : public ConnectionlessPacket
{
public:
    static constexpr char constant_id = 'k';

    ALWAYS_INLINE char id() const override { return constant_id; }

    ErrorOr<void> write(WritableBitStream& stream) const override
    {
        // TODO: Write a writer!
        return Error::from_string_literal("Unimplemented Connect::write");

        return {};
    }

    static ErrorOr<Connect> read(ReadableBitStream& stream)
    {
        Connect connect;
        TRY(stream >> connect.m_protocol_version);
        connect.m_auth_protocol = static_cast<AuthProtocol>(TRY(stream.read_typed<int>()));

        // TODO: Support more auth protocols? I doubt there is a reason to, ever.
        if (connect.m_auth_protocol != AuthProtocol::Steam)
            return Error::from_string_literal("Cannot use any other auth protocol than Steam");

        TRY(stream >> connect.m_server_challenge);
        TRY(stream >> connect.m_client_challenge);
        TRY(stream >> connect.m_client_name);
        TRY(stream >> connect.m_password);
        TRY(stream >> connect.m_version_string);
        auto steam_cookie_length = TRY(stream.read_typed<u16>());
        connect.m_steam_cookie = TRY(stream.read_bytes(steam_cookie_length));

        return connect;
    }

    int product_version() const { return m_protocol_version; }
    AuthProtocol auth_protocol() const { return m_auth_protocol; }
    int server_challenge() const { return m_server_challenge; }
    int client_challenge() const { return m_client_challenge; }
    const String& client_name() const { return m_client_name; }
    const String& password() const { return m_password; }
    const String& version_string() const { return m_version_string; }
    const ByteBuffer& steam_cookie() const { return m_steam_cookie; }

private:
    int m_protocol_version{};
    AuthProtocol m_auth_protocol{};
    int m_server_challenge{};
    int m_client_challenge{};
    String m_client_name;
    String m_password;
    String m_version_string;
    ByteBuffer m_steam_cookie;
};
}