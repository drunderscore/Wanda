/*
 * Copyright (c) 2022, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <LibSourceEngine/Packet.h>

namespace SourceEngine::Packets::Connectionless::Clientbound
{
class ConnectReject final : public ConnectionlessPacket
{
public:
    static constexpr char constant_id = '9';

    ALWAYS_INLINE char id() const override { return constant_id; }

    ErrorOr<void> write(WritableBitStream& stream) const override
    {
        TRY(ConnectionlessPacket::write(stream));
        TRY(stream.write_typed(m_challenge));
        TRY(stream << m_reason);

        return {};
    }

    // TODO: Reader

    void set_challenge(int value) { m_challenge = value; }
    void set_reason(String value) { m_reason = move(value); }

private:
    int m_challenge{};
    String m_reason;
};
}