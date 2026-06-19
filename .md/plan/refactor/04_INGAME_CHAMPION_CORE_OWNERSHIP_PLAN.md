Session - 기존 InGame/Champion 혼합 구조를 보존하지 않고 SkillTypes, SkillCommand, ChampionGameData, SummonerSpellGameData, ReplicatedAction, ChampionVisualSource에서도 파생 필드를 제거한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/.md/architecture/WINTERS_CODEBASE_COMPASS.md

기존 코드:

```md
### Shared / GameSim

역할:
- 서버 권위 gameplay truth의 데이터와 deterministic simulation contract
- `GameCommand`, Snapshot/Event schema, gameplay component, champion data, server-side skill validation

의존 규칙:
- `Shared/GameSim`은 `Engine`, `Client`, `Renderer`, `UI`, `ImGui`, `DX11`을 include하지 않는다.
- gameplay 결과는 Shared/Server에서 만들어지고 Client는 presentation으로 소비한다.
```

아래로 교체:

```md
### Shared / GameSim

역할:
- 서버 권위 gameplay truth와 deterministic simulation contract
- gameplay enum/type, input command, gameplay data, replicated pose/action state, Snapshot/Event schema
- champion gameplay validation과 simulation. 단, model/animation/FX/display/resource path는 소유하지 않는다.

의존 규칙:
- `Shared/GameSim`은 `Engine`, `Client`, `Renderer`, `UI`, `ImGui`, `DX11`을 include하지 않는다.
- Shared gameplay data에는 판정 가능한 값만 둔다: stat, range, cooldown, mana cost, skill id, scaling id, gameplay policy id, action lock, dash distance/duration/grace.
- Shared gameplay data에는 표현 값을 두지 않는다: model/texture/shader path, animation key, cast/recovery animation frame, animation playback speed, visual cue id/name, model yaw offset, display name.
- champion gameplay data에는 champion 사실만 둔다. summoner spell은 champion 소유가 아니므로 별도 gameplay data로 분리한다.
- replicated state도 더 나눈다. Idle/Run/Dead 같은 지속 몸 상태는 pose state, BasicAttack/Skill/Recall/DeathStart 같은 시작 시점이 있는 행위는 action state다.
- Shared yaw는 gameplay/world yaw다. model yaw offset 보정은 Client render path에서만 한다.
- 서버 Snapshot/Event는 visual cue를 직접 고르지 않는다. champion id, slot, stage, action id, semantic gameplay/effect id처럼 Client가 해석할 수 있는 gameplay fact만 보낸다.
- gameplay 결과는 Shared/Server에서 만들어지고 Client는 presentation으로 소비한다.
```

기존 코드:

```md
### Client

역할:
- LoL/Elden 같은 제품별 scene, input, camera, presentation bridge, UI state build, weak prediction, interpolation, animation/FX playback
- 서버 Snapshot/Event를 visual state로 적용

의존 규칙:
- Client는 Engine/EngineSDK와 Shared schema/component를 읽을 수 있다.
- Client가 authoritative gameplay truth를 새로 만들면 안 된다. 예외는 명시된 local-only smoke path뿐이다.
- Client/Public에 새 `ID3D11*` 의존을 넓히지 않는다. DX11 concrete handle은 Engine backend 또는 좁은 bridge/adapter에 가둔다.
```

아래로 교체:

```md
### Client

역할:
- LoL/Elden 같은 제품별 scene, input, camera, presentation bridge, UI state build, weak prediction, interpolation, animation/FX playback
- 서버 Snapshot/Event를 visual state로 적용
- champion visual source 소유: model/texture/shader path, pose/action-to-animation mapping, playback speed, loop 여부, modelYawOffset, modelScale, display name, visual event marker

의존 규칙:
- Client는 Engine/EngineSDK와 Shared schema/component를 읽을 수 있다.
- Client가 authoritative gameplay truth를 새로 만들면 안 된다. 예외는 명시된 local-only smoke path뿐이다.
- Client visual data는 Shared id를 해석할 수 있지만, Shared gameplay data가 Client visual data를 include하거나 참조하면 안 된다.
- Client/Public에 새 `ID3D11*` 의존을 넓히지 않는다. DX11 concrete handle은 Engine backend 또는 좁은 bridge/adapter에 가둔다.
```

`### Tools / Editor` 섹션 바로 위에 아래에 추가:

```md
### Data / Collaboration

역할:
- `Data/Gameplay/ChampionGameData`는 기획/서버 협업 원천이다. gameplay fact만 가진다.
- `Data/Client/ChampionVisualData`는 아트/클라 협업 원천이다. 보이는 방식만 가진다.
- codegen tool은 한 원천 데이터를 한 ownership target으로만 보낸다.

의존 규칙:
- gameplay JSON에서 `visual*`, `anim*`, `castFrame`, `recoveryFrame`, resource path, display name을 금지한다.
- visual JSON에서 cooldown, damage, stat, server validation policy를 금지한다.
- visual core JSON에서 cast/recovery frame을 gameplay stage의 기본 필드로 두지 않는다. 프레임 기반 hook이 필요하면 `ChampionActionVisualEventData` 같은 visual event source로 분리하고, gameplay lock/cooldown/hit 판정과 섞지 않는다.
- 같은 값이 gameplay와 visual 양쪽에 필요해 보이면 먼저 이름을 의심한다. 판정에 쓰이면 gameplay, 보이는 타이밍이면 visual이다.
- 기존 `SkillDef`, `ChampionDef`, `SkillTable`, `ChampionTable`, `NetAnimationComponent` 같은 혼합 구조는 compatibility layer로 장기 보존하지 않는다. 전환 중 임시 bridge가 필요하면 삭제 조건과 삭제 대상 파일을 같은 계획에 적는다.
- 기능 회귀를 막기 위해 legacy runtime reader를 끊기 전에 새 pose/action/visual-event 값과 기존 NetAnimation 값이 같은 프레임에서 같은 의미로 해석되는지 먼저 비교한다.
```

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/SkillTypes.h

새 파일:

```cpp
#pragma once

#include <cstdint>

enum class eTargetMode : uint8_t
{
    Self,
    UnitTarget,
    GroundTarget,
    Direction,
};

enum class eTargetResolvePolicy : uint8_t
{
    Direct,
    Contextual,
};

enum class eSkillSlot : uint8_t
{
    BasicAttack = 0,
    Q = 1,
    W = 2,
    E = 3,
    R = 4,
    SLOT_END = 5,
};
```

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/SkillCommand.h

새 파일:

```cpp
#pragma once

#include "ECS/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

struct CastSkillCommand
{
    u8_t      slot = 0;
    u8_t      resolvedTargetMode = 0;
    EntityID  targetEntityId = NULL_ENTITY;
    Vec3      groundPos{ 0.f, 0.f, 0.f };
    Vec3      direction{ 0.f, 0.f, 0.f };
};
```

검토 메모:
- `resolvedTargetMode`는 최종 본질로는 `SkillTargetResolver`가 서버에서 재계산할 수 있어야 한다.
- 하지만 현재 `Scene_InGame.cpp`, `EventApplier.cpp`, `Irelia_Skills.cpp`가 conditional skill의 실제 해석값을 프레임 안에서 읽는다.
- 따라서 1차 구현에서 삭제하지 않는다. `eTargetResolvePolicy::Contextual` 기반 resolver가 모든 conditional skill을 대체한 뒤 삭제한다.

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ReplicatedPoseComponent.h 및 C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ReplicatedActionComponent.h

C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ReplicatedPoseComponent.h

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"

#include <type_traits>

enum class eReplicatedPoseId : u16_t
{
    None = 0,
    Idle = 1,
    Run = 2,
    Dead = 3,
};

struct ReplicatedPoseComponent
{
    u16_t poseId = static_cast<u16_t>(eReplicatedPoseId::Idle);
    u64_t startTick = 0;
    u32_t sequence = 0;
};

static_assert(std::is_trivially_copyable_v<ReplicatedPoseComponent>,
    "ReplicatedPoseComponent must be trivially_copyable for sim determinism.");
```

C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ReplicatedActionComponent.h

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"

#include <type_traits>

enum class eReplicatedActionId : u16_t
{
    None = 0,
    BasicAttack = 10,
    SkillQ = 20,
    SkillW = 21,
    SkillE = 22,
    SkillR = 23,
    Recall = 30,
    DeathStart = 50,
    ViegoConsumeSoul = 60,
};

struct ReplicatedActionComponent
{
    u16_t actionId = static_cast<u16_t>(eReplicatedActionId::None);
    u64_t startTick = 0;
    u32_t sequence = 0;
    u8_t stage = 1;
};

static_assert(std::is_trivially_copyable_v<ReplicatedActionComponent>,
    "ReplicatedActionComponent must be trivially_copyable for sim determinism.");
```

검토 메모:
- 기존 `NetAnimationComponent`는 Idle/Run 같은 지속 상태와 SkillQ/ViegoConsumeSoul 같은 action start를 한 struct에 섞는다.
- 더 본질적인 원자는 `pose`와 `action`이다. pose는 snapshot에서 계속 읽히는 몸 상태이고, action은 event/snapshot이 시작 시점과 sequence를 읽는 행위다.
- `animPhaseFrame`, `playbackRateQ8`, `flags`, `priority`는 최종 Shared state에서 제거한다. 단, Client visual source가 `playbackSpeed`, `bLoop`, visual event marker를 먼저 제공한 뒤 제거한다.

1-5. C:/Users/tnest/Desktop/Winters/Client/Public/GameObject/ChampionVisualData.h

새 파일:

```cpp
#pragma once

#include "GameContext.h"
#include "WintersTypes.h"

#include <cstdint>

inline constexpr uint8_t kChampionVisualTextureSlotMax = 8;
inline constexpr uint8_t kChampionVisualPoseMax = 8;
inline constexpr uint8_t kChampionVisualActionMax = 16;
inline constexpr uint8_t kChampionVisualActionStageMax = 2;
inline constexpr uint8_t kChampionVisualActionEventMax = 4;

enum class eChampionVisualEventKind : uint8_t
{
    None = 0,
    Cast = 1,
    Recovery = 2,
    KeySwap = 3,
    CastAccepted = 4,
};

struct ChampionModelVisualData
{
    const char* displayName = nullptr;
    const char* fbxPath = nullptr;
    const wchar_t* shaderPath = L"Shaders/Mesh3D.hlsl";
    const wchar_t* defaultTexturePath = nullptr;
    const wchar_t* texturePath[kChampionVisualTextureSlotMax] = {};
    f32_t modelYawOffset = 0.f;
    f32_t modelScale = 0.01f;
};

struct ChampionActionVisualEventData
{
    u8_t kind = static_cast<u8_t>(eChampionVisualEventKind::None);
    f32_t frame = 0.f;
    u32_t hookId = 0;
};

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

struct ChampionActionVisualData
{
    bool_t bValid = false;
    u16_t actionId = 0;
    u8_t stageCount = 1;
    ChampionActionVisualStageData stages[kChampionVisualActionStageMax] = {};
};

struct ChampionVisualData
{
    bool_t bValid = false;
    eChampion champion = eChampion::END;
    ChampionModelVisualData model{};
    ChampionPoseVisualData poses[kChampionVisualPoseMax] = {};
    ChampionActionVisualData actions[kChampionVisualActionMax] = {};
};
```

검토 메모:
- `bLoop`와 frame hook을 Shared action state에 두면 animation 소유권이 서버로 되돌아간다.
- 하지만 현재 Jax E loop, cast/recovery hook, visual hook dispatch가 프레임 중 읽힌다. 제거가 아니라 Client visual source로 이동해야 한다.
- `castFrame/recoveryFrame` 이름은 gameplay stage에서 삭제하고, visual-only event marker로만 남긴다.

1-6. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/SkillDef.h

삭제할 범위:
파일 전체 삭제.
`eTargetMode`, `eSkillSlot`은 `Shared/GameSim/Definitions/SkillTypes.h`로 이동한다.
`CastSkillCommand`는 `Shared/GameSim/Definitions/SkillCommand.h`로 이동한다.
`eRotateMode`, `SkillDef`, `g_SkillTable`, `g_SkillCount`, `FindSkillDef`는 최종 구조에 남기지 않는다.

1-7. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/ChampionDef.h

삭제할 범위:
파일 전체 삭제.
`ChampionDef`가 갖던 model/texture/shader/display/spawnScale/animation key는 `Data/Client/ChampionVisualData`와 `ChampionVisualData` runtime view로 이동한다.
`basicAttackRange`는 `ChampionStatsDef::baseAttackRange`만 사용한다.
`spawnPosition`은 champion visual 데이터가 아니라 server spawn policy 또는 명시적 local smoke spawn config 소유로 분리한다.

1-8. C:/Users/tnest/Desktop/Winters/Client/Public/GameObject/SkillDef.h

삭제할 범위:
파일 전체 삭제.
Client가 skill slot/target type을 읽어야 하면 `Shared/GameSim/Definitions/SkillTypes.h`를 include한다.
Client가 cast command payload를 읽어야 하면 `Shared/GameSim/Definitions/SkillCommand.h`를 include한다.
Client가 animation key/hook/frame을 읽어야 하면 `Client/Public/GameObject/ChampionVisualData.h`를 include한다.

1-9. C:/Users/tnest/Desktop/Winters/Client/Public/GameObject/ChampionDef.h

삭제할 범위:
파일 전체 삭제.
Client visual 표현 데이터는 `Client/Public/GameObject/ChampionVisualData.h`로 이동한다.

1-10. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/ChampionGameData.h

기존 코드:

```cpp
#include "Shared/GameSim/Definitions/SkillDef.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Definitions/SkillTypes.h"
```

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

기존 코드:

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

검토 메모:
- `Conditional`은 더 본질적으로 보면 target mode가 아니다. 문맥에 따라 `GroundTarget`/`Direction`/`UnitTarget` 중 하나를 고르는 resolve policy다.
- current code에는 `Conditional` 데이터와 `resolvedTargetMode` reader가 많으므로, 1차 구현은 JSON의 `"targetMode": "Conditional"`을 `targetMode + targetPolicy: Contextual`로 변환하는 bridge를 둔다.
- 모든 contextual skill이 `SkillTargetResolver`에서 서버 권위로 해석되기 전까지 `CastSkillCommand::resolvedTargetMode`를 삭제하지 않는다.

기존 코드:

```cpp
struct ChampionGameDataSummonerSpell
{
    bool_t bValid = false;
    u16_t spellId = 0;
    f32_t rangeMax = 0.f;
    f32_t cooldownSec = 0.f;
    u32_t gameplayPolicyId = 0;
    u32_t visualCueId = 0;
};
```

삭제할 코드:

```cpp
struct ChampionGameDataSummonerSpell
{
    bool_t bValid = false;
    u16_t spellId = 0;
    f32_t rangeMax = 0.f;
    f32_t cooldownSec = 0.f;
    u32_t gameplayPolicyId = 0;
    u32_t visualCueId = 0;
};
```

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

1-11. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/SummonerSpellGameData.h

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

1-12. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/ChampionRuntimeDefaults.h

CONFIRM_NEEDED:
- 최종 구조에서는 이 파일을 champion gameplay query owner로 남기지 않는다.
- `BuildDefaultChampionStat`, `GetDefaultChampionSkillRange`, `GetDefaultChampionSkillCooldown`, `GetDefaultChampionSkillTiming`, `GetDefaultChampionBasicAttackTiming`, `IsDefaultChampionSkillTwoStage`, `GetDefaultChampionSkillStageWindowSec`, tick helper callsite를 `ChampionGameDataDB`로 직접 이관한다.
- yaw helper callsite는 새 helper 파일을 만들지 말고 `WintersMath::NormalizeRadians`, `WintersMath::NearestEquivalentRadians`, `WintersMath::YawFromDirectionXZ`, `WintersMath::DirectionFromYawXZ`를 직접 사용한다.
- 이관 후 파일 전체 삭제.

삭제할 코드:

```cpp
f32_t GetDefaultChampionVisualYawOffset(eChampion champion);
// Canonical yaw is for wire values, debug deltas, and comparisons.
// Do not store canonical yaw directly into Transform body rotation.
f32_t NormalizeChampionVisualYaw(f32_t yaw);
f32_t ResolveChampionVisualYawFromDirection(eChampion champion, const Vec3& direction);
// Near yaw is for continuous Transform body rotation state.
f32_t MakeChampionVisualYawNear(f32_t yaw, f32_t referenceYaw);
f32_t ResolveChampionVisualYawNear(eChampion champion, const Vec3& direction,
    f32_t referenceYaw);
```

삭제할 코드:

```cpp
u16_t EncodeSkillPlaybackRateQ8(f32_t playSpeed);
```

1-13. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp

CONFIRM_NEEDED:
- 최종 구조에서는 파일 전체 삭제.
- 삭제 전 callsite가 남아 있으면 `ChampionGameDataDB` 또는 `WintersMath` 직접 호출로 먼저 이관한다.

삭제할 코드:

```cpp
f32_t GetDefaultChampionVisualYawOffset(eChampion champion)
{
    return ChampionGameDataDB::ResolveVisualYawOffset(champion);
}

f32_t NormalizeChampionVisualYaw(f32_t yaw)
{
    return WintersMath::NormalizeRadians(yaw);
}

f32_t MakeChampionVisualYawNear(f32_t yaw, f32_t referenceYaw)
{
    return WintersMath::NearestEquivalentRadians(yaw, referenceYaw);
}

f32_t ResolveChampionVisualYawFromDirection(eChampion champion, const Vec3& direction)
{
    return WintersMath::NormalizeRadians(WintersMath::YawFromDirectionXZ(
        direction,
        GetDefaultChampionVisualYawOffset(champion)));
}

f32_t ResolveChampionVisualYawNear(eChampion champion, const Vec3& direction, f32_t referenceYaw)
{
    return MakeChampionVisualYawNear(
        ResolveChampionVisualYawFromDirection(champion, direction),
        referenceYaw);
}
```

삭제할 코드:

```cpp
u16_t EncodeSkillPlaybackRateQ8(f32_t playSpeed)
{
    const f32_t sanitized = std::clamp(SanitizePositive(playSpeed, 1.f), 0.05f, 4.f);
    const i32_t encoded = static_cast<i32_t>(std::lround(sanitized * 256.f));
    return static_cast<u16_t>(std::clamp(encoded, 1, 1024));
}
```

1-14. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h

삭제할 코드:

```cpp
    f32_t ResolveVisualYawOffset(eChampion champion);
```

1-15. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.cpp

기존 코드:

```cpp
    ChampionSkillTimingDefaults MakeSkillTiming(f32_t lockDurationSec, f32_t animPlaySpeed)
    {
        ChampionSkillTimingDefaults timing{};
        timing.lockDurationSec = SanitizePositive(lockDurationSec, 0.6f);
        timing.animPlaySpeed = SanitizePositive(animPlaySpeed, 1.f);
        return timing;
    }
```

아래로 교체:

```cpp
    ChampionSkillTimingDefaults MakeSkillTiming(f32_t lockDurationSec)
    {
        ChampionSkillTimingDefaults timing{};
        timing.lockDurationSec = SanitizePositive(lockDurationSec, 0.6f);
        return timing;
    }
```

기존 코드:

```cpp
    ChampionSkillTimingDefaults MakeFallbackSkillTiming(u8_t slot)
    {
        ChampionSkillTimingDefaults timing{};
        timing.lockDurationSec =
            slot == static_cast<u8_t>(eSkillSlot::BasicAttack)
            ? 0.75f
            : 0.6f;
        timing.animPlaySpeed = 1.f;
        return timing;
    }
```

아래로 교체:

```cpp
    ChampionSkillTimingDefaults MakeFallbackSkillTiming(u8_t slot)
    {
        ChampionSkillTimingDefaults timing{};
        timing.lockDurationSec =
            slot == static_cast<u8_t>(eSkillSlot::BasicAttack)
            ? 0.75f
            : 0.6f;
        return timing;
    }
```

기존 코드:

```cpp
    ChampionBasicAttackTimingDefaults BuildBasicAttackTiming(
        const ChampionSkillTimingDefaults& skillTiming)
    {
        ChampionBasicAttackTimingDefaults timing{};
        timing.fActionDurationSec = SanitizePositive(skillTiming.lockDurationSec, 0.75f);
        timing.fAnimPlaySpeed = SanitizePositive(skillTiming.animPlaySpeed, 1.f);

        const f32_t fRawWindup = timing.fActionDurationSec * 0.35f;
        const f32_t fMaxWindup = (std::max)(0.05f, timing.fActionDurationSec - 0.03f);
        timing.fWindupSec = std::clamp(fRawWindup, 0.12f, fMaxWindup);
        return timing;
    }
```

아래로 교체:

```cpp
    ChampionBasicAttackTimingDefaults BuildBasicAttackTiming(
        const ChampionSkillTimingDefaults& skillTiming)
    {
        ChampionBasicAttackTimingDefaults timing{};
        timing.fActionDurationSec = SanitizePositive(skillTiming.lockDurationSec, 0.75f);

        const f32_t fRawWindup = timing.fActionDurationSec * 0.35f;
        const f32_t fMaxWindup = (std::max)(0.05f, timing.fActionDurationSec - 0.03f);
        timing.fWindupSec = std::clamp(fRawWindup, 0.12f, fMaxWindup);
        return timing;
    }
```

삭제할 코드:

```cpp
    f32_t ResolveFallbackVisualYawOffset(eChampion champion)
    {
        switch (champion)
        {
        case eChampion::NONE:
        case eChampion::END:
            return 0.f;
        default:
            return WintersMath::kPi;
        }
    }
```

기존 코드:

```cpp
                const ChampionGameDataSkillStage& skillStage = pSkill->stages[stageIndex];
                return MakeSkillTiming(skillStage.lockDurationSec, skillStage.animPlaySpeed);
```

아래로 교체:

```cpp
                const ChampionGameDataSkillStage& skillStage = pSkill->stages[stageIndex];
                return MakeSkillTiming(skillStage.lockDurationSec);
```

삭제할 코드:

```cpp
    f32_t ResolveVisualYawOffset(eChampion champion)
    {
        if (const ChampionGameData* pData = FindChampion(champion))
        {
            return pData->visualYawOffset;
        }

        return ResolveFallbackVisualYawOffset(champion);
    }
```

1-16. C:/Users/tnest/Desktop/Winters/Tools/ChampionData/build_champion_game_data.py

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

삭제할 코드:

```python
        "visualCueId": as_int(skill.get("visualCueId", 0), f"{path}.visualCueId"),
```

아래 로직 추가:

```python
        raw_target_mode = str(skill.get("targetMode", "Self"))
        target_policy = "Direct"
        if raw_target_mode == "Conditional":
            target_policy = "Contextual"
            raw_target_mode = str(skill.get("defaultTargetMode", "Direction"))
```

검토 메모:
- `Conditional` 문자열을 `eTargetMode`로 생성하지 않는다.
- 기존 JSON migration 전까지 generator bridge가 `"targetMode": "Conditional"`을 `targetPolicy: "Contextual"`로 해석한다.
- 각 champion skill의 정확한 default target mode는 `SkillTable.cpp`, champion skill cpp, input preview 경로를 확인해 채운다.

삭제할 코드:

```python
        "visualCueId": as_int(spell.get("visualCueId", 0), f"{path}.visualCueId"),
```

삭제할 범위:
`def normalize_spell(spell: dict, index: int) -> dict:` 줄부터
`def normalize_passive_dash(passive_dash: object, champion: str) -> dict | None:` 바로 위까지 삭제.

삭제할 코드:

```python
    spells = root.get("summonerSpells", [])
    if not isinstance(spells, list):
        fail("summonerSpells must be an array")

    normalized_spells = []
    for index in range(SUMMONER_SPELL_COUNT):
        source = spells[index] if index < len(spells) else {}
        normalized_spells.append(normalize_spell(source, index))
```

삭제할 코드:

```python
        "dataVersion": as_int(champion.get("dataVersion", 1), f"champions[{name}].dataVersion"),
```

삭제할 코드:

```python
        "visualYawOffset": as_float(champion.get("visualYawOffset", 0.0), f"champions[{name}].visualYawOffset"),
```

삭제할 코드:

```python
    lines.append(f"        skill{slot}.visualCueId = {skill['visualCueId']}u;")
```

삭제할 코드:

```python
        lines.append(f"        stage{slot}_{stage_index}.animPlaySpeed = {cpp_float(stage['animPlaySpeed'])};")
        lines.append(f"        stage{slot}_{stage_index}.castFrame = {cpp_float(stage['castFrame'])};")
        lines.append(f"        stage{slot}_{stage_index}.recoveryFrame = {cpp_float(stage['recoveryFrame'])};")
```

삭제할 코드:

```python
    lines.append(f"        spell{index}.visualCueId = {spell['visualCueId']}u;")
```

삭제할 범위:
`def append_spell(lines: list[str], spell: dict, index: int) -> None:` 줄부터
`def append_passive_dash(lines: list[str], passive_dash: dict | None) -> None:` 바로 위까지 삭제.

삭제할 코드:

```python
        lines.append(f"        data.dataVersion = {champion['dataVersion']}u;")
        lines.append("        data.authoringHash = kGeneratedChampionGameDataBuildHash;")
```

삭제할 코드:

```python
        lines.append(f"        data.visualYawOffset = {cpp_float(champion['visualYawOffset'])};")
```

삭제할 코드:

```python
        for index, spell in enumerate(data["summonerSpells"]):
            append_spell(lines, spell, index)
```

CONFIRM_NEEDED:
- root `summonerSpells`는 champion generator에서 제거하고 별도 summoner spell generator/source로 이동한다.

1-17. C:/Users/tnest/Desktop/Winters/Data/Gameplay/ChampionGameData/champions.json

CONFIRM_NEEDED:
- 파일 전체가 크고 champion별 수치가 많으므로 JSON parser로 안전하게 적용한다.
- 모든 champion root에서 `dataVersion`, `visualYawOffset` 삭제.
- 모든 skill에서 `visualCueId` 삭제.
- `"targetMode": "Conditional"`은 그대로 남기지 않는다. `targetMode`는 실제 기본 target atom으로 바꾸고 `targetPolicy: "Contextual"`을 추가한다.
- root `summonerSpells` 삭제. summoner spell gameplay source는 별도 파일로 이동.
- 모든 skill stage에서 `animPlaySpeed`, `castFrame`, `recoveryFrame` 삭제.
- 삭제한 표현 값은 `Data/Client/ChampionVisualData/champions.json`로 이동한다.
- gameplay JSON에는 stat/range/cooldown/mana/skill id/scaling id/gameplay policy/action lock/passive dash만 남긴다.

1-18. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Generated/ChampionGameData.generated.cpp

CONFIRM_NEEDED:
- `Tools/ChampionData/build_champion_game_data.py` 수정 후 생성물로만 갱신한다.
- 직접 손수 편집하지 않는다.
- 생성 후 `dataVersion`, `authoringHash`, `visualYawOffset`, `visualCueId`, `animPlaySpeed`, `castFrame`, `recoveryFrame`가 champion runtime table에 남아 있으면 실패로 본다.
- 생성 후 `summonerSpells`가 champion runtime table에 남아 있으면 실패로 본다.

1-19. C:/Users/tnest/Desktop/Winters/Data/Gameplay/SummonerSpellGameData/spells.json

CONFIRM_NEEDED:
- 새 파일이지만 complete body는 현재 root `summonerSpells`, spell id 정책, loadout 소유 구조를 확인한 뒤 작성한다.
- 이 파일에는 spell id, range, cooldown, gameplay policy만 둔다.
- visual cue, icon, animation, display name은 넣지 않는다.

1-20. C:/Users/tnest/Desktop/Winters/Data/Client/ChampionVisualData/champions.json

CONFIRM_NEEDED:
- 새 파일이지만 complete body는 현재 champion registration, `SkillTable.cpp`, `ChampionTable.cpp`, resource path, 기존 gameplay JSON visual field를 모두 읽어야 안전하게 만들 수 있다.
- visual source는 skill이 아니라 replicated pose/action을 기준으로 작성한다.
- 의도한 schema 원자는 아래 값만 가진다.

```json
{
  "schemaVersion": 1,
  "champions": [
    {
      "champion": "LEESIN",
      "model": {
        "displayName": "LeeSin",
        "modelYawOffset": 3.14159265,
        "fbxPath": "Client/Bin/Resource/Texture/Character/LeeSin/leesin.fbx",
        "shaderPath": "Shaders/Mesh3D.hlsl",
        "defaultTexturePath": "Client/Bin/Resource/Texture/Character/LeeSin/leesin_base_tx_cm.png",
        "texturePaths": [],
        "modelScale": 0.01
      },
      "poses": [
        {
          "poseId": "Idle",
          "animationKey": "Idle1",
          "playbackSpeed": 1.0,
          "loop": true
        },
        {
          "poseId": "Run",
          "animationKey": "run",
          "playbackSpeed": 1.0,
          "loop": true
        }
      ],
      "actions": [
        {
          "actionId": "BasicAttack",
          "stageCount": 1,
          "stages": [
            {
              "stage": 1,
              "animationKey": "skinned_mesh_attack1",
              "playbackSpeed": 1.0,
              "loop": false,
              "events": [
                {
                  "kind": "Cast",
                  "frame": 0.0,
                  "hook": "BasicAttackCast"
                }
              ]
            }
          ]
        }
      ]
    }
  ]
}
```

1-21. C:/Users/tnest/Desktop/Winters/Tools/ChampionVisualData/build_champion_visual_data.py

CONFIRM_NEEDED:
- 새 tool의 complete body는 `Data/Client/ChampionVisualData/champions.json` complete body 확정 후 작성한다.
- 출력 target은 Client 전용이어야 한다. `Shared/GameSim`으로 visual generated file을 만들지 않는다.
- wide string path 변환, symbolic action id 변환, champion enum 변환 규칙을 먼저 확정해야 한다.
- animation event marker와 hook id는 gameplay output에 넣지 않는다.
- visual source 안에서는 `ChampionActionVisualEventData`로만 출력한다. `castFrame`, `recoveryFrame`이라는 gameplay stage 필드명을 재사용하지 않는다.

1-22. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/NetAnimationComponent.h

최종 삭제 범위:
파일 전체 삭제.
서버가 복제하는 것은 animation이 아니라 pose/action state다.
대체 파일은 `Shared/GameSim/Components/ReplicatedPoseComponent.h`와 `Shared/GameSim/Components/ReplicatedActionComponent.h`다.

삭제 전 필수 bridge:
- 1차 구현에서는 `NetAnimationComponent`를 바로 삭제하지 않는다.
- 기존 NetAnimation writer가 있는 곳에서 `ReplicatedPoseComponent`와 `ReplicatedActionComponent`를 같은 tick에 dual-write한다.
- Client/Server가 새 pose/action path를 읽고, normal F5에서 Idle/Run/Skill/Recall/DeathStart/Dead/Jax E loop가 기존과 같은 프레임 의미로 재생되는 것을 확인한 뒤 삭제한다.
- bridge 기간이 끝나기 전까지 `animPhaseFrame`, `playbackRateQ8`, `flags`는 legacy reader를 위한 값으로만 남고 새 owner가 되면 안 된다.

1-23. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Snapshot.fbs

기존 코드:

```fbs
    animId:ushort;
    animPhaseFrame:ushort;
```

아래로 교체:

```fbs
    poseId:ushort;
    actionId:ushort;
```

기존 코드:

```fbs
    animStartTick:ulong;
    actionSeq:uint;
    animPlaybackRateQ8:ushort = 256;
    animFlags:ushort;
```

아래로 교체:

```fbs
    poseStartTick:ulong;
    poseSeq:uint;
    actionStartTick:ulong;
    actionSeq:uint;
    actionStage:ubyte = 1;
```

검토 메모:
- `animPhaseFrame`은 최종 schema에서 제거한다.
- 프레임 중 지속적으로 읽히는 Idle/Run은 action이 아니라 pose다. Snapshot에서 `poseId/poseStartTick/poseSeq`를 제공하지 않으면 이동 중 animation 선택이 Client 추론에만 의존하게 되어 기존 동작과 달라질 수 있다.
- action은 one-shot/loop skill/recall/death start 같은 시작 시점이 있는 행위만 표현한다.

1-24. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Event.fbs

기존 코드:

```fbs
    AnimationStart = 18,
```

아래로 교체:

```fbs
    ActionStart = 18,
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
```

아래로 교체:

```fbs
    actionStart:ActionStartEvent;
```

1-25. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Generated/cpp/Event_generated.h

CONFIRM_NEEDED:
- `Shared/Schemas/run_codegen.bat`로만 갱신한다.
- 직접 손수 편집하지 않는다.
- 생성 후 `AnimationStartEvent`, `playbackRateQ8`, `animId`, `flags`, `actionLoop`가 action replication path에 남아 있으면 실패다.
- 단, migration bridge 단계에서 legacy `NetAnimationComponent` reader가 남아 있는 것은 허용한다. final deletion gate에서만 실패로 본다.

1-26. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Generated/cpp/Snapshot_generated.h

CONFIRM_NEEDED:
- `Shared/Schemas/run_codegen.bat`로만 갱신한다.
- 직접 손수 편집하지 않는다.
- 생성 후 `poseId`, `poseStartTick`, `poseSeq`, `actionId`, `actionStartTick`, `actionSeq`, `actionStage`가 있어야 한다.
- 생성 후 `animPlaybackRateQ8`, `animId`, `animFlags`, `animStartTick`, `actionLoop`, `actionPhaseFrame`이 final entity pose/action path에 남아 있으면 실패다.
- 단, migration bridge 단계에서 legacy `NetAnimationComponent` reader가 남아 있는 것은 허용한다. final deletion gate에서만 실패로 본다.

1-27. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/Move/MoveSystem.cpp

CONFIRM_NEEDED:
- 최종 구조에서는 `#include "Shared/GameSim/Components/NetAnimationComponent.h"`를 제거하고 `#include "Shared/GameSim/Components/ReplicatedPoseComponent.h"`를 사용한다.
- 1차 bridge에서는 기존 `NetAnimationComponent` Idle/Run 갱신 옆에 `ReplicatedPoseComponent`를 dual-write한다.
- `ResolveEntityVisualYawOffset`, `ResolveVisualYawFromDirection`, `ResolveVisualYawNear`, `GameplayForwardFromVisualYaw`를 삭제한다.
- `NormalizeChampionVisualYaw(x)`는 `WintersMath::NormalizeRadians(x)`로 교체한다.
- `ResolveVisualYawFromDirection(direction, visualYawOffset)`는 `WintersMath::NormalizeRadians(WintersMath::YawFromDirectionXZ(direction))`로 교체한다.
- `ResolveVisualYawNear(direction, rot.y, visualYawOffset)`는 `WintersMath::NearestEquivalentRadians(WintersMath::NormalizeRadians(WintersMath::YawFromDirectionXZ(direction)), rot.y)`로 교체한다.
- `GameplayForwardFromVisualYaw(rot.y, visualYawOffset)`는 `WintersMath::DirectionFromYawXZ(rot.y)`로 교체한다.
- jungle/champion model offset 보정은 Shared에서 하지 않는다. 필요하면 Client visual apply path에서 `ChampionVisualData::model.modelYawOffset`을 더한다.
- Idle/Run은 action이 아니라 pose다. `eNetAnimId::Idle/Run`을 `eReplicatedPoseId::Idle/Run`으로 옮기고, Skill/Recall/DeathStart는 action path로만 옮긴다.

1-28. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

CONFIRM_NEEDED:
- 최종 구조에서는 `NetAnimationComponent` include/use를 `ReplicatedActionComponent`로 교체한다.
- 1차 bridge에서는 기존 `StartNetAnimation` 호출 옆에서 `ReplicatedActionComponent`를 dual-write한다.
- `eNetAnimId::SkillQ/W/E/R/Recall/Death`는 `eReplicatedActionId::SkillQ/W/E/R/Recall/DeathStart`로 교체한다. `eNetAnimId::Idle/Run`은 action으로 옮기지 않는다.
- `StartNetAnimation`은 `StartReplicatedAction`으로 교체한다.
- `playbackRateQ8` parameter와 `EncodeSkillPlaybackRateQ8(...)` 호출은 삭제한다.
- action stage는 `ReplicatedActionComponent::stage`에 명시한다. loop 여부는 서버 action state에 두지 않고 Client visual source의 action stage mapping에서 판단한다.
- `ResolveEntityVisualYawOffset`, `ResolveVisualYawFromDirection`, `ResolveVisualYawNear`, `ForwardFromYaw(yaw, visualYawOffset)`, `ForwardFromYaw(yaw, champion)`를 삭제하고 `WintersMath` yaw helper만 직접 사용한다.
- `ChampionGameDataDB::ResolveSkillTiming(...).animPlaySpeed` 접근은 삭제한다.
- 서버가 action speed를 정하지 않는다. Client는 action id/stage/startTick을 visual data의 `playbackSpeed`로 해석한다.
- Jax E 같은 지속 skill은 `ReplicatedActionComponent`에는 action id/stage/startTick만 남기고, `bLoop`는 `ChampionActionVisualStageData::bLoop`에서 읽는다.

1-29. C:/Users/tnest/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

CONFIRM_NEEDED:
- 최종 구조에서는 `NetAnimationComponent` include/use를 `ReplicatedPoseComponent`와 `ReplicatedActionComponent`로 교체한다.
- bridge 단계에서는 기존 snapshot과 새 pose/action snapshot을 같은 tick 기준으로 비교할 수 있게 bounded debug를 둔다.
- local 변수 `animId`, `animPhaseFrame`, `animStartTick`, `animPlaybackRateQ8`, `animFlags`는 `poseId`, `poseStartTick`, `poseSeq`, `actionId`, `actionStartTick`, `actionSeq`, `actionStage`로 정리한다.
- FlatBuffer `EntitySnapshot` 생성 인자도 schema 변경에 맞춰 갱신한다.
- 서버 snapshot에서 playback rate를 보내지 않는다.
- Snapshot은 pose와 action을 모두 보낸다. Client가 movement state만 보고 Idle/Run을 새로 추론하게 만들지 않는다.

1-30. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.h

CONFIRM_NEEDED:
- `#include "Shared/GameSim/Components/NetAnimationComponent.h"`를 `#include "Shared/GameSim/Components/ReplicatedActionComponent.h"`로 교체한다.
- `BuildAnimationStart`를 `BuildActionStart`로 이름 변경한다.
- parameter type을 `const ReplicatedActionComponent& action`으로 교체한다.
- pose change는 이벤트가 아니라 snapshot state로 보낸다. 필요한 경우에만 별도 `PoseChanged` 이벤트를 검토하되, 기본 계획에는 넣지 않는다.

1-31. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.cpp

CONFIRM_NEEDED:
- 아래 교체 본문은 `Event.fbs` 변경 의도를 반영한 기준 코드다.
- `Shared/Schemas/run_codegen.bat` 실행 후 `CreateActionStartEvent`, `CreateEventPacket`, `EventKind::ActionStart`의 실제 generated signature를 확인하고 인자 순서는 생성된 헤더에 맞춘다.

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
```

`BuildActionStart` 본문은 아래 코드로 교체:

```cpp
    {
        Reset(out);

        if (netId == NULL_NET_ENTITY || action.sequence == 0)
            return false;

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
            0), out);
    }
```

1-32. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomReplication.cpp

CONFIRM_NEEDED:
- action event 전송은 `ForEach<ReplicatedActionComponent>` 기준으로 교체한다.
- pose는 event로 쏘지 않고 snapshot에서 복제한다.
- `BuildAnimationStart` 호출을 `BuildActionStart`로 교체한다.
- dedup key가 `anim.actionSeq`/`anim.animId`에 의존하면 `action.sequence`/`action.actionId`로 교체한다.
- bridge 단계에서는 기존 `NetAnimationComponent` event와 새 action event를 동시에 발생시키지 않는다. 중복 재생을 막기 위해 한쪽만 active로 두고 debug 비교만 한다.

1-33. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

CONFIRM_NEEDED:
- yaw 비교/forward 계산은 gameplay yaw 기준으로 바꾼다.
- `ChampionGameDataDB::ResolveVisualYawOffset` 호출을 삭제한다.
- local debug message의 `offset=...`는 Client visual model offset debug가 필요한 위치로 옮긴다.
- snapshot schema 변경에 맞춰 `es->animId()`, `es->animPhaseFrame()`, `es->animStartTick()`, `es->animPlaybackRateQ8()`, `es->animFlags()`를 `pose*`/`action*` 접근자로 교체한다.
- `NetAnimationComponent` local mirror는 최종적으로 `ReplicatedPoseComponent`와 `ReplicatedActionComponent`로 교체한다.
- bridge 단계에서는 새 pose/action 값을 기존 local visual state와 비교하되, 기존 reader가 남아 있으면 같은 프레임에 동일한 animation start가 두 번 발생하지 않게 한다.
- `poseId`는 Idle/Run/Dead visual base state로 적용하고, `actionId`는 Skill/Recall/DeathStart overlay 또는 one-shot action으로 적용한다.

1-34. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

CONFIRM_NEEDED:
- `AnimationStart` event path를 `ActionStart`로 교체한다.
- `PlayNetworkAnimation`은 `PlayReplicatedActionVisual`로 rename한다.
- playback speed는 event/snapshot에서 받지 않고 `ChampionVisualData`의 champion/action/stage visual stage에서 조회한다.
- loop 여부도 event/snapshot에서 받지 않고 `ChampionActionVisualStageData::bLoop`에서 조회한다.
- `SlotFromNetAnim`, `IsOneShotNetAnim`, `ShouldLoopNetworkAnimation`은 action id 이름으로 정리한다.
- existing network animation event와 new action event가 bridge 기간에 동시에 같은 action을 재생하지 않도록 feature flag 또는 단일 active path를 둔다.

1-35. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

CONFIRM_NEEDED:
- `SkillDef`/`ChampionDef` 의존을 제거한다.
- `FindNetworkSkillDef`, `ResolveNetworkAnimName`, `ResolveNetworkActionDurationSec`는 `ChampionVisualData`의 pose/action mapping 기반으로 다시 작성한다.
- `DecodePlaybackRateQ8` 삭제.
- `ChampionGameDataDB::ResolveSkillTiming(...).animPlaySpeed` 접근 삭제.
- client action visual duration은 gameplay lockDurationSec와 visual playbackSpeed를 섞어 authoritative lock을 바꾸지 않도록 한다. 화면 재생 속도만 조정한다.
- `castFrame`, `recoveryFrame`, `castFrameHookId`, `recoveryHookId`, `keySwapHookId`, `onCastAcceptedHookId` reader는 제거 전에 `ChampionActionVisualEventData` reader로 대체한다.
- visual event marker는 hook dispatch와 FX timing만 담당한다. hit 판정, cooldown, action lock tick을 바꾸면 안 된다.

1-36. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/SkillTable.cpp

삭제할 범위:
파일 전체 삭제.
이 파일은 champion skill gameplay, animation key, frame hook, playback speed가 섞인 legacy table이다.
대체 소유자는 `Data/Gameplay/ChampionGameData`와 `Data/Client/ChampionVisualData`다.

1-37. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/ChampionTable.cpp

CONFIRM_NEEDED:
- 파일 전체 삭제가 목표다.
- complete removal 전에 `GetChampionDisplayName`, `FindChampionDef`, `RegisterAllLegacy` callsite를 모두 확인한다.
- display/model/texture path는 `ChampionVisualData` loader/catalog로 이동한다.
- champion spawn position을 이 파일에서 제공하고 있다면 server spawn policy 또는 explicit local smoke spawn config로 분리한다.

1-38. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/Champion/*/*_Registration.cpp

CONFIRM_NEEDED:
- registration cpp는 hook 함수 등록만 남긴다.
- `ChampionDef cd{}`, `SkillDef s{}`, `RegisterSkill`, `RegisterChampionDef` 작성 코드는 제거한다.
- champion-specific visual hook id는 symbolic name 기반 visual data 또는 narrow hook registry bridge로 연결한다.
- champion gameplay hook registration은 Shared/GameSim champion module 쪽에 남기고 Client visual hook registration과 섞지 않는다.

1-39. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h

기존 코드:

```cpp
#include "Shared/GameSim/Definitions/SkillDef.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Definitions/SkillTypes.h"
```

1-40. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/*/*.cpp

CONFIRM_NEEDED:
- `SkillDef.h` include가 있으면 `SkillTypes.h` 또는 `SkillCommand.h`로 교체한다.
- `eSkillSlot`, `eTargetMode`, `CastSkillCommand` 외의 `SkillDef` field 접근이 있으면 Shared에서 제거한다.
- `NetAnimationComponent`, `eNetAnimId`, `playbackRateQ8`, `EncodeSkillPlaybackRateQ8` 접근이 있으면 pose/action 분류부터 한다. Idle/Run/Dead는 `ReplicatedPoseComponent`, Skill/Recall/DeathStart는 `ReplicatedActionComponent`다.
- `animPlaySpeed`를 gameplay formula에 쓰고 있으면 해당 로직은 삭제하거나 Client visual로 이동한다.

1-41. C:/Users/tnest/Desktop/Winters/Server/Private/Game/*.cpp

CONFIRM_NEEDED:
- `NetAnimationComponent` include/use를 최종적으로 `ReplicatedPoseComponent`와 `ReplicatedActionComponent`로 교체한다.
- `eNetAnimId` 이름을 일괄 치환하지 않는다. 먼저 Idle/Run/Dead pose와 Skill/Recall/DeathStart action으로 분류한다.
- server는 pose id/startTick/seq와 action id/stage/startTick/seq만 기록한다.
- playback speed, animation key, visual duration 계산은 제거한다.

1-42. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/ChampionSpawnService.cpp

CONFIRM_NEEDED:
- `ChampionDef` 대신 `ChampionVisualData`를 사용한다.
- Shared gameplay stat/range와 Client model/texture/modelScale를 같은 struct에서 읽지 않는다.
- local-only smoke spawn이면 그 사실을 함수명 또는 config명에 드러낸다.

2. 검증

핵심 원칙:
- 기능 회귀를 막는다. 본질 분리는 기존 런타임 의미와 같은 프레임에서 parity가 확인된 뒤 legacy field를 지우는 순서로 진행한다.
- normal F5에서 roster, map, minion, snapshot, champion, UI, FX를 숨기지 않는다.
- bridge는 임시다. 하지만 bridge를 두지 않고 삭제하면 현재 코드베이스 기준으로 Idle/Run, loop action, visual hook이 회귀할 가능성이 높다.

Stage 0 - 계획/생성물 준비:
- `git diff --check`
- `python Tools/ChampionData/build_champion_game_data.py`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`
- 확인: 기존 behavior가 변경되지 않는다.

Stage 1 - pose/action dual-write:
- `ReplicatedPoseComponent`, `ReplicatedActionComponent`를 추가한다.
- 기존 `NetAnimationComponent` writer 옆에서 pose/action을 dual-write한다.
- Snapshot/Event schema는 아직 끊지 않거나, 새 path를 debug 비교용으로만 둔다.
- 확인: Idle/Run, SkillQ/W/E/R, BasicAttack, Recall, DeathStart가 기존 `NetAnimationComponent`와 같은 tick/sequence 의미를 가진다.
- 확인 명령: `rg -n "ReplicatedPoseComponent|ReplicatedActionComponent" Shared Server Client`

Stage 2 - Snapshot/Event migration:
- `Shared/Schemas/run_codegen.bat`
- Snapshot은 `poseId/poseStartTick/poseSeq`와 `actionId/actionStartTick/actionSeq/actionStage`를 함께 보낸다.
- Event는 `ActionStart`만 보낸다. pose는 snapshot state로 유지한다.
- 확인: Client에서 같은 action이 legacy event와 new action event로 두 번 재생되지 않는다.
- 확인 명령: `rg -n "AnimationStartEvent|animPlaybackRateQ8|animFlags|actionLoop|actionPhaseFrame" Shared/Schemas Shared/GameSim Server Client`

Stage 3 - Client visual source migration:
- `Data/Client/ChampionVisualData/champions.json`과 Client generated/catalog를 붙인다.
- playback speed, loop 여부, visual hook marker는 `ChampionVisualData`에서만 읽는다.
- `castFrame/recoveryFrame`은 gameplay stage에서 삭제하고 `ChampionActionVisualEventData`로 대체한다.
- 확인: Jax E loop, Yasuo/Irelia contextual skill preview, basic attack hook, skill cast/recovery visual hook이 기존처럼 동작한다.
- 확인 명령: `rg -n "animPlaySpeed|castFrame|recoveryFrame|castFrameHookId|recoveryHookId" Shared Data/Gameplay Tools/ChampionData`

Stage 4 - final deletion gate:
- `rg -n "visualCueId|visualYawOffset|dataVersion|authoringHash|summonerSpells" Shared Data/Gameplay Tools/ChampionData`
- `rg -n "\"targetMode\"\\s*:\\s*\"Conditional\"|eTargetMode::Conditional" Data/Gameplay Shared Tools/ChampionData`
- `rg -n "SkillDef|ChampionDef|NetAnimationComponent|playbackRateQ8|ResolveVisualYawOffset|NormalizeChampionVisualYaw|ResolveChampionVisualYaw|ChampionFacing" Shared Server Client`
- `rg -n "animId|animPhaseFrame|animPlaybackRateQ8|animFlags|AnimationStartEvent|actionLoop|actionPhaseFrame|phaseFrame|priority" Shared/Schemas Shared/GameSim Server Client`
- `rg -n "ID3D11|DX11|Renderer|ImGui|Client/Public/GameObject/ChampionVisualData.h" Shared/GameSim`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`

수동 확인:
- Shared/GameSim 회전값은 gameplay yaw이며 model yaw offset을 더하지 않는다.
- Client 렌더 적용 지점에서만 `ChampionVisualData::model.modelYawOffset`을 적용한다.
- Client visual playback speed 변경이 server action lock tick, cooldown, hit timing을 바꾸지 않는다.
- Summoner spell range/cooldown은 champion table이 아니라 `SummonerSpellGameData` resolver에서 읽힌다.
- normal F5에서 player champion, AI champion, lane minion, jungle, snapshot interpolation, skill FX, UI cooldown을 확인한다.

확인 필요:
- 새로 추가한 `.h/.cpp` 파일이 빌드 프로젝트에 포함되는지 확인한다.
- schema 생성 후 Go/cpp generated output이 둘 다 갱신되는지 확인한다.
- `Data/Client/ChampionVisualData/champions.json` complete body는 모든 champion registration과 resource path를 읽은 뒤 별도 계획에서 확정한다.
