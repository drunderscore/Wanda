#include <LibCrypto/Hash/MD5.h>
#include <LibSourceEngine/BitStream.h>
#include <LibSourceEngine/Messages/Clientbound/CreateStringTable.h>
#include <LibSourceEngine/Messages/Clientbound/Print.h>
#include <LibSourceEngine/Messages/Clientbound/ServerInfo.h>
#include <LibSourceEngine/Messages/Disconnect.h>
#include <LibSourceEngine/Messages/SetConVar.h>
#include <LibSourceEngine/Messages/SignOnState.h>
#include <LibSourceEngine/Packets/Connectionless/Clientbound/Challenge.h>
#include <LibSourceEngine/Packets/Connectionless/Clientbound/ConnectReject.h>
#include <LibSourceEngine/Packets/Connectionless/Clientbound/Connection.h>
#include <LibSourceEngine/Packets/Connectionless/Serverbound/Connect.h>
#include <LibSourceEngine/Packets/Connectionless/Serverbound/GetChallenge.h>
#include <LibSourceEngine/StringTable.h>
#include <Server/Server.h>

Server::Server(String map_name, SourceEngine::BSP map)
    : m_server(Core::UDPServer::construct()), m_map_name(move(map_name)), m_map(move(map))
{
    m_server->on_ready_to_receive = [this] {
        sockaddr_in from{};
        auto bytes = m_server->receive(bytes_to_receive, from);
        try_or_disconnect(receive(bytes, from), from);
    };
}

ErrorOr<void> Server::bind(const IPv4Address& address, u16 port)
{
    if (!m_server->bind(address, port))
        return Error::from_string_literal("Failed to bind");

    return {};
}

int Server::exec()
{
    // FIXME: The first tick of the server will take milliseconds_per_tick milliseconds. Do we care _that_ much?
    // FIXME: Honestly, millisecond accuracy isn't that great... can we can get more accurate?
    m_tick_timer = Core::Timer::create_repeating(milliseconds_per_tick, [this] {
        auto maybe_tick_error = tick();

        if (maybe_tick_error.is_error())
        {
            warnln("\u001b[31mError whilst ticking: \u001b[35m{}\u001b[0m", maybe_tick_error.error());
            m_event_loop.quit(1);
        }
    });
    m_tick_timer->start();
    return m_event_loop.exec();
}

ErrorOr<void> Server::disconnect(Client& client, String reason)
{
    // Can't remove the client TOO soon, we might still be using it, so remove it once we're certainly not doing
    // anything with it, so defer it for later.
    // We do this absolutely first to be sure they aren't considered a client anymore, even if any TRYs fail.

    // TODO: Ensure taking the client by ref here is okay? Does it die?
    m_event_loop.deferred_invoke([this, &client] { m_clients.remove(client.address()); });

    // FIXME: Depending on how far this client has connected, it might be more appropriate (or required!) to
    //        use a different packet. We are only using the connectionless ConnectReject packet here

    SourceEngine::Packets::Connectionless::Clientbound::ConnectReject connect_reject;
    connect_reject.set_challenge(client.client_challenge());
    connect_reject.set_reason(move(reason));

    TRY(send(connect_reject, client.address()));

    return {};
}

ErrorOr<void> Server::send(const SourceEngine::ConnectionlessPacket& packet, const sockaddr_in& destination)
{
    SourceEngine::ExpandingBitStream bit_stream;
    TRY(bit_stream.write_typed(SourceEngine::ConnectionlessPacket::packet_header));
    TRY(packet.write(bit_stream));

    auto bytes = bit_stream.bytes();
    TRY(m_server->send(bytes, destination));

    return {};
}

ErrorOr<void> Server::send(const SourceEngine::SendingPacket& packet, const sockaddr_in& destination)
{
    auto buffer = TRY(packet.write());
    TRY(m_server->send(buffer.bytes(), destination));

    return {};
}

ErrorOr<void> Server::tick()
{
    auto tick_beginning_time = Time::now_monotonic();

    // TODO: actually tick something

    auto tick_ending_time = Time::now_monotonic();
    auto tick_duration_time = tick_ending_time - tick_beginning_time;
    // to_milliseconds will round up to a full millisecond, so let's calculate it ourselves from the nanoseconds
    auto tick_duration_accurate_milliseconds = tick_duration_time.to_nanoseconds() / 1000000.0f;

    // TODO: May want to introduce a "fluff" value in the future, only complain if the tick goes over a full tick plus a
    //       little extra (the "fluff").
    auto tick_additional_milliseconds = tick_duration_accurate_milliseconds - milliseconds_per_tick;

    if (tick_additional_milliseconds > 0.0f)
        warnln("\u001b[35mTick took too long! {} additional milliseconds passed.\u001b[0m",
               tick_additional_milliseconds);

    m_tick_timer->set_interval(max(milliseconds_per_tick - tick_duration_accurate_milliseconds, 0.0f));

    return {};
}

ErrorOr<void> Server::receive(ByteBuffer& bytes, sockaddr_in& from)
{
    outln("Received {} bytes", bytes.size());

    if (bytes.size() < sizeof(SourceEngine::ConnectionlessPacket::packet_header))
        return Error::from_string_literal("Not enough bytes for even a connectionless packet header");

    auto maybe_client_iterator = m_clients.find(from);
    Client* maybe_client{};
    if (maybe_client_iterator != m_clients.end())
        maybe_client = &maybe_client_iterator->value;

    SourceEngine::MemoryBitStream bit_stream(bytes.bytes());
    auto peeked_header = *reinterpret_cast<const int*>(bytes.data());
    if (peeked_header == SourceEngine::ConnectionlessPacket::packet_header)
    {

        TRY(bit_stream.skip(sizeof(SourceEngine::ConnectionlessPacket::packet_header) << 3));
        auto id = TRY(bit_stream.read_typed<char>());
        outln("Got connectionless packet {} from {}", id, from);

        switch (id)
        {
            case SourceEngine::Packets::Connectionless::Serverbound::GetChallenge::constant_id:
            {
                auto get_challenge_packet =
                    TRY(SourceEngine::Packets::Connectionless::Serverbound::GetChallenge::read(bit_stream));
                outln("Client wants a challenge, they have {}", get_challenge_packet.challenge());

                VERIFY(m_clients.set(from, {from}) == AK::HashSetResult::InsertedNewEntry);
                auto& client = m_clients.find(from)->value;

                client.set_client_challenge(get_challenge_packet.challenge());
                client.set_server_challenge(0xDEADBEEF);

                SourceEngine::Packets::Connectionless::Clientbound::Challenge challenge;
                challenge.set_magic_version(challenge_magic_version);
                challenge.set_challenge(client.server_challenge());
                challenge.set_client_challenge(client.client_challenge());
                challenge.set_auth_protocol(SourceEngine::AuthProtocol::Steam);
                challenge.set_steam_id(0xDEAD'CAFE'BABE'BEEF);
                challenge.set_is_secure(false);

                TRY(send(challenge, from));

                break;
            }

            case SourceEngine::Packets::Connectionless::Serverbound::Connect::constant_id:
            {
                auto connect_packet =
                    TRY(SourceEngine::Packets::Connectionless::Serverbound::Connect::read(bit_stream));

                if (!maybe_client)
                    return Error::from_string_literal("Client tried to connect without asking for a challenge");

                outln("{} is connecting with password {}, {} steam cookie length", connect_packet.client_name(),
                      connect_packet.password(), connect_packet.steam_cookie().size());

                SourceEngine::Packets::Connectionless::Clientbound::Connection connection;

                connection.set_challenge(maybe_client->client_challenge());

                TRY(send(connection, from));

                break;
            }
        }
    }
    else
    {
        auto packet = TRY(SourceEngine::ReceivingPacket::read(bit_stream));
        outln("Got packet sequence {} from {}", packet.sequence(), from);

        auto process_messages = [&](ReadonlyBytes bytes) -> ErrorOr<void> {
            SourceEngine::MemoryBitStream message_bit_stream(bytes);

            size_t last_message_position;

            while (bytes.size() >
                   (TRY(message_bit_stream.position()) + SourceEngine::Message::number_of_bits_for_message_id) >> 3)
            {
                auto cmd = TRY(message_bit_stream.read_typed<u8>(6));

                last_message_position = TRY(message_bit_stream.position());

                outln("message has cmd {}!", cmd);

                switch (cmd)
                {
                    case 0: // FIXME: Put a constant in LibSourceEngine somewhere for this
                    {
                        // This is a NOP
                        break;
                    }
                    case SourceEngine::Messages::Disconnect::constant_id:
                    {
                        auto disconnect = TRY(SourceEngine::Messages::Disconnect::read(message_bit_stream));
                        outln("Client disconnected because \"{}\"", disconnect.reason());
                        m_event_loop.deferred_invoke(
                            [this, maybe_client] { m_clients.remove(maybe_client->address()); });

                        break;
                    }
                    case SourceEngine::Messages::SetConVar::constant_id:
                    {
                        auto set_con_var = TRY(SourceEngine::Messages::SetConVar::read(message_bit_stream));

                        outln("Client has some convars for us!");

                        for (auto& convar : set_con_var.convars())
                            outln("{}: {}", convar.key, convar.value);

                        break;
                    }
                    case SourceEngine::Messages::SignOnState::constant_id:
                    {
                        auto sign_on_state = TRY(SourceEngine::Messages::SignOnState::read(message_bit_stream));

                        outln("Got sign on state {}, spawn count {}", static_cast<u8>(sign_on_state.sign_on_state()),
                              sign_on_state.spawn_count());

                        if (sign_on_state.sign_on_state() == SourceEngine::SignOnState::Connected)
                        {
                            outln("Client is connected, let's give them some server info");
                            SourceEngine::Messages::Clientbound::ServerInfo server_info;
                            server_info.set_player_slot(1);

                            // TODO: Constant!
                            server_info.set_protocol(24);
                            server_info.set_server_count(0);
                            // FIXME: Client will probably disconnect if map MD5 hash is not correct (we don't set it)
                            server_info.set_max_clients(16);
                            server_info.set_max_classes(200);

                            server_info.set_is_dedicated(true);
                            server_info.set_is_hltv(false);
                            // W for Windows, L for Linux. A lowercase is a hack to signify the server is "new"
                            server_info.set_operating_system('l');
                            server_info.set_tick_interval(milliseconds_per_tick / 1000);
                            server_info.set_game_dir("tf");

                            server_info.set_map_name(m_map_name);
                            auto map_md5 = m_map.calculate_md5_hash();
                            map_md5.bytes().copy_to(server_info.map_md5().span());

                            server_info.set_sky_name("sky_day01_01");
                            server_info.set_host_name("Wanda Server!");
                            server_info.set_is_replay(false);

                            SourceEngine::Messages::Clientbound::Print print;
                            print.set_message("This is a Wanda server, bruh");

                            SourceEngine::Messages::SignOnState sign_on_state_message;
                            sign_on_state_message.set_sign_on_state(SourceEngine::SignOnState::New);
                            sign_on_state_message.set_spawn_count(0);

                            SourceEngine::Messages::Clientbound::CreateStringTable create_string_table;
                            create_string_table.set_name("downloadables");

                            SourceEngine::SendingPacket sending_packet;
                            sending_packet.set_sequence(1);
                            sending_packet.set_challenge(maybe_client->server_challenge());
                            sending_packet.add_unreliable_message(server_info);
                            sending_packet.add_unreliable_message(print);
                            sending_packet.add_unreliable_message(sign_on_state_message);
                            sending_packet.add_unreliable_message(create_string_table);
                            TRY(send(sending_packet, from));
                        }

                        break;
                    }
                    default:
                        return Error::from_string_literal("Unknown message");
                }

                auto current_message_position = TRY(message_bit_stream.position());
                outln("Message was {} bits, {} bytes", current_message_position - last_message_position,
                      (current_message_position - last_message_position) >> 3);
            }

            return {};
        };

        auto& channel_data = packet.channel_data();
        auto maybe_normal_channel_data_iterator = channel_data.find(SourceEngine::Packet::Channel::Normal);
        if (maybe_normal_channel_data_iterator != channel_data.end())
        {
            auto& normal_channel_data = maybe_normal_channel_data_iterator->value;
            TRY(process_messages(normal_channel_data.data().bytes()));
        }

        outln("We have {} unreliable bytes", packet.unreliable_data().size());
        if (packet.unreliable_data().size() > 0)
            TRY(process_messages(packet.unreliable_data().bytes()));
    }

    return {};
}

template<typename T>
void Server::try_or_disconnect(ErrorOr<T> maybe_error, sockaddr_in& from)
{
    if (maybe_error.is_error())
    {
        warnln("\u001b[31mError whilst receiving from {}: \u001b[35m{}\u001b[0m", from, maybe_error.error());

        auto maybe_client = m_clients.find(from);
        if (maybe_client != m_clients.end())
        {
            auto& client = maybe_client->value;

            // FIXME: Can we improve this ternary with a constexpr if?
            auto reason = show_try_or_disconnect_errors_to_clients ? String::formatted("{}", maybe_error.error())
                                                                   : "Error in connection caused disconnect";

            // FIXME: Don't die just because we can't disconnect the client
            MUST(disconnect(client, reason));
        }
    }
}