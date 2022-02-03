/*
 * Copyright (c) 2022, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

namespace SourceEngine
{
enum class SignOnState
{
    None = 0,
    Challenge,
    Connected,
    New,
    PreSpawn,
    Spawn,
    Full,
    ChangeLevel,
    __Count
};
}