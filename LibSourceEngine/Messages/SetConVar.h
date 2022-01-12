#pragma once

#include <AK/HashMap.h>
#include <AK/String.h>
#include <LibSourceEngine/Message.h>

namespace SourceEngine::Messages
{
class SetConVar final : public Message
{
public:
    static constexpr u8 constant_id = 5;

    ALWAYS_INLINE u8 id() const override { return constant_id; }

    ALWAYS_INLINE ErrorOr<void> write(WritableBitStream& stream) const override
    {
        TRY(Message::write(stream));

        if (m_convars.size() >= NumericLimits<u8>::max())
            return Error::from_string_literal("Too many convars in SetConVar message");

        TRY(stream.write_typed<u8>(m_convars.size()));

        for (auto& kv : m_convars)
        {
            TRY(stream << kv.key);
            TRY(stream << kv.value);
        }

        return {};
    }

    static ErrorOr<SetConVar> read(ReadableBitStream& stream)
    {
        SetConVar set_con_var;

        auto size = TRY(stream.read_typed<u8>());
        TRY(set_con_var.m_convars.try_ensure_capacity(size));

        for (auto i = 0; i < size; i++)
        {
            String key;
            String value;
            TRY(stream >> key);
            TRY(stream >> value);

            set_con_var.m_convars.set(move(key), move(value));
        }

        return set_con_var;
    }

    const HashMap<String, String>& convars() const { return m_convars; }

private:
    HashMap<String, String> m_convars;
};
}