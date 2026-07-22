#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Definitions/SpawnLoadoutPolicyDef.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "WintersMath.h"

class CWorld;
struct ChampionGameplayDef;

// 서버/클라가 공유하는 챔피언 gameplay component 조립 코어.
// presentation(Render/Model/Anim/NavAgent)과 server-only(collider/spatial/vision/visibility,
// sim component 부착)은 포함하지 않는다. 호출자가 def/loadout/override를 주입한다.
struct ChampionAssemblyContext
{
    eChampion champion = eChampion::NONE;
    u8_t team = 0u;
    Vec3 spawnPos{};
    f32_t maxHpOverride = 0.f;          // 0 = override 없음
    bool_t bAssignBotSkillRanks = false;
    SpawnLoadoutPolicyDef loadout{};    // startGold/startLevel/startRune/startRuneCount/respawnDelaySec
    const ChampionGameplayDef* pDef = nullptr; // 있으면 우선, 없으면 fallbackStats 사용
};

namespace ChampionGameplayAssembly
{
    // entity를 생성하고 gameplay component를 부착한 뒤 EntityID 반환.
    EntityID Build(CWorld& world, const ChampionAssemblyContext& ctx);
}
