#pragma once

#include <LibSourceEngine/Message.h>

namespace SourceEngine::Messages::Clientbound
{
class Print final : public Message
{
public:
    static constexpr u8 constant_id = 7;

    ALWAYS_INLINE u8 id() const override { return constant_id; }

    static ErrorOr<Print> read(ReadableBitStream& stream)
    {
        Print print;

        TRY(stream >> print.m_message);

        return print;
    }

    ALWAYS_INLINE ErrorOr<void> write(WritableBitStream& stream) const override
    {
        TRY(Message::write(stream));

        auto message = m_automatically_append_newline_on_write ? String::formatted("{}\n", m_message) : m_message;

        TRY(stream << message);

        return {};
    }

    bool automatically_append_newline_on_write() const { return m_automatically_append_newline_on_write; }
    void set_automatically_append_newline_on_write(bool value) { m_automatically_append_newline_on_write = value; }
    const String& message() const { return m_message; }
    void set_message(String value) { m_message = move(value); }

private:
    // The Engine expects this message to have a new line, but that's a bit obtuse. Let's give the option of
    // automatically putting a new line, with it enabled by default.
    bool m_automatically_append_newline_on_write{true};
    String m_message;
};
}