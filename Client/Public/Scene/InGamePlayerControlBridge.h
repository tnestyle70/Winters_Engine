#pragma once

#include "Defines.h"

class CScene_InGame;

class CInGamePlayerControlBridge final
{
public:
    static void Update(
        CScene_InGame& scene,
        f32_t dt,
        bool_t bNetworkActive,
        bool_t bSkipGroundMove,
        bool_t bActionLockedBefore);
};
