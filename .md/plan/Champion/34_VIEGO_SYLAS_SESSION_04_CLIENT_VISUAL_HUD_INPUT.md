Session - Snapshot, client visual, HUD, input이 훔친 폼/궁극기 상태를 표현하게 만든다.

1. 반영해야 하는 코드

이번 세션은 서버에서 결정된 오버라이드 상태를 클라이언트에 내리는 단계다. 성공 기준은 `championId`가 원본 권한 챔피언으로 남고, 비주얼 모델은 `visualChampionId`, Q/W/E/BA/HUD/입력은 slot override, 사일러스 R 입력은 훔친 궁의 targetMode를 사용하게 되는 것이다.

1-1. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Snapshot.fbs

기존 코드:

```fbs
    aiDebugTrace:[AIDebugTraceRow];
    lethalTempoStacks:ubyte;
}
```

아래로 교체:

```fbs
    aiDebugTrace:[AIDebugTraceRow];
    lethalTempoStacks:ubyte;

    baseChampionId:ubyte;
    visualChampionId:ubyte;
    skillChampionId:ubyte;
    skillSlotMask:ubyte;
    spellbookChampionId:ubyte;
    spellbookSlot:ubyte;
    spellbookRemaining:float;
}
```

1-2. C:/Users/tnest/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
```

기존 코드:

```cpp
        u16_t projectileKind = 0;
        u32_t projectileOwnerNet = 0;
        u32_t projectileTargetNet = 0;
        f32_t projectileSpeed = 0.f;
```

아래로 교체:

```cpp
        u16_t projectileKind = 0;
        u32_t projectileOwnerNet = 0;
        u32_t projectileTargetNet = 0;
        f32_t projectileSpeed = 0.f;
        u8_t baseChampionId = 0;
        u8_t visualChampionId = 0;
        u8_t skillChampionId = 0;
        u8_t skillSlotMask = 0;
        u8_t spellbookChampionId = 0;
        u8_t spellbookSlot = 0;
        f32_t spellbookRemaining = 0.f;
```

기존 코드:

```cpp
            championId = static_cast<u8_t>(champion.id);
            team = static_cast<u8_t>(champion.team);
            subtype = static_cast<u16_t>(champion.id);
```

아래로 교체:

```cpp
            championId = static_cast<u8_t>(champion.id);
            baseChampionId = championId;
            visualChampionId = championId;
            skillChampionId = championId;
            team = static_cast<u8_t>(champion.team);
            subtype = static_cast<u16_t>(champion.id);
```

기존 코드:

```cpp
        if (world.HasComponent<FormOverrideComponent>(entity))
        {
            const auto& form = world.GetComponent<FormOverrideComponent>(entity);
            if (form.bActive &&
                form.visualChampion != eChampion::END &&
                form.visualChampion != eChampion::NONE)
            {
                championId = static_cast<u8_t>(form.visualChampion);
                if (entityKind == Shared::Schema::EntityKind::Champion)
                    subtype = static_cast<u16_t>(form.visualChampion);
            }
        }
```

아래로 교체:

```cpp
        if (world.HasComponent<FormOverrideComponent>(entity))
        {
            const auto& form = world.GetComponent<FormOverrideComponent>(entity);
            if (form.bActive)
            {
                if (form.visualChampion != eChampion::END &&
                    form.visualChampion != eChampion::NONE)
                {
                    visualChampionId = static_cast<u8_t>(form.visualChampion);
                }
                if (form.skillChampion != eChampion::END &&
                    form.skillChampion != eChampion::NONE)
                {
                    skillChampionId = static_cast<u8_t>(form.skillChampion);
                    skillSlotMask = form.skillSlotMask;
                }
            }
        }

        if (world.HasComponent<SpellbookOverrideComponent>(entity))
        {
            const auto& spellbook = world.GetComponent<SpellbookOverrideComponent>(entity);
            if (spellbook.bActive &&
                spellbook.sourceChampion != eChampion::END &&
                spellbook.sourceChampion != eChampion::NONE)
            {
                spellbookChampionId = static_cast<u8_t>(spellbook.sourceChampion);
                spellbookSlot = spellbook.sourceSlot;
                spellbookRemaining = spellbook.fRemainingSec;
            }
        }
```

기존 코드:

```cpp
            aiDebugTraceOffset,
            lethalTempoStacks));
```

아래로 교체:

```cpp
            aiDebugTraceOffset,
            lethalTempoStacks,
            baseChampionId,
            visualChampionId,
            skillChampionId,
            skillSlotMask,
            spellbookChampionId,
            spellbookSlot,
            spellbookRemaining));
```

CONFIRM_NEEDED: `Shared/Schemas/run_codegen.bat` 실행 후 `Snapshot_generated.h`의 `CreateEntitySnapshot` 인자 순서가 위 schema append 순서와 일치하는지 확인한다.

1-3. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/ChampionComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
```

기존 코드:

```cpp
        const EntityID e = EnsureEntity(
            world,
            entityMap,
            es->netId(),
            static_cast<u8_t>(es->entityKind()),
            es->championId(),
            es->subtype(),
            Vec3{ es->posX(), es->posY(), es->posZ() },
            es->team());
```

아래로 교체:

```cpp
        const u8_t snapshotChampionId = es->championId();
        const u8_t snapshotVisualChampionId =
            es->visualChampionId() != 0u ? es->visualChampionId() : snapshotChampionId;
        const EntityID e = EnsureEntity(
            world,
            entityMap,
            es->netId(),
            static_cast<u8_t>(es->entityKind()),
            snapshotChampionId,
            es->subtype(),
            Vec3{ es->posX(), es->posY(), es->posZ() },
            es->team());
```

기존 코드:

```cpp
            auto& champ = world.GetComponent<ChampionComponent>(e);
            champ.id = static_cast<eChampion>(es->championId());
```

아래로 교체:

```cpp
            auto& champ = world.GetComponent<ChampionComponent>(e);
            champ.id = static_cast<eChampion>(snapshotChampionId);
```

기존 코드:

```cpp
            xp.current = es->xpCurrent();
            xp.requiredForNextLevel = es->xpRequired();
        }
```

아래로 교체:

```cpp
            xp.current = es->xpCurrent();
            xp.requiredForNextLevel = es->xpRequired();
        }

        if (kind == Shared::Schema::EntityKind::Champion)
        {
            const eChampion visualChampion =
                static_cast<eChampion>(snapshotVisualChampionId);
            const eChampion skillChampion =
                es->skillChampionId() != 0u
                    ? static_cast<eChampion>(es->skillChampionId())
                    : static_cast<eChampion>(snapshotChampionId);

            if (snapshotVisualChampionId != snapshotChampionId ||
                es->skillSlotMask() != 0u)
            {
                auto& form = world.HasComponent<FormOverrideComponent>(e)
                    ? world.GetComponent<FormOverrideComponent>(e)
                    : world.AddComponent<FormOverrideComponent>(e, FormOverrideComponent{});
                form.baseChampion = static_cast<eChampion>(snapshotChampionId);
                form.visualChampion = visualChampion;
                form.skillChampion = skillChampion;
                form.skillSlotMask = es->skillSlotMask();
                form.fRemainingSec = 1.f;
                form.bActive = true;
            }
            else if (world.HasComponent<FormOverrideComponent>(e))
            {
                world.RemoveComponent<FormOverrideComponent>(e);
            }

            if (es->spellbookChampionId() != 0u)
            {
                auto& spellbook = world.HasComponent<SpellbookOverrideComponent>(e)
                    ? world.GetComponent<SpellbookOverrideComponent>(e)
                    : world.AddComponent<SpellbookOverrideComponent>(e, SpellbookOverrideComponent{});
                spellbook.sourceChampion = static_cast<eChampion>(es->spellbookChampionId());
                spellbook.sourceSlot = es->spellbookSlot();
                spellbook.localSlot = static_cast<u8_t>(eSkillSlot::R);
                spellbook.fRemainingSec = es->spellbookRemaining();
                spellbook.bActive = true;
            }
            else if (world.HasComponent<SpellbookOverrideComponent>(e))
            {
                world.RemoveComponent<SpellbookOverrideComponent>(e);
            }
        }
```

기존 코드:

```cpp
            soul.champion = static_cast<eChampion>(es->subtype());
```

아래로 교체:

```cpp
            soul.champion = static_cast<eChampion>(
                snapshotVisualChampionId != 0u ? snapshotVisualChampionId : es->subtype());
```

CONFIRM_NEEDED: `EnsureEntity` 내부는 아직 champion mismatch 때 visual callback을 `championId` 기준으로 호출한다. 구현 시에는 `EnsureEntity`가 원본 championId만 다루고, visual 변경 callback은 `OnSnapshot`에서 `snapshotVisualChampionId` 변경 감지로 별도 호출하도록 분리한다.

1-4. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/NetAnimationComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/NetAnimationComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
```

기존 코드:

```cpp
    const auto& champion = world.GetComponent<ChampionComponent>(entity);
    const ChampionDef* cd = FindClientChampionDefForEvent(champion.id);
```

아래로 교체:

```cpp
    const auto& champion = world.GetComponent<ChampionComponent>(entity);
    eChampion animationChampion = champion.id;
    const u8_t animSlot = SlotFromNetAnim(animId);
    if (world.HasComponent<FormOverrideComponent>(entity))
    {
        const auto& form = world.GetComponent<FormOverrideComponent>(entity);
        if (form.bActive &&
            form.skillChampion != eChampion::END &&
            form.skillChampion != eChampion::NONE &&
            animSlot < 8u &&
            (form.skillSlotMask & static_cast<u8_t>(1u << animSlot)) != 0u)
        {
            animationChampion = form.skillChampion;
        }
        else if (form.bActive &&
            form.visualChampion != eChampion::END &&
            form.visualChampion != eChampion::NONE &&
            animId != static_cast<u16_t>(eNetAnimId::SkillR))
        {
            animationChampion = form.visualChampion;
        }
    }
    const ChampionDef* cd = FindClientChampionDefForEvent(animationChampion);
```

기존 코드:

```cpp
        const u8_t slot = SlotFromNetAnim(animId);
        const SkillDef* def = CSkillRegistry::Instance().Find(champion.id, slot);
        if (!def)
            def = FindSkillDef(champion.id, slot);
```

아래로 교체:

```cpp
        const u8_t slot = SlotFromNetAnim(animId);
        const SkillDef* def = CSkillRegistry::Instance().Find(animationChampion, slot);
        if (!def)
            def = FindSkillDef(animationChampion, slot);
```

기존 코드:

```cpp
        if (champion.id == eChampion::YASUO && id == eNetAnimId::SkillQ)
```

아래로 교체:

```cpp
        if (animationChampion == eChampion::YASUO && id == eNetAnimId::SkillQ)
```

1-5. C:/Users/tnest/Desktop/Winters/Engine/Public/Manager/UI/ChampionHUDState.h

기존 코드:

```cpp
        eChampion Champion = eChampion::IRELIA;
```

아래로 교체:

```cpp
        eChampion Champion = eChampion::IRELIA;
        std::array<eChampion, 4> SkillIconChampions{
            eChampion::IRELIA,
            eChampion::IRELIA,
            eChampion::IRELIA,
            eChampion::IRELIA
        };
```

1-6. C:/Users/tnest/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

기존 코드:

```cpp
            State.Champion = Champion.id;
            State.LocalEntity = Entity;
```

아래로 교체:

```cpp
            State.Champion = Champion.id;
            State.SkillIconChampions.fill(Champion.id);
            if (m_pWorld->HasComponent<FormOverrideComponent>(Entity))
            {
                const auto& form = m_pWorld->GetComponent<FormOverrideComponent>(Entity);
                if (form.bActive &&
                    form.skillChampion != eChampion::END &&
                    form.skillChampion != eChampion::NONE)
                {
                    for (u32_t Index = 0; Index < State.SkillIconChampions.size(); ++Index)
                    {
                        const u8_t Slot = static_cast<u8_t>(Index + 1u);
                        if ((form.skillSlotMask & static_cast<u8_t>(1u << Slot)) != 0u)
                            State.SkillIconChampions[Index] = form.skillChampion;
                    }
                }
            }
            if (m_pWorld->HasComponent<SpellbookOverrideComponent>(Entity))
            {
                const auto& spellbook = m_pWorld->GetComponent<SpellbookOverrideComponent>(Entity);
                if (spellbook.bActive &&
                    spellbook.localSlot == static_cast<u8_t>(eSkillSlot::R) &&
                    spellbook.sourceChampion != eChampion::END &&
                    spellbook.sourceChampion != eChampion::NONE)
                {
                    State.SkillIconChampions[3] = spellbook.sourceChampion;
                }
            }
            State.LocalEntity = Entity;
```

CONFIRM_NEEDED: `UI_Manager.cpp`에 `FormOverrideComponent.h`와 `SpellbookOverrideComponent.h` include를 추가한다. 또한 현재 `LoadChampionHUDAssetsForChampion(State.Champion)` 구조는 슬롯별 SRV 캐시를 지원하지 않으므로, 구현 시 `LoadChampionHUDSkillIconForChampionSlot(eChampion, u32_t)` helper를 추가하고 `m_pSRV_ChampionSkillIcons[Index]`를 `State.SkillIconChampions[Index]` 기준으로 채운다.

1-7. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/InGameSkillDispatchBridge.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/SkillRankComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
```

기존 코드:

```cpp
    using namespace Engine;
    const eChampion champ = scene.GetPlayerChampionId();
    const SkillDef* def = CSkillRegistry::Instance().Find(champ, slot);
```

아래로 교체:

```cpp
    using namespace Engine;
    eChampion champ = scene.GetPlayerChampionId();
    u8_t lookupSlot = slot;
    if (scene.m_World.HasComponent<SpellbookOverrideComponent>(scene.m_PlayerEntity))
    {
        const auto& spellbook = scene.m_World.GetComponent<SpellbookOverrideComponent>(scene.m_PlayerEntity);
        if (spellbook.bActive && spellbook.localSlot == slot)
        {
            champ = spellbook.sourceChampion;
            lookupSlot = spellbook.sourceSlot;
        }
    }
    else if (scene.m_World.HasComponent<FormOverrideComponent>(scene.m_PlayerEntity))
    {
        const auto& form = scene.m_World.GetComponent<FormOverrideComponent>(scene.m_PlayerEntity);
        if (form.bActive &&
            form.skillChampion != eChampion::END &&
            form.skillChampion != eChampion::NONE &&
            slot < 8u &&
            (form.skillSlotMask & static_cast<u8_t>(1u << slot)) != 0u)
        {
            champ = form.skillChampion;
        }
    }
    const SkillDef* def = CSkillRegistry::Instance().Find(champ, lookupSlot);
```

기존 코드:

```cpp
        cmd.slot = slot;
```

아래로 교체:

```cpp
        cmd.slot = slot;
```

CONFIRM_NEEDED: `BuildCastCommand`는 local slot을 그대로 보내야 하므로 `cmd.slot`은 바꾸지 않는다. 다만 lookup은 훔친 챔피언/훔친 슬롯으로 해야 한다.

2. 검증

`Shared/Schemas/run_codegen.bat`를 실행하고 `Shared/Schemas/Generated/cpp/Snapshot_generated.h`, `Shared/Schemas/Generated/go/Shared/Schema/EntitySnapshot.go`가 갱신되는지 확인한다.

비에고 빙의 중 서버 snapshot에서 `championId == VIEGO`, `visualChampionId == stolen`, `skillChampionId == stolen`, `skillSlotMask`가 BA/Q/W/E만 포함하는지 확인한다.

사일러스가 궁을 훔친 뒤 snapshot에서 `spellbookChampionId == targetChampion`, `spellbookSlot == R`인지 확인한다.

클라이언트에서 비에고 모델이 훔친 챔피언으로 바뀌지만 `ChampionComponent.id`는 VIEGO로 유지되는지 확인한다.

HUD에서 비에고 Q/W/E 아이콘은 훔친 챔피언, R 아이콘은 비에고로 표시되는지 확인한다. 사일러스는 훔친 궁이 있을 때 R 아이콘만 대상 궁으로 표시되어야 한다.

Engine public header를 수정하므로 구현 후 `UpdateLib.bat`를 실행해 `EngineSDK/inc` 동기화를 확인한다.

`git diff --check`를 실행한다.

Server와 Client를 빌드한다.
