#pragma once

#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "WintersTypes.h"

struct ChampionAIItemBuildOrder
{
    const u16_t* pItemIds = nullptr;
    u32_t count = 0u;
};

// v1 빌드 오더는 CItemRegistry 등록 아이템 내에서만 구성한다.
// Bot AI는 BuyItem GameCommand를 생산할 뿐, 골드 차감/인벤토리 변경은 executor가 검증한다.
ChampionAIItemBuildOrder GetChampionAIItemBuildOrder(eChampion champion);
