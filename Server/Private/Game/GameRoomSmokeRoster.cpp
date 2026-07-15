#include "GameRoomSmokeRoster.h"

#include "GameRoomInternal.h"
#include "Game/LobbyAuthority.h"
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <cwchar>
#include <cwctype>

namespace
{
    // Debug smoke roster keeps a red Sylas bot in this fixed slot.
    constexpr u8_t kSmokeRedSylasSlot = 5;
    constexpr f32_t kSmokeRedSylasMaxHp = 600.f;
    constexpr std::array<eChampion, 17u> kAttackSpeedLabChampionPool =
    {
        eChampion::IRELIA,
        eChampion::YASUO,
        eChampion::KALISTA,
        eChampion::SYLAS,
        eChampion::VIEGO,
        eChampion::ANNIE,
        eChampion::ASHE,
        eChampion::FIORA,
        eChampion::GAREN,
        eChampion::RIVEN,
        eChampion::ZED,
        eChampion::EZREAL,
        eChampion::YONE,
        eChampion::JAX,
        eChampion::MASTERYI,
        eChampion::KINDRED,
        eChampion::LEESIN,
    };

    bool_t HasServerFlag(const wchar_t* pFlag)
    {
        const wchar_t* pCommandLine = ::GetCommandLineW();
        if (!pCommandLine || !pFlag || *pFlag == L'\0')
            return false;

        const size_t flagLength = std::wcslen(pFlag);
        const wchar_t* pMatch = pCommandLine;
        while ((pMatch = std::wcsstr(pMatch, pFlag)) != nullptr)
        {
            const bool_t bLeftBoundary =
                pMatch == pCommandLine || std::iswspace(*(pMatch - 1)) != 0;
            const wchar_t right = pMatch[flagLength];
            const bool_t bRightBoundary =
                right == L'\0' || std::iswspace(right) != 0;
            if (bLeftBoundary && bRightBoundary)
                return true;
            pMatch += flagLength;
        }
        return false;
    }

    u8_t ResolveAttackSpeedLabBotLane(u8_t slotId)
    {
        switch (slotId % 5u)
        {
        case 1u:
            return kGameSimLaneTop;
        case 2u:
            return kGameSimLaneMid;
        case 3u:
        case 4u:
            return kGameSimLaneBot;
        default:
            return kGameSimLaneMid;
        }
    }
}

bool_t ShouldUseAttackSpeedLabRoster()
{
#if defined(_DEBUG)
    return HasServerFlag(L"--attack-speed-lab");
#else
    return false;
#endif
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

    OutputServerAITrace("[Smoke] red Sylas dummy enabled slot=5 pos=(36,1,-6)\n");
}

bool_t EnsureAttackSpeedLabRoster(LobbySlotState* pSlots, u32_t slotCount)
{
    if (!pSlots || slotCount != kGameRosterSlotCount)
        return false;

    u32_t humanCount = 0u;
    u32_t humanIndex = 0u;
    for (u32_t i = 0u; i < slotCount; ++i)
    {
        if (!pSlots[i].bHuman)
            continue;
        ++humanCount;
        humanIndex = i;
    }
    if (humanCount != 1u ||
        pSlots[humanIndex].sessionId == 0u ||
        pSlots[humanIndex].champion == eChampion::NONE ||
        pSlots[humanIndex].champion == eChampion::END ||
        std::find(
            kAttackSpeedLabChampionPool.begin(),
            kAttackSpeedLabChampionPool.end(),
            pSlots[humanIndex].champion) ==
                kAttackSpeedLabChampionPool.end())
    {
        return false;
    }

    std::array<LobbySlotState, kGameRosterSlotCount> nextSlots{};
    size_t championPoolIndex = 0u;

    for (u32_t i = 0u; i < slotCount; ++i)
    {
        LobbySlotState& slot = nextSlots[i];
        slot.slotId = static_cast<u8_t>(i);
        slot.team = i < 5u ? 0u : 1u;
        slot.netId = NULL_NET_ENTITY;
        slot.bReady = true;
        if (i == humanIndex)
        {
            slot.bHuman = true;
            slot.bBot = false;
            slot.bDummy = false;
            slot.sessionId = pSlots[humanIndex].sessionId;
            slot.champion = pSlots[humanIndex].champion;
            slot.botDifficulty = pSlots[humanIndex].botDifficulty;
            slot.botLane = ResolveAttackSpeedLabBotLane(slot.slotId);
            continue;
        }

        while (championPoolIndex < kAttackSpeedLabChampionPool.size() &&
            kAttackSpeedLabChampionPool[championPoolIndex] ==
                pSlots[humanIndex].champion)
        {
            ++championPoolIndex;
        }
        if (championPoolIndex >= kAttackSpeedLabChampionPool.size())
            return false;

        slot.bBot = true;
        slot.champion = kAttackSpeedLabChampionPool[championPoolIndex++];
        slot.botDifficulty = 2u;
        slot.botLane = ResolveAttackSpeedLabBotLane(slot.slotId);
    }

    for (u32_t i = 0u; i < slotCount; ++i)
        pSlots[i] = nextSlots[i];
    return true;
}
