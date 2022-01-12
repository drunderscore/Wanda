#include <AK/EnumBits.h>
#include <AK/Optional.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/EventLoop.h>
#include <LibCore/File.h>
#include <LibCore/Timer.h>
#include <LibCore/UDPServer.h>
#include <LibCore/UDPSocket.h>
#include <LibCrypto/Checksum/CRC32.h>
#include <LibMain/Main.h>
#include <LibSourceEngine/BitStream.h>
#include <LibSourceEngine/Message.h>
#include <LibSourceEngine/Messages/Clientbound/Print.h>
#include <LibSourceEngine/Packet.h>

static constexpr size_t bytes_to_receive = 2 * MiB;
// Can't say Vector<SourceEngine::Message>, or we'll slice those objects :(
static Vector<NonnullOwnPtr<SourceEngine::Message>> serverbound_messages;
static Vector<NonnullOwnPtr<SourceEngine::Message>> clientbound_messages;

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

    args_parser.parse(arguments);

    Core::EventLoop event_loop;

    auto chatter_server = Core::UDPServer::construct();
    auto destination_socket = Core::UDPSocket::construct();

    Optional<sockaddr_in> first_from;

    chatter_server->on_ready_to_receive = [&] {
        sockaddr_in from{};
        auto bytes = chatter_server->receive(bytes_to_receive, from);
        auto packet_length = bytes.size();

        if (!first_from.has_value())
            first_from = from;

        // Serverbound
        if (serverbound_messages.size() > 0)
        {
            // We need to append the message to the unreliable stream (which is just stuffed at the end of the packet),
            // rewrite the checksum, and the flags padding bits
            SourceEngine::ExpandingBitStream stream(move(bytes));

            // Go to (about) the end of the stream, so we can get the existing flags down below. We'll override this
            // very soon below.
            MUST(stream.set_position((packet_length << 3) - 1));

            // We don't want to set our position to the very end, but just before the padding bits start, overwriting
            // them. We will put the necessary padding bits back later.
            auto flags = stream.bytes()[8];
            auto number_of_padding_bits = ((flags >> 5) & 0xff);
            outln("Packet has {} padding bits", number_of_padding_bits);
            MUST(stream.set_position((packet_length << 3) - number_of_padding_bits));

            // FIXME: We really should try to stay under 1400 bytes for... reasons... find a better reason too! The
            //        Engine probably says it somewhere
            for (auto& message : serverbound_messages)
                MUST(message->write(stream));

            serverbound_messages.clear();

            // Now, let's put back our padding bits
            auto additional_bits = (int)(MUST(stream.position()) % 8);

            if (additional_bits > 0)
            {
                auto bits_to_pad = 8 - additional_bits;
                flags |= ((bits_to_pad << 5) & 0xFF);
                MUST(stream.write_typed<u8>(0, bits_to_pad));

                stream.bytes()[8] = flags;
            }

            auto end_position = MUST(stream.position());

            auto checksum = Crypto::Checksum::CRC32(stream.bytes().slice(11)).digest();
            auto compressed_checksum = SourceEngine::Packet::compress_checksum_to_u16(checksum);
            MUST(stream.set_position(9 << 3));
            MUST(stream << compressed_checksum);
            MUST(stream.set_position(end_position));

            destination_socket->send(stream.bytes());
        }
        else
        {
            destination_socket->send(bytes);
        }
    };

    destination_socket->on_ready_to_read = [&] {
        auto bytes = destination_socket->receive(bytes_to_receive);
        if (!first_from.has_value())
        {
            warnln("Got {} bytes from the destination, but don't know who to send it to!");
            return;
        }

        auto packet_length = bytes.size();

        // Clientbound
        if (clientbound_messages.size() > 0)
        {
            {
                auto file = MUST(Core::File::open("original_packet.bin", Core::OpenMode::WriteOnly));
                file->write(bytes.data(), bytes.size());
            }

            // We need to append the message to the unreliable stream (which is just stuffed at the end of the packet),
            // rewrite the checksum, and the flags padding bits
            SourceEngine::ExpandingBitStream stream(move(bytes));

            // Go to (about) the end of the stream, so we can get the existing flags down below. We'll override this
            // very soon below.
            MUST(stream.set_position((packet_length << 3) - 1));

            // We don't want to set our position to the very end, but just before the padding bits start, overwriting
            // them. We will put the necessary padding bits back later.
            auto flags = stream.bytes()[8];
            auto number_of_padding_bits = ((flags >> 5) & 0xff);
            outln("Packet has {} padding bits", number_of_padding_bits);
            MUST(stream.set_position((packet_length << 3) - number_of_padding_bits));

            // FIXME: We really should try to stay under 1400 bytes for... reasons... find a better reason too! The
            //        Engine probably says it somewhere
            for (auto& message : clientbound_messages)
                MUST(message->write(stream));

            clientbound_messages.clear();

            // Now, let's put back our padding bits
            auto additional_bits = (int)(MUST(stream.position()) % 8);

            if (additional_bits > 0)
            {
                auto bits_to_pad = 8 - additional_bits;
                flags |= ((bits_to_pad << 5) & 0xFF);
                MUST(stream.write_typed<u8>(0, bits_to_pad));

                stream.bytes()[8] = flags;
            }

            auto end_position = MUST(stream.position());

            auto checksum = Crypto::Checksum::CRC32(stream.bytes().slice(11)).digest();
            auto compressed_checksum = SourceEngine::Packet::compress_checksum_to_u16(checksum);
            MUST(stream.set_position(9 << 3));
            MUST(stream << compressed_checksum);
            MUST(stream.set_position(end_position));

            outln("Packet is now {} bytes", stream.bytes().size());

            {
                auto file = MUST(Core::File::open("modified_packet.bin", Core::OpenMode::WriteOnly));
                file->write(stream.bytes().data(), stream.bytes().size());
            }

            MUST(chatter_server->send(stream.bytes(), *first_from));
        }
        else
        {
            MUST(chatter_server->send(bytes, *first_from));
        }
    };

    if (!destination_socket->connect(destination_address, destination_port))
        return Error::from_string_literal("Failed to connect to destination server");

    if (!chatter_server->bind({}, source_port))
        return Error::from_string_literal("Failed to bind introspect server port");

    RefPtr<Core::Timer> bruh_timer;

    bruh_timer = Core::Timer::create_repeating(10000, [&bruh_timer] {
        outln("Okay, appending a clientbound packet");

        auto message = adopt_own(*new SourceEngine::Messages::Clientbound::Print);
        message->set_message("hello there!");
        clientbound_messages.append(move(message));
        bruh_timer->set_interval(1000);
        bruh_timer->start();
    });

    bruh_timer->start();

    return event_loop.exec();
}