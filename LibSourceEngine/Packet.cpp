/*
 * Copyright (c) 2022, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <LibCrypto/Checksum/CRC32.h>
#include <LibSourceEngine/Packet.h>

namespace SourceEngine
{
ErrorOr<ReceivingPacket> ReceivingPacket::read(MemoryBitStream& stream)
{
    ReceivingPacket packet;

    TRY(stream >> packet.m_sequence);
    TRY(stream >> packet.m_sequence_ack);

    // The amount of padding bits at the end is also included in these flags, but I'm not sure if we care too much
    // about those.
    auto flags = static_cast<Packet::Flags>(TRY(stream.read_typed<u8>()));
    TRY(stream >> packet.m_checksum);

    auto bytes_to_checksum = stream.readonly_bytes().slice(TRY(stream.position()) >> 3);

    auto our_calculated_checksum = Crypto::Checksum::CRC32(bytes_to_checksum).digest();
    auto our_calculated_checksum_compressed = Packet::compress_checksum_to_u16(our_calculated_checksum);

    if (our_calculated_checksum_compressed != packet.m_checksum)
        return Error::from_string_literal("Checksum does not match data");

    TRY(stream >> packet.m_reliable_state);

    if (has_flag(flags, Packet::Flags::Choked))
    {
        auto choked_number = TRY(stream.read_typed<u8>());
        packet.m_choked_number = choked_number;
    }

    if (has_flag(flags, Packet::Flags::Challenge))
    {
        auto challenge = TRY(stream.read_typed<int>());
        packet.m_challenge = challenge;
    }

    if (has_flag(flags, Packet::Flags::Reliable))
    {
        auto subchannel = TRY(stream.read_typed<u8>(3));

        for (auto i = 0; i < static_cast<size_t>(Packet::Channel::__Count); i++)
        {
            // This channel has data
            if (TRY(stream.read()))
                TRY(read_channel(static_cast<Packet::Channel>(i), subchannel, stream, packet));
        }
    }

    // FIXME: This calculation kinda sucks, any way we can do better?
    //        If we are on a byte boundary, we don't want an additional byte, but otherwise we need to include that
    //        last partial byte
    auto unreliable_data_position = TRY(stream.position());
    auto remaining_length_in_bytes = stream.readonly_bytes().size() -
                                     ((unreliable_data_position >> 3) + (unreliable_data_position % 8 == 0 ? 0 : 1));
    packet.m_unreliable_data = TRY(stream.read_bytes(remaining_length_in_bytes));

    return packet;
}

ErrorOr<void> ReceivingPacket::read_channel(Packet::Channel channel, u8 subchannel, MemoryBitStream& stream,
                                            ReceivingPacket& packet)
{
    // Engine calls this "is_single_block", where I assume a "block" means a fragment (like they use everywhere else...)
    // I'm going to rename this to a much more fitting name, but it also makes the meaning in the Engine inverse.
    auto is_fragmented = TRY(stream.read());

    // FIXME: Know how to deal with fragmented channel data
    if (is_fragmented)
        return Error::from_string_literal("Don't know how to deal with fragmented channel data");

    auto compressed = TRY(stream.read());

    // FIXME: Know how to deal with compressed channel data
    if (compressed)
        return Error::from_string_literal("Don't know how to deal with compressed channel data");

    auto data_size_in_bytes = TRY(stream.read_varint());

    auto data_buffer = TRY(stream.read_bytes(data_size_in_bytes));
    ChannelData channel_data(move(data_buffer));
    channel_data.m_subchannel = subchannel;

    packet.m_channel_data.set(channel, move(channel_data));

    return {};
}

ErrorOr<ByteBuffer> SendingPacket::write() const
{
    SourceEngine::ExpandingBitStream bit_stream;
    TRY(bit_stream << m_sequence);
    TRY(bit_stream << m_sequence_ack);

    Packet::Flags flags{};

    if (m_challenge.has_value())
        flags |= Packet::Flags::Challenge;

    if (m_choked_number.has_value())
        flags |= Packet::Flags::Choked;

    auto is_reliable = m_reliable_messages.size() > 0;

    if (is_reliable)
        return Error::from_string_literal("TODO Write reliable messages to packets");

    if (is_reliable)
        flags |= Packet::Flags::Reliable;

    // We're going to modify this later
    auto flags_position = TRY(bit_stream.position());
    TRY(bit_stream << static_cast<u8>(flags));
    // We'll write the checksum later
    auto checksum_position = TRY(bit_stream.position());
    TRY(bit_stream.write_typed<u16>(0));
    auto position_to_checksum_from = TRY(bit_stream.position());

    TRY(bit_stream.write_typed<u8>(0)); // Reliable state

    if (m_choked_number.has_value())
        TRY(bit_stream << *m_choked_number);

    if (m_challenge.has_value())
        TRY(bit_stream << *m_challenge);

    if (is_reliable)
    {
        // This is the subchannel index. There are 8 of them.
        TRY(bit_stream.write_typed<u8>(0, 3));

        for (auto i = 0; i < static_cast<size_t>(Packet::Channel::__Count); i++)
        {
            // We don't support any other channel
            if (static_cast<Packet::Channel>(i) == Packet::Channel::Normal)
            {
                TRY(bit_stream.write(true));
                TRY(bit_stream.write(false)); // Not fragmented
                TRY(bit_stream.write(false)); // Not compressed
            }
            else
            {
                TRY(bit_stream.write(false));
            }
        }
    }

    for (auto& message : m_unreliable_messages)
        TRY(message.write(bit_stream));

    // Pad the remaining bits explicitly (WE don't have to, but the Engine does, and we have to know how many to pad to
    // put it in the flags)

    auto additional_bits = (int)(TRY(bit_stream.position()) % 8);
    auto flags_integral = static_cast<u8>(flags);

    if (additional_bits > 0)
    {
        auto bits_to_pad = 8 - additional_bits;
        flags_integral |= ((bits_to_pad << 5) & 0xFF);
        TRY(bit_stream.write_typed<u8>(0, bits_to_pad));
    }

    auto end_position = TRY(bit_stream.position());

    TRY(bit_stream.set_position(flags_position));
    TRY(bit_stream << flags_integral);

    TRY(bit_stream.set_position(end_position));

    auto checksum = Crypto::Checksum::CRC32(bit_stream.bytes().slice(position_to_checksum_from >> 3)).digest();
    auto compressed_checksum = Packet::compress_checksum_to_u16(checksum);
    TRY(bit_stream.set_position(checksum_position));
    TRY(bit_stream << compressed_checksum);
    TRY(bit_stream.set_position(end_position));

    return bit_stream.release_bytes();
}
}