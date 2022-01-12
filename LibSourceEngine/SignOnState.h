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