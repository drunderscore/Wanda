#pragma once

#include <AK/Format.h>
#include <AK/HashMap.h>
#include <LibCore/EventLoop.h>
#include <LibCore/Timer.h>
#include <LibCore/UDPServer.h>
#include <LibSourceEngine/BSP.h>
#include <LibSourceEngine/Message.h>
#include <LibSourceEngine/Packet.h>
#include <Server/Client.h>

class Server
{
public:
    // FIXME: Can we avoid passing the map name around like this? We need to tell the client it, should the Server be
    //        told to load the map from file instead?
    Server(String map_name, SourceEngine::BSP);

    ErrorOr<void> bind(const IPv4Address&, u16 port);
    int exec();

    // We actually do need to own this String
    ErrorOr<void> disconnect(Client&, String reason);
    ErrorOr<void> send(const SourceEngine::ConnectionlessPacket&, const sockaddr_in&);
    ErrorOr<void> send(const SourceEngine::SendingPacket&, const sockaddr_in&);

private:
    ErrorOr<void> tick();
    ErrorOr<void> receive(ByteBuffer&, sockaddr_in& from);

    template<typename T>
    void try_or_disconnect(ErrorOr<T>, sockaddr_in&);

    Core::EventLoop m_event_loop;
    RefPtr<Core::Timer> m_tick_timer;
    NonnullRefPtr<Core::UDPServer> m_server;
    HashMap<sockaddr_in, Client> m_clients;
    String m_map_name;
    SourceEngine::BSP m_map;

    static constexpr float milliseconds_per_tick = 1000.0 / 66.0;
    static constexpr size_t bytes_to_receive = 2 * KiB;
    static constexpr int challenge_magic_version = 0x5A4F4933;

    // NOTE: You probably don't want this if you're actually running a server!
    // This may expose internal information about the server, the clients connection or other clients, without any
    // regard to it. For debugging, you probably want it on.
    static constexpr bool show_try_or_disconnect_errors_to_clients = true;
};

namespace AK
{
template<>
struct Traits<sockaddr_in> : public GenericTraits<sockaddr_in>
{
    static unsigned hash(const sockaddr_in& value)
    {
        unsigned hash = 0;
        // FIXME: Could OR sin_port and sin_family together into one 32-bit value
        hash = pair_int_hash(hash, value.sin_port);
        hash = pair_int_hash(hash, value.sin_addr.s_addr);
        hash = pair_int_hash(hash, value.sin_family);
        return hash;
    }

    static constexpr bool equals(const sockaddr_in& a, const sockaddr_in& b)
    {
        return a.sin_port == b.sin_port && a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_family == b.sin_family;
    }
};

template<>
struct Formatter<sockaddr_in> : public Formatter<String>
{
    ErrorOr<void> format(FormatBuilder& builder, const sockaddr_in& value)
    {
        IPv4Address address(value.sin_addr.s_addr);
        TRY(builder.put_string(address.to_string()));
        TRY(builder.put_string(":"sv));
        TRY(builder.put_u64(value.sin_port));
        return {};
    }
};
}