#pragma once

#include <AK/Error.h>
#include <AK/Types.h>
#include <LibSourceEngine/BitStream.h>

namespace SourceEngine
{
class UserMessage
{
public:
    ALWAYS_INLINE virtual u8 id() const = 0;

    ALWAYS_INLINE virtual ErrorOr<void> write(WritableBitStream& stream) const
    {
        TRY(stream << id());
        return {};
    }
};
}