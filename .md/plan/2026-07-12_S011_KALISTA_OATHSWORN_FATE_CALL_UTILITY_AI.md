Session - Kalista의 3599 계약 아이템과 Oathsworn 기반 Fate's Call을 서버 권위로 완성하고, 기존 Champion AI를 `Fact -> Perception -> Utility -> GameCommand`의 결정적 30 Hz 평가 구조로 정리한다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/.md/architecture/WINTERS_CODEBASE_COMPASS.md

서버 AI 소유권 규칙 아래에 다음 구조를 추가한다.

```text
LoL bot decision ownership

Server World Fact
-> ChampionAIPerception (read-only flattened facts, rebuilt every 30 Hz tick)
-> ChampionAIValuation (pure deterministic utility scores)
-> IChampionAIBrain (intent selection + hysteresis)
-> Champion profile/combo executor (Q/W/E/R/BA/Combo steps)
-> GameCommand
-> CDefaultCommandExecutor

AI는 Transform, HP, cooldown, status, damage를 직접 수정하지 않는다.
30 Hz는 perception/score 갱신 주기이며 동일 Move/Attack command의 무제한 재전송을 뜻하지 않는다.
긴급 Retreat/Flash와 진행 중 combo는 즉시 평가하고, 일반 command emission은 intent hold와 equivalent-command suppression을 유지한다.
```

### 1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/KalistaBondComponent.h

새 파일:

```cpp
#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersTypes.h"

#include <type_traits>

inline constexpr u16_t kKalistaOathswornItemId = 3599u;
inline constexpr u8_t kKalistaOathswornInventorySlot = 5u;
inline constexpr f32_t kKalistaOathswornContractRange = 14.f;
inline constexpr u16_t kKalistaFateCallLaunchCommandMarker =
    kKalistaOathswornItemId;

enum class eKalistaOathswornStage : u8_t
{
    Binding = 0,
    Bound,
};

struct KalistaOathswornComponent
{
    EntityID entityAlly = NULL_ENTITY;
    eKalistaOathswornStage eStage = eKalistaOathswornStage::Binding;
    f32_t fRemainingSec = 1.5f;
};

struct KalistaOathswornByComponent
{
    EntityID entityKalista = NULL_ENTITY;
};

struct KalistaFateCallCarriedComponent
{
    EntityID entityOwner = NULL_ENTITY;
    bool_t bHidden = false;
};

static_assert(std::is_trivially_copyable_v<KalistaOathswornComponent>);
static_assert(std::is_trivially_copyable_v<KalistaOathswornByComponent>);
static_assert(std::is_trivially_copyable_v<KalistaFateCallCarriedComponent>);
```

### 1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/KalistaSentinelComponent.h

기존 코드:

```cpp
enum class eKalistaFateCallStage : uint8_t
{
    None = 0,
    Carrying,
    Launching,
};
```

아래로 교체:

```cpp
enum class eKalistaFateCallStage : uint8_t
{
    None = 0,
    Pulling,
    Carrying,
    Launching,
};
```

`KalistaFateCallComponent`의 `vLaunchStart` 위에 아래를 추가:

```cpp
Vec3 vPullStart{};
f32_t fPullElapsedSec = 0.f;
f32_t fPullDurationSec = 0.25f;
```

### 1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/GameplayComponents.h

기존 코드:

```cpp
GenericAirborne = 10,
```

아래로 교체:

```cpp
GenericAirborne = 10,
KalistaOathswornRitual = 11,
```

### 1-5. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomSpawn.cpp

기존 코드:

```cpp
InventoryComponent inventory{};
m_world.AddComponent<InventoryComponent>(entity, inventory);
```

아래로 교체:

```cpp
InventoryComponent inventory{};
if (slot.champion == eChampion::KALISTA)
{
    inventory.itemIds[kKalistaOathswornInventorySlot] =
        kKalistaOathswornItemId;
    inventory.count = InventoryComponent::kMaxSlots;
}
m_world.AddComponent<InventoryComponent>(entity, inventory);
```

include 영역에 아래를 추가:

```cpp
#include "Shared/GameSim/Components/KalistaBondComponent.h"
```

`count=6`은 snapshot이 빈 1~5번 칸과 채워진 6번 칸을 함께 보존하기 위한 occupied span이다. 구매는 1-9에서 첫 빈 칸 탐색으로 바꿔 1~5번 칸을 계속 사용할 수 있게 한다.

### 1-6. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/CommandSerializer.h

기존 코드:

```cpp
void SendUseItem(CClientNetwork& net, u16_t itemId,
    const Vec3& groundPos, const Vec3& direction = {});
```

아래로 교체:

```cpp
void SendUseItem(CClientNetwork& net, u16_t itemId,
    const Vec3& groundPos, const Vec3& direction = {},
    NetEntityId targetNet = NULL_NET_ENTITY);
```

### 1-7. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/CommandSerializer.cpp

기존 코드:

```cpp
void CCommandSerializer::SendUseItem(CClientNetwork& net, u16_t itemId,
    const Vec3& groundPos, const Vec3& direction)
```

아래로 교체:

```cpp
void CCommandSerializer::SendUseItem(CClientNetwork& net, u16_t itemId,
    const Vec3& groundPos, const Vec3& direction, NetEntityId targetNet)
```

기존 코드:

```cpp
wire.itemId = itemId;
wire.groundPos = groundPos;
wire.direction = WintersMath::NormalizeXZ(direction, Vec3{}, 0.0001f);
```

아래로 교체:

```cpp
wire.itemId = itemId;
wire.targetNet = targetNet;
wire.groundPos = groundPos;
wire.direction = WintersMath::NormalizeXZ(direction, Vec3{}, 0.0001f);
```

### 1-8. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameInput.cpp

include 영역에 아래를 추가:

```cpp
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/KalistaBondComponent.h"
```

`UpdateCombatInput`에서 `IsPlayerStunned()` 검사 바로 위에 아래를 추가한다. Fate's Call에 들어간 실제 플레이어의 우클릭은 일반 이동이 아니라 발사 지점 선택 command가 된다.

```cpp
const bool_t bKalistaCarried =
    m_PlayerEntity != NULL_ENTITY &&
    m_World.HasComponent<ReplicatedStateComponent>(m_PlayerEntity) &&
    (m_World.GetComponent<ReplicatedStateComponent>(m_PlayerEntity).stateFlags &
        kSnapshotStateKalistaCarriedFlag) != 0u;
if (bKalistaCarried)
{
    outSkipGroundMove = true;
    if (!bImGuiMouse && in.IsRButtonPressed() &&
        m_pCommandSerializer && m_pNetworkView &&
        m_pNetworkView->IsConnected())
    {
        const Vec3 launchTarget = ResolveMouseMapSurfacePos();
        m_pCommandSerializer->SendKalistaFateCallLaunch(
            *m_pNetworkView,
            launchTarget);
    }
    return;
}
```

기존 4번 ward 입력 블록 아래에 6번 계약 아이템 입력을 추가한다.

```cpp
if (in.IsKeyPressed('6') &&
    IsNetworkAuthoritativeGameplay() &&
    GetPlayerChampionId() == eChampion::KALISTA &&
    m_PlayerEntity != NULL_ENTITY &&
    GetHoveredEntity() != NULL_ENTITY &&
    GetHoveredEntity() != m_PlayerEntity &&
    GetHoveredTeam() == GetPlayerTeam() &&
    m_World.HasComponent<InventoryComponent>(m_PlayerEntity) &&
    m_World.GetComponent<InventoryComponent>(m_PlayerEntity)
        .itemIds[kKalistaOathswornInventorySlot] == kKalistaOathswornItemId &&
    m_pCommandSerializer && m_pNetworkView && m_pEntityIdMap &&
    m_pNetworkView->IsConnected())
{
    const NetEntityId targetNet = m_pEntityIdMap->ToNet(GetHoveredEntity());
    if (targetNet != NULL_NET_ENTITY &&
        m_World.HasComponent<TransformComponent>(GetHoveredEntity()))
    {
        const Vec3 targetPos = m_World.GetComponent<TransformComponent>(
            GetHoveredEntity()).GetPosition();
        const Vec3 origin = m_pPlayerTransform
            ? m_pPlayerTransform->GetPosition()
            : targetPos;
        ClearNetworkAttackIntent();
        m_pCommandSerializer->SendUseItem(
            *m_pNetworkView,
            kKalistaOathswornItemId,
            targetPos,
            WintersMath::DirectionXZ(origin, targetPos, Vec3{}),
            targetNet);
    }
}
```

### 1-9. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

include 영역에 아래를 추가:

```cpp
#include "Shared/GameSim/Components/KalistaBondComponent.h"
```

`HandleMove`의 finite-position 검사 다음, `CanMove` 검사 전에 아래를 추가한다. 이 위치여야 Fate's Call의 CannotMove 상태가 player launch command까지 막지 않는다.

```cpp
if (cmd.itemId == kKalistaFateCallLaunchCommandMarker)
{
    if (world.HasComponent<KalistaFateCallCarriedComponent>(cmd.issuerEntity))
        KalistaGameSim::TryLaunchCarriedAlly(
            world, tc, cmd.issuerEntity, cmd.groundPos);
    return;
}
if (world.HasComponent<KalistaFateCallCarriedComponent>(cmd.issuerEntity))
    return;
```

발사 선택은 일반 `Move`와 wire shape만 공유하고 `itemId=3599` discriminator로
명시적으로 구분한다. 따라서 R 이전의 지연 우클릭은 발사로 오인되지 않고,
발사 완료 뒤 도착한 중복 발사 packet도 예전 착지점으로의 일반 이동이 되지 않는다.

`HandleBuyItem`의 inventory full/append 코드를:

```cpp
if (inventory.count >= InventoryComponent::kMaxSlots)
    return;
if (gold.amount < pItem->price)
    return;

gold.amount -= pItem->price;
inventory.itemIds[inventory.count++] = pItem->itemId;
```

아래로 교체:

```cpp
u8_t emptySlot = InventoryComponent::kMaxSlots;
for (u8_t slot = 0u; slot < InventoryComponent::kMaxSlots; ++slot)
{
    if (inventory.itemIds[slot] == 0u)
    {
        emptySlot = slot;
        break;
    }
}
if (emptySlot >= InventoryComponent::kMaxSlots || gold.amount < pItem->price)
    return;

gold.amount -= pItem->price;
inventory.itemIds[emptySlot] = pItem->itemId;
inventory.count = (std::max)(
    inventory.count,
    static_cast<u8_t>(emptySlot + 1u));
```

`HandleUseItem` 시작 부분의 ward-only 검사 위에 아래를 추가:

```cpp
if (cmd.itemId == kKalistaOathswornItemId)
{
    if (!world.HasComponent<InventoryComponent>(cmd.issuerEntity))
        return;
    const auto& inventory = world.GetComponent<InventoryComponent>(cmd.issuerEntity);
    if (inventory.itemIds[kKalistaOathswornInventorySlot] !=
        kKalistaOathswornItemId)
    {
        return;
    }
    if (KalistaGameSim::TryBeginOathswornContract(
        world,
        tc,
        cmd.issuerEntity,
        cmd.targetEntity))
    {
        inventory.itemIds[kKalistaOathswornInventorySlot] = 0u;
        while (inventory.count > 0u &&
            inventory.itemIds[inventory.count - 1u] == 0u)
        {
            --inventory.count;
        }
    }
    return;
}
```

3599는 계약 성공 시에만 소비한다. 실패한 대상/상태에서는 그대로 남고, 성공하면
occupied span을 뒤에서 정규화해 일반 아이템 6칸을 다시 모두 사용할 수 있게 한다.

### 1-10. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Kalista/KalistaGameSim.h

기존 `CanCastFateCall` 선언 위에 아래를 추가:

```cpp
bool_t TryBeginOathswornContract(
    CWorld& world,
    const TickContext& tc,
    EntityID kalista,
    EntityID ally);
bool_t TryLaunchCarriedAlly(
    CWorld& world,
    const TickContext& tc,
    EntityID carried,
    const Vec3& targetPosition);
```

### 1-11. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Kalista/KalistaGameSim.cpp

include 영역에 아래를 추가:

```cpp
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/KalistaBondComponent.h"
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
```

계약 아이템 handler는 다음 조건을 모두 서버에서 검증한다.

```cpp
bool_t TryBeginOathswornContract(
    CWorld& world,
    const TickContext& tc,
    EntityID kalista,
    EntityID ally)
{
    if (!IsAliveChampion(world, kalista) ||
        !IsAliveChampion(world, ally) ||
        kalista == ally ||
        !world.HasComponent<ChampionComponent>(kalista) ||
        world.GetComponent<ChampionComponent>(kalista).id != eChampion::KALISTA ||
        world.GetComponent<ChampionComponent>(kalista).team !=
            world.GetComponent<ChampionComponent>(ally).team ||
        world.HasComponent<KalistaOathswornComponent>(kalista) ||
        world.HasComponent<KalistaOathswornByComponent>(ally))
    {
        return false;
    }

    StatusEffectApplyDesc ritual{};
    ritual.effectId = eStatusEffectId::KalistaOathswornRitual;
    ritual.stackPolicy = eStatusStackPolicy::RefreshDuration;
    ritual.sourceEntity = kalista;
    ritual.stackGroup = GameplayStatus::MakeStatusStackGroup(
        eChampion::KALISTA,
        eSkillSlot::BasicAttack);
    ritual.stateFlags =
        kGameplayStateUntargetableFlag |
        kGameplayStateCannotMoveFlag |
        kGameplayStateCannotAttackFlag |
        kGameplayStateCannotCastFlag;
    ritual.fDurationSec = 1.5f;
    ritual.fMoveSpeedMul = 1.f;
    if (!GameplayStatus::TryApplyStatusEffect(world, ally, ritual, tc))
        return false;

    KalistaOathswornComponent oath{};
    oath.entityAlly = ally;
    oath.eStage = eKalistaOathswornStage::Binding;
    oath.fRemainingSec = ritual.fDurationSec;
    world.AddComponent<KalistaOathswornComponent>(kalista, oath);
    world.AddComponent<KalistaOathswornByComponent>(
        ally,
        KalistaOathswornByComponent{ kalista });

    ClearMove(world, ally);
    StartActionState(world, ally, eActionStateId::DeathStart, tc.tickIndex);
    SetPoseState(world, ally, ePoseStateId::Idle, tc.tickIndex, true);
    return true;
}
```

계약 의식은 실제 `Dead` pose를 쓰지 않는다. `DeathStart` action과 아래의
`kSnapshotStateKalistaOathswornRitualFlag`를 조합해 죽음 애니메이션만 재생한다.
따라서 계약 대상 플레이어에게 사망 화면, HP 0, respawn, kill/death/gold 보상이
발생하지 않는다.

`FindFateCallAlly`는 실제 Kalista이면 nearest ally를 찾지 않고 `KalistaOathswornComponent::entityAlly`만 반환한다. Sylas가 훔친 Kalista R은 현재 ultimate-coverage 계약을 보존하기 위해 nearest ally fallback을 유지한다.

`StartFateCallCarry`는 즉시 teleport하지 않고 다음 상태를 만든다.

```cpp
state.entityCarried = ally;
state.eStage = eKalistaFateCallStage::Pulling;
state.vPullStart = world.GetComponent<TransformComponent>(ally).GetPosition();
state.fPullElapsedSec = 0.f;
state.fPullDurationSec = 0.25f;
state.fRemainingSec = carryDurationSec;
world.AddComponent<KalistaFateCallCarriedComponent>(
    ally,
    KalistaFateCallCarriedComponent{ ctx.casterEntity, false });
```

`Tick`의 oath 처리 규칙:

```cpp
if (oath.eStage == eKalistaOathswornStage::Binding)
{
    oath.fRemainingSec = (std::max)(0.f, oath.fRemainingSec - tc.fDt);
    if (oath.fRemainingSec <= 0.f)
    {
        oath.eStage = eKalistaOathswornStage::Bound;
        GameplayStatus::RemoveStatusEffect(
            world,
            oath.entityAlly,
            eStatusEffectId::KalistaOathswornRitual,
            owner);
        StartActionState(
            world,
            oath.entityAlly,
            eActionStateId::None,
            tc.tickIndex);
        SetPoseState(
            world,
            oath.entityAlly,
            ePoseStateId::Idle,
            tc.tickIndex,
            true);
    }
}
```

owner disconnect/entity destroy 시에는 `KalistaOathswornByComponent`를 역방향으로
감사해 owner component가 없는 orphan tag와 ritual status/action을 제거한다. 실제
사망/respawn은 동일 entity를 유지하므로 계약은 보존한다.

Fate's Call tick은 `Pulling -> Carrying -> Launching`으로 진행한다.

```text
Pulling:
- 0.25초 동안 ally 위치를 Kalista 위치로 보간
- 이동/공격/스킬/피격 차단, mesh는 표시

Carrying:
- Kalista 위치에 고정
- KalistaFateCallCarriedComponent.bHidden = true
- bot ally는 3초 뒤 Kalista gameplay forward로 자동 launch
- human ally는 자신의 Move command 목표로 launch
- 4초 timeout은 Kalista gameplay forward로 안전 launch

Launching:
- bHidden = false
- nav segment clamp와 terrain height 적용
- segment 충돌 적에게 GenericAirborne 1회
- 종료 시 status/tag/FateCall component 정리
```

`TryLaunchCarriedAlly`는 carried tag의 owner와 owner FateCall state를 검증한 뒤 target 방향과 R range로 `vLaunchEnd`를 만들고 stage를 `Launching`으로 바꾼다. `HandleMove`가 이 함수를 호출하므로 human launch도 사람의 일반 이동 command와 같은 네트워크 경로를 사용한다.
실제 Kalista는 carried ally의 Move가 R2 command를 대신하므로 launch 시작 시 R slot의
`currentStage/stageWindow`를 terminal 상태로 정리한다. Sylas stolen Kalista R은 기존
stage2 command executor가 같은 정리를 소유한다.

### 1-11-A. C:/Users/user/Desktop/Winters/Data/Gameplay/ChampionGameData/champions.json

Kalista R의 canonical server-authoritative cooldown을 legacy client definition과 같은
120초로 교체하고 LoL definition pack을 재생성한다. 생성된
`SkillGameplayDefs.json`과 `LoLGameplayDefinitions.generated.cpp`를 직접 원본으로
편집하지 않는다.

```json
{
  "slot": 4,
  "targetMode": "Direction",
  "stageCount": 2,
  "stageWindowSec": 4.0,
  "cooldownSec": 120.0,
  "rangeMax": 0.0
}
```

### 1-12. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/SnapshotStateFlags.h

기존 코드:

```cpp
inline constexpr u32_t kSnapshotStateViegoSoulFlag = 1u << 4;
```

아래로 교체:

```cpp
inline constexpr u32_t kSnapshotStateViegoSoulFlag = 1u << 4;
inline constexpr u32_t kSnapshotStateKalistaCarriedFlag = 1u << 5;
inline constexpr u32_t kSnapshotStateKalistaOathswornRitualFlag = 1u << 6;
```

### 1-13. C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

include 영역에 아래를 추가:

```cpp
#include "Shared/GameSim/Components/KalistaBondComponent.h"
```

gameplay state flag 처리 다음에 아래를 추가:

```cpp
if (world.HasComponent<KalistaFateCallCarriedComponent>(entity) &&
    world.GetComponent<KalistaFateCallCarriedComponent>(entity).bHidden)
{
    stateFlags |= kSnapshotStateKalistaCarriedFlag;
}
```

같은 위치에 계약 대상의 의식 표현 플래그를 추가한다.

```cpp
if (world.HasComponent<KalistaOathswornByComponent>(entity))
{
    const EntityID kalista =
        world.GetComponent<KalistaOathswornByComponent>(entity).entityKalista;
    if (kalista != NULL_ENTITY &&
        world.IsAlive(kalista) &&
        world.HasComponent<KalistaOathswornComponent>(kalista) &&
        world.GetComponent<KalistaOathswornComponent>(kalista).eStage ==
            eKalistaOathswornStage::Binding)
    {
        stateFlags |= kSnapshotStateKalistaOathswornRitualFlag;
    }
}
```

### 1-13-A. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameNetwork.cpp

`Dead` pose와 계약 의식 표현을 분리한다. 아래의
`bDeathPresentation`만 death animation branch에 사용하고 `IsPlayerDead()`는
HP/dead snapshot/실제 Dead pose만 계속 검사한다.

```cpp
const bool_t bPoseRequestsDeath =
    pPose && pPose->poseId == static_cast<u16_t>(ePoseStateId::Dead);
const bool_t bOathswornRitual =
    m_World.HasComponent<ReplicatedStateComponent>(e) &&
    (m_World.GetComponent<ReplicatedStateComponent>(e).stateFlags &
        kSnapshotStateKalistaOathswornRitualFlag) != 0u;
const bool_t bDeathPresentation =
    bPoseRequestsDeath || bOathswornRitual;
```

### 1-13-B. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Kalista/Kalista_Registration.cpp

R의 local input은 1단계로 유지하되, server action stage 2를 표현할 수 있도록
이미 존재하는 throw animation을 등록한다.

```cpp
s.animKey = "spell4_call";
s.stageCount = 2u;
s.stageWindowSec = 4.f;
s.stage2TargetMode = eTargetMode::Direction;
s.stage2AnimKey = "spell4_throw";
s.stage2LockSec = 0.35f;
s.stage2Rotate = eRotateMode::TowardsCursor;
s.stage2VisualCastFrame = 4.f;
s.stage2VisualRecoveryFrame = 10.f;
s.stage2VisualPlaySpeed = 1.f;
```

실제 Kalista의 발사는 oathsworn ally Move command가 소유하지만, Sylas가 훔친
Kalista R은 R2 direction command를 직접 보내야 한다. 따라서 client skill definition도
2단계/window/direction을 등록하고, server `CanCastFateCall`이 실제 Kalista의 직접 R2만
거절한다.

### 1-14. C:/Users/user/Desktop/Winters/Client/Public/Scene/RenderVisibilityFilter.h

`bInvisible` 계산 바로 위에 아래를 추가:

```cpp
const bool_t bKalistaCarried =
    world.HasComponent<ReplicatedStateComponent>(entity) &&
    (world.GetComponent<ReplicatedStateComponent>(entity).stateFlags &
        kSnapshotStateKalistaCarriedFlag) != 0u;
if (bKalistaCarried)
    return false;
```

이 flag는 팀/Fog 판정보다 먼저 적용해 carried 본인, 아군, 적 모두 같은 entity mesh를 숨긴다. 발사 시작 tick에는 server tag의 `bHidden=false`가 snapshot에 반영되어 다시 보인다.

### 1-15. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIPerception.h

새 파일:

```cpp
#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersTypes.h"

struct ChampionAIPerception
{
    u64_t factTick = 0u;

    EntityID enemyChampion = NULL_ENTITY;
    EntityID lowHpEnemyChampion = NULL_ENTITY;
    EntityID diveTarget = NULL_ENTITY;
    EntityID enemyMinion = NULL_ENTITY;
    EntityID enemyStructure = NULL_ENTITY;
    EntityID alliedWave = NULL_ENTITY;
    EntityID abilityTarget = NULL_ENTITY;
    EntityID mobilityTarget = NULL_ENTITY;

    f32_t selfHpRatio = 1.f;
    f32_t enemyHpRatio = 1.f;
    f32_t selfGold = 0.f;
    f32_t enemyGold = 0.f;
    u8_t selfLevel = 1u;
    u8_t enemyLevel = 1u;
    f32_t enemyDistance = 999.f;
    f32_t lowHpEnemyRatio = 1.f;
    f32_t lowHpEnemyDistance = 999.f;
    f32_t attackRange = 1.5f;
    f32_t waveDistance = 999.f;
    f32_t turretDanger = 0.f;

    u32_t activeSkillMask = 0u;
    bool_t bEnemyChampionTargetable = false;
    bool_t bCanMove = true;
    bool_t bCanAttack = true;
    bool_t bCanCast = true;
    bool_t bAlliedWaveNearby = false;
    bool_t bStructureWaveTanking = false;
    bool_t bInsideEnemyTurretDanger = false;
};
```

### 1-16. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.h

`ValueInput`에 아래를 추가:

```cpp
f32_t reengageHpRatio = 0.25f;
f32_t fightUtilityWeight = 1.f;
f32_t farmUtilityWeight = 1.f;
f32_t siegeUtilityWeight = 1.f;
f32_t turretRiskWeight = 1.f;
bool_t bEnemyChampionTargetable = false;
```

namespace 마지막에 아래를 추가:

```cpp
struct UtilityScores
{
    f32_t retreat = 0.f;
    f32_t fight = 0.f;
    f32_t farm = 0.f;
    f32_t siege = 0.f;
};

f32_t RetreatValue(const ValueInput& in);
UtilityScores BuildUtilityScores(const ValueInput& in);
```

### 1-17. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIValuation.cpp

`TradeWindow` 아래에 추가:

```cpp
f32_t RetreatValue(const ValueInput& in)
{
    const f32_t reengage = (std::max)(in.reengageHpRatio, 0.01f);
    const f32_t healthPressure = WintersMath::Clamp01(
        (reengage - in.selfHpRatio) / reengage);
    f32_t score = healthPressure * 0.75f +
        in.turretDanger * 0.45f * in.turretRiskWeight;
    if (in.selfHpRatio <= in.retreatHpRatio)
        score = 1.f;
    return WintersMath::Clamp01(score);
}

UtilityScores BuildUtilityScores(const ValueInput& in)
{
    UtilityScores scores{};
    scores.retreat = RetreatValue(in);
    scores.fight = in.bEnemyChampionTargetable
        ? WintersMath::Clamp01(
            ChampionFightValue(in) * in.fightUtilityWeight)
        : 0.f;
    scores.farm = WintersMath::Clamp01(
        MinionFarmValue(in) * in.farmUtilityWeight);
    scores.siege = WintersMath::Clamp01(
        StructureValue(in) * in.siegeUtilityWeight);
    return scores;
}
```

### 1-18. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.h

`ChampionAIBrainInput`을 아래로 교체:

```cpp
struct ChampionAIBrainInput
{
    f32_t fDt = 0.f;
    bool_t bCanAttackChampion = false;
    bool_t bCanAttackStructure = false;
    bool_t bPostComboBAWindow = false;
    f32_t fRetreatScore = 0.f;
    f32_t fChampionScore = 0.f;
    f32_t fFarmScore = 0.f;
    f32_t fStructureScore = 0.f;
};
```

### 1-19. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAIBrain.cpp

RuleBased와 PlayerLike brain은 고정 if 체인 대신 다음 deterministic 우선순위를 사용한다.

```text
hard safety: HP <= retreat threshold 또는 치명적 turret danger -> Retreat
post combo window -> AttackChampion
intent hold가 유효하고 action이 아직 feasible -> 기존 intent 유지
utility max: Retreat / AttackChampion / SiegeStructure / FarmMinion
동점 tie-break: Retreat > AttackChampion > FarmMinion > SiegeStructure
PlayerLike: fight score에 보수적 margin, intent hold 1.5배
```

RuleBased의 새 선택 핵심:

```cpp
if (input.fRetreatScore >= 0.65f)
    return eChampionAIIntent::Retreat;
if (input.bCanAttackChampion &&
    input.fChampionScore >= input.fFarmScore + ai.fChampionScoreMargin &&
    input.fChampionScore >= input.fStructureScore)
{
    return eChampionAIIntent::AttackChampion;
}
if (input.bCanAttackStructure &&
    input.fStructureScore > input.fFarmScore)
{
    return eChampionAIIntent::SiegeStructure;
}
return eChampionAIIntent::FarmMinion;
```

### 1-20. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h

기존 decision score 묶음에 아래를 추가:

```cpp
f32_t fRetreatDecisionScore = 0.f;
```

### 1-21. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

include 영역에 아래를 추가:

```cpp
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIPerception.h"
```

파일 내부 `struct ChampionAIContext` 전체를 삭제하고 아래 compatibility alias로 교체한다.

```cpp
using ChampionAIContext = ChampionAIPerception;
```

`BuildChampionAIContext` 시작에 fact tick과 공용 legality Fact를 기록한다.

```cpp
ChampionAIContext ctx{};
ctx.factTick = tc.tickIndex;
ctx.bCanMove = GameplayStateQuery::CanMove(world, self);
ctx.bCanAttack = GameplayStateQuery::CanAttack(world, self);
ctx.bCanCast = GameplayStateQuery::CanCast(world, self);
```

enemy target 확정 뒤 아래를 추가한다. 현재 서버에는 팀 FOW query가 없으므로
이 값은 `Visible`이 아니라 정확히 `Targetable`이라는 의미로 고정한다.

```cpp
ctx.bEnemyChampionTargetable = targetChampion != NULL_ENTITY;
```

`UpdateChampionAIDecisionEvidence`의 개별 score 계산을 아래로 교체:

```cpp
vin.reengageHpRatio = ai.reengageHpRatio;
vin.fightUtilityWeight = profile.aggression;
vin.farmUtilityWeight =
    profile.minionPressureWeight * profile.lastHitWeight;
vin.siegeUtilityWeight = profile.siegeWeight;
vin.turretRiskWeight = profile.turretRiskWeight;
vin.bEnemyChampionTargetable = ctx.bEnemyChampionTargetable;
const ChampionAIValuation::UtilityScores utility =
    ChampionAIValuation::BuildUtilityScores(vin);
ai.fRetreatDecisionScore = utility.retreat;
ai.fChampionDecisionScore = ai.bCanAttackChampion ? utility.fight : 0.f;
ai.fFarmDecisionScore = utility.farm;
ai.fStructureDecisionScore = utility.siege;
```

`SampleLaneCombatIntent`의 input 구성에 아래를 추가:

```cpp
input.bCanAttackStructure =
    ctx.enemyStructure != NULL_ENTITY && ctx.bStructureWaveTanking;
input.fRetreatScore = ai.fRetreatDecisionScore;
```

`EmitMove/BasicAttack/Skill/FlashCommand`는 command를 큐에 넣기 전에 각각
`CanMove/CanAttack/CanCast`를 검사한다. 서버가 확실히 거절할 CC 상태에서
AI가 콤보 단계를 먼저 넘기는 회귀를 막는다.

```cpp
if (!GameplayStateQuery::CanCast(world, self))
{
    SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::StateBlocked);
    return false;
}
```

`ExecuteLaneCombat` 실행 순서는 다음으로 고정한다.

```text
hard safety interrupt
-> active Jax dive commitment
-> active champion combo commitment
-> new utility/brain selection
-> champion-specific tactics
-> highest feasible ChampionAISkillRule::score
-> BA/chase
SiegeStructure intent:
  structure attack
FarmMinion intent:
  last-hit/farm -> follow wave
```

진행 중 combo/dive는 farm/fight 점수의 순간 변화로 취소하지 않는다. 단, HP 임계치나
치명적 turret danger인 hard safety는 commitment보다 우선한다. 일반 deliberation은
기존 `decisionInterval=0.20s`로 5 Hz를 유지하고, self HP/turret emergency는 timer를
bypass해 30 Hz로 반응한다. 명령은 equivalent-move suppression과 action lock을 계속
통과하므로 같은 Move를 매 tick 재전송하지 않는다.

NEXT NATION의 action utility 본질은 두 층으로 나눈다. 현재 반영 범위는
`Retreat/Fight/Farm/Siege` 전략 utility와 기존 `ChampionAISkillRule::score`,
champion combo plan을 연결하는 1차 수직 슬라이스다. Q/W/E/R/BA/Flash는
availability/cooldown/range/status를 통과한 뒤 기존 `GameCommand` 실행기로만
나간다. 여러 `Combo1/2/3`의 rollout/ETA 기대값 비교, 귀환·구매·시야 추론,
offline self-play 학습은 이 경계를 교체하지 않고 `Decision` brain과 데이터
profile에 추가하는 다음 단계다. 현재 `Decision` brain은 deterministic
RuleBased fallback을 유지하므로 학습 모델이 없어도 서버와 replay가 동일하다.

공용 Perception에는 챔피언 이름이 들어간 필드를 두지 않는다. Yasuo R 대상과
E gap-close 미니언도 `abilityTarget`, `mobilityTarget`, `activeSkillMask`로 전달한다.
150 챔피언별 예외 후보 생성은 다음 단계의 champion tactics registry가 소유한다.

`ShouldAttackChampion` 안의 중복 `SampleLaneCombatIntent` 호출은 삭제한다. Perception/score는 서버 매 tick 갱신하지만 한 decision에서 brain을 두 번 호출해 intent hold timer를 중복 차감하지 않는다.

#### 1-21-A. 최종 P1 회귀 차단 보정

- 계약 아이템은 성공할 때만 소비한다. 서버는 `CanCast`, 같은 팀, 생존,
  targetable, 기존 계약 중복 여부, 14 유닛 거리를 전부 확인한다.
- bot Kalista도 인벤토리 3599를 확인하고 가장 가까운 유효 아군을 선택해
  반드시 `UseItem GameCommand`로 계약한다. GameSim component를 AI가 직접 쓰지 않는다.
- Kalista R combo step은 `CanCastFateCall`을 command enqueue 전에 검사한다.
  계약 없음·거리 밖·상태 capacity 부족을 성공한 combo step으로 기록하지 않는다.
- profile의 `retreatHpRatio/reengageHpRatio`를 hard-coded `0.10/0.25`로
  덮지 않는다. `intentHoldTimer`는 brain 호출 횟수가 아니라 30 Hz simulation tick에서
  `tc.fDt`만큼 정확히 한 번 감소한다.
- Jax dive의 BasicAttack 단계는 사거리 밖이면 chase하고, dive window가 끝나면
  FlashExit/Retreat로 빠져 살아 있는 원거리 target 때문에 영구 정지하지 않는다.
- Fate's Call R2 action은 기존 Sylas command action을 덮지 않는다. 실제 Kalista의
  oathsworn 이동 입력으로 시작할 때도 stage 2 authored move policy/lock과 queued Move를
  보존한다.
- owner entity가 Binding 중 또는 hidden Carrying 중 제거되면 reverse oath/carry tag,
  untargetable status, visibility lock을 reverse sweep으로 제거한다.
- carried/ritual 대상은 snapshot gameplay state를 기준으로 mesh뿐 아니라 world HP bar,
  minimap, hover/attack picking에서도 제외한다.

### 1-22. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

기존 Sylas ultimate coverage probe는 유지한다. 그 아래에 `RunKalistaOathswornFateCallProbe`를 추가해 다음을 자동 검증한다.

```text
1. Kalista slot 6에 item 3599가 없으면 UseItem reject
2. item 3599 + allied target이면 성공 시 3599 소비, Binding, DeathStart action + Idle pose, 이동/공격/시전/피격 lock
3. 1.5초 뒤 Bound, Idle pose, 실제 death/respawn/kill credit 없음
4. non-oathsworn ally 또는 range 밖 ally이면 Kalista R reject/cooldown 미소비
5. bound ally R이면 Pulling -> hidden Carrying
6. human carried ally의 명시적 launch-marker command만 Launching으로 소비됨
7. R 이전 ordinary Move와 발사 완료 뒤 duplicate marker가 발사/이동으로 오인되지 않음
8. segment 충돌 enemy champion/minion에 airborne 적용
9. bot Kalista가 AI GameCommand로 계약하고, bot ally는 Carrying 3초 뒤 자동 launch
10. release 뒤 carried status/tag와 owner FateCall state가 제거됨
11. 기존 Sylas stolen Kalista R stage1/stage2 및 R2 action lock/command sequence 유지
12. Kalista entity destroy 뒤 Binding reverse bond와 hidden Carrying orphan 모두 정리
13. stunned Kalista 및 14 유닛 밖 계약 reject, item 미소비
```

`RunChampionAIStateGateCommitmentProbe`를 추가해 다음을 검증한다.

```text
1. GenericStun 중 Q/W/E/R/BA/Move command 0개
2. GenericAirborne 중 Q/W/E/R/BA/Move command 0개
3. 두 상태에서 active comboStep과 Jax divePhase가 진행되지 않음
4. block reason은 StateBlocked
5. farm utility가 fight utility보다 높아져도 active combo는 계속 실행
6. Kalista profile retreat/reengage 값과 intent hold가 실제 30 Hz 시간으로 적용
7. Jax dive timeout이 BasicAttack 교착을 종료
8. no-oath Kalista R을 command enqueue 전에 reject
9. profile utility weight와 skill-rule score 적용 후 same-seed determinism 유지
```

## 2. 검증

자동 검증:

```text
git diff --check
powershell -ExecutionPolicy Bypass -File Tools/Harness/Check-SharedBoundary.ps1
powershell -ExecutionPolicy Bypass -File Tools/Harness/Run-BotAiValidation.ps1 -SkipFullPipeline
MSBuild Shared/GameSim/Include/GameSim.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
MSBuild Tools/SimLab/SimLab.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
Tools/SimLab/Bin/Debug/SimLab.exe
MSBuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
MSBuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

리소스 검증:

```text
atlas texture: Client/Bin/Resource/Texture/UI/item_icons_atlas.png
manifest sprite: item:3599_kalistapassiveitem.png
1-based cell: column 13, row 14
pixel rect: x=768, y=832, w=64, h=64
individual icon: Client/Bin/Resource/Texture/UI/Items/3599_kalistapassiveitem.png
```

수동 LAN 검증:

```text
Kalista client A + allied human client B + enemy client C
1. A 시작 시 인벤토리 6번째 칸에 3599 아이콘 표시
2. A가 B hover + 6 -> B death animation, 1.5초 입력 잠금, Idle 복귀
3. 계약 전 R은 서버 reject, 계약 후 범위 안 R만 accept
4. R accept -> B가 A에게 당겨지고 숨겨짐
5. B가 지면 우클릭 -> 선택 방향 launch, 세 client에서 같은 위치/visibility
6. 적 충돌 -> server GenericAirborne + 모든 client forced-motion/CC 동기화
7. B 대신 bot을 계약한 경우 3초 뒤 A facing 방향 자동 launch
8. 계약 의식은 HP 0/death score/respawn timer/kill gold를 만들지 않음
```

완료 기준:

- 계약/R/airborne의 gameplay truth가 Shared/Server에만 존재한다.
- Client는 item input, snapshot visibility, animation/FX presentation만 소유한다.
- AI는 매 30 Hz tick Fact를 Perception으로 갱신하고 utility/brain 결과를 기존 GameCommand path로만 실행한다.
- 기존 Sylas stolen Kalista R, ward item 4키, 일반 inventory 구매, Kalista E status 변경 dirty 작업이 회귀하지 않는다.

실행 결과 (2026-07-12 05:30 KST 최종 재검증):

```text
PASS  GameSim Debug x64 build
PASS  Server Debug x64 build -> Server/Bin/Debug/WintersServer.exe
PASS  Client Debug x64 build -> Client/Bin/Debug/WintersGame.exe
PASS  SimLab Debug x64 build and Tools/Bin/Debug/SimLab.exe 1800 42
PASS  [KalistaR] authority gates, ritual, Bound, explicit player/AI launch, airborne, rewards, Binding/Carrying orphan cleanup
PASS  [SylasR] 16-opponent coverage, authority guards, R2 action lock/command sequence, terminal/expiry consume
PASS  [ChampionAI] CC gates, commitment, profile cadence, Jax dive timeout, Kalista R precheck
PASS  same-seed replay hash 87E878A052DB9B1C
PASS  seed+1 sensitivity hash 2B4C36848F981A6F
PASS  Shared boundary audit
PASS  LoL definition pack check, Bot AI full data-driven pipeline and SimLab regression
PASS  git diff --check (whitespace error 0; existing LF/CRLF conversion warnings only)
```

현재 남은 비차단 P2:

```text
서버의 Bound 계약 component와 R mechanics는 reconnect 뒤에도 유지된다.
다만 Snapshot.fbs에 oath ownerNet/allyNet/Bound stage 전용 field가 없어서,
late join/reconnect client가 미래의 계약 상대 UI를 완전히 재구성할 수는 없다.
계약 UI를 추가하는 세션에서 OathswornSnapshot(ownerNet, allyNet, stage)을
명시적으로 추가하고 snapshot reconnect probe로 고정한다.
```

검증 보고서:

```text
C:/Users/user/Desktop/Winters/.md/build/2026-07-12_S011_BOT_AI_VALIDATION.md
```
