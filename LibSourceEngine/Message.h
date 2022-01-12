#pragma once

#include <AK/Error.h>
#include <AK/Types.h>
#include <LibSourceEngine/BitStream.h>

namespace SourceEngine
{
class Message
{
public:
    static constexpr u32 number_of_bits_for_message_id = 6;

    ALWAYS_INLINE virtual u8 id() const = 0;

    ALWAYS_INLINE virtual ErrorOr<void> write(WritableBitStream& stream) const
    {
        TRY(stream.write_typed(id(), number_of_bits_for_message_id));
        return {};
    }
};
}