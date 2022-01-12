#pragma once

#include <LibSourceEngine/AuthProtocol.h>
#include <LibSourceEngine/Packet.h>

namespace SourceEngine::Packets::Connectionless::Clientbound
{
class Connection final : public ConnectionlessPacket
{
public:
    static constexpr char constant_id = 'B';

    ALWAYS_INLINE char id() const override { return constant_id; }

    ErrorOr<void> write(WritableBitStream& stream) const override
    {
        TRY(ConnectionlessPacket::write(stream));
        TRY(stream.write_typed(m_challenge));

        return {};
    }

    // TODO: Reader

    void set_challenge(int value) { m_challenge = value; }

private:
    int m_challenge{};
};
}