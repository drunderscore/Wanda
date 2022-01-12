#include <AK/Error.h>
#include <AK/MemoryStream.h>
#include <AK/Random.h>
#include <LibCore/EventLoop.h>
#include <LibCore/File.h>
#include <LibCore/UDPServer.h>
#include <LibCore/UDPSocket.h>
#include <LibMain/Main.h>
#include <LibSourceEngine/BitStream.h>

void print(ReadonlyBytes bytes)
{
    for (auto i = 0; i < bytes.size(); i++)
    {
        out("0x{:x} ", bytes[i]);

        if ((i + 1) % 5 == 0)
            outln();
    }

    outln();
}

ErrorOr<String> read_string(MemoryBitStream& stream)
{
    Vector<char> characters;

    char c;

    TRY(stream >> c);

    while (c != '\0')
    {
        characters.append(c);
        TRY(stream >> c);
    }

    return String(characters.data(), characters.size());
}

// TODO: test this!
ErrorOr<void> write_string(MemoryBitStream& stream, StringView value)
{
    for (const char& c : value)
        TRY(stream << c);

    TRY(stream << '\0');

    return {};
}

ErrorOr<u32> read_varint32(MemoryBitStream& stream)
{
    u32 result = 0;
    int count = 0;
    u32 b = 0;

    do
    {
        if (count == 5)
            return result;

        b = TRY(stream.read_integral<u8>());
        result |= (b & 0x7F) << (7 * count);
        ++count;
    } while (b & 0x80);

    return result;
}

template<typename T>
void try_or_warnln(ErrorOr<T> error)
{
    if (error.is_error())
        warnln("Errored! {}", error.error());
}

enum class Side
{
    Client,
    Server
};

ErrorOr<void> try_parse_message(MemoryBitStream& stream, Side bound_to_side)
{
    static constexpr u8 netmsg_type_bits = 6;

    // we keep reading until we're out of messages
    // FIXME: should this be somewhere else? just read a single message by default?
    while (((stream.current_bit() + 6) >> 3) < stream.bytes().size())
    {
        auto cmd = TRY(stream.read<u8>(netmsg_type_bits));
        //        outln("cmd {}", cmd);

        if (cmd == 0)
        {
            // NOP
            continue;
        }
        else if (cmd == 1)
        {
            auto reason = TRY(read_string(stream));
            outln("Disconnected :( Reason: {}", reason);

            continue;
        }
        else if (cmd == 3)
        {
            auto tick = TRY(stream.read_integral<u32>());
            // #if PROTOCOL_VERSION > 1
            auto host_frame_time = TRY(stream.read_integral<u16>());
            auto host_frame_time_std_dev = TRY(stream.read_integral<u16>());
            // #endif

            //            outln("Tick {} (frametime {}, frametime std dev {})", tick, host_frame_time,
            //            host_frame_time_std_dev);

            continue;
        }
        else if (cmd == 4)
        {
            auto command = TRY(read_string(stream));
            outln("StringCmd: {}", command);
            continue;
        }
        else if (cmd == 5)
        {
            auto number_of_convars = TRY(stream.read_integral<u8>());
            outln("{} convars", number_of_convars);

            for (auto i = 0; i < number_of_convars; i++)
            {
                auto name = TRY(read_string(stream));
                auto value = TRY(read_string(stream));
                outln("\"{}\" = \"{}\"", name, value);
            }

            continue;
        }
        else if (cmd == 6)
        {
            auto signon_state = TRY(stream.read_integral<u8>());
            auto spawn_count = TRY(stream.read_integral<i32>());

            outln("Signon state {}, spawn count {}", signon_state, spawn_count);

            continue;
        }
        else if (bound_to_side == Side::Client)
        {
            if (cmd == 7)
            {
                auto text = TRY(read_string(stream));
                outln("Print: {}", text);

                continue;
            }
            else if (cmd == 9)
            {
                auto needs_decoder = TRY(stream.read());
                auto length = TRY(stream.read_integral<u16>());

                // FIXME: ew, don't want to over-read when we have exactly a multiple of 8 bits, but this is yucky
                auto length_bytes = (length >> 3) + (length % 8 == 0 ? 0 : 1);
                auto data = TRY(stream.read(length_bytes));

                static constexpr u32 propinfobits_numprops = 10;

                auto name = TRY(read_string(stream));
                auto number_of_props = TRY(stream.read<u32>(propinfobits_numprops));

                outln("Table {} has {} props", name, number_of_props);

                return Error::from_string_literal("TODO continue reading");

                continue;
            }
            else if (cmd == 23)
            {
                static constexpr u32 netmsg_length_bits = 11;

                auto message_type = TRY(stream.read_integral<u8>());
                auto message_length = TRY(stream.read<u32>(11));
                // FIXME: ew, don't want to over-read when we have exactly a multiple of 8 bits, but this is yucky
                auto message_length_bytes = (message_length >> 3) + (message_length % 8 == 0 ? 0 : 1);
                auto message_data = TRY(stream.read(message_length_bytes));
                outln("User message, type {}, length bits {}, length bytes {}", message_type, message_length,
                      message_length_bytes);
                MemoryBitStream message_stream(message_data.bytes());

                if (message_type == 2)
                {
                    auto text = TRY(read_string(message_stream));
                    outln("HudText: {}", text);
                }
                else if (message_type == 4)
                {
                    auto entity_index = TRY(message_stream.read_integral<u8>());
                    auto is_chat = TRY(message_stream.read_integral<u8>()) == 1;
                    auto message_name = TRY(read_string(message_stream));
                    auto param1 = TRY(read_string(message_stream));
                    auto param2 = TRY(read_string(message_stream));
                    auto param3 = TRY(read_string(message_stream));
                    auto param4 = TRY(read_string(message_stream));

                    outln(
                        "SayText2! Entity {}, is chat {}, message name {}, param1 {}, param2 {}, param3 {}, param4 {}",
                        entity_index, is_chat, message_name, param1, param2, param3, param4);
                }
                else if (message_type == 5)
                {
                    // FIXME: no floating point support in MemoryBitStream :(
                    auto channel = TRY(message_stream.read_integral<u8>());
                    auto x_int = TRY(message_stream.read_integral<u32>());
                    auto y_int = TRY(message_stream.read_integral<u32>());
                    auto r1 = TRY(message_stream.read_integral<u8>());
                    auto g1 = TRY(message_stream.read_integral<u8>());
                    auto b1 = TRY(message_stream.read_integral<u8>());
                    auto a1 = TRY(message_stream.read_integral<u8>());
                    auto r2 = TRY(message_stream.read_integral<u8>());
                    auto g2 = TRY(message_stream.read_integral<u8>());
                    auto b2 = TRY(message_stream.read_integral<u8>());
                    auto a2 = TRY(message_stream.read_integral<u8>());
                    auto effect = TRY(message_stream.read_integral<u8>());
                    auto fadein_time_int = TRY(message_stream.read_integral<u32>());
                    auto fadeout_time_int = TRY(message_stream.read_integral<u32>());
                    auto hold_time_int = TRY(message_stream.read_integral<u32>());
                    auto fx_time_int = TRY(message_stream.read_integral<u32>());
                    auto message = TRY(read_string(message_stream));

                    outln("HudMsg: {}", message);
                }

                continue;
            }
            else if (cmd == 26)
            {
                static constexpr u32 max_edict_bits = 11;
                static constexpr u32 delta_size_bits = 20;

                auto max_entries = TRY(stream.read<u32>(max_edict_bits));
                auto is_delta = TRY(stream.read());
                u32 delta_from = 0;

                if (is_delta)
                    delta_from = TRY(stream.read_integral<u32>());

                auto base_line = TRY(stream.read());
                auto updated_entries = TRY(stream.read<u32>(max_edict_bits));
                auto length = TRY(stream.read<u32>(delta_size_bits));
                auto updated_base_line = TRY(stream.read());

                TRY(stream.skip(length));

                //                outln("Max entries {}, delta from {}, updated entries {}", max_entries, delta_from,
                //                updated_entries);

                continue;
            }
        }
        else if (bound_to_side == Side::Server)
        {
            if (cmd == 9)
            {
                static constexpr u32 num_new_command_bit = 4;
                static constexpr u32 num_backup_command_bit = 3;

                auto new_commands = TRY(stream.read<u8>(num_new_command_bit));
                auto backup_commands = TRY(stream.read<u8>(num_backup_command_bit));

                auto data_length = TRY(stream.read_integral<u16>());
                TRY(stream.skip(data_length));

                continue;
            }
        }

        return Error::from_string_literal(String::formatted(
            "Don't know what cmd {} is ({})", cmd, bound_to_side == Side::Client ? "clientbound" : "serverbound"));
    }

    return {};
}

ErrorOr<void> try_parse_connectionless_packet(MemoryBitStream& stream)
{
    auto id = TRY(stream.read_integral<char>());

    outln("ID is {}", id);

    if (id == 'q')
    {
        u32 challenge;
        TRY(stream >> challenge);

        outln("Client wants a challenge! They have {}", challenge);
    }
    else if (id == 'k')
    {
        u32 protocol_version;
        u32 auth_protocol;
        u32 challenge_nr;
        u32 retry_challenge;
        u16 steam_cookie_length;

        TRY(stream >> protocol_version);
        TRY(stream >> auth_protocol);
        TRY(stream >> challenge_nr);
        TRY(stream >> retry_challenge);

        auto client_name = TRY(read_string((stream)));
        auto password = TRY(read_string((stream)));
        auto version = TRY(read_string((stream)));

        TRY(stream >> steam_cookie_length);
        outln("Steam cookie is {} bytes", steam_cookie_length);
        TRY(stream.skip(steam_cookie_length << 3));

        outln("Connecting with protocol version {} auth {} challenge nr {} retry challenge {} client name {} "
              "password {} version {}",
              protocol_version, auth_protocol, challenge_nr, retry_challenge, client_name, password, version);
    }
    else
    {
        // Don't error here, it's not a problem if we don't understand it as there is only one. try_parse_message
        // needs to error because if we can't understand a message, trying to read the next one would nearly always fail
        // because the buffer would be incorrectly offset.
        warnln("Don't know what this id is :(");
    }

    return {};
}

// FIXME: no sequence passed here :(
ErrorOr<void> try_parse_packet(MemoryBitStream& stream, Side bound_to_side)
{
    u32 sequence_ack;
    u8 flags;
    u16 checksum;
    u8 reliable_state;

    TRY(stream >> sequence_ack);
    TRY(stream >> flags);
    TRY(stream >> checksum);
    TRY(stream >> reliable_state);

    bool reliable = flags & (1 << 0);
    bool choked = flags & (1 << 4);
    bool challenge = flags & (1 << 5);

    //    outln("Sequence FUCK we lost it, sequence ACK {}, flags {} (aka reliable {}, choked {}, challenge {})",
    //    sequence_ack,flags, reliable, choked, challenge);

    if (choked)
    {
        u8 choked_number;
        TRY(stream >> choked_number);
        //        outln("choked number {}", choked_number);
    }

    u32 challenge_number;
    if (challenge)
    {
        TRY(stream >> challenge_number);
        //        outln("challenge number {}", challenge_number);
    }

    if (reliable)
    {
        // Reliable packets have MAX_STREAMS "sub channels" (seems to be 2 in TF2, probably everywhere else)
        static constexpr u8 max_streams = 2;
        auto bit = TRY(stream.read<u8>(3));

        auto read_sub_channel_data = [&]() -> ErrorOr<void> {
            static constexpr u32 max_file_size_bits = 26;
            static constexpr u32 fragment_bits = 8;
            static constexpr u32 fragment_size = 1 << fragment_bits;

            auto is_single_block = TRY(stream.read()) == 0;

            u32 start_fragment = 0;
            u32 number_of_fragments = 0;
            u32 offset = 0;
            u32 length = 0;

            //            outln("is single block {}", is_single_block);

            // i assume a block is also a fragment?
            if (!is_single_block)
            {
                start_fragment = TRY(stream.read<u32>(max_file_size_bits - fragment_bits));
                number_of_fragments = TRY(stream.read<u32>(3));
                offset = start_fragment * fragment_size;
                length = number_of_fragments * fragment_size;
            }

            //            outln("start_fragment {}, number of fragments {}, offset {}, length {}", start_fragment,
            //                  number_of_fragments, offset, length);

            if (offset == 0)
            {
                if (is_single_block)
                {
                    auto is_compressed = TRY(stream.read());
                    //                    outln("is compressed {}", is_compressed);

                    if (is_compressed)
                    {
                        auto uncompressed_size = TRY(stream.read<u32>(max_file_size_bits));
                        //                        outln("uncompressed size {}", uncompressed_size);
                    }

                    auto data_size = TRY(read_varint32(stream));
                    //                    outln("data size varint is {} bytes, gonna read it and then try to parse it as
                    //                    a message",
                    //                          data_size);
                    auto data = TRY(stream.read(data_size));
                    MemoryBitStream data_stream(data.bytes());
                    TRY(try_parse_message(data_stream, bound_to_side));
                    return Error::from_string_literal(
                        "not actually an error, we just already tried to see if it's a message!");
                }
                else
                {
                    auto is_file = TRY(stream.read());
                    //                    outln("is file {}", is_file);

                    if (is_file)
                    {
                        auto transfer_id = TRY(stream.read_integral<u32>());
                        auto file_name = TRY(read_string(stream));
                        //                        outln("Transfer ID {}, file name {}", transfer_id, file_name);
                    }

                    auto is_compressed = TRY(stream.read());
                    //                    outln("is compressed {}", is_compressed);

                    if (is_compressed)
                    {
                        auto uncompressed_size = TRY(stream.read<u32>(max_file_size_bits));
                        //                        outln("uncompressed size {}", uncompressed_size);
                    }

                    // This is different than above, it's not a varint32 for SOME reason...
                    auto data_size = TRY(stream.read<u32>(max_file_size_bits));
                    //                    outln("data size {}", data_size);
                }
            }

            //            outln("going to skip {} bytes, {} bits", length, length << 3);
            TRY(stream.skip(length << 3));

            return {};
        };

        for (auto i = 0; i < max_streams; i++)
        {
            if (TRY(stream.read()))
            {
                //                outln("We have subchannel data for {}", i);
                TRY(read_sub_channel_data());
            }
        }
    }

    TRY(try_parse_message(stream, bound_to_side));

    return {};
}

ErrorOr<int> do_loopback_server()
{
    Core::EventLoop event_loop;

    auto our_server = Core::UDPServer::construct();
    auto destination_server = Core::UDPSocket::construct();

    Optional<sockaddr_in> who_to_send_to;

    our_server->on_ready_to_receive = [&] {
        try_or_warnln([&]() -> ErrorOr<void> {
            struct sockaddr_in from;
            auto bytes = our_server->receive(2 * KiB, from);

            if (!who_to_send_to.has_value())
                who_to_send_to = from;

            // Always forward first -- if we error down the line (very likely) then it wouldn't be forwarded :(
            destination_server->send(bytes.bytes());

            MemoryBitStream bytes_stream(bytes.bytes());
            u32 header;
            TRY(bytes_stream >> header);

            // Connectionless header, very simple queries
            if (header == 0xFFFFFFFF)
                TRY(try_parse_connectionless_packet(bytes_stream));
            else
                TRY(try_parse_packet(bytes_stream, Side::Server));

            return {};
        }());
    };

    destination_server->on_ready_to_read = [&] {
        try_or_warnln([&]() -> ErrorOr<void> {
            auto bytes = destination_server->read(2 * KiB);
            if (!who_to_send_to.has_value())
                warnln("Received data from destination, but our server hasn't received anything yet so we don't know "
                       "where "
                       "to send to!");
            else
            {
                TRY(our_server->send(bytes.bytes(), *who_to_send_to));

                MemoryBitStream bytes_stream(bytes.bytes());
                u32 header;
                TRY(bytes_stream >> header);

                // Connectionless header, very simple queries
                if (header == 0xFFFFFFFF)
                    TRY(try_parse_connectionless_packet(bytes_stream));
                else
                    TRY(try_parse_packet(bytes_stream, Side::Client));
            }

            return {};
        }());
    };

    our_server->bind({}, 6666);
    destination_server->connect("10.0.0.20", 27015);

    return event_loop.exec();
}

ErrorOr<int> do_replay_server()
{
    Core::EventLoop event_loop;

    auto server = Core::UDPServer::construct();

    u64 packet_index = 0;

    server->on_ready_to_receive = [&] {
        struct sockaddr_in from;
        server->receive(2 * KiB, from);
        outln("sending packet {}", packet_index);
        auto file =
            MUST(Core::File::open(String::formatted("clientbound/{}.bin", packet_index), Core::OpenMode::ReadOnly));
        packet_index++;
        auto bytes = file->read_all();

        MUST(server->send(bytes, from));
    };

    server->bind({}, 6666);

    return event_loop.exec();
}

ErrorOr<int> do_random_shit()
{
    Core::EventLoop event_loop;

    auto server = Core::UDPServer::construct();

    server->on_ready_to_receive = [&] {
        try_or_warnln([&]() -> ErrorOr<void> {
            struct sockaddr_in from;
            auto bytes = server->receive(2 * KiB, from);

            outln("Received {} bytes from {}:{}", bytes.size(), IPv4Address(from.sin_addr.s_addr), from.sin_port);

            MemoryBitStream bytes_stream(bytes.bytes());
            u32 header;
            TRY(bytes_stream >> header);

            // Connectionless header, very simple queries
            if (header == 0xFFFFFFFF)
            {
                char id;
                TRY(bytes_stream >> id);

                outln("Connectionless packet ID {}", id);

                if (id == 'q')
                {
                    u32 challenge;
                    TRY(bytes_stream >> challenge);

                    outln("Client wants a challenge! They have {}", challenge);

                    DuplexMemoryStream response;
                    response << 0xFFFFFFFF;
                    response << 'A';
                    response << 0x5A4F4933;
                    // FIXME: This needs to calculate our response to the challenge!
                    response << 0xDEADBEEF;
                    response << challenge;
                    // We are using the Steam protocol to authenticate
                    response << 3;
                    // Legacy thing from Steam2 protocol
                    response << static_cast<short>(0);
                    response << static_cast<u64>(0xDEAD'CAFE'BABE'BEEF);
                    // Is secure (no we aren't)
                    response << static_cast<u8>(0);

                    auto response_bytes = response.copy_into_contiguous_buffer();

                    TRY(server->send(response_bytes, from));
                }
                else if (id == 'k')
                {
                    outln("Client wants to connect!");

                    u32 protocol_version;
                    u32 auth_protocol;
                    u32 challenge_nr;
                    u32 retry_challenge;
                    u16 steam_cookie_length;

                    TRY(bytes_stream >> protocol_version);
                    TRY(bytes_stream >> auth_protocol);
                    TRY(bytes_stream >> challenge_nr);
                    TRY(bytes_stream >> retry_challenge);

                    auto client_name = TRY(read_string((bytes_stream)));
                    auto password = TRY(read_string((bytes_stream)));
                    auto version = TRY(read_string((bytes_stream)));

                    TRY(bytes_stream >> steam_cookie_length);
                    outln("Steam cookie is {} bytes", steam_cookie_length);
                    TRY(bytes_stream.skip(steam_cookie_length << 3));

                    outln(
                        "Connecting with protocol version {} auth {} challenge nr {} retry challenge {} client name {} "
                        "password {} version {}",
                        protocol_version, auth_protocol, challenge_nr, retry_challenge, client_name, password, version);

                    DuplexMemoryStream response;
                    response << 0xFFFFFFFF;
                    response << 'B';
                    response << retry_challenge;

                    auto response_bytes = response.copy_into_contiguous_buffer();
                    TRY(server->send(response_bytes.bytes(), from));
                }
                else
                {
                    warnln("idk what '{}' is", id);
                }
            }
            else
            {
                u32 sequence_ack;
                u8 flags;
                u16 checksum;
                u8 reliable_state;

                TRY(bytes_stream >> sequence_ack);
                TRY(bytes_stream >> flags);
                TRY(bytes_stream >> checksum);
                TRY(bytes_stream >> reliable_state);

                bool reliable = flags & (1 << 0);
                bool choked = flags & (1 << 4);
                bool challenge = flags & (1 << 5);

                outln("Sequence {}, sequence ACK {}, flags {} (aka reliable {}, choked {}, challenge {})", header,
                      sequence_ack, flags, reliable, choked, challenge);

                if (choked)
                {
                    u8 choked_number;
                    TRY(bytes_stream >> choked_number);
                    outln("choked number {}", choked_number);
                }

                u32 challenge_number;
                if (challenge)
                {
                    TRY(bytes_stream >> challenge_number);
                    outln("challenge number {}", challenge_number);
                }

                if (reliable)
                {
                    // Reliable packets have MAX_STREAMS "sub channels" (seems to be 2 in TF2, probably everywhere else)
                    static constexpr u8 max_streams = 2;
                    auto bit = TRY(bytes_stream.read<u8>(3));

                    auto read_sub_channel_data = [&]() -> ErrorOr<void> {
                        static constexpr u32 max_file_size_bits = 26;
                        static constexpr u32 fragment_bits = 8;
                        static constexpr u32 fragment_size = 1 << fragment_bits;

                        auto is_single_block = TRY(bytes_stream.read()) == 0;

                        u32 start_fragment = 0;
                        u32 number_of_fragments = 0;
                        u32 offset = 0;
                        u32 length = 0;

                        outln("is single block {}", is_single_block);

                        // i assume a block is also a fragment?
                        if (!is_single_block)
                        {
                            start_fragment = TRY(bytes_stream.read<u32>(max_file_size_bits - fragment_bits));
                            number_of_fragments = TRY(bytes_stream.read<u32>(3));
                            offset = start_fragment * fragment_size;
                            length = number_of_fragments * fragment_size;
                        }

                        outln("start_fragment {}, number of fragments {}, offset {}, length {}", start_fragment,
                              number_of_fragments, offset, length);

                        if (offset == 0)
                        {
                            if (is_single_block)
                            {
                                auto is_compressed = TRY(bytes_stream.read());
                                outln("is compressed {}", is_compressed);

                                if (is_compressed)
                                {
                                    auto uncompressed_size = TRY(bytes_stream.read<u32>(max_file_size_bits));
                                    outln("uncompressed size {}", uncompressed_size);
                                }

                                auto data_size = TRY(read_varint32(bytes_stream));
                                outln(
                                    "data size varint is {} bytes, gonna read it and then try to parse it as a message",
                                    data_size);
                                auto data = TRY(bytes_stream.read(data_size));
                                MemoryBitStream data_stream(data.bytes());
                                TRY(try_parse_message(data_stream, Side::Server));
                                return Error::from_string_literal(
                                    "not actually an error, we just already tried to see if it's a message!");
                            }
                            else
                            {
                                auto is_file = TRY(bytes_stream.read());
                                outln("is file {}", is_file);

                                if (is_file)
                                {
                                    auto transfer_id = TRY(bytes_stream.read_integral<u32>());
                                    auto file_name = TRY(read_string(bytes_stream));
                                    outln("Transfer ID {}, file name {}", transfer_id, file_name);
                                }

                                auto is_compressed = TRY(bytes_stream.read());
                                outln("is compressed {}", is_compressed);

                                if (is_compressed)
                                {
                                    auto uncompressed_size = TRY(bytes_stream.read<u32>(max_file_size_bits));
                                    outln("uncompressed size {}", uncompressed_size);
                                }

                                // This is different than above, it's not a varint32 for SOME reason...
                                auto data_size = TRY(bytes_stream.read<u32>(max_file_size_bits));
                                outln("data size {}", data_size);
                            }
                        }

                        outln("going to skip {} bytes, {} bits", length, length << 3);
                        TRY(bytes_stream.skip(length << 3));

                        return {};
                    };

                    for (auto i = 0; i < max_streams; i++)
                    {
                        if (TRY(bytes_stream.read()))
                        {
                            outln("We have subchannel data for {}", i);
                            TRY(read_sub_channel_data());
                        }
                    }
                }

                TRY(try_parse_message(bytes_stream, Side::Server));
            }

            return {};
        }());
    };

    server->bind({}, 6666);

    return event_loop.exec();
}

ErrorOr<int> serenity_main(Main::Arguments) { return do_loopback_server(); }
