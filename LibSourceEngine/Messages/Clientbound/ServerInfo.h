#pragma once

#include <LibSourceEngine/Message.h>

namespace SourceEngine::Messages::Clientbound
{
class ServerInfo final : public Message
{
public:
    static constexpr u8 constant_id = 8;

    ALWAYS_INLINE u8 id() const override { return constant_id; }

    static ErrorOr<ServerInfo> read(ReadableBitStream& stream)
    {
        ServerInfo server_info;

        TRY(stream >> server_info.m_protocol);
        TRY(stream >> server_info.m_server_count);
        server_info.m_is_hltv = TRY(stream.read());
        server_info.m_is_dedicated = TRY(stream.read());
        TRY(stream.read_typed<i32>()); // client.dll CRC, used long ago before signed binaries, VAC, etc.
        TRY(stream >> server_info.m_max_classes);
        TRY(stream.read_bytes(server_info.m_map_md5.span()));
        TRY(stream >> server_info.m_player_slot);
        TRY(stream >> server_info.m_max_clients);
        TRY(stream >> server_info.m_tick_interval);
        TRY(stream >> server_info.m_operating_system);
        TRY(stream >> server_info.m_game_dir);
        TRY(stream >> server_info.m_map_name);
        TRY(stream >> server_info.m_sky_name);
        TRY(stream >> server_info.m_host_name);
        if (has_replay)
            server_info.m_is_replay = TRY(stream.read());

        return server_info;
    }

    ALWAYS_INLINE ErrorOr<void> write(WritableBitStream& stream) const override
    {
        TRY(Message::write(stream));

        TRY(stream << m_protocol);
        TRY(stream << m_server_count);
        TRY(stream.write(m_is_hltv));
        TRY(stream.write(m_is_dedicated));
        TRY(stream.write_typed<i32>(1337420)); // client.dll CRC, used long ago before signed binaries, VAC, etc.
        TRY(stream << m_max_classes);
        TRY(stream.write_bytes(m_map_md5));
        TRY(stream << m_player_slot);
        TRY(stream << m_max_clients);
        TRY(stream << m_tick_interval);
        TRY(stream << m_operating_system);
        TRY(stream << m_game_dir);
        TRY(stream << m_map_name);
        TRY(stream << m_sky_name);
        TRY(stream << m_host_name);
        if (has_replay)
            TRY(stream.write(m_is_replay));

        return {};
    }

    i16 protocol() const { return m_protocol; }
    int server_count() const { return m_server_count; }
    bool is_hltv() const { return m_is_hltv; }
    bool is_dedicated() const { return m_is_dedicated; }
    u16 max_classes() const { return m_max_classes; }
    Array<u8, 16>& map_md5() { return m_map_md5; }
    u8 player_slot() const { return m_player_slot; }
    u8 max_clients() const { return m_max_clients; }
    float tick_interval() const { return m_tick_interval; }
    char operating_system() const { return m_operating_system; }
    const String& game_dir() const { return m_game_dir; }
    const String& map_name() const { return m_map_name; }
    const String& sky_name() const { return m_sky_name; }
    const String& host_name() const { return m_host_name; }
    bool is_replay() const { return m_is_replay; }

    void set_protocol(i16 value) { m_protocol = value; }
    void set_server_count(int value) { m_server_count = value; }
    void set_is_hltv(bool value) { m_is_hltv = value; }
    void set_is_dedicated(bool value) { m_is_dedicated = value; }
    void set_max_classes(u16 value) { m_max_classes = value; }
    void set_player_slot(u8 value) { m_player_slot = value; }
    void set_max_clients(u8 value) { m_max_clients = value; }
    void set_tick_interval(float value) { m_tick_interval = value; }
    void set_operating_system(char value) { m_operating_system = value; }
    void set_game_dir(String value) { m_game_dir = move(value); }
    void set_map_name(String value) { m_map_name = move(value); }
    void set_sky_name(String value) { m_sky_name = move(value); }
    void set_host_name(String value) { m_host_name = move(value); }
    void set_is_replay(bool value) { m_is_replay = value; }

private:
    // TODO: Put this constant in a more accessible and global place
    static constexpr bool has_replay = true;
    static constexpr size_t map_md5_size = 16;

    i16 m_protocol{};
    int m_server_count{};
    bool m_is_hltv{};
    bool m_is_dedicated{};
    u16 m_max_classes{};
    Array<u8, map_md5_size> m_map_md5{};
    u8 m_player_slot{};
    u8 m_max_clients{};
    float m_tick_interval{};
    char m_operating_system{};
    String m_game_dir;
    String m_map_name;
    String m_sky_name;
    String m_host_name;
    bool m_is_replay{};
};
}