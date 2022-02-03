/*
 * Copyright (c) 2022, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <LibSourceEngine/UserMessage.h>

namespace SourceEngine::UserMessages
{
class SayText2 final : public UserMessage
{
public:
    ALWAYS_INLINE u8 id() const override { return 4; }

    ALWAYS_INLINE ErrorOr<void> write(WritableBitStream& stream) const override
    {
        TRY(UserMessage::write(stream));

        TRY(stream << m_entity_index);
        TRY(stream.write_typed<u8>(m_is_chat ? 1 : 0));
        TRY(stream << m_message);

        for (auto& param : m_params)
            TRY(stream << param);

        return {};
    }

private:
    u8 m_entity_index{};
    bool m_is_chat{};
    String m_message;
    Array<String, 4> m_params;
};
}