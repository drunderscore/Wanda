/*
 * Copyright (c) 2022, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

namespace SourceEngine
{
class StringTable
{
public:
    const HashMap<String, ByteBuffer>& entries() const { return m_entries; }
    HashMap<String, ByteBuffer>& entries() { return m_entries; }

private:
    HashMap<String, ByteBuffer> m_entries;
    u16 m_max_entries{};
};
}