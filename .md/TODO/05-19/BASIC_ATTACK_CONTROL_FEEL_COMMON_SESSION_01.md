Session - 1 공통 BasicAttack timing과 CombatAction 상태 컴포넌트 토대를 추가한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/CombatActionComponent.h

새 파일:

```cpp
#pragma once

#include "ECS/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <type_traits>

enum class eCombatActionKind : u8_t
{
    None = 0,
    BasicAttack = 1,
    Skill = 2,
};

enum class eCombatActionMovePolicy : u8_t
{
    None = 0,
    CancelBeforeImpactMoveAfterImpact = 1,
    QueueMoveUntilImpact = 2,
    LockUntilEnd = 3,
};

struct CombatActionComponent
{
    eCombatActionKind eKind = eCombatActionKind::None;
    eCombatActionMovePolicy eMovePolicy = eCombatActionMovePolicy::None;
    u8_t uSlot = 0;
    u8_t uStage = 1;
    u16_t uFlags = 0;
    EntityID entityTarget = NULL_ENTITY;
    u32_t uSequenceNum = 0;
    u64_t uStartTick = 0;
    u64_t uImpactTick = 0;
    u64_t uEndTick = 0;
    bool_t bImpactIssued = false;
    bool_t bQueuedMove = false;
    Vec3 vQueuedMoveTarget{};
    Vec3 vQueuedMoveDirection{};
};

static_assert(std::is_trivially_copyable_v<CombatActionComponent>,
    "CombatActionComponent must be trivially_copyable for sim determinism.");
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionRuntimeDefaults.h

기존 코드:

```cpp
struct ChampionSkillTimingDefaults
{
    f32_t lockDurationSec = 0.6f;
    f32_t animPlaySpeed = 1.f;
};
```

아래에 추가:

```cpp
struct ChampionBasicAttackTimingDefaults
{
    f32_t fWindupSec = 0.25f;
    f32_t fActionDurationSec = 0.75f;
    f32_t fAnimPlaySpeed = 1.f;
};
```

기존 코드:

```cpp
ChampionSkillTimingDefaults GetDefaultChampionSkillTiming(eChampion champion, u8_t slot);
ChampionSkillTimingDefaults GetDefaultChampionSkillTiming(eChampion champion, u8_t slot, u8_t stage);
bool_t IsDefaultChampionSkillTwoStage(eChampion champion, u8_t slot);
```

아래에 추가:

```cpp
ChampionBasicAttackTimingDefaults GetDefaultChampionBasicAttackTiming(eChampion champion);
u64_t GetDefaultChampionBasicAttackWindupTicks(eChampion champion);
```

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp

기존 코드:

```cpp
ChampionSkillTimingDefaults GetDefaultChampionSkillTiming(eChampion champion, u8_t slot)
{
    return GetDefaultChampionSkillTiming(champion, slot, 1);
}
```

아래에 추가:

```cpp
ChampionBasicAttackTimingDefaults GetDefaultChampionBasicAttackTiming(eChampion champion)
{
    const ChampionSkillTimingDefaults skillTiming =
        GetDefaultChampionSkillTiming(
            champion,
            static_cast<u8_t>(eSkillSlot::BasicAttack));

    ChampionBasicAttackTimingDefaults timing{};
    timing.fActionDurationSec = SanitizePositive(skillTiming.lockDurationSec, 0.75f);
    timing.fAnimPlaySpeed = SanitizePositive(skillTiming.animPlaySpeed, 1.f);

    const f32_t fRawWindup = timing.fActionDurationSec * 0.35f;
    const f32_t fMaxWindup = (std::max)(0.05f, timing.fActionDurationSec - 0.03f);
    timing.fWindupSec = std::clamp(fRawWindup, 0.12f, fMaxWindup);
    return timing;
}
```

기존 코드:

```cpp
u64_t GetDefaultChampionSkillActionLockTicks(eChampion champion, u8_t slot)
{
    return GetDefaultChampionSkillActionLockTicks(champion, slot, 1);
}
```

위 코드 바로 위에 추가:

```cpp
u64_t GetDefaultChampionBasicAttackWindupTicks(eChampion champion)
{
    const ChampionBasicAttackTimingDefaults timing =
        GetDefaultChampionBasicAttackTiming(champion);
    const f64_t ticks =
        static_cast<f64_t>(timing.fWindupSec) *
        static_cast<f64_t>(DeterministicTime::kTicksPerSecond);
    const u64_t roundedUp = static_cast<u64_t>(std::ceil(ticks));
    return roundedUp > 0 ? roundedUp : 1u;
}
```

1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/BotLaneAISystem.cpp

기존 코드:

```cpp
    f32_t ResolveBasicAttackWindupSec(eChampion champion)
    {
        const ChampionSkillTimingDefaults timing =
            GetDefaultChampionSkillTiming(champion, static_cast<u8_t>(eSkillSlot::BasicAttack));
        return std::clamp(timing.lockDurationSec * 0.35f, 0.20f, 0.65f);
    }
```

아래로 교체:

```cpp
    f32_t ResolveBasicAttackWindupSec(eChampion champion)
    {
        return GetDefaultChampionBasicAttackTiming(champion).fWindupSec;
    }
```

1-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Components/CombatActionComponent.h"
```

기존 코드:

```cpp
    f32_t ResolveBasicAttackCooldown(CWorld& world, EntityID entity, eChampion champion)
```

위 코드 바로 위에 추가:

```cpp
    eCombatActionMovePolicy ResolveBasicAttackMovePolicy(eChampion champion)
    {
        if (champion == eChampion::KALISTA)
            return eCombatActionMovePolicy::QueueMoveUntilImpact;

        return eCombatActionMovePolicy::CancelBeforeImpactMoveAfterImpact;
    }

    void ClearCombatAction(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<CombatActionComponent>(entity))
            world.RemoveComponent<CombatActionComponent>(entity);
    }

    bool_t HasActiveBasicAttackAction(CWorld& world, EntityID entity)
    {
        return entity != NULL_ENTITY &&
            world.HasComponent<CombatActionComponent>(entity) &&
            world.GetComponent<CombatActionComponent>(entity).eKind ==
                eCombatActionKind::BasicAttack;
    }
```

기존 코드:

```cpp
    ClearAttackChase(world, cmd.issuerEntity);
```

`CDefaultCommandExecutor::HandleMove(...)` 안의 위 코드 바로 아래에 추가:

```cpp
    if (HasActiveBasicAttackAction(world, cmd.issuerEntity))
        ClearCombatAction(world, cmd.issuerEntity);
```

기존 코드:

```cpp
    ClearAttackChase(world, cmd.issuerEntity);
    ClearMoveTarget(world, cmd.issuerEntity);
```

`CDefaultCommandExecutor::HandleBasicAttack(...)` 안의 위 코드 바로 아래에 추가:

```cpp
    auto& action = world.HasComponent<CombatActionComponent>(cmd.issuerEntity)
        ? world.GetComponent<CombatActionComponent>(cmd.issuerEntity)
        : world.AddComponent<CombatActionComponent>(cmd.issuerEntity, CombatActionComponent{});
    const ChampionBasicAttackTimingDefaults attackTiming =
        GetDefaultChampionBasicAttackTiming(champion);
    const u64_t windupTicks = GetDefaultChampionBasicAttackWindupTicks(champion);
    const u64_t actionTicks = GetDefaultChampionSkillActionLockTicks(
        champion,
        static_cast<u8_t>(eSkillSlot::BasicAttack));

    action.eKind = eCombatActionKind::BasicAttack;
    action.eMovePolicy = ResolveBasicAttackMovePolicy(champion);
    action.uSlot = static_cast<u8_t>(eSkillSlot::BasicAttack);
    action.uStage = 1;
    action.uFlags = 0;
    action.entityTarget = cmd.targetEntity;
    action.uSequenceNum = cmd.sequenceNum;
    action.uStartTick = tc.tickIndex;
    action.uImpactTick = tc.tickIndex + windupTicks;
    action.uEndTick = tc.tickIndex + ((actionTicks > windupTicks) ? actionTicks : windupTicks);
    action.bImpactIssued = false;
    action.bQueuedMove = false;
    action.vQueuedMoveTarget = {};
    action.vQueuedMoveDirection = {};

    (void)attackTiming;
```

확인 필요:

- 세션 1에서는 기존 즉시 데미지/쿨다운/FX 동작을 아직 제거하지 않는다.
- 세션 2에서 `HandleBasicAttack(...)`의 즉시 impact 코드를 `CombatActionSystem` impact tick으로 옮긴다.
- 세션 2에서 `HandleMove(...)`의 `ClearCombatAction(...)`을 정책 기반 cancel/queued move 처리로 교체한다.

1-6. C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj

확인 필요:

- 새 파일 `..\..\Shared\GameSim\Components\CombatActionComponent.h`가 프로젝트 header 목록에 필요한지 확인한다.
- Visual Studio Solution Explorer 노출이 필요하면 `<ClInclude>`에 추가한다.

1-7. C:/Users/user/Desktop/Winters/Client/Include/Client.vcxproj

확인 필요:

- 새 파일 `..\..\Shared\GameSim\Components\CombatActionComponent.h`가 프로젝트 header 목록에 필요한지 확인한다.
- Visual Studio Solution Explorer 노출이 필요하면 `<ClInclude>`에 추가한다.

2. 검증

미검증:

- 빌드 미검증.
- 세션 1은 공통 timing/action-state 토대 추가이며, BA impact 지연과 우클릭 backswing cancel은 세션 2에서 검증한다.
- Kalista의 `QueueMoveUntilImpact` 정책은 세션 1에서 enum/초기값만 들어가며, 실제 passive dash 소비는 세션 3 또는 세션 4에서 검증한다.

검증 명령:

```powershell
git diff --check -- Shared/GameSim/Components/CombatActionComponent.h Shared/GameSim/Definitions/ChampionRuntimeDefaults.h Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp Shared/GameSim/Systems/BotLaneAISystem.cpp Shared/GameSim/Systems/CommandExecutor.cpp
```

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

확인 필요:

- `CombatActionComponent`가 Shared/GameSim component 네이밍과 include 규칙을 깨지 않는지 확인.
- Bot AI의 BA hit estimate가 기존보다 너무 빨라지거나 늦어지지 않는지 로그로 확인.
- 세션 2 진입 전 `HandleBasicAttack(...)`의 즉시 damage/cooldown/effect 블록을 impact 함수로 분리할 정확한 anchor를 다시 읽는다.
