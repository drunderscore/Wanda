#include <LibCore/ArgsParser.h>
#include <LibCore/EventLoop.h>
#include <LibCore/Stream.h>
#include <LibCore/UDPServer.h>
#include <LibMain/Main.h>
#include <LibSourceEngine/Message.h>
#include <LibSourceEngine/Messages/Clientbound/ServerInfo.h>
#include <LibSourceEngine/Messages/Disconnect.h>
#include <LibSourceEngine/Messages/SetConVar.h>
#include <LibSourceEngine/Messages/SignOnState.h>
#include <LibSourceEngine/Packet.h>

static constexpr size_t bytes_to_receive = 2 * MiB;

enum class Side
{
    Client,
    Server
};

static bool describe_clientbound_packets{};
static bool describe_serverbound_packets{};
static bool describe_clientbound_unreliable_messages{};
static bool describe_serverbound_unreliable_messages{};
static bool describe_clientbound_reliable_messages{};
static bool describe_serverbound_reliable_messages{};

ErrorOr<void> process_message(ReadonlyBytes bytes, Side bound, bool reliable)
{
    SourceEngine::MemoryBitStream stream(bytes);

    while (bytes.size() > (TRY(stream.position()) + SourceEngine::Message::number_of_bits_for_message_id) >> 3)
    {
        auto bound_prefix = bound == Side::Client ? 'C' : 'S';
        auto cmd = TRY(stream.read_typed<u8>(6));
        outln("[{}] {} message {}", bound_prefix, reliable ? "Reliable"sv : "Unreliable"sv, cmd);

        // Serverbound or clientbound only
        if (cmd >= SourceEngine::Messages::SignOnState::constant_id)
        {
            if (bound == Side::Client)
            {
                switch (cmd)
                {
                    case SourceEngine::Messages::Clientbound::ServerInfo::constant_id:
                    {
                        auto server_info = TRY(SourceEngine::Messages::Clientbound::ServerInfo::read(stream));

                        outln("[{}] Server Info: Map {}, map MD5 {:hex-dump}, player slot {}, max clients {}, tick "
                              "interval {}",
                              bound_prefix, server_info.map_name(), server_info.map_md5().span(),
                              server_info.player_slot(), server_info.max_clients(), server_info.tick_interval());

                        break;
                    }
                    default:
                        return Error::from_string_literal("Unknown message");
                }
            }
            else
            {
                return Error::from_string_literal("Unknown message");
            }
        }
        else
        {
            switch (cmd)
            {
                case 0: // NOP
                    break;
                case SourceEngine::Messages::Disconnect::constant_id:
                {
                    auto disconnect = TRY(SourceEngine::Messages::Disconnect::read(stream));
                    outln("[{}] Disconnected: {}", bound_prefix, disconnect.reason());

                    break;
                }
                case SourceEngine::Messages::SetConVar::constant_id:
                {
                    auto set_con_var = TRY(SourceEngine::Messages::SetConVar::read(stream));
                    outln("[{}] Set {} convars", bound_prefix, set_con_var.convars().size());

                    break;
                }
                case SourceEngine::Messages::SignOnState::constant_id:
                {
                    auto sign_on_state = TRY(SourceEngine::Messages::SignOnState::read(stream));
                    outln("[{}] Sign on state {}, count {}", bound_prefix,
                          static_cast<u8>(sign_on_state.sign_on_state()), sign_on_state.spawn_count());

                    break;
                }
                default:
                    return Error::from_string_literal("Unknown message");
            }
        }
    }

    return {};
}

ErrorOr<void> process_packet(Bytes bytes, Side bound)
{
    if (bytes.size() < sizeof(SourceEngine::ConnectionlessPacket::packet_header))
        return {};

    auto header = *reinterpret_cast<i32*>(bytes.data());

    if (header == SourceEngine::ConnectionlessPacket::packet_header)
        return {};

    SourceEngine::MemoryBitStream stream(bytes);

    auto packet = TRY(SourceEngine::ReceivingPacket::read(stream));
    auto challenge_string = packet.challenge().has_value() ? String::formatted("{}", *packet.challenge()) : "(none)";

    if ((bound == Side::Client && describe_clientbound_packets) ||
        (bound == Side::Server && describe_serverbound_packets))
    {
        outln("[{}] Sequence {}, ACK {}, Challenge {}, {} bytes", bound == Side::Client ? 'C' : 'S', packet.sequence(),
              packet.sequence_ack(), challenge_string, bytes.size());
    }

    if ((bound == Side::Client && describe_clientbound_unreliable_messages) ||
        (bound == Side::Server && describe_serverbound_unreliable_messages))
    {
        auto& unreliable_data = packet.unreliable_data();

        if (unreliable_data.size() > 0)
            TRY(process_message(unreliable_data.bytes(), bound, false));
    }

    if ((bound == Side::Client && describe_clientbound_reliable_messages) ||
        (bound == Side::Server && describe_serverbound_reliable_messages))
    {
        auto& channel_data = packet.channel_data();
        auto maybe_normal_channel_data_iterator = channel_data.find(SourceEngine::Packet::Channel::Normal);
        if (maybe_normal_channel_data_iterator != channel_data.end())
        {
            auto& normal_channel_data = maybe_normal_channel_data_iterator->value;
            TRY(process_message(normal_channel_data.data().bytes(), bound, true));
        }
    }

    return {};
}

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    Core::ArgsParser args_parser;

    String destination_address;
    int destination_port;
    int source_port;

    args_parser.add_positional_argument(destination_address, "Address of the destination server",
                                        "destination-address");
    args_parser.add_positional_argument(destination_port, "Port of the destination server", "destination-port");
    args_parser.add_positional_argument(source_port, "Port of the introspect server", "source-port");

    args_parser.add_option(describe_clientbound_packets, "Describe clientbound packets", "describe-clientbound-packets",
                           'a');
    args_parser.add_option(describe_serverbound_packets, "Describe serverbound packets", "describe-serverbound-packets",
                           'b');
    args_parser.add_option(describe_clientbound_unreliable_messages, "Describe clientbound unreliable messages",
                           "describe-clientbound-unreliable-messages", 'c');
    args_parser.add_option(describe_serverbound_unreliable_messages, "Describe serverbound unreliable messages",
                           "describe-serverbound-unreliable-messages", 'd');
    args_parser.add_option(describe_clientbound_reliable_messages, "Describe clientbound reliable messages",
                           "describe-clientbound-reliable-messages", 'e');
    args_parser.add_option(describe_serverbound_reliable_messages, "Describe serverbound reliable messages",
                           "describe-serverbound-reliable-messages", 'f');
    args_parser.parse(arguments);

    Core::EventLoop event_loop;

    auto introspect_server = TRY(Core::UDPServer::try_create());
    auto destination_socket = TRY(Core::Stream::UDPSocket::connect(destination_address, destination_port));

    Optional<sockaddr_in> first_from;

    introspect_server->on_ready_to_receive = [&] {
        sockaddr_in from{};
        auto bytes = introspect_server->receive(bytes_to_receive, from);

        if (!first_from.has_value())
            first_from = from;

        MUST(destination_socket->write(bytes));

        auto maybe_error = process_packet(bytes.bytes(), Side::Server);
        if (maybe_error.is_error())
            warnln("Failed to process serverbound packet: {}", maybe_error.error());
    };

    destination_socket->on_ready_to_read = [&] {
        auto buffer = MUST(ByteBuffer::create_uninitialized(bytes_to_receive));
        auto buffer_bytes = MUST(destination_socket->read(buffer.bytes()));
        if (!first_from.has_value())
        {
            warnln("Got {} bytes from the destination, but don't know who to send it to!");
            return;
        }

        MUST(introspect_server->send(buffer_bytes, *first_from));
        auto maybe_error = process_packet(buffer_bytes, Side::Client);
        if (maybe_error.is_error())
            warnln("Failed to process clientbound packet: {}", maybe_error.error());
    };

    if (!introspect_server->bind({}, source_port))
        return Error::from_string_literal("Failed to bind introspect server port");

    return event_loop.exec();
}