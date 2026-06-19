Session - InGame/Champion replication을 더 나눌 수 없는 Pose/Action 원자 contract로 바꾸고 legacy animation을 삭제 가능한 migration layer로 격리한다.

1. 반영해야 하는 코드

개념:
- 다시 의심한 결론: 이전 계획서는 "최종 본질"이 아니라 "회귀 방지용 병행 계획"이었다.
- `AnimationStartEvent`를 유지하면서 `ActionStartEvent`를 준비만 하는 구조는 본질이 아니다. 쓰지 않는 새 event는 원자가 아니라 중복이다.
- Snapshot에 `poseSeq`를 추가하는 것도 본질이 아니다. pose는 지속 상태이고, 변경 시점은 `poseStartTick` 하나로 충분하다.
- wire의 `playbackRateQ8`, `flags`, `animPhaseFrame`도 본질이 아니다. 이것들은 "어떻게 보여줄지"에 속한다.
- 최종 본질은 아래 네 가지뿐이다.

```text
Pose   = 지금 어떤 몸 상태인가?
Action = 지금 어떤 행위가 언제 시작되었는가?
Visual = 그 pose/action을 어떤 애니메이션/속도/loop/hook으로 보여줄 것인가?
Legacy = 기존 기능 회귀를 막기 위해 잠깐 남는 변환층이다.
```

원자 단위:
- `ReplicatedPoseComponent`: `poseId`, `startTick`
- `ReplicatedActionComponent`: `actionId`, `startTick`, `sequence`, `stage`
- `Snapshot`: 지속 상태인 pose와 현재/최근 action fact만 보낸다.
- `Event`: 새 action 시작만 보낸다.
- `Client visual`: action fact를 animation으로 해석한다.
- `NetAnimationComponent`: 최종 본질이 아니다. migration layer 밖에서는 읽거나 쓰지 않게 만든 뒤 삭제한다.

의심 결과:
- `poseSeq`는 삭제한다. 같은 pose 재시작이 필요하면 `startTick`이 바뀌면 된다.
- `action.sequence`는 유지한다. 같은 tick 안에서 같은 action이 다시 시작될 수 있고, event dedup에는 별도 sequence가 필요하다.
- `action.stage`는 유지한다. stage는 animation 선택값이 아니라 같은 skill action 안의 gameplay variant다.
- `ActionStartEvent`는 준비만 하지 않는다. 추가하는 순간 active event가 된다.
- `AnimationStartEvent`는 최종 wire에서 삭제한다. 단, 삭제 전 client visual 호환 resolver를 먼저 붙여 기능 회귀를 막는다.
- `playbackRateQ8`는 server wire에서 삭제한다. Client가 legacy visual resolver나 `ChampionVisualData`에서 재생 속도를 구한다.
- `flags`는 server wire에서 삭제한다. loop/stage/hook은 각각 `action.stage`와 Client visual source가 가진다.

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ReplicatedPoseComponent.h

기존 코드:

```cpp
struct ReplicatedPoseComponent
{
    u16_t poseId = static_cast<u16_t>(eReplicatedPoseId::Idle);
    u64_t startTick = 0;
    u32_t sequence = 0;
};
```

아래로 교체:

```cpp
struct ReplicatedPoseComponent
{
    u16_t poseId = static_cast<u16_t>(eReplicatedPoseId::Idle);
    u64_t startTick = 0;
};
```

검토 메모:
- pose는 event가 아니다. 그래서 sequence가 본질이 아니다.
- Idle -> Idle을 강제로 다시 시작해야 하는 경우도 `startTick` 변경으로 충분하다.

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ReplicatedActionComponent.h

기존 코드는 유지한다.

```cpp
struct ReplicatedActionComponent
{
    u16_t actionId = static_cast<u16_t>(eReplicatedActionId::None);
    u64_t startTick = 0;
    u32_t sequence = 0;
    u8_t stage = 1;
};
```

검토 메모:
- `actionId`: 무엇을 시작했는가.
- `startTick`: 언제 시작했는가.
- `sequence`: 같은 action 재시작과 event dedup을 어떻게 구분하는가.
- `stage`: 같은 action 안의 어떤 gameplay variant인가.
- 이 넷은 더 줄이면 runtime 의미가 사라진다.

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/NetAnimationMigrationBridge.h

새 파일:

```cpp
#pragma once

#include "ECS/Entity.h"
#include "Shared/GameSim/Components/NetAnimationComponent.h"
#include "Shared/GameSim/Components/ReplicatedActionComponent.h"
#include "Shared/GameSim/Components/ReplicatedPoseComponent.h"
#include "WintersTypes.h"

inline bool_t TryResolvePoseIdFromLegacyNetAnimation(
    eNetAnimId animId,
    u16_t& outPoseId)
{
    switch (animId)
    {
    case eNetAnimId::Idle:
        outPoseId = static_cast<u16_t>(eReplicatedPoseId::Idle);
        return true;
    case eNetAnimId::Run:
        outPoseId = static_cast<u16_t>(eReplicatedPoseId::Run);
        return true;
    case eNetAnimId::Death:
        outPoseId = static_cast<u16_t>(eReplicatedPoseId::Dead);
        return true;
    default:
        outPoseId = static_cast<u16_t>(eReplicatedPoseId::None);
        return false;
    }
}

inline bool_t TryResolveActionIdFromLegacyNetAnimation(
    eNetAnimId animId,
    u16_t& outActionId)
{
    switch (animId)
    {
    case eNetAnimId::BasicAttack:
        outActionId = static_cast<u16_t>(eReplicatedActionId::BasicAttack);
        return true;
    case eNetAnimId::SkillQ:
        outActionId = static_cast<u16_t>(eReplicatedActionId::SkillQ);
        return true;
    case eNetAnimId::SkillW:
        outActionId = static_cast<u16_t>(eReplicatedActionId::SkillW);
        return true;
    case eNetAnimId::SkillE:
        outActionId = static_cast<u16_t>(eReplicatedActionId::SkillE);
        return true;
    case eNetAnimId::SkillR:
        outActionId = static_cast<u16_t>(eReplicatedActionId::SkillR);
        return true;
    case eNetAnimId::Recall:
        outActionId = static_cast<u16_t>(eReplicatedActionId::Recall);
        return true;
    case eNetAnimId::Death:
        outActionId = static_cast<u16_t>(eReplicatedActionId::DeathStart);
        return true;
    case eNetAnimId::ViegoConsumeSoul:
        outActionId = static_cast<u16_t>(eReplicatedActionId::ViegoConsumeSoul);
        return true;
    default:
        outActionId = static_cast<u16_t>(eReplicatedActionId::None);
        return false;
    }
}

inline u8_t ResolveActionStageFromLegacyNetAnimation(const NetAnimationComponent& anim)
{
    const u8_t stage = static_cast<u8_t>((anim.flags >> 12) & 0x0fu);
    return stage == 0u ? 1u : stage;
}

template <typename TWorld>
void SyncPoseActionFromLegacyNetAnimation(
    TWorld& world,
    EntityID entity,
    const NetAnimationComponent& anim)
{
    const eNetAnimId animId = static_cast<eNetAnimId>(anim.animId);

    u16_t poseId = static_cast<u16_t>(eReplicatedPoseId::None);
    if (TryResolvePoseIdFromLegacyNetAnimation(animId, poseId))
    {
        auto& pose = world.template HasComponent<ReplicatedPoseComponent>(entity)
            ? world.template GetComponent<ReplicatedPoseComponent>(entity)
            : world.template AddComponent<ReplicatedPoseComponent>(
                entity,
                ReplicatedPoseComponent{});

        pose.poseId = poseId;
        pose.startTick = anim.animStartTick;
    }

    u16_t actionId = static_cast<u16_t>(eReplicatedActionId::None);
    if (TryResolveActionIdFromLegacyNetAnimation(animId, actionId))
    {
        auto& action = world.template HasComponent<ReplicatedActionComponent>(entity)
            ? world.template GetComponent<ReplicatedActionComponent>(entity)
            : world.template AddComponent<ReplicatedActionComponent>(
                entity,
                ReplicatedActionComponent{});

        action.actionId = actionId;
        action.startTick = anim.animStartTick;
        action.sequence = anim.actionSeq;
        action.stage = ResolveActionStageFromLegacyNetAnimation(anim);
    }
}
```

검토 메모:
- 이 파일은 본질 파일이 아니라 migration 파일이다.
- 이름에 `Legacy`/`Migration`이 들어가야 한다. 그래야 삭제 대상임이 코드에서 보인다.
- `None` action을 idle update 때마다 써서 action component를 덮지 않는다. action이 없으면 action fact도 없다.

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ReplicatedAnimationBridge.h

삭제할 코드:

```cpp
#pragma once
```

삭제 범위:
파일 전체 삭제.

검토 메모:
- 이 이름은 마치 최종 replicated 구조의 일부처럼 보인다.
- 실제 역할은 legacy `NetAnimationComponent`를 pose/action으로 번역하는 migration bridge다.
- 따라서 `NetAnimationMigrationBridge.h`로 교체한다.

1-5. 아래 파일들의 include와 호출명 교체

대상 파일:
- C:/Users/tnest/Desktop/Winters/Tools/SimLab/main.cpp
- C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/ChampionSpawnService.cpp
- C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp
- C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp
- C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomInternal.cpp
- C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomMinionAI.cpp
- C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomSpawn.cpp
- C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Annie/AnnieGameSim.cpp
- C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Jax/JaxGameSim.cpp
- C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Viego/ViegoGameSim.cpp
- C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Yone/YoneGameSim.cpp
- C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp
- C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/Move/MoveSystem.cpp
- C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/Recall/RecallSystem.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/ReplicatedAnimationBridge.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/NetAnimationMigrationBridge.h"
```

기존 코드:

```cpp
SyncReplicatedPoseActionFromNetAnimation(
```

아래로 교체:

```cpp
SyncPoseActionFromLegacyNetAnimation(
```

검토 메모:
- 이 교체는 이름을 정직하게 만드는 작업이다.
- 본질 path가 아니라 migration path임을 드러내는 것이 목표다.

1-6. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Snapshot.fbs

`table EntitySnapshot`의 animation field를 pose/action field로 교체한다.

기존 코드:

```fbs
    moveSpeed:float;
    animId:ushort;
    animPhaseFrame:ushort;
    skillCooldowns:[float];
```

아래로 교체:

```fbs
    moveSpeed:float;
    poseId:ushort;
    poseStartTick:ulong;
    actionId:ushort;
    actionStartTick:ulong;
    actionSeq:uint;
    actionStage:ubyte = 1;
    skillCooldowns:[float];
```

기존 코드:

```fbs
    stateFlags:uint;
    animStartTick:ulong;
    actionSeq:uint;
    animPlaybackRateQ8:ushort = 256;
    animFlags:ushort;
    projectileKind:ushort;
```

아래로 교체:

```fbs
    stateFlags:uint;
    projectileKind:ushort;
```

검토 메모:
- Snapshot은 "지금 상태"만 보낸다.
- animation id, phase frame, playback rate, flags는 wire 본질이 아니다.
- `actionSeq`는 action field 묶음으로 이동한다.

1-7. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Event.fbs

`enum EventKind`에서 animation 이름을 action 이름으로 교체한다.

기존 코드:

```fbs
    AnimationStart = 18,
    EffectTrigger = 19
```

아래로 교체:

```fbs
    ActionStart = 18,
    EffectTrigger = 19
```

기존 코드:

```fbs
table AnimationStartEvent {
    netId:uint;
    animId:ushort;
    actionSeq:uint;
    startTick:ulong;
    playbackRateQ8:ushort;
    flags:ushort;
}
```

아래로 교체:

```fbs
table ActionStartEvent {
    netId:uint;
    actionId:ushort;
    actionStage:ubyte = 1;
    actionSeq:uint;
    startTick:ulong;
}
```

기존 코드:

```fbs
    animation:AnimationStartEvent;
    effect:EffectTriggerEvent;
```

아래로 교체:

```fbs
    actionStart:ActionStartEvent;
    effect:EffectTriggerEvent;
```

검토 메모:
- event는 "시작했다"는 사실만 보낸다.
- 어떤 animation clip을 재생할지는 Client visual source의 일이다.
- `ActionStart = 18`로 기존 kind 번호를 재사용한다. 의미는 유지하고 이름과 payload만 본질화한다.

1-8. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Generated/cpp/Snapshot_generated.h

CONFIRM_NEEDED:
- 직접 편집하지 않는다.
- `Shared/Schemas/run_codegen.bat`로만 갱신한다.
- 생성 후 아래 accessor가 있어야 한다.

```cpp
uint16_t poseId() const;
uint64_t poseStartTick() const;
uint16_t actionId() const;
uint64_t actionStartTick() const;
uint32_t actionSeq() const;
uint8_t actionStage() const;
```

생성 후 아래 accessor는 없어야 한다.

```cpp
uint16_t animId() const;
uint16_t animPhaseFrame() const;
uint64_t animStartTick() const;
uint16_t animPlaybackRateQ8() const;
uint16_t animFlags() const;
```

1-9. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Generated/cpp/Event_generated.h

CONFIRM_NEEDED:
- 직접 편집하지 않는다.
- `Shared/Schemas/run_codegen.bat`로만 갱신한다.
- 생성 후 아래 타입이 있어야 한다.

```cpp
struct ActionStartEvent;
inline ::flatbuffers::Offset<ActionStartEvent> CreateActionStartEvent(...);
EventKind::ActionStart;
```

생성 후 아래 타입은 없어야 한다.

```cpp
struct AnimationStartEvent;
inline ::flatbuffers::Offset<AnimationStartEvent> CreateAnimationStartEvent(...);
EventKind::AnimationStart;
```

1-10. C:/Users/tnest/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

include 영역에서 아래 기존 코드 삭제:

삭제할 코드:

```cpp
#include "Shared/GameSim/Components/NetAnimationComponent.h"
```

include 영역에 아래 코드 추가:

```cpp
#include "Shared/GameSim/Components/ReplicatedActionComponent.h"
#include "Shared/GameSim/Components/ReplicatedPoseComponent.h"
```

entity local 변수 선언 영역에서 아래 기존 코드 교체:

기존 코드:

```cpp
        u16_t animId = static_cast<u16_t>(eNetAnimId::Idle);
        u16_t animPhaseFrame = 0;
        u64_t animStartTick = 0;
        u32_t actionSeq = 0;
        u16_t animPlaybackRateQ8 = 256;
        u16_t animFlags = 0;
```

아래로 교체:

```cpp
        u16_t poseId = static_cast<u16_t>(eReplicatedPoseId::Idle);
        u64_t poseStartTick = 0;
        u16_t actionId = static_cast<u16_t>(eReplicatedActionId::None);
        u64_t actionStartTick = 0;
        u32_t actionSeq = 0;
        u8_t actionStage = 1;
```

아래 기존 block 삭제:

삭제할 코드:

```cpp
        if (world.HasComponent<NetAnimationComponent>(entity))
        {
            const auto& anim = world.GetComponent<NetAnimationComponent>(entity);
            animId = anim.animId;
            animPhaseFrame = anim.animPhaseFrame;
            animStartTick = anim.animStartTick;
            actionSeq = anim.actionSeq;
            animPlaybackRateQ8 = anim.playbackRateQ8;
            animFlags = anim.flags;
        }
```

삭제한 위치에 아래 코드 추가:

```cpp
        if (world.HasComponent<ReplicatedPoseComponent>(entity))
        {
            const auto& pose = world.GetComponent<ReplicatedPoseComponent>(entity);
            poseId = pose.poseId;
            poseStartTick = pose.startTick;
        }

        if (world.HasComponent<ReplicatedActionComponent>(entity))
        {
            const auto& action = world.GetComponent<ReplicatedActionComponent>(entity);
            actionId = action.actionId;
            actionStartTick = action.startTick;
            actionSeq = action.sequence;
            actionStage = action.stage;
        }
```

`[YawTrace][ServerSnapshot]` debug format에서 `anim=%u`를 `pose=%u action=%u stage=%u`로 교체한다.

기존 코드:

```cpp
                    static_cast<u32_t>(animId),
                    actionSeq,
```

아래로 교체:

```cpp
                    static_cast<u32_t>(poseId),
                    static_cast<u32_t>(actionId),
                    static_cast<u32_t>(actionStage),
                    actionSeq,
```

`CreateEntitySnapshot` 호출 인자는 schema codegen 후 실제 signature에 맞춰 교체한다.

CONFIRM_NEEDED:
- `Shared/Schemas/run_codegen.bat` 실행 후 `CreateEntitySnapshot`의 정확한 parameter 순서를 확인한다.
- 기존 `animId`, `animPhaseFrame`, `animStartTick`, `animPlaybackRateQ8`, `animFlags` 인자를 모두 제거한다.
- 새 `poseId`, `poseStartTick`, `actionId`, `actionStartTick`, `actionSeq`, `actionStage` 인자를 넣는다.

1-11. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.h

include 영역에서 아래 기존 코드 교체:

기존 코드:

```cpp
#include "Shared/GameSim/Components/NetAnimationComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/ReplicatedActionComponent.h"
```

public 영역에서 아래 기존 코드 교체:

기존 코드:

```cpp
        static bool_t BuildAnimationStart(
            NetEntityId netId,
            const NetAnimationComponent& anim,
            u64_t serverTick,
            SerializedReplicatedEvent& out);
```

아래로 교체:

```cpp
        static bool_t BuildActionStart(
            NetEntityId netId,
            const ReplicatedActionComponent& action,
            u64_t serverTick,
            SerializedReplicatedEvent& out);
```

1-12. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.cpp

아래 기존 함수 전체 교체:

기존 코드:

```cpp
    bool_t CReplicatedEventSerializer::BuildAnimationStart(
        NetEntityId netId,
        const NetAnimationComponent& anim,
        u64_t serverTick,
        SerializedReplicatedEvent& out)
```

아래로 교체:

```cpp
    bool_t CReplicatedEventSerializer::BuildActionStart(
        NetEntityId netId,
        const ReplicatedActionComponent& action,
        u64_t serverTick,
        SerializedReplicatedEvent& out)
    {
        Reset(out);

        if (netId == NULL_NET_ENTITY ||
            action.actionId == static_cast<u16_t>(eReplicatedActionId::None) ||
            action.sequence == 0)
        {
            return false;
        }

        flatbuffers::FlatBufferBuilder fbb(128);
        const auto actionStart = Shared::Schema::CreateActionStartEvent(
            fbb,
            netId,
            action.actionId,
            action.stage,
            action.sequence,
            action.startTick);

        return Finish(fbb, Shared::Schema::CreateEventPacket(
            fbb,
            Shared::Schema::EventKind::ActionStart,
            serverTick,
            0,
            0,
            0,
            0,
            0,
            actionStart,
            0,
            0), out);
    }
```

CONFIRM_NEEDED:
- codegen 후 `CreateEventPacket` parameter 순서를 실제 generated signature와 맞춘다.
- `BuildAnimationStart`는 남기지 않는다.

1-13. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomReplication.cpp

include 영역에서 아래 기존 코드 교체:

기존 코드:

```cpp
#include "Shared/GameSim/Components/NetAnimationComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/ReplicatedActionComponent.h"
```

`Phase_BroadcastEvents` 안의 animation event 수집 block을 교체한다.

기존 코드:

```cpp
    struct AnimEvent
    {
        NetEntityId netId = NULL_NET_ENTITY;
        NetAnimationComponent anim{};
    };

    std::vector<AnimEvent> events;
    m_world.ForEach<NetAnimationComponent>(
        std::function<void(EntityID, NetAnimationComponent&)>(
            [&](EntityID entity, NetAnimationComponent& anim)
            {
                if (anim.actionSeq == 0)
                    return;

                const NetEntityId netId = m_entityMap.ToNet(entity);
                if (netId == NULL_NET_ENTITY)
                    return;

                u32_t& lastSeq = m_lastBroadcastActionSeq[entity];
                if (lastSeq == anim.actionSeq)
                    return;

                lastSeq = anim.actionSeq;
                events.push_back(AnimEvent{ netId, anim });
            }));
```

아래로 교체:

```cpp
    struct ActionEvent
    {
        NetEntityId netId = NULL_NET_ENTITY;
        ReplicatedActionComponent action{};
    };

    std::vector<ActionEvent> events;
    m_world.ForEach<ReplicatedActionComponent>(
        std::function<void(EntityID, ReplicatedActionComponent&)>(
            [&](EntityID entity, ReplicatedActionComponent& action)
            {
                if (action.actionId == static_cast<u16_t>(eReplicatedActionId::None) ||
                    action.sequence == 0)
                {
                    return;
                }

                const NetEntityId netId = m_entityMap.ToNet(entity);
                if (netId == NULL_NET_ENTITY)
                    return;

                u32_t& lastSeq = m_lastBroadcastActionSeq[entity];
                if (lastSeq == action.sequence)
                    return;

                lastSeq = action.sequence;
                events.push_back(ActionEvent{ netId, action });
            }));
```

event serialize 호출을 교체한다.

기존 코드:

```cpp
        if (!SharedSim::CReplicatedEventSerializer::BuildAnimationStart(
            ev.netId,
            ev.anim,
            tc.tickIndex,
            serialized))
```

아래로 교체:

```cpp
        if (!SharedSim::CReplicatedEventSerializer::BuildActionStart(
            ev.netId,
            ev.action,
            tc.tickIndex,
            serialized))
```

1-14. C:/Users/tnest/Desktop/Winters/Client/Public/Network/Client/EventApplier.h

namespace forward declaration 영역에서 아래 기존 코드 교체:

기존 코드:

```cpp
    struct AnimationStartEvent;
```

아래로 교체:

```cpp
    struct ActionStartEvent;
```

private 영역에서 아래 기존 코드 교체:

기존 코드:

```cpp
    void ApplyAnimationStart(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::AnimationStartEvent* ev);
```

아래로 교체:

```cpp
    void ApplyActionStart(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::ActionStartEvent* ev);
```

기존 코드:

```cpp
    void PlayNetworkAnimation(
        CWorld& world,
        EntityID entity,
        u16_t animId,
        u16_t playbackRateQ8,
        u16_t flags);
```

아래로 교체:

```cpp
    void PlayReplicatedActionVisual(
        CWorld& world,
        EntityID entity,
        u16_t actionId,
        u8_t actionStage);
```

기존 코드:

```cpp
    std::unordered_map<NetEntityId, u32_t> m_lastAnimationSeq;
```

아래로 교체:

```cpp
    std::unordered_map<NetEntityId, u32_t> m_lastActionSeq;
```

1-15. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

dispatch switch에서 아래 기존 코드 교체:

기존 코드:

```cpp
    case Shared::Schema::EventKind::AnimationStart:
        ApplyAnimationStart(world, entityMap, packet->animation());
        break;
```

아래로 교체:

```cpp
    case Shared::Schema::EventKind::ActionStart:
        ApplyActionStart(world, entityMap, packet->actionStart());
        break;
```

아래 기존 함수 전체 교체:

기존 코드:

```cpp
void CEventApplier::ApplyAnimationStart(
    CWorld& world,
    EntityIdMap& entityMap,
    const Shared::Schema::AnimationStartEvent* ev)
```

아래로 교체:

```cpp
void CEventApplier::ApplyActionStart(
    CWorld& world,
    EntityIdMap& entityMap,
    const Shared::Schema::ActionStartEvent* ev)
{
    if (!ev || ev->netId() == NULL_NET_ENTITY)
        return;

    const EntityID entity = entityMap.FromNet(ev->netId());
    if (entity == NULL_ENTITY)
        return;

    auto& action = world.HasComponent<ReplicatedActionComponent>(entity)
        ? world.GetComponent<ReplicatedActionComponent>(entity)
        : world.AddComponent<ReplicatedActionComponent>(
            entity,
            ReplicatedActionComponent{});

    const u32_t previousPlayedSeq = m_lastActionSeq[ev->netId()];
    const bool_t bShouldPlay =
        IsNewerActionSeq(ev->actionSeq(), previousPlayedSeq);

    action.actionId = ev->actionId();
    action.stage = ev->actionStage();
    action.sequence = ev->actionSeq();
    action.startTick = ev->startTick();

    if (!bShouldPlay)
        return;

    m_lastActionSeq[ev->netId()] = ev->actionSeq();
    PlayReplicatedActionVisual(world, entity, ev->actionId(), ev->actionStage());
}
```

`PlayNetworkAnimation` 함수는 `PlayReplicatedActionVisual`로 교체한다.

CONFIRM_NEEDED:
- 현재 함수 본문은 `eNetAnimId`와 legacy `ChampionDef`/`SkillDef`를 직접 사용한다.
- 교체 함수는 action id를 입력으로 받아 Client-side legacy visual resolver에서 animation name, playback speed, loop 여부를 얻어야 한다.
- 이 resolver는 최종 본질이 아니며 `ChampionVisualData` migration 전까지만 존재한다.

1-16. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

Snapshot animation mirror 적용 block을 pose/action mirror 적용 block으로 교체한다.

기존 코드:

```cpp
        if (!world.HasComponent<NetAnimationComponent>(e))
            world.AddComponent<NetAnimationComponent>(e, NetAnimationComponent{});

        auto& anim = world.GetComponent<NetAnimationComponent>(e);
        anim.animId = es->animId();
        anim.animPhaseFrame = es->animPhaseFrame();
        anim.animStartTick = es->animStartTick();
        anim.actionSeq = es->actionSeq();
        anim.playbackRateQ8 = es->animPlaybackRateQ8();
        anim.flags = es->animFlags();
        SyncReplicatedPoseActionFromNetAnimation(world, e, anim);
```

아래로 교체:

```cpp
        auto& pose = world.HasComponent<ReplicatedPoseComponent>(e)
            ? world.GetComponent<ReplicatedPoseComponent>(e)
            : world.AddComponent<ReplicatedPoseComponent>(
                e,
                ReplicatedPoseComponent{});

        pose.poseId = es->poseId();
        pose.startTick = es->poseStartTick();

        auto& action = world.HasComponent<ReplicatedActionComponent>(e)
            ? world.GetComponent<ReplicatedActionComponent>(e)
            : world.AddComponent<ReplicatedActionComponent>(
                e,
                ReplicatedActionComponent{});

        action.actionId = es->actionId();
        action.startTick = es->actionStartTick();
        action.sequence = es->actionSeq();
        action.stage = es->actionStage();
```

minion state update에서 아래 기존 코드 교체:

기존 코드:

```cpp
                    const auto netAnim = static_cast<eNetAnimId>(es->animId());
                    const bool_t bServerAttack =
                        ((es->stateFlags() & kSnapshotStateAttackFlag) != 0u) ||
                        netAnim == eNetAnimId::BasicAttack;
```

아래로 교체:

```cpp
                    const auto poseId =
                        static_cast<eReplicatedPoseId>(es->poseId());
                    const auto actionId =
                        static_cast<eReplicatedActionId>(es->actionId());
                    const bool_t bServerAttack =
                        ((es->stateFlags() & kSnapshotStateAttackFlag) != 0u) ||
                        actionId == eReplicatedActionId::BasicAttack;
```

기존 코드:

```cpp
                    else if (netAnim == eNetAnimId::Run)
```

아래로 교체:

```cpp
                    else if (poseId == eReplicatedPoseId::Run)
```

기존 코드:

```cpp
                    else if (netAnim == eNetAnimId::Idle)
```

아래로 교체:

```cpp
                    else if (poseId == eReplicatedPoseId::Idle)
```

`EnsureSnapshotEntity` 안의 아래 기존 코드 삭제:

삭제할 코드:

```cpp
    if (!world.HasComponent<NetAnimationComponent>(e))
        world.AddComponent<NetAnimationComponent>(e, NetAnimationComponent{});
```

검토 메모:
- Client snapshot mirror는 더 이상 legacy animation component를 만들지 않는다.
- visual 재생은 Event의 `ActionStart`와 Snapshot의 `Pose`를 따로 본다.

1-17. C:/Users/tnest/Desktop/Winters/Client/Public/Network/Client/NetworkEventTrace.h

`eTraceKind` enum에서 아래 기존 코드 교체:

기존 코드:

```cpp
        AnimationStart,
```

아래로 교체:

```cpp
        ActionStart,
```

1-18. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/NetworkEventTrace.cpp

event trace switch에서 아래 기존 코드 교체:

기존 코드:

```cpp
    case Shared::Schema::EventKind::AnimationStart:
        if (const auto* ev = packet->animation())
        {
            entry.kind = eTraceKind::AnimationStart;
            entry.sourceNet = ev->netId();
            entry.idA = ev->animId();
            entry.idB = ev->actionSeq();
            entry.value = static_cast<f32_t>(ev->playbackRateQ8()) / 256.f;
        }
        break;
```

아래로 교체:

```cpp
    case Shared::Schema::EventKind::ActionStart:
        if (const auto* ev = packet->actionStart())
        {
            entry.kind = eTraceKind::ActionStart;
            entry.sourceNet = ev->netId();
            entry.idA = ev->actionId();
            entry.idB = ev->actionSeq();
            entry.value = static_cast<f32_t>(ev->actionStage());
        }
        break;
```

UI label 교체:

기존 코드:

```cpp
    ImGui::Text("AnimationStart: %u", GetCount(eTraceKind::AnimationStart));
```

아래로 교체:

```cpp
    ImGui::Text("ActionStart: %u", GetCount(eTraceKind::ActionStart));
```

문자열 교체:

기존 코드:

```cpp
    case eTraceKind::AnimationStart: return "AnimationStart";
```

아래로 교체:

```cpp
    case eTraceKind::ActionStart: return "ActionStart";
```

1-19. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/NetAnimationComponent.h

삭제 대상:
파일 전체 삭제.

CONFIRM_NEEDED:
- 아래 검색이 0이 된 뒤 삭제한다.

```text
rg -n "NetAnimationComponent|eNetAnimId|playbackRateQ8|animPhaseFrame|animFlags|AnimationStart" Shared Server Client Tools
```

검토 메모:
- 삭제가 마지막이 아니다. 삭제 가능한 상태를 만드는 것이 먼저다.
- `NetAnimationMigrationBridge.h`도 이 검색이 0이 되는 마지막 단계에서 같이 삭제한다.

1-20. C:/Users/tnest/Desktop/Winters/Client visual compatibility resolver

CONFIRM_NEEDED:
- `PlayReplicatedActionVisual`이 기존 visual 기능을 회귀 없이 재생하려면 action id에서 animation name, playback speed, loop 여부를 얻는 client-only resolver가 필요하다.
- 이 resolver는 최종적으로 `ChampionVisualData`로 대체한다.
- resolver는 `Client` 아래에 둔다. `Shared/GameSim` 또는 `Server`에 두지 않는다.

필수 반환값:

```cpp
struct ReplicatedActionVisualPlayback
{
    std::string animationName{};
    f32_t playbackSpeed = 1.f;
    bool_t bLoop = false;
};
```

필수 입력값:

```cpp
eChampion champion;
u16_t actionId;
u8_t actionStage;
```

검토 메모:
- 이 resolver가 legacy `ChampionDef`/`SkillDef`를 읽는 것은 허용한다.
- 단, 파일명이나 namespace에 `Legacy`가 들어가야 한다.
- Server가 animation name, speed, loop를 고르는 구조로 되돌아가면 실패다.

2. 검증

자동 검증:
- `git diff --check`
- `Shared/Schemas/run_codegen.bat`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`
- `Tools/Bin/Debug/SimLab.exe 300 42`
- `Tools/Bin/Debug/SimLab.exe`

본질 검색:
- `rg -n "AnimationStart|AnimationStartEvent|animId\\(|animPhaseFrame\\(|animStartTick\\(|animPlaybackRateQ8\\(|animFlags\\(" Shared/Schemas Shared/GameSim Server Client`
- 위 검색은 최종 단계에서 0이어야 한다.

허용되는 legacy 검색:
- `rg -n "NetAnimationMigrationBridge|Legacy.*Visual|NetAnimationComponent|eNetAnimId" Shared Server Client Tools`
- 구현 중간 단계에서는 legacy/migration 파일과 아직 전환하지 않은 writer에만 남을 수 있다.
- 최종 삭제 gate에서는 0이어야 한다.

Snapshot 확인:
- `rg -n "poseId|poseStartTick|actionId|actionStartTick|actionSeq|actionStage" Shared/Schemas/Generated/cpp/Snapshot_generated.h Server/Private/Game/SnapshotBuilder.cpp Client/Private/Network/Client/SnapshotApplier.cpp`
- `poseSeq`가 남아 있으면 실패다.

Event 확인:
- `rg -n "ActionStart|ActionStartEvent|BuildActionStart|ApplyActionStart" Shared/Schemas/Generated/cpp/Event_generated.h Shared/GameSim/Systems/ReplicatedEventSerializer Server/Private/Game Client/Private/Network/Client`
- `BuildAnimationStart`, `ApplyAnimationStart`, `PlayNetworkAnimation`, `m_lastAnimationSeq`가 남아 있으면 실패다.

회귀 확인:
- normal F5에서 player champion Idle/Run 확인.
- lane minion Idle/Run/Attack/Death 확인.
- jungle monster Idle/Attack/Death 확인.
- BasicAttack, SkillQ/W/E/R, Recall, DeathStart, ViegoConsumeSoul 확인.
- Jax E loop와 release 확인.
- Yasuo Q stage 확인.
- skill animation speed가 기존과 달라지지 않는지 확인.
- Network Event Trace에 `ActionStart`가 기록되고 `AnimationStart`가 더 이상 보이지 않는지 확인.

실패 조건:
- Server 또는 Shared/GameSim이 animation key, playback speed, loop, visual hook을 고르면 실패다.
- Snapshot/Event wire에 `anim*`, `playbackRate`, `flags`가 남으면 실패다.
- pose에 sequence가 남으면 실패다.
- 새 action event를 추가만 하고 active path로 쓰지 않으면 실패다.
- migration/legacy 이름이 아닌 파일에 legacy 변환 로직이 숨어 있으면 실패다.
