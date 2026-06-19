Session - InGame/Champion 구조를 원자 Fact에서 최상위 도메인까지 끌어올려 본질만 남기는 최종 리팩터링을 확정한다.

1. 반영해야 하는 코드

본질 사다리:

```text
0. 원자 Fact
   숫자 id, tick, sequence, stage처럼 더 나누면 의미가 사라지는 값.

1. 의도
   Player/AI가 무엇을 하려는지 담은 command. 아직 결과가 아니다.

2. 규칙
   서버가 의도를 받아들일지 판단하는 gameplay data. 보여주는 값이 아니다.

3. 결과
   GameSim이 만든 authoritative state. HP, 위치, cooldown, pose, action.

4. 복제
   결과 중 Client가 알아야 하는 최소 fact. Snapshot/Event.

5. 표현
   Client가 fact를 model, animation, FX, UI로 보여주는 방법.

6. 도메인
   Engine, Shared/GameSim, Server, Client, Data/Tools의 소유권과 의존 방향.
```

본질 검증 질문:
- 이 값이 없으면 gameplay truth가 사라지는가?
- 이 값이 없어도 Client가 다른 visual source로 보여줄 수 있는가?
- 이 값이 Server 판단에 필요한가, Client 표현에 필요한가?
- 이 값이 같은 owner 안에서만 쓰이는가, 다른 owner까지 새고 있는가?
- 이름이 실제 소유권을 말하는가, 아니면 legacy를 본질처럼 숨기는가?

최종 원자:
- `SkillTypes`: skill slot, target mode, target resolve policy.
- `SkillCommand`: client/AI 의도 payload.
- `ChampionGameData`: champion gameplay rule.
- `SummonerSpellGameData`: champion이 아닌 summoner spell gameplay rule.
- `ReplicatedPose`: 지속되는 몸 상태. `poseId`, `startTick`.
- `ReplicatedAction`: 시작된 행위. `actionId`, `startTick`, `sequence`, `stage`.
- `ChampionVisualData`: pose/action을 어떻게 보여줄지 정하는 Client visual source.

도메인까지 끌어올린 최종 의존:

```text
Data/Gameplay -> Shared/GameSim generated data
Data/Client   -> Client visual generated data

Client Input/AI Intent -> SkillCommand
SkillCommand -> Server GameSim
Server GameSim -> ChampionGameData/SummonerSpellGameData
Server GameSim -> ReplicatedPose/ReplicatedAction
ReplicatedPose/ReplicatedAction -> Snapshot/Event
Snapshot/Event -> Client mirror
Client mirror + ChampionVisualData -> animation/FX/UI

Engine <- Client
Shared/GameSim <- Server
Shared/GameSim <- Client
Client -> Engine
Server -> Shared/GameSim
Shared/GameSim -> no Client/Renderer/UI/DX/ImGui
Server -> no Client visual data
```

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

검증 질문:
- pose는 event인가? 아니다. 지속 상태다.
- 같은 pose 재시작을 구분해야 하는가? 필요하면 `startTick`이 바뀐다.
- 그래서 `sequence`는 본질인가? 아니다. 삭제한다.

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

검증 질문:
- `actionId`가 없으면 어떤 행위인지 모른다. 본질이다.
- `startTick`이 없으면 언제 시작했는지 모른다. 본질이다.
- `sequence`가 없으면 같은 tick/같은 action 재시작 dedup이 깨질 수 있다. 본질이다.
- `stage`가 없으면 Yone/Yasuo/Jax처럼 같은 skill 안의 gameplay variant를 구분하기 어렵다. 본질이다.
- animation speed, loop, key, hook id가 없는가? 없다. 그래서 action은 표현이 아니라 fact다.

1-3. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Snapshot.fbs

`table EntitySnapshot`에서 아래 기존 코드 교체:

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

`table EntitySnapshot`에서 아래 기존 코드 교체:

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

검증 질문:
- Snapshot은 현재 상태인가? 그렇다.
- animation clip id는 현재 상태인가? 아니다. Client 표현 선택이다.
- playback speed는 현재 상태인가? 아니다. Client visual source다.
- `poseSeq`가 필요한가? 아니다. pose는 `poseId/startTick`으로 충분하다.

1-4. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Event.fbs

`enum EventKind`에서 아래 기존 코드 교체:

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

아래 기존 table 전체 교체:

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

`table EventPacket`에서 아래 기존 코드 교체:

기존 코드:

```fbs
    skillCast:SkillCastEvent;
    animation:AnimationStartEvent;
    effect:EffectTriggerEvent;
```

아래로 교체:

```fbs
    skillCast:SkillCastEvent;
    actionStart:ActionStartEvent;
    effect:EffectTriggerEvent;
```

검증 질문:
- Event는 "무엇이 시작됐다"만 말해야 하는가? 그렇다.
- animation name/speed/loop를 Event가 가져야 하는가? 아니다.
- `ActionStartEvent`를 추가만 하고 `AnimationStartEvent`를 남겨도 되는가? 아니다. 중복 event는 본질이 아니다.

1-5. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Generated/cpp/Snapshot_generated.h

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

생성 후 아래 accessor가 남아 있으면 실패다.

```cpp
uint16_t animId() const;
uint16_t animPhaseFrame() const;
uint64_t animStartTick() const;
uint16_t animPlaybackRateQ8() const;
uint16_t animFlags() const;
```

1-6. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Generated/cpp/Event_generated.h

CONFIRM_NEEDED:
- 직접 편집하지 않는다.
- `Shared/Schemas/run_codegen.bat`로만 갱신한다.
- 생성 후 아래 타입과 accessor가 있어야 한다.

```cpp
struct ActionStartEvent;
Shared::Schema::EventKind::ActionStart;
const Shared::Schema::ActionStartEvent* actionStart() const;
```

생성 후 아래 타입과 accessor가 남아 있으면 실패다.

```cpp
struct AnimationStartEvent;
Shared::Schema::EventKind::AnimationStart;
const Shared::Schema::AnimationStartEvent* animation() const;
```

1-7. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/ChampionGameData.h

include 영역에서 아래 기존 코드 교체:

기존 코드:

```cpp
#include "Shared/GameSim/Definitions/SkillDef.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Definitions/SkillTypes.h"
```

아래 기존 코드 교체:

기존 코드:

```cpp
struct ChampionGameDataSkillStage
{
    f32_t lockDurationSec = 0.6f;
    f32_t animPlaySpeed = 1.f;
    f32_t castFrame = 0.f;
    f32_t recoveryFrame = 0.f;
};
```

아래로 교체:

```cpp
struct ChampionGameDataSkillStage
{
    f32_t lockDurationSec = 0.6f;
};
```

아래 기존 코드 교체:

기존 코드:

```cpp
struct ChampionGameDataSkill
{
    bool_t bValid = false;
    u8_t slot = 0;
    eTargetMode targetMode = eTargetMode::Self;
    u8_t stageCount = 1;
    f32_t stageWindowSec = 0.f;
    f32_t cooldownSec = 0.f;
    f32_t rangeMax = 0.f;
    f32_t manaCost = 0.f;
    u16_t skillId = 0;
    u16_t scalingTableId = 0;
    u32_t gameplayPolicyId = 0;
    u32_t visualCueId = 0;
    ChampionGameDataSkillStage stages[kChampionGameDataSkillStageMax] = {};
};
```

아래로 교체:

```cpp
struct ChampionGameDataSkill
{
    bool_t bValid = false;
    u8_t slot = 0;
    eTargetMode targetMode = eTargetMode::Self;
    eTargetResolvePolicy targetPolicy = eTargetResolvePolicy::Direct;
    u8_t stageCount = 1;
    f32_t stageWindowSec = 0.f;
    f32_t cooldownSec = 0.f;
    f32_t rangeMax = 0.f;
    f32_t manaCost = 0.f;
    u16_t skillId = 0;
    u16_t scalingTableId = 0;
    u32_t gameplayPolicyId = 0;
    ChampionGameDataSkillStage stages[kChampionGameDataSkillStageMax] = {};
};
```

아래 기존 struct 전체 삭제:

삭제할 범위:
`struct ChampionGameDataSummonerSpell` 줄부터
해당 struct의 닫는 `};` 줄까지 삭제.

아래 기존 코드 교체:

기존 코드:

```cpp
struct ChampionGameData
{
    bool_t bValid = false;
    eChampion champion = eChampion::END;
    u32_t dataVersion = 1;
    u32_t authoringHash = 0;
    ChampionStatsDef stats{};
    f32_t visualYawOffset = 0.f;
    ChampionGameDataSkill skills[kChampionGameDataSkillSlotCount] = {};
    ChampionGameDataSummonerSpell summonerSpells[2] = {};
    ChampionGameDataPassiveDash passiveDash{};
};
```

아래로 교체:

```cpp
struct ChampionGameData
{
    bool_t bValid = false;
    eChampion champion = eChampion::END;
    ChampionStatsDef stats{};
    ChampionGameDataSkill skills[kChampionGameDataSkillSlotCount] = {};
    ChampionGameDataPassiveDash passiveDash{};
};
```

검증 질문:
- `animPlaySpeed`, `castFrame`, `recoveryFrame`은 서버 판정에 필요한가? 아니다. visual source다.
- `visualCueId`는 gameplay rule인가? 아니다. visual/effect source다.
- `visualYawOffset`은 world yaw인가 model 보정인가? model 보정이다. Client visual source다.
- `summonerSpells`는 champion 본질인가? 아니다. loadout/spell rule이다.
- `dataVersion`, `authoringHash`는 runtime gameplay rule인가? 아니다. Data/Tools metadata다.

1-8. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/SummonerSpellGameData.h

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"

inline constexpr u8_t kSummonerSpellGameDataSlotCount = 2;

struct SummonerSpellGameData
{
    bool_t bValid = false;
    u16_t spellId = 0;
    f32_t rangeMax = 0.f;
    f32_t cooldownSec = 0.f;
    u32_t gameplayPolicyId = 0;
};
```

검증 질문:
- spell은 champion에 종속되는가? 아니다. loadout이 선택한다.
- spell에도 visual cue가 필요한가? 표현에는 필요할 수 있다. 하지만 gameplay data에는 두지 않는다.

1-9. C:/Users/tnest/Desktop/Winters/Client/Public/GameObject/ChampionVisualData.h

기존 구조는 최종 owner 방향과 맞다.

확인할 본질:

```cpp
struct ChampionPoseVisualData
{
    bool_t bValid = false;
    u16_t poseId = 0;
    const char* animationKey = nullptr;
    f32_t playbackSpeed = 1.f;
    bool_t bLoop = true;
};

struct ChampionActionVisualStageData
{
    u8_t stage = 1;
    const char* animationKey = nullptr;
    f32_t playbackSpeed = 1.f;
    bool_t bLoop = false;
    u8_t eventCount = 0;
    ChampionActionVisualEventData events[kChampionVisualActionEventMax] = {};
};
```

검증 질문:
- animation key는 Client 표현인가? 그렇다.
- playback speed는 Client 표현인가? 그렇다.
- hook frame은 gameplay 판정인가? 아니다. visual event marker다.
- 이 header를 Shared/GameSim이나 Server가 include하면 실패다.

1-10. C:/Users/tnest/Desktop/Winters/Tools/ChampionData/build_champion_game_data.py

아래 기존 코드 교체:

기존 코드:

```python
STAGE_FIELDS = {
    "lockDurationSec": 0.6,
    "animPlaySpeed": 1.0,
    "castFrame": 0.0,
    "recoveryFrame": 0.0,
}
```

아래로 교체:

```python
STAGE_FIELDS = {
    "lockDurationSec": 0.6,
}
```

`normalize_skill` 반환 dict에서 아래 기존 코드 삭제:

삭제할 코드:

```python
        "visualCueId": as_int(skill.get("visualCueId", 0), f"{path}.visualCueId"),
```

아래 기존 함수 전체 삭제:

삭제할 범위:
`def normalize_spell(spell: dict, index: int) -> dict:` 줄부터
해당 함수의 마지막 `}` 반환 block 끝까지 삭제.

`emit_cpp` 안에서 아래 기존 코드 삭제:

삭제할 코드:

```python
        lines.append(f"        data.dataVersion = {champion['dataVersion']}u;")
        lines.append("        data.authoringHash = kGeneratedChampionGameDataBuildHash;")
        lines.append(f"        data.visualYawOffset = {cpp_float(champion['visualYawOffset'])};")
```

`append_skill` 안에서 아래 기존 코드 삭제:

삭제할 코드:

```python
    lines.append(f"        skill{slot}.visualCueId = {skill['visualCueId']}u;")
```

`append_skill` 안에서 아래 기존 코드 삭제:

삭제할 코드:

```python
        lines.append(f"        stage{slot}_{stage_index}.animPlaySpeed = {cpp_float(stage['animPlaySpeed'])};")
        lines.append(f"        stage{slot}_{stage_index}.castFrame = {cpp_float(stage['castFrame'])};")
        lines.append(f"        stage{slot}_{stage_index}.recoveryFrame = {cpp_float(stage['recoveryFrame'])};")
```

아래 기존 함수 전체 삭제:

삭제할 범위:
`def append_spell(lines: list[str], spell: dict, index: int) -> None:` 줄부터
해당 함수의 마지막 `lines.append(...)` 줄까지 삭제.

`emit_cpp` 안에서 아래 기존 코드 삭제:

삭제할 코드:

```python
        for index, spell in enumerate(data["summonerSpells"]):
            append_spell(lines, spell, index)
```

검증 질문:
- generator가 visual field를 다시 만들면 코드에서 제거해도 다시 섞인다.
- generator는 gameplay data만 만든다.
- visual data generator는 별도 Client/Data pipeline에서 만든다.

1-11. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/NetAnimationMigrationBridge.h

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

검증 질문:
- 왜 이 파일이 남는가? legacy writer가 아직 존재하기 때문이다.
- 이것이 본질인가? 아니다. 이름에 `Migration`을 넣어 삭제 대상임을 드러낸다.
- 최종 gate에서 이 파일도 삭제한다.

1-12. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ReplicatedAnimationBridge.h

삭제 범위:
파일 전체 삭제.

검증 질문:
- `ReplicatedAnimationBridge`라는 이름은 최종 구조처럼 들린다.
- 실제 역할은 legacy animation을 pose/action으로 번역하는 것이다.
- 이름이 소유권을 흐리면 본질이 아니다.

1-13. 아래 파일들의 include와 호출명 교체

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

검증 질문:
- legacy writer가 아직 있으면 migration 이름 아래에만 있어야 한다.
- migration 이름이 아닌 곳에 legacy 변환이 숨어 있으면 실패다.

1-14. C:/Users/tnest/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

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

CONFIRM_NEEDED:
- `Shared/Schemas/run_codegen.bat` 실행 후 `CreateEntitySnapshot`의 정확한 parameter 순서를 확인한다.
- 기존 animation 인자 대신 `poseId`, `poseStartTick`, `actionId`, `actionStartTick`, `actionSeq`, `actionStage`를 전달한다.

검증 질문:
- SnapshotBuilder가 animation을 읽으면 실패다.
- SnapshotBuilder는 simulation fact만 wire로 포장한다.

1-15. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.h

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

검증 질문:
- serializer가 animation component를 알면 실패다.
- serializer는 action fact만 event packet으로 바꾼다.

1-16. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.cpp

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

검증 질문:
- `BuildAnimationStart`가 남아 있으면 실패다.
- `BuildActionStart`가 animation speed나 flags를 받으면 실패다.

1-17. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomReplication.cpp

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

검증 질문:
- Server가 animation event를 broadcast하면 실패다.
- Server는 action 시작 fact만 broadcast한다.

1-18. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

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

검증 질문:
- Client mirror가 `NetAnimationComponent`를 만들면 실패다.
- Client mirror는 서버 fact를 그대로 저장한다.
- visual 선택은 별도 visual path가 한다.

1-19. C:/Users/tnest/Desktop/Winters/Client/Public/Network/Client/EventApplier.h

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
    std::unordered_map<NetEntityId, u32_t> m_lastAnimationSeq;
```

아래로 교체:

```cpp
    std::unordered_map<NetEntityId, u32_t> m_lastActionSeq;
```

검증 질문:
- Client event applier가 animation event type을 알면 실패다.
- dedup 대상은 animation이 아니라 action sequence다.

1-20. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

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

CONFIRM_NEEDED:
- `PlayNetworkAnimation` 함수는 `PlayReplicatedActionVisual`로 교체한다.
- `PlayReplicatedActionVisual`은 action id/stage와 Client visual source만으로 animation을 고른다.
- Server에서 온 playback speed/flags를 읽으면 실패다.

1-21. C:/Users/tnest/Desktop/Winters/Client/Public/Network/Client/NetworkEventTrace.h

`eTraceKind` enum에서 아래 기존 코드 교체:

기존 코드:

```cpp
        AnimationStart,
```

아래로 교체:

```cpp
        ActionStart,
```

1-22. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/NetworkEventTrace.cpp

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

검증 질문:
- debug UI 이름도 animation이면 사고가 그쪽으로 되돌아간다.
- trace도 action fact를 보여줘야 한다.

1-23. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/SkillDef.h

삭제 범위:
파일 전체 삭제.

CONFIRM_NEEDED:
- 아래 include/use가 모두 제거된 뒤 삭제한다.

```text
rg -n "SkillDef.h|SkillDef|FindSkillDef|g_SkillTable|g_SkillCount" Shared Server Client Tools
```

검증 질문:
- `SkillDef`는 command, gameplay data, visual data를 한 struct에 섞는다.
- 섞인 struct는 원자가 아니다.
- slot/target은 `SkillTypes`, command payload는 `SkillCommand`, server rule은 `ChampionGameData`, visual은 `ChampionVisualData`로 나뉜다.

1-24. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/ChampionDef.h

삭제 범위:
파일 전체 삭제.

CONFIRM_NEEDED:
- 아래 include/use가 모두 제거된 뒤 삭제한다.

```text
rg -n "ChampionDef.h|ChampionDef|FindChampionDef|g_ChampionTable|GetChampionDisplayName" Shared Server Client Tools
```

검증 질문:
- champion display/model/spawn/gameplay가 한 struct에 있으면 원자가 아니다.
- gameplay는 `ChampionGameData`, visual은 `ChampionVisualData`, spawn/loadout은 Server config나 explicit smoke config다.

1-25. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/SkillTable.cpp

삭제 범위:
파일 전체 삭제.

CONFIRM_NEEDED:
- Client visual resolver가 `ChampionVisualData`를 사용하도록 전환된 뒤 삭제한다.
- 아래 검색이 0이어야 한다.

```text
rg -n "SkillTable|RegisterSkill|CSkillRegistry|FindSkillDef" Client Shared Server Tools
```

검증 질문:
- gameplay field와 animation key/frame/hook/playback speed가 한 table에 있으면 원자가 아니다.

1-26. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/ChampionTable.cpp

삭제 범위:
파일 전체 삭제.

CONFIRM_NEEDED:
- model/display/texture lookup이 `ChampionVisualData`로 이동한 뒤 삭제한다.
- spawn position이나 local smoke config는 별도 config로 이동한다.

검증 질문:
- display/model/spawn/gameplay가 한 table에 있으면 원자가 아니다.

2. 검증

자동 검증:
- `git diff --check`
- `python Tools/ChampionData/build_champion_game_data.py`
- `Shared/Schemas/run_codegen.bat`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`
- `Tools/Bin/Debug/SimLab.exe 300 42`
- `Tools/Bin/Debug/SimLab.exe`

원자 검증:
- `rg -n "poseSeq|animPhaseFrame|animPlaybackRateQ8|animFlags|AnimationStart|AnimationStartEvent" Shared/Schemas Shared/GameSim Server Client`
- 결과가 남으면 실패다.

gameplay data 검증:
- `rg -n "visualYawOffset|visualCueId|animPlaySpeed|castFrame|recoveryFrame|summonerSpells|ChampionGameDataSummonerSpell" Shared/GameSim Data/Gameplay Tools/ChampionData`
- 최종 gate에서 결과가 남으면 실패다.

visual data 검증:
- `rg -n "ChampionVisualData" Shared/GameSim Server`
- 결과가 있으면 실패다.
- `rg -n "ChampionVisualData|ChampionPoseVisualData|ChampionActionVisualData" Client Data Tools`
- Client/Data/Tools 쪽에만 있어야 한다.

legacy 검증:
- `rg -n "NetAnimationComponent|eNetAnimId|ReplicatedAnimationBridge|NetAnimationMigrationBridge" Shared Server Client Tools`
- 구현 중간에는 `NetAnimationMigrationBridge`와 아직 전환하지 않은 writer에만 남을 수 있다.
- 최종 gate에서는 0이어야 한다.

도메인 검증:
- `rg -n "ID3D11|Renderer|ImGui|ChampionVisualData|Client/" Shared/GameSim`
- 결과가 있으면 Shared/GameSim 의존성 실패다.
- `rg -n "ChampionVisualData|animationKey|playbackSpeed|modelYawOffset|fbxPath|texturePath" Server Shared/GameSim`
- 결과가 있으면 Server/Shared가 visual을 고르는 실패다.

wire 검증:
- Snapshot에는 `poseId`, `poseStartTick`, `actionId`, `actionStartTick`, `actionSeq`, `actionStage`만 있어야 한다.
- Event에는 `ActionStartEvent`만 있어야 한다.
- wire에 animation name, animation id, playback rate, loop flag, visual hook id가 있으면 실패다.

회귀 검증:
- normal F5에서 player champion Idle/Run 확인.
- lane minion Idle/Run/Attack/Death 확인.
- jungle monster Idle/Attack/Death 확인.
- BasicAttack, SkillQ/W/E/R, Recall, DeathStart, ViegoConsumeSoul 확인.
- Jax E loop와 release 확인.
- Yasuo Q stage 확인.
- skill animation speed가 기존과 달라지지 않는지 확인.
- cast/recovery visual hook이 FX timing만 바꾸고 hit/cooldown/action lock을 바꾸지 않는지 확인.
- Network Event Trace에 `ActionStart`가 기록되고 `AnimationStart`가 보이지 않는지 확인.

최종 실패 조건:
- Server 또는 Shared/GameSim이 animation key, playback speed, loop, hook frame, model path, texture path를 고르면 실패다.
- Client가 cooldown, damage, hit, action lock 같은 authoritative gameplay truth를 새로 만들면 실패다.
- Data/Gameplay에 visual field가 남으면 실패다.
- Data/Client에 cooldown, damage, range, mana cost, server validation policy가 들어가면 실패다.
- migration/legacy 이름이 아닌 파일에 legacy 변환 로직이 숨어 있으면 실패다.
- 삭제 대상인 `SkillDef`, `ChampionDef`, `SkillTable`, `ChampionTable`, `NetAnimationComponent`가 compatibility layer 밖에서 계속 owner처럼 쓰이면 실패다.
