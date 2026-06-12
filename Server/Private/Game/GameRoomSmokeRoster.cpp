#include "GameRoomSmokeRoster.h"

#include "Game/GameRoom.h"

#include <Windows.h>
#include <cwchar>

namespace
{
    // Debug smoke roster keeps a red Sylas bot in this fixed slot.
    constexpr u8_t kSmokeRedSylasSlot = 5;
    constexpr f32_t kSmokeRedSylasMaxHp = 600.f;

    bool_t HasServerFlag(const wchar_t* pFlag)
    {
        const wchar_t* pCommandLine = ::GetCommandLineW();
        return pCommandLine != nullptr && pFlag != nullptr
            && std::wcsstr(pCommandLine, pFlag) != nullptr;
    }
}

bool_t ShouldUseRedSylasSmokeRoster()
{
#ifdef _DEBUG
    if (HasServerFlag(L"--no-sylas-smoke") || HasServerFlag(L"--no-irelia-sylas-smoke"))
        return false;
    return true;
#else
    if (HasServerFlag(L"--no-sylas-smoke") || HasServerFlag(L"--no-irelia-sylas-smoke"))
        return false;
    return HasServerFlag(L"--sylas-smoke") || HasServerFlag(L"--irelia-sylas-smoke");
#endif
}

Vec3 GetRedSylasSmokeDummyPosition()
{
    return Vec3{ 36.f, 1.f, -6.f };
}

bool_t IsRedSylasSmokeDummySlot(const LobbySlotState& slot)
{
    return slot.bDummy && slot.champion == eChampion::SYLAS;
}

u8_t GetRedSylasSmokePatrolPointCount()
{
    return 2;
}

Vec3 GetRedSylasSmokePatrolPoint(u8_t index)
{
    return index == 0u
        ? Vec3{ 32.f, 1.f, -6.f }
        : Vec3{ 40.f, 1.f, -6.f };
}

f32_t ResolveServerChampionMaxHpForSlot(const LobbySlotState& slot, f32_t defaultMaxHp)
{
    if (IsRedSylasSmokeDummySlot(slot))
        return kSmokeRedSylasMaxHp;
    if (slot.bDummy)
        return 100000.f;
    return defaultMaxHp;
}

void EnsureRedSylasSmokeRoster(LobbySlotState* pSlots, u32_t slotCount)
{
    bool_t bHasHumanChampion = false;
    for (u32_t i = 0; i < slotCount; ++i)
    {
        const LobbySlotState& slot = pSlots[i];
        if (slot.bHuman && slot.champion != eChampion::NONE && slot.champion != eChampion::END)
        {
            bHasHumanChampion = true;
            break;
        }
    }

    if (!bHasHumanChampion || kSmokeRedSylasSlot >= slotCount)
        return;

    LobbySlotState& dummy = pSlots[kSmokeRedSylasSlot];
    if (dummy.bHuman)
        return;

    dummy = LobbySlotState{};
    dummy.slotId = kSmokeRedSylasSlot;
    dummy.team = 1;
    dummy.bBot = true;
    dummy.bDummy = true;
    dummy.champion = eChampion::SYLAS;
    dummy.botDifficulty = 0;

    WintersOutputAIDebugStringA("[Smoke] red Sylas dummy enabled slot=5 pos=(36,1,-6)\n");
}
