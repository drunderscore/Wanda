#pragma once

#include <LibSourceEngine/Packet.h>

namespace SourceEngine::Packets::Connectionless::Serverbound
{
class GetChallenge final : public ConnectionlessPacket
{
public:
    static constexpr char constant_id = 'q';

    ALWAYS_INLINE char id() const override { return constant_id; }

    ErrorOr<void> write(WritableBitStream& stream) const override
    {
        TRY(ConnectionlessPacket::write(stream));
        TRY(stream.write_typed(m_challenge));

        return {};
    }

    static ErrorOr<GetChallenge> read(ReadableBitStream& stream)
    {
        GetChallenge get_challenge;
        TRY(stream >> get_challenge.m_challenge);

        return get_challenge;
    }

    int challenge() const { return m_challenge; }

private:
    int m_challenge{};
};
}