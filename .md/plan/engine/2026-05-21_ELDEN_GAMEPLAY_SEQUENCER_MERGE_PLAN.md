Session - Elden Ring 준비용 서버 권위 Gameplay Sequencer MVP를 Winters Engine에 병합한다. 범위는 런타임 tick, AnimationStart, EffectTrigger, DamageRequest까지이며 editor/asset serialization은 후속 세션으로 둔다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/GameplaySequenceComponent.h

새 파일:

```cpp
#pragma once

#include "ECS/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <type_traits>

constexpr u8_t kGameplaySequenceMaxCues = 8;

enum class eGameplaySequenceCueKind : u8_t
{
    None = 0,
    Animation = 1,
    Effect = 2,
    Damage = 3,
};

enum class eGameplaySequenceTarget : u8_t
{
    Source = 0,
    Target = 1,
    WorldPosition = 2,
};

struct GameplaySequenceCue
{
    eGameplaySequenceCueKind eKind = eGameplaySequenceCueKind::None;
    eGameplaySequenceTarget eTarget = eGameplaySequenceTarget::Source;
    u16_t uTriggerTickOffset = 0;
    u16_t uDurationMs = 0;
    u16_t uAnimId = 0;
    u16_t uPlaybackRateQ8 = 256;
    u16_t uFlags = 0;
    u16_t uAttachBone = 0;
    u16_t uSkillId = 0;
    u16_t uReserved = 0;
    u32_t uEffectId = 0;
    f32_t fDamageAmount = 0.f;
    u8_t uDamageType = 0;
    u8_t uRank = 1;
    u16_t uPad = 0;
    Vec3 vPosition{};
    Vec3 vDirection{};
};

struct GameplaySequenceComponent
{
    u32_t uSequenceId = 0;
    EntityID entitySource = NULL_ENTITY;
    EntityID entityTarget = NULL_ENTITY;
    u64_t uStartTick = 0;
    u16_t uDurationTicks = 0;
    u8_t uCueCount = 0;
    u8_t uFiredMask = 0;
    bool_t bActive = false;
    u8_t uPad0 = 0;
    u16_t uPad1 = 0;
    GameplaySequenceCue cues[kGameplaySequenceMaxCues]{};
};

static_assert(std::is_trivially_copyable_v<GameplaySequenceCue>,
    "GameplaySequenceCue must remain POD for deterministic playback.");
static_assert(std::is_trivially_copyable_v<GameplaySequenceComponent>,
    "GameplaySequenceComponent must remain POD for deterministic playback.");
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/GameplaySequencerSystem.h

새 파일:

```cpp
#pragma once

#include "ECS/Entity.h"
#include "Shared/GameSim/Components/GameplaySequenceComponent.h"
#include "WintersTypes.h"

class CWorld;
struct TickContext;

class CGameplaySequencerSystem final
{
public:
    static bool_t Start(
        CWorld& world,
        EntityID entityOwner,
        const TickContext& tc,
        const GameplaySequenceComponent& desc);

    static void Execute(CWorld& world, const TickContext& tc);

private:
    CGameplaySequencerSystem() = delete;
};
```

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/GameplaySequencerSystem.cpp

새 파일:

```cpp
#include "Shared/GameSim/Systems/GameplaySequencerSystem.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/NetAnimationComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Systems/DamagePipeline.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/ICommandExecutor.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue.h"
#include "Shared/GameSim/World.h"
#include "WintersMath.h"

#include <algorithm>

namespace
{
    bool_t IsZeroVector(const Vec3& v)
    {
        return v.x == 0.f && v.y == 0.f && v.z == 0.f;
    }

    eTeam ResolveTeam(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<ChampionComponent>(entity))
            return world.GetComponent<ChampionComponent>(entity).team;
        if (entity != NULL_ENTITY && world.HasComponent<MinionComponent>(entity))
            return world.GetComponent<MinionComponent>(entity).team;
        if (entity != NULL_ENTITY && world.HasComponent<StructureComponent>(entity))
            return world.GetComponent<StructureComponent>(entity).team;
        return eTeam::Neutral;
    }

    EntityID ResolveCueEntity(
        const GameplaySequenceComponent& sequence,
        const GameplaySequenceCue& cue)
    {
        switch (cue.eTarget)
        {
        case eGameplaySequenceTarget::Target:
            return sequence.entityTarget;
        case eGameplaySequenceTarget::Source:
            return sequence.entitySource;
        default:
            return NULL_ENTITY;
        }
    }

    Vec3 ResolveEntityPosition(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<TransformComponent>(entity))
            return world.GetComponent<TransformComponent>(entity).GetPosition();
        return {};
    }

    Vec3 ResolveCuePosition(
        CWorld& world,
        const GameplaySequenceComponent& sequence,
        const GameplaySequenceCue& cue)
    {
        if (!IsZeroVector(cue.vPosition) ||
            cue.eTarget == eGameplaySequenceTarget::WorldPosition)
        {
            return cue.vPosition;
        }

        const EntityID cueEntity = ResolveCueEntity(sequence, cue);
        if (cueEntity != NULL_ENTITY)
            return ResolveEntityPosition(world, cueEntity);

        return ResolveEntityPosition(world, sequence.entitySource);
    }

    Vec3 ResolveCueDirection(
        CWorld& world,
        const GameplaySequenceComponent& sequence,
        const GameplaySequenceCue& cue)
    {
        if (!IsZeroVector(cue.vDirection))
            return cue.vDirection;

        if (sequence.entitySource != NULL_ENTITY &&
            sequence.entityTarget != NULL_ENTITY &&
            world.HasComponent<TransformComponent>(sequence.entitySource) &&
            world.HasComponent<TransformComponent>(sequence.entityTarget))
        {
            const Vec3 sourcePos =
                world.GetComponent<TransformComponent>(sequence.entitySource).GetPosition();
            const Vec3 targetPos =
                world.GetComponent<TransformComponent>(sequence.entityTarget).GetPosition();
            return WintersMath::DirectionXZ(sourcePos, targetPos);
        }

        return {};
    }

    void StartNetAnimation(
        CWorld& world,
        EntityID entity,
        const TickContext& tc,
        const GameplaySequenceCue& cue)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity) || cue.uAnimId == 0)
            return;

        auto& anim = world.HasComponent<NetAnimationComponent>(entity)
            ? world.GetComponent<NetAnimationComponent>(entity)
            : world.AddComponent<NetAnimationComponent>(entity, NetAnimationComponent{});

        ++anim.actionSeq;
        anim.animId = cue.uAnimId;
        anim.animPhaseFrame = 0;
        anim.animStartTick = tc.tickIndex;
        anim.playbackRateQ8 = cue.uPlaybackRateQ8 != 0 ? cue.uPlaybackRateQ8 : 256;
        anim.flags = cue.uFlags;
        anim.priority = 0;
    }

    void EnqueueEffect(
        CWorld& world,
        const TickContext& tc,
        const GameplaySequenceComponent& sequence,
        const GameplaySequenceCue& cue)
    {
        if (cue.uEffectId == 0)
            return;

        ReplicatedEventComponent event{};
        event.kind = eReplicatedEventKind::EffectTrigger;
        event.sourceEntity = sequence.entitySource;
        event.targetEntity = ResolveCueEntity(sequence, cue);
        event.effectId = cue.uEffectId;
        event.attachBone = cue.uAttachBone;
        event.durationMs = cue.uDurationMs;
        event.flags = cue.uFlags;
        event.skillId = cue.uSkillId;
        event.position = ResolveCuePosition(world, sequence, cue);
        event.direction = ResolveCueDirection(world, sequence, cue);
        event.startTick = tc.tickIndex;
        EnqueueReplicatedEvent(world, event);
    }

    void EnqueueDamage(
        CWorld& world,
        const GameplaySequenceComponent& sequence,
        const GameplaySequenceCue& cue)
    {
        const EntityID target = ResolveCueEntity(sequence, cue);
        if (sequence.entitySource == NULL_ENTITY ||
            target == NULL_ENTITY ||
            cue.fDamageAmount <= 0.f)
        {
            return;
        }

        DamageRequest request{};
        request.source = sequence.entitySource;
        request.target = target;
        request.sourceTeam = ResolveTeam(world, sequence.entitySource);
        request.type = static_cast<eDamageType>(cue.uDamageType);
        request.flatAmount = cue.fDamageAmount;
        request.skillId = cue.uSkillId;
        request.rank = cue.uRank != 0 ? cue.uRank : 1;
        EnqueueDamageRequest(world, request);
    }

    void FireCue(
        CWorld& world,
        const TickContext& tc,
        const GameplaySequenceComponent& sequence,
        const GameplaySequenceCue& cue)
    {
        switch (cue.eKind)
        {
        case eGameplaySequenceCueKind::Animation:
            StartNetAnimation(world, ResolveCueEntity(sequence, cue), tc, cue);
            break;
        case eGameplaySequenceCueKind::Effect:
            EnqueueEffect(world, tc, sequence, cue);
            break;
        case eGameplaySequenceCueKind::Damage:
            EnqueueDamage(world, sequence, cue);
            break;
        default:
            break;
        }
    }
}

bool_t CGameplaySequencerSystem::Start(
    CWorld& world,
    EntityID entityOwner,
    const TickContext& tc,
    const GameplaySequenceComponent& desc)
{
    if (entityOwner == NULL_ENTITY || !world.IsAlive(entityOwner))
        return false;

    GameplaySequenceComponent sequence = desc;
    sequence.entitySource =
        sequence.entitySource != NULL_ENTITY ? sequence.entitySource : entityOwner;
    sequence.uStartTick = tc.tickIndex;
    sequence.uCueCount = std::min(sequence.uCueCount, kGameplaySequenceMaxCues);
    sequence.uFiredMask = 0;
    sequence.bActive = true;

    if (world.HasComponent<GameplaySequenceComponent>(entityOwner))
        world.GetComponent<GameplaySequenceComponent>(entityOwner) = sequence;
    else
        world.AddComponent<GameplaySequenceComponent>(entityOwner, sequence);

    return true;
}

void CGameplaySequencerSystem::Execute(CWorld& world, const TickContext& tc)
{
    const auto entities =
        DeterministicEntityIterator<GameplaySequenceComponent>::CollectSorted(world);

    for (EntityID entity : entities)
    {
        if (!world.IsAlive(entity) ||
            !world.HasComponent<GameplaySequenceComponent>(entity))
        {
            continue;
        }

        auto& sequence = world.GetComponent<GameplaySequenceComponent>(entity);
        if (!sequence.bActive)
        {
            world.RemoveComponent<GameplaySequenceComponent>(entity);
            continue;
        }

        const u64_t elapsedTicks =
            tc.tickIndex > sequence.uStartTick
                ? tc.tickIndex - sequence.uStartTick
                : 0;

        const u8_t cueCount =
            std::min(sequence.uCueCount, kGameplaySequenceMaxCues);
        for (u8_t i = 0; i < cueCount; ++i)
        {
            const u8_t bit = static_cast<u8_t>(1u << i);
            if ((sequence.uFiredMask & bit) != 0)
                continue;

            const GameplaySequenceCue& cue = sequence.cues[i];
            if (cue.eKind == eGameplaySequenceCueKind::None)
            {
                sequence.uFiredMask |= bit;
                continue;
            }

            if (elapsedTicks < cue.uTriggerTickOffset)
                continue;

            FireCue(world, tc, sequence, cue);
            sequence.uFiredMask |= bit;
        }

        const bool_t bAllCuesFired =
            cueCount == 0 ||
            sequence.uFiredMask == static_cast<u8_t>((1u << cueCount) - 1u);
        const bool_t bDurationFinished =
            sequence.uDurationTicks > 0 &&
            elapsedTicks >= sequence.uDurationTicks;

        if ((sequence.uDurationTicks == 0 && bAllCuesFired) ||
            bDurationFinished)
        {
            world.RemoveComponent<GameplaySequenceComponent>(entity);
        }
    }
}
```

1-4. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 include 블록에서 아래 코드 바로 아래에 추가:

기존 코드:

```cpp
#include "Shared/GameSim/Systems/GameplayStateQuery.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Systems/GameplaySequencerSystem.h"
```

`CGameRoom::Phase_SimulationSystems` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    CCombatActionSystem::Execute(m_world, tc);
```

아래에 추가:

```cpp
    CGameplaySequencerSystem::Execute(m_world, tc);
```

1-5. C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj

기존 코드:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Systems\CombatActionSystem.cpp" />
```

아래에 추가:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Systems\GameplaySequencerSystem.cpp" />
```

기존 코드:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Components\CombatActionComponent.h" />
```

아래에 추가:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Components\GameplaySequenceComponent.h" />
```

기존 코드:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Systems\GameplayStateQuery.h" />
```

아래에 추가:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Systems\GameplaySequencerSystem.h" />
```

1-6. C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj.filters

기존 코드:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Systems\CombatActionSystem.cpp">
      <Filter>04. Shared\GameSim\Systems</Filter>
    </ClCompile>
```

아래에 추가:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Systems\GameplaySequencerSystem.cpp">
      <Filter>04. Shared\GameSim\Systems</Filter>
    </ClCompile>
```

기존 코드:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Components\CombatActionComponent.h">
      <Filter>04. Shared\GameSim\Components</Filter>
    </ClInclude>
```

아래에 추가:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Components\GameplaySequenceComponent.h">
      <Filter>04. Shared\GameSim\Components</Filter>
    </ClInclude>
```

기존 코드:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Systems\GameplayStateQuery.h">
      <Filter>04. Shared\GameSim\Systems</Filter>
    </ClInclude>
```

아래에 추가:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Systems\GameplaySequencerSystem.h">
      <Filter>04. Shared\GameSim\Systems</Filter>
    </ClInclude>
```

2. 검증

검증 명령:

```text
git diff --check
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64
```

런타임 확인:

```text
1. 임시 boss AI 또는 debug hook에서 CGameplaySequencerSystem::Start를 호출해 Animation, Effect, Damage cue가 같은 sequenceId 안에서 순서대로 실행되는지 확인한다.
2. Animation cue는 서버 NetAnimationComponent.actionSeq 증가 -> GameRoom Phase_BroadcastEvents -> 클라 EventApplier::ApplyAnimationStart 한 번으로 이어져야 한다.
3. Effect cue는 ReplicatedEventComponent(EffectTrigger) -> CReplicatedEventSerializer::Build -> 클라 EventApplier::ApplyEffectTrigger 한 번으로 이어져야 한다.
4. Damage cue는 DamageRequestComponent -> CDamageQueueSystem -> ReplicatedEventComponent(Damage) -> 클라 UI damage number로 이어져야 한다.
5. normal LoL F5 flow에서 roster, map, minion, champion, snapshot, champion FX가 숨겨지거나 우회되지 않아야 한다.
```

미검증:

```text
- 실제 Elden boss arena scene과 boss AI trigger는 아직 없으므로 이번 세션에서는 sequencer runtime의 server-authoritative event path까지만 검증한다.
- Client local-only cinematic preview가 필요하면 같은 헤더를 Client.vcxproj에도 등록할지 별도 확인한다.
- JSON/.wseq asset loader, editor timeline, camera/audio/sub-sequence track은 후속 세션으로 분리한다.
```
