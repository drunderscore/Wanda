#pragma once

#include <AK/HashMap.h>
#include <AK/String.h>
#include <LibSourceEngine/Message.h>
#include <LibSourceEngine/SignOnState.h>

namespace SourceEngine::Messages
{
class SignOnState final : public Message
{
public:
    static constexpr u8 constant_id = 6;

    ALWAYS_INLINE u8 id() const override { return constant_id; }

    ALWAYS_INLINE ErrorOr<void> write(WritableBitStream& stream) const override
    {
        TRY(Message::write(stream));

        TRY(stream.write_typed(static_cast<u8>(m_sign_on_state)));
        TRY(stream.write_typed(m_spawn_count));

        return {};
    }

    static ErrorOr<SignOnState> read(ReadableBitStream& stream)
    {
        SignOnState sign_on_state_message;

        auto sign_on_state = TRY(stream.read_typed<u8>());
        sign_on_state_message.m_sign_on_state = static_cast<SourceEngine::SignOnState>(sign_on_state);
        TRY(stream >> sign_on_state_message.m_spawn_count);

        return sign_on_state_message;
    }

    SourceEngine::SignOnState sign_on_state() const { return m_sign_on_state; }
    void set_sign_on_state(SourceEngine::SignOnState value) { m_sign_on_state = value; }

    int spawn_count() const { return m_spawn_count; }
    void set_spawn_count(int value) { m_spawn_count = value; }

private:
    SourceEngine::SignOnState m_sign_on_state{SourceEngine::SignOnState::None};
    int m_spawn_count{};
};
}