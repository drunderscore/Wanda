/*
 * Copyright (c) 2022, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <LibSourceEngine/AuthProtocol.h>
#include <LibSourceEngine/Packet.h>

namespace SourceEngine::Packets::Connectionless::Clientbound
{
class Challenge final : public ConnectionlessPacket
{
public:
    static constexpr char constant_id = 'A';

    ALWAYS_INLINE char id() const override { return constant_id; }

    ErrorOr<void> write(WritableBitStream& stream) const override
    {
        TRY(ConnectionlessPacket::write(stream));
        TRY(stream.write_typed(m_magic_version));
        TRY(stream.write_typed(m_challenge));
        TRY(stream.write_typed(m_client_challenge));
        TRY(stream.write_typed(static_cast<int>(m_auth_protocol)));
        TRY(stream.write_typed<u16>(0)); // Legacy Steam2 encryption key, doesn't exist anymore
        TRY(stream.write_typed(m_steam_id));
        TRY(stream.write_typed<u8>(m_is_secure ? 1 : 0));

        return {};
    }

    // TODO: Reader

    void set_magic_version(int value) { m_magic_version = value; }
    void set_challenge(int value) { m_challenge = value; }
    void set_client_challenge(int value) { m_client_challenge = value; }
    void set_auth_protocol(AuthProtocol value) { m_auth_protocol = value; }
    void set_steam_id(u64 value) { m_steam_id = value; }
    void set_is_secure(bool value) { m_is_secure = value; }

private:
    int m_magic_version{};
    int m_challenge{};
    int m_client_challenge{};
    AuthProtocol m_auth_protocol{};
    u64 m_steam_id{};
    bool m_is_secure{};
};
}