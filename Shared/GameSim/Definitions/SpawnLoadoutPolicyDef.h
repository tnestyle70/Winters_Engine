#pragma once

#include "WintersTypes.h"
#include "Shared/GameSim/Components/RuneComponent.h"

struct SpawnLoadoutPolicyDef
{
    u32_t startGold = 0u;
    u8_t startLevel = 0u;
    eRuneId startRune = eRuneId::None;
    u8_t startRuneCount = 0u;
    f32_t respawnDelaySec = 0.f;
};
