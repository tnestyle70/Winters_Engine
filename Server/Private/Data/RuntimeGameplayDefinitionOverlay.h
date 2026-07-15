#pragma once

#include "Shared/GameSim/Definitions/GameplayDefinitionPack.h"
#include "Shared/GameSim/Definitions/SpawnObjectDefinitionPack.h"

#include <string>

// M0/M4 디자이너 핫리로드 오버레이 (Debug 전용).
// 코드젠 팩(LoLGameplayDefinitions.generated.cpp)의 복사본 위에 진실 JSON
// (champions.json / SkillEffectGameplayDefs.json / SummonerSpellGameplayDefs.json /
//  EconomyGameplayDefs.json / ItemGameplayDefs.json / SpawnObjectGameplayDefs.json)의
// 수치를 덮어써 활성 팩으로 원자 교체한다. 파싱/검증 실패 시 활성 팩은 불변.
// 구조(스테이지 수/타겟 모드/키 집합)는 코드젠 소관 — 수치만 오버레이한다.
// SpawnObjectDefinitionPack 은 스폰 시점 복사 의미론 — 리로드 후 다음 스폰/웨이브부터 신값.
namespace ServerData
{
    // 런타임 오버레이가 있으면 그것, 없으면 코드젠 팩. 모든 룸 틱 바인딩이 이걸 쓴다.
    const GameplayDefinitionPack& GetActiveLoLGameplayDefinitionPack();

    // SpawnObject 팩도 동일 원자 교체 패턴 (revision 은 GameplayDefinitionPack 과 공유).
    const SpawnObjectDefinitionPack& GetActiveLoLSpawnObjectDefinitionPack();

    // 진실 JSON 3종을 재파싱해 활성 팩을 교체한다. Release 빌드는 항상 실패.
    bool_t TryReloadRuntimeGameplayDefinitions(std::string& outError);

    // 오버레이 해제 — 코드젠 팩으로 복귀.
    void ClearRuntimeGameplayDefinitions();

    // 0 = 오버레이 없음. 성공 리로드마다 +1. (M6 Hello 핸드셰이크 편입 예정)
    u32_t GetRuntimeGameplayDefinitionRevision();
}
