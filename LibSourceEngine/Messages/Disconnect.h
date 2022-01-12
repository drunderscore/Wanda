#pragma once

#include <AK/HashMap.h>
#include <AK/String.h>
#include <LibSourceEngine/Message.h>
#include <LibSourceEngine/SignOnState.h>

namespace SourceEngine::Messages
{
class Disconnect final : public Message
{
public:
    static constexpr u8 constant_id = 1;

    ALWAYS_INLINE u8 id() const override { return constant_id; }

    ALWAYS_INLINE ErrorOr<void> write(WritableBitStream& stream) const override
    {
        TRY(Message::write(stream));

        TRY(stream << m_reason);

        return {};
    }

    static ErrorOr<Disconnect> read(ReadableBitStream& stream)
    {
        Disconnect disconnect;

        TRY(stream >> disconnect.m_reason);

        return disconnect;
    }

    const String& reason() const { return m_reason; }
    void set_reason(String value) { m_reason = move(value); }

private:
    String m_reason;
};
}