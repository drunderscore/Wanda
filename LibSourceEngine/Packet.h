#pragma once

#include <AK/EnumBits.h>
#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/Types.h>
#include <LibCrypto/Checksum/CRC32.h>
#include <LibSourceEngine/BitStream.h>
#include <LibSourceEngine/Message.h>

namespace SourceEngine
{
struct Packet
{
    constexpr static u16 compress_checksum_to_u16(u32 checksum)
    {
        return static_cast<u16>((checksum & 0xFFFF) ^ (checksum >> 16));
    }

    enum class Flags
    {
        Reliable = 1 << 0,
        Choked = 1 << 4,
        Challenge = 1 << 5
    };

    // This is what TF2 says, probably every other Source game too
    enum class Channel
    {
        Normal = 0,
        File,
        __Count
    };

    // FIXME: Better names, these are the same as the Engine
    static constexpr u32 max_file_size_bits = 26;
    static constexpr u32 fragment_bits = 8;

    static constexpr u32 fragment_size = 1 << fragment_bits;
};

class ConnectionlessPacket
{
public:
    static constexpr int packet_header = -1;

    ALWAYS_INLINE virtual char id() const = 0;

    ALWAYS_INLINE virtual ErrorOr<void> write(WritableBitStream& stream) const
    {
        TRY(stream.write_typed(id()));
        return {};
    }
};

// Receiving a packet and sending a packet are two vastly different operations, so we have two different types

class ReceivingPacket
{
public:
    class ChannelData
    {
    public:
        friend ReceivingPacket;

        u8 subchannel() const { return m_subchannel; }
        const ByteBuffer& data() const { return m_data; }

    private:
        ChannelData() = default;
        explicit ChannelData(ByteBuffer&& data) : m_data(move(data)) {}
        u8 m_subchannel{};
        ByteBuffer m_data;
    };

    // We need a MemoryBitStream here because we need to calculate the checksum from all bytes after the checksum,
    // without consuming them, and this is the best way without performing a redundant allocation.
    // FIXME: Would be nice to inline this?
    static ErrorOr<ReceivingPacket> read(MemoryBitStream& stream);

    int sequence() const { return m_sequence; }
    int sequence_ack() const { return m_sequence_ack; }
    Optional<int> challenge() const { return m_challenge; }
    const HashMap<Packet::Channel, ChannelData>& channel_data() const { return m_channel_data; }
    const ByteBuffer& unreliable_data() const { return m_unreliable_data; }

private:
    ALWAYS_INLINE static ErrorOr<void> read_channel(Packet::Channel, u8 subchannel, MemoryBitStream&, ReceivingPacket&);

    int m_sequence{};
    int m_sequence_ack{};
    u16 m_checksum{};
    u8 m_reliable_state{};
    Optional<u8> m_choked_number;
    Optional<int> m_challenge;
    HashMap<Packet::Channel, ChannelData> m_channel_data;
    // Unreliable data is stuffed at the end of a packet, if the client has enough room for it.
    ByteBuffer m_unreliable_data;
};

class SendingPacket
{
public:
    void add_reliable_message(Message& value) { m_reliable_messages.append(value); }
    void add_unreliable_message(Message& value) { m_unreliable_messages.append(value); };

    void set_sequence(int value) { m_sequence = value; }
    void set_sequence_ack(int value) { m_sequence_ack = value; }
    void set_challenge(int value) { m_challenge = value; }

    ErrorOr<ByteBuffer> write() const;

private:
    int m_sequence{};
    int m_sequence_ack{};
    Optional<u8> m_choked_number;
    Optional<int> m_challenge;
    // FIXME: This should own the messages. Can't say Vector<Message>, or we'll slice the objects, but we could OwnPtr
    //        it, but then whoever cals it would need an ownptr to a message? What if we require a Message&& and then
    //        move it into an ownptr? still might slice it because it wouldn't know the proper size of object it's
    //        moving into.
    Vector<Message&> m_reliable_messages;
    Vector<Message&> m_unreliable_messages;
};
}

AK_ENUM_BITWISE_OPERATORS(SourceEngine::Packet::Flags)