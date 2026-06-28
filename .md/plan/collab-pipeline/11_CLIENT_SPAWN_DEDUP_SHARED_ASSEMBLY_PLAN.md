Session - 서버 ServerEntityFactory와 클라 ChampionSpawnService가 중복 조립하는 gameplay component 조립을 Shared 코어 1개로 합쳐, 클라에는 표시(Render/Model/Anim/NavAgent)만 남긴다.

배경(한 줄): 챔피언 스폰 조립은 서버(`Server/Private/Game/Factory/ServerChampionEntityFactory.cpp`)와 클라(`Client/Private/GameObject/ChampionSpawnService.cpp:162`)가 같은 stat/skillRank/experience/vision/spatial/collider를 따로 만든다. 클라는 `CChampionStatsRegistry`+하드코딩 level 6(kSpawnChampionLevel)+하드코딩 collider 1.5f를 쓰고, 서버는 pack/def를 쓴다. 규칙·게이트는 07 문서를 따른다.

검토 반영(2026-06-28) — 확정 결정:
- (선행) ChampionSpawnService는 **local-smoke 전용**으로 본다. 네트워크 모드 챔피언 엔티티는 `SnapshotApplier`/`Scene_InGameNetwork::SpawnChampionEntity`가 생성하므로(검토 확인) 코어 dedup은 권위(I5)와 무관. 착수 전 검증: `rg "CChampionSpawnService::Spawn" Client`로 호출처가 local-smoke뿐인지 확정.
- (확정) collider/spatial/vision은 **코어에서 제외**한다. 서버 collider는 pack `championCollider.bodyHeight=1.8f`, 클라 local-smoke는 1.5f로 값이 다르다 → 코어에 넣으면 byte-parity가 깨진다. 코어는 stat/health/respawn/skillrank/experience/gold/rune/score/summoner/champion만 조립하고, collider/spatial/vision/visibility는 각 진영이 부착.
- (확정) `SkillLoadoutComponent`는 Shared(`Shared/GameSim/Components/SkillLoadoutComponent.h`)라 코어에서 안전하게 사용 가능. 이동 결정이 필요한 것은 `AttachChampionSimComponents`(현재 Server `Factory/ChampionSimComponentTable.*`)뿐 → Shared로 못 올리면 `ctx.bAttachSimComponents=false`로 두고 호출자가 Build() 후 부착.
- (확정) maxHpOverride는 코어가 stat을 먼저 만든 뒤 `if(ctx.maxHpOverride>0) stat.hpMax=ctx.maxHpOverride;`로 적용(서버는 `ResolveServerChampionMaxHpForSlot` 결과를 ctx로 주입). normal player는 override 0이라 영향 없음 → parity 유지.
- (위치) 본 계획은 P3~P8과 직교한다. 병렬 또는 P3 이후 진행 가능.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Spawn/ChampionGameplayAssembly.h (새 파일)

두 진영이 공유할 "gameplay component 조립 코어". CWorld + 입력 컨텍스트만 받는다. Engine/Client/Renderer/UI/DX include 없음(I1). presentation(Render/Model/Anim)은 포함하지 않는다.

```cpp
#pragma once

#include "ECS/Entity.h"
#include "Shared/GameSim/Definitions/ChampionStatsDef.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "WintersMath.h"

class CWorld;
struct ChampionGameplayDef;

struct ChampionAssemblyContext
{
    eChampion champion = eChampion::NONE;
    u8_t team = 0u;
    u8_t level = 1u;
    Vec3 spawnPos{};
    f32_t maxHpOverride = 0.f;          // 0 = override 없음 (server smoke/dummy 전용)
    bool_t bAttachSimComponents = true; // 챔피언별 sim component 부착 여부
    bool_t bAssignBotSkillRanks = false;
    const ChampionGameplayDef* pDef = nullptr; // 있으면 우선, 없으면 호출자 fallback stats 사용
    ChampionStatsDef fallbackStats{};          // pDef 없을 때 사용할 stats
};

namespace ChampionGameplayAssembly
{
    // entity를 생성하고 서버 권위 gameplay component만 부착한 뒤 EntityID 반환.
    // 표시(Render/Model/Anim/NavAgent)는 호출자가 entity에 덧붙인다.
    EntityID Build(CWorld& world, const ChampionAssemblyContext& ctx);
}
```

확인 필요: `SkillLoadoutComponent`/`AttachChampionSimComponents`는 현재 Server `Factory/ChampionSimComponentTable.h`에 있다. Shared 코어가 sim component 부착까지 하려면 그 table을 `Shared/GameSim/Spawn/`로 올려야 한다(서버/클라 공용). 올릴 수 없으면 `bAttachSimComponents=false`로 두고 각 진영이 부착한다.

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Spawn/ChampionGameplayAssembly.cpp (새 파일)

`ServerEntityFactory::BuildChampionEntity`의 조립 본문(Transform~TargetableTag)을 그대로 옮긴다. `slot.*` 접근을 `ctx.*`로, `ResolveServerChampionMaxHpForSlot(slot, hp)`는 `ctx.maxHpOverride>0 ? ctx.maxHpOverride : hp`로, `AssignDefaultBotSkillRanks`는 `ctx.bAssignBotSkillRanks` 분기로 바꾼다. `BuildServerVisibleToAll`은 server-only(가시성 권위)이므로 코어에서 빼고 `VisibilityComponent`는 호출자가 부착한다(또는 ctx로 받는다). 값/순서는 byte-identical 유지.

확인 필요: 코어 추출 시 collider/spatial/vision도 포함하면 클라의 하드코딩 1.5f가 사라지므로, 클라 표시 collider와 서버 권위 collider 의미가 동일한지 확인. 다르면 collider는 코어에서 빼고 각자 부착.

1-3. C:/Users/tnest/Desktop/Winters/Server/Private/Game/Factory/ServerChampionEntityFactory.cpp

`BuildChampionEntity` 본문을 `ChampionGameplayAssembly::Build` 호출 + server-only 후처리(VisibilityComponent, bot rank 컨텍스트 구성)로 축약한다. `SpawnObjectDefinitionPack`/`GameplayDefinitionPack` 조회로 `ChampionAssemblyContext`를 채우고, `ResolveServerChampionMaxHpForSlot(slot, stat.hpMax)` 결과를 `ctx.maxHpOverride`로 넘긴다.

확인 필요: maxHpOverride는 stat.hpMax 계산 후 결정되므로, 코어가 stat을 먼저 만들고 override를 적용하는 순서를 유지해야 byte-parity가 깨지지 않는다. override를 ctx로 선계산해 넘기려면 `ResolveServerChampionMaxHpForSlot`이 stat.hpMax에 의존하지 않는 분기(smoke/dummy)만 쓰는지 확인(현재 GameRoomSmokeRoster.cpp:58-65는 smoke/dummy면 고정값, else면 defaultMaxHp -> override=0 처리 가능).

1-4. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/ChampionSpawnService.cpp

`Spawn`(162)의 gameplay 조립부(stat/health/skillRank/experience/vision/spatial/collider — 202~265 부근)를 `ChampionGameplayAssembly::Build` 호출로 교체한다. 클라 컨텍스트: `level=kSpawnChampionLevel`(현행 유지), `bAssignBotSkillRanks=false`, `pDef = ServerData가 아닌 클라가 접근 가능한 def 경로`(확인 필요), 없으면 `fallbackStats = CChampionStatsRegistry::Instance().Resolve(champion)`. 그 후 클라 전용(ModelRenderer/anim/SetScale/RenderComponent/NavAgent/AttachChampionStateComponents/network prediction 필드 174-271)은 그대로 둔다.

확인 필요:
- 클라가 GameplayDefinitionPack에 접근 가능한가? `ServerData::GetLoLGameplayDefinitionPack`은 Server 소유다. 클라는 별도 접근자가 필요하거나 fallbackStats만 사용. 네트워크 모드에서 챔피언 엔티티는 SnapshotApplier가 만든다면, ChampionSpawnService는 local-smoke 전용일 수 있다 -> 이 경우 dedup 대상은 local-smoke 경로뿐이고 권위와 무관(I5 안전).
- 클라 collider 하드코딩 1.5f를 코어 값으로 바꾸면 표시가 달라지는지 F5로 확인.

2. 검증 (07 §6 게이트)

미검증:
- 빌드 미검증, 클라 local-smoke 스폰 동작 불변 미검증

검증 명령:
- python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
- GameSim/Server/Client/SimLab Debug x64 빌드 (MSBuild)
- powershell -ExecutionPolicy Bypass -File Tools/LoLData/Verify-LoLDataDrivenPipeline.ps1

통과 기준:
- G1 Build PASS. G4 SimLab same-seed 해시 불변(서버 스폰 동작 동일).
- 클라/서버가 같은 `ChampionGameplayAssembly::Build`를 호출. 클라 전용 gameplay 리터럴(level 6 제외 정책값) 0.
- F5: local-smoke 챔피언 스폰 stat/collider/vision이 기존과 동일(G6).

확인 필요:
- 새 `Shared/GameSim/Spawn/*` 파일이 GameSim/Server/Client 빌드 프로젝트에 포함되는지.
- I1: Shared 코어가 Engine/Client/Server-private 타입을 include하지 않는지(특히 ServerData/pack 접근은 호출자가 주입).
