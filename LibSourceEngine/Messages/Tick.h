/*
 * Copyright (c) 2022, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <AK/HashMap.h>
#include <AK/String.h>
#include <LibSourceEngine/Message.h>
#include <LibSourceEngine/SignOnState.h>

namespace SourceEngine::Messages
{
class Tick final : public Message
{
public:
    static constexpr u8 constant_id = 3;

    ALWAYS_INLINE u8 id() const override { return constant_id; }

    ALWAYS_INLINE ErrorOr<void> write(WritableBitStream& stream) const override
    {
        TRY(Message::write(stream));

        TRY(stream << m_tick);
        // FIXME: Host frame time and host frame time standard deviation is only included with protocol version > 10...
        //        Should we support earlier protocols? For reference, TF2 is protocol version 24
        TRY(stream << m_host_frame_time);
        TRY(stream << m_host_frame_time_standard_deviation);

        return {};
    }

    int tick() const { return m_tick; }
    void set_tick(int value) { m_tick = value; }
    u16 host_frame_time() const { return m_host_frame_time; }
    void set_host_frame_time(u16 value) { m_host_frame_time = value; }
    u16 host_frame_time_standard_deviation() const { return m_host_frame_time_standard_deviation; }
    void set_host_frame_time_standard_deviation(u16 value) { m_host_frame_time_standard_deviation = value; }

private:
    int m_tick{};
    // Host frame time and the standard deviation are often represented as floats, but in this packet they are truncated
    // to u16s.
    u16 m_host_frame_time{};
    u16 m_host_frame_time_standard_deviation{};
};
}