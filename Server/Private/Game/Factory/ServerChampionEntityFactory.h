#pragma once

#include "ECS/Entity.h"
#include "ECS/Components/VisionComponents.h"
#include "WintersMath.h"

class CWorld;
struct LobbySlotState;

namespace ServerEntityFactory
{
    // 데이터(SpawnObjectDefinitionPack + GameplayDefinitionPack)로 챔피언 1기의
    // 서버 권위 gameplay component를 조립한다. AI/네트워크/세션/포즈 등 GameRoom 특화
    // wiring은 호출자가 한다. entity를 생성하고 그 EntityID를 반환한다.
    EntityID BuildChampionEntity(CWorld& world, const LobbySlotState& slot, const Vec3& spawnPos);
}

// GameRoom 분할 spawn(미니언/구조물/정글/챔피언)이 공유하는 가시성 헬퍼.
VisibilityComponent BuildServerVisibleToAll();
