#pragma once

#include <LibSourceEngine/Message.h>
#include <LibSourceEngine/UserMessage.h>

namespace SourceEngine::Messages::Clientbound
{
class UserMessage final : public Message
{
public:
    explicit UserMessage(SourceEngine::UserMessage& user_message) : m_user_message(user_message) {}

    ALWAYS_INLINE u8 id() const override { return 23; }

    ALWAYS_INLINE ErrorOr<void> write(WritableBitStream& stream) const override
    {
        TRY(Message::write(stream));

        auto user_message_size_position = TRY(stream.position());
        TRY(stream.write_typed(0, number_of_bits_for_user_message_size));
        TRY(m_user_message.write(stream));
        auto user_message_after_write_position = TRY(stream.position());

        TRY(stream.set_position(user_message_size_position));
        // Need to subtract number_of_bits_for_user_message_size as that's not included in the size of the data
        TRY(stream.write_typed(user_message_after_write_position - user_message_size_position -
                               number_of_bits_for_user_message_size));
        TRY(stream.set_position(user_message_after_write_position));

        return {};
    }

private:
    static constexpr size_t number_of_bits_for_user_message_size = 11;
    SourceEngine::UserMessage& m_user_message;
};
}