Session - Scene_InGame active skill runtime에서 SkillDef를 진실 자리에서 내리고 command/game atom/visual atom/legacy hook bridge만 남긴다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

기존 코드:

```cpp
#include "GameObject/SkillDef.h"
#include "GameObject/ChampionDef.h"
```

아래로 교체:

```cpp
#include "GameObject/SkillDef.h"
#include "GameObject/SkillVisualData.h"
#include "GameObject/ChampionDef.h"
#include "Shared/GameSim/Definitions/SkillAtomData.h"
```

기존 코드:

```cpp
    bool_t      m_bEndTransitionMoving = false;
    const SkillDef* m_pLastDispatchedSkill = nullptr;
```

아래로 교체:

```cpp
    bool_t      m_bEndTransitionMoving = false;
    SkillVisualData m_LastSkillVisual{};
    bool_t      m_bHasLastSkillVisual = false;
```

기존 코드:

```cpp
    SkillDef        m_ActiveSkillDefStorage{};
    CastSkillCommand m_ActiveSkillCommandStorage{};
    const SkillDef* m_pActiveSkillDef = nullptr;
    f32_t           m_fActivePrevFrame = 0.f;
    bool_t          m_bLogFrameEvents = false;
```

아래로 교체:

```cpp
    struct ActiveSkillRuntime
    {
        bool_t bActive = false;
        eChampion champion = eChampion::END;
        u8_t slot = 0;
        u8_t stage = 1;
        CastSkillCommand command{};
        SkillGameAtomBundle game{};
        SkillVisualData visual{};
        SkillDef legacyHookBridge{};
        f32_t prevFrame = 0.f;
        bool_t bCastFrameFired = false;
        bool_t bRecoveryFrameFired = false;
    };

    ActiveSkillRuntime m_ActiveSkill{};
    bool_t             m_bLogFrameEvents = false;
```

삭제할 코드:

```cpp
    bool_t m_bCastFrameFired = false;
    bool_t m_bRecoveryFrameFired = false;
```

기존 코드:

```cpp
    void ApplyLocalPrediction(const CastSkillCommand& cmd, const SkillDef& def, u8_t skillStage = 1);
    bool BuildCastCommand(const SkillDef& def, CastSkillCommand& outCmd);

    void RotatePlayerToward(eRotateMode mode, const CastSkillCommand& cmd);
```

아래로 교체:

```cpp
    void ApplyLocalPrediction(
        const CastSkillCommand& cmd,
        const SkillGameAtomBundle& gameData,
        const SkillVisualData& visualData,
        const SkillDef& legacyDef,
        u8_t skillStage = 1);
    bool BuildCastCommand(const SkillTargetSpec& targetSpec, u8_t skillStage, CastSkillCommand& outCmd);

    void RotatePlayerToward(const SkillFacingSpec& facingSpec, u8_t skillStage, const CastSkillCommand& cmd);
    void ClearActiveSkillRuntime();
    void BeginActiveSkillRuntime(
        const CastSkillCommand& cmd,
        const SkillGameAtomBundle& gameData,
        const SkillVisualData& visualData,
        const SkillDef& legacyDef,
        u8_t skillStage);
```

본질 의심 결과:
- `m_pActiveSkillDef`는 본질이 아니다. 활성 스킬의 본질은 `command`, `game atom`, `visual atom`, `stage`다.
- `m_ActiveSkillDefStorage`는 본질이 아니다. 다만 기존 hook API가 `SkillDef*`를 받으므로 `legacyHookBridge`라는 이름으로 격리한다.
- `m_pLastDispatchedSkill`은 본질이 아니다. 종료 전환에 필요한 것은 마지막 `SkillVisualData`뿐이다.
- `m_bCastFrameFired`, `m_bRecoveryFrameFired`, `m_fActivePrevFrame`는 전역성 필드가 아니라 active runtime 내부 상태다.

1-2. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

```cpp
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
```

아래로 교체:

```cpp
#include "GamePlay/SkillRegistry.h"
#include "GamePlay/VisualHookRegistry.h"
#include "GameObject/SkillDefVisualDataAdapter.h"
#include "Shared/GameSim/Definitions/SkillDefGameDataAdapter.h"
```

기존 코드:

```cpp
    constexpr f32_t kNetworkActorInterpMinYaw = 0.0005f;

    f32_t SmoothStep01(f32_t t)
```

아래로 교체:

```cpp
    constexpr f32_t kNetworkActorInterpMinYaw = 0.0005f;

    u8_t GetSkillStageIndex(u8_t skillStage)
    {
        return skillStage >= 2u ? 1u : 0u;
    }

    eTargetShape GetTargetShape(const SkillTargetSpec& target, u8_t skillStage)
    {
        return target.shape[GetSkillStageIndex(skillStage)];
    }

    eSkillFacingMode GetFacingMode(const SkillFacingSpec& facing, u8_t skillStage)
    {
        return facing.mode[GetSkillStageIndex(skillStage)];
    }

    SkillVisualStageData GetVisualStage(const SkillVisualData& visual, u8_t skillStage)
    {
        return visual.stages[GetSkillStageIndex(skillStage)];
    }

    const VisualEventData* FindVisualEvent(
        const SkillVisualStageData& stage,
        eVisualEventKind kind)
    {
        for (u8_t i = 0; i < stage.eventCount; ++i)
        {
            if (stage.events[i].kind == static_cast<u8_t>(kind))
                return &stage.events[i];
        }

        return nullptr;
    }

    eTargetMode ToLegacyTargetMode(eTargetShape shape)
    {
        switch (shape)
        {
        case eTargetShape::Unit:
            return eTargetMode::UnitTarget;
        case eTargetShape::Ground:
            return eTargetMode::GroundTarget;
        case eTargetShape::Direction:
            return eTargetMode::Direction;
        case eTargetShape::Self:
        default:
            return eTargetMode::Self;
        }
    }

    eRotateMode ToLegacyRotateMode(eSkillFacingMode mode)
    {
        switch (mode)
        {
        case eSkillFacingMode::TowardsTarget:
            return eRotateMode::TowardsTarget;
        case eSkillFacingMode::TowardsCommandDirection:
            return eRotateMode::TowardsCursor;
        case eSkillFacingMode::None:
        default:
            return eRotateMode::None;
        }
    }

    SkillDef BuildLegacyHookBridge(
        const SkillGameAtomBundle& gameData,
        const SkillVisualData& visualData,
        const SkillDef& legacyDef,
        u8_t skillStage)
    {
        SkillDef bridge = legacyDef;
        const SkillVisualStageData visualStage = GetVisualStage(visualData, skillStage);

        bridge.champ = gameData.slot.champion;
        bridge.slot = gameData.slot.slot;
        bridge.targetMode = ToLegacyTargetMode(GetTargetShape(gameData.target, skillStage));
        bridge.cooldownSec = gameData.cooldown.cooldownSec;
        bridge.rangeMax = gameData.range.rangeMax;
        bridge.manaCost = gameData.cost.manaCost;
        bridge.lockDurationSec = gameData.stage.lockDurationSec[GetSkillStageIndex(skillStage)];
        bridge.stageCount = gameData.stage.stageCount;
        bridge.stageWindowSec = gameData.stage.stageWindowSec;
        bridge.rotate = ToLegacyRotateMode(GetFacingMode(gameData.facing, skillStage));
        bridge.animKey = visualStage.animationKey ? visualStage.animationKey : legacyDef.animKey;
        bridge.animPlaySpeed = visualStage.playbackSpeed;

        if (const VisualEventData* eventData = FindVisualEvent(visualStage, eVisualEventKind::Cast))
        {
            bridge.castFrame = eventData->frame;
            bridge.castFrameHookId = eventData->hookId;
        }
        if (const VisualEventData* eventData = FindVisualEvent(visualStage, eVisualEventKind::Recovery))
        {
            bridge.recoveryFrame = eventData->frame;
            bridge.recoveryHookId = eventData->hookId;
        }

        bridge.endTransitionIdleAnim = visualData.endTransitionIdleAnim;
        bridge.endTransitionRunAnim = visualData.endTransitionRunAnim;
        bridge.endTransitionDuration = visualData.endTransitionDuration;
        bridge.skillId = gameData.slot.skillId;
        bridge.scalingTableId = gameData.effect.scalingTableId;
        return bridge;
    }

    f32_t SmoothStep01(f32_t t)
```

본질 의심 결과:
- `BuildLegacyHookBridge`는 새 진실이 아니다. 기존 hook 함수들이 아직 `SkillDef`를 읽기 때문에 만드는 임시 어댑터다.
- `SkillDef s2`처럼 gameplay/visual 값을 섞은 새 스킬을 만들지 않는다.
- stage별 frame/hook은 `SkillVisualData.events`에서 읽고, stage별 target/facing/lock은 `SkillGameAtomBundle`에서 읽는다.

1-3. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

```cpp
            && m_bCastFrameFired
            && m_pLastDispatchedSkill != nullptr)
```

아래로 교체:

```cpp
            && m_ActiveSkill.bCastFrameFired
            && m_ActiveSkill.bActive)
```

기존 코드:

```cpp
                static_cast<u32_t>(m_pLastDispatchedSkill->slot),
                m_pLastDispatchedSkill->castFrameHookId);
```

아래로 교체:

```cpp
                static_cast<u32_t>(m_ActiveSkill.slot),
                m_ActiveSkill.legacyHookBridge.castFrameHookId);
```

기존 코드:

```cpp
    if (m_bNetworkAuthoritativeGameplay && m_pActiveSkillDef)
    {
        m_pActiveSkillDef = nullptr;
        m_fActivePrevFrame = 0.f;
        m_bCastFrameFired = false;
        m_bRecoveryFrameFired = false;
    }
    else if (m_pActiveSkillDef && m_pPlayerRenderer)
```

아래로 교체:

```cpp
    if (m_bNetworkAuthoritativeGameplay && m_ActiveSkill.bActive)
    {
        ClearActiveSkillRuntime();
    }
    else if (m_ActiveSkill.bActive && m_pPlayerRenderer)
```

`else if` 내부의 기존 active skill 읽기 구간에서 아래 기존 코드를 교체:

```cpp
            const SkillDef& d = *m_pActiveSkillDef;
```

아래로 교체:

```cpp
            const SkillDef& d = m_ActiveSkill.legacyHookBridge;
            const CastSkillCommand& activeCommand = m_ActiveSkill.command;
```

기존 코드:

```cpp
                !m_bCastFrameFired
                && d.castFrame > 0.f
                && pAnim->HasFramePassed(d.castFrame, m_fActivePrevFrame);
```

아래로 교체:

```cpp
                !m_ActiveSkill.bCastFrameFired
                && d.castFrame > 0.f
                && pAnim->HasFramePassed(d.castFrame, m_ActiveSkill.prevFrame);
```

기존 코드:

```cpp
                !m_bRecoveryFrameFired
                && d.recoveryFrame > 0.f
                && pAnim->HasFramePassed(d.recoveryFrame, m_fActivePrevFrame);
```

아래로 교체:

```cpp
                !m_ActiveSkill.bRecoveryFrameFired
                && d.recoveryFrame > 0.f
                && pAnim->HasFramePassed(d.recoveryFrame, m_ActiveSkill.prevFrame);
```

이 active frame block 안에서 아래 읽기를 모두 교체:

```text
m_bCastFrameFired -> m_ActiveSkill.bCastFrameFired
m_bRecoveryFrameFired -> m_ActiveSkill.bRecoveryFrameFired
m_ActiveSkillCommandStorage -> activeCommand
m_pActiveSkillDef -> &m_ActiveSkill.legacyHookBridge
m_fActivePrevFrame -> m_ActiveSkill.prevFrame
m_pActiveSkillDef = nullptr -> ClearActiveSkillRuntime()
```

본질 의심 결과:
- frame event dispatch의 본질은 active runtime 안의 event frame과 hook id다.
- 기존 hook API 때문에 `SkillDef& d`를 쓰지만, 그 값은 `legacyHookBridge`로 stage-aware하게 만든다.
- active frame block 바깥에서 `m_bCastFrameFired` 같은 느슨한 전역 상태를 읽지 않는다.

1-4. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

```cpp
    bool_t bPassiveDashWindow =
        m_pActiveSkillDef &&
        (m_pActiveSkillDef->slot == 0 || m_pActiveSkillDef->slot == 1) &&
        !m_bRecoveryFrameFired;
```

아래로 교체:

```cpp
    bool_t bPassiveDashWindow =
        m_ActiveSkill.bActive &&
        (m_ActiveSkill.slot == 0 || m_ActiveSkill.slot == 1) &&
        !m_ActiveSkill.bRecoveryFrameFired;
```

기존 코드:

```cpp
        passiveSlot = m_pActiveSkillDef->slot;
```

아래로 교체:

```cpp
        passiveSlot = m_ActiveSkill.slot;
```

기존 코드:

```cpp
        if (!m_bKalistaPassiveDashHasFaceDir && m_pActiveSkillDef)
        {
            const auto& activeCmd = m_ActiveSkillCommandStorage;
```

아래로 교체:

```cpp
        if (!m_bKalistaPassiveDashHasFaceDir && m_ActiveSkill.bActive)
        {
            const auto& activeCmd = m_ActiveSkill.command;
```

본질 의심 결과:
- Kalista passive dash가 필요한 것은 active skill pointer가 아니라 active slot, recovery fired 여부, command 방향이다.

1-5. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

```cpp
                const f32_t skillSpeed = m_pActiveSkillDef ? m_pActiveSkillDef->animPlaySpeed : 1.f;
                s = m_fAttackSpeedMul * m_fGlobalAnimSpeed * skillSpeed;
```

아래로 교체:

```cpp
                const SkillVisualStageData visualStage =
                    m_ActiveSkill.bActive
                        ? GetVisualStage(m_ActiveSkill.visual, m_ActiveSkill.stage)
                        : SkillVisualStageData{};
                const f32_t skillSpeed =
                    m_ActiveSkill.bActive ? visualStage.playbackSpeed : 1.f;
                s = m_fAttackSpeedMul * m_fGlobalAnimSpeed * skillSpeed;
```

본질 의심 결과:
- animation playback speed는 visual atom이다.
- active skill pointer에서 읽는 것은 본질이 아니다.

1-6. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

```cpp
            if (m_pLastDispatchedSkill)
            {
                pTransition = m_bMoving
                    ? m_pLastDispatchedSkill->endTransitionRunAnim
                    : m_pLastDispatchedSkill->endTransitionIdleAnim;
                fDur = m_pLastDispatchedSkill->endTransitionDuration;
            }
```

아래로 교체:

```cpp
            if (m_bHasLastSkillVisual)
            {
                pTransition = m_bMoving
                    ? m_LastSkillVisual.endTransitionRunAnim
                    : m_LastSkillVisual.endTransitionIdleAnim;
                fDur = m_LastSkillVisual.endTransitionDuration;
            }
```

본질 의심 결과:
- end transition은 visual이다.
- 마지막 스킬 전체 정의를 붙들 이유가 없다.

1-7. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

```cpp
    m_fLastActionTimer = 0.f;
    m_pActiveSkillDef = nullptr;
    m_fActivePrevFrame = 0.f;
    ResetLocalSkillRuntimeState();
```

아래로 교체:

```cpp
    m_fLastActionTimer = 0.f;
    ClearActiveSkillRuntime();
    ResetLocalSkillRuntimeState();
```

`PreemptAction` 함수 바로 아래에 추가:

```cpp
void CScene_InGame::ClearActiveSkillRuntime()
{
    m_ActiveSkill = ActiveSkillRuntime{};
}
```

`ClearActiveSkillRuntime` 바로 아래에 추가:

```cpp
void CScene_InGame::BeginActiveSkillRuntime(
    const CastSkillCommand& cmd,
    const SkillGameAtomBundle& gameData,
    const SkillVisualData& visualData,
    const SkillDef& legacyDef,
    u8_t skillStage)
{
    m_ActiveSkill = ActiveSkillRuntime{};
    m_ActiveSkill.bActive = true;
    m_ActiveSkill.champion = gameData.slot.champion;
    m_ActiveSkill.slot = gameData.slot.slot;
    m_ActiveSkill.stage = skillStage == 0u ? 1u : skillStage;
    m_ActiveSkill.command = cmd;
    m_ActiveSkill.game = gameData;
    m_ActiveSkill.visual = visualData;
    m_ActiveSkill.legacyHookBridge =
        BuildLegacyHookBridge(gameData, visualData, legacyDef, m_ActiveSkill.stage);
    m_ActiveSkill.prevFrame = 0.f;
    m_ActiveSkill.bCastFrameFired = false;
    m_ActiveSkill.bRecoveryFrameFired = false;

    m_LastSkillVisual = visualData;
    m_bHasLastSkillVisual = true;
}
```

본질 의심 결과:
- active runtime 시작은 한 함수만 책임진다.
- stage2 field override는 `BeginActiveSkillRuntime`이 아니라 `BuildLegacyHookBridge`에만 갇힌다.
- `SkillDef` bridge는 hook API 삭제 전까지의 경계다.

1-8. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

`DispatchSkillInput` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    if (!def)
    {
        Winters::DevSmoke::Log(
            "[SkillDispatch] rejected slot=%u champ=%u reason=no-def\n",
            static_cast<u32_t>(slot),
            static_cast<u32_t>(champ));
        return false;
    }
```

아래에 추가:

```cpp
    SkillGameAtomBundle gameData{};
    SkillVisualData visualData{};
    if (!CSkillRegistry::Instance().ResolveGameAtoms(champ, lookupSlot, gameData))
    {
        gameData = SkillDefAdapters::BuildSkillGameAtomBundle(*def);
    }
    if (!CSkillRegistry::Instance().ResolveSkillVisualData(champ, lookupSlot, visualData))
    {
        visualData = SkillDefAdapters::BuildSkillVisualData(*def);
    }
```

삭제할 코드:

```cpp
        SkillDef s2 = *def;
        s2.targetMode = def->stage2TargetMode;
        s2.animKey = def->stage2AnimKey ? def->stage2AnimKey : def->animKey;
        s2.lockDurationSec = def->stage2LockSec > 0.f ? def->stage2LockSec : def->lockDurationSec;
        s2.rotate = def->stage2Rotate;
        s2.animPlaySpeed = def->stage2PlaySpeed;
        s2.castFrame = def->stage2CastFrame;
        s2.recoveryFrame = def->stage2RecoveryFrame;
        s2.stageCount = 1;
```

`DispatchSkillInput` 안의 직접 읽기를 아래처럼 교체:

```text
def->stageCount -> gameData.stage.stageCount
def->cooldownSec -> gameData.cooldown.cooldownSec
def->stageWindowSec -> gameData.stage.stageWindowSec
BuildCastCommand(s2, cmd) -> BuildCastCommand(gameData.target, 2, cmd)
BuildCastCommand(*def, cmd) -> BuildCastCommand(gameData.target, 1, cmd)
RotatePlayerToward(s2.rotate, cmd) -> RotatePlayerToward(gameData.facing, 2, cmd)
RotatePlayerToward(def->rotate, cmd) -> RotatePlayerToward(gameData.facing, 1, cmd)
ApplyLocalPrediction(cmd, s2, 2) -> ApplyLocalPrediction(cmd, gameData, visualData, *def, 2)
ApplyLocalPrediction(cmd, *def, 1) -> ApplyLocalPrediction(cmd, gameData, visualData, *def, 1)
ApplyLocalPrediction(cmd, *def) -> ApplyLocalPrediction(cmd, gameData, visualData, *def, 1)
```

본질 의심 결과:
- Dispatch는 skill definition을 재구성하지 않는다.
- Dispatch는 현재 입력이 어떤 target/facing/stage/cooldown/visual을 쓰는지만 읽는다.

1-9. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

```cpp
bool CScene_InGame::BuildCastCommand(const SkillDef& def, CastSkillCommand& outCmd)
{
    eTargetMode mode = def.targetMode;

    if (mode == eTargetMode::Conditional)
    {
        mode = eTargetMode::Direction;
    }
```

아래로 교체:

```cpp
bool CScene_InGame::BuildCastCommand(
    const SkillTargetSpec& targetSpec,
    u8_t skillStage,
    CastSkillCommand& outCmd)
{
    const eTargetMode mode = ToLegacyTargetMode(GetTargetShape(targetSpec, skillStage));
```

본질 의심 결과:
- command build에는 `SkillDef`가 필요 없다.
- command wire 호환 때문에 `eTargetMode` 값만 마지막에 변환한다.

1-10. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

```cpp
void CScene_InGame::ApplyLocalPrediction(const CastSkillCommand& cmd, const SkillDef& def, u8_t skillStage)
```

아래로 교체:

```cpp
void CScene_InGame::ApplyLocalPrediction(
    const CastSkillCommand& cmd,
    const SkillGameAtomBundle& gameData,
    const SkillVisualData& visualData,
    const SkillDef& legacyDef,
    u8_t skillStage)
```

이 함수 안에서 아래 읽기를 교체:

```text
def.slot -> gameData.slot.slot
def.champ -> gameData.slot.champion
def.animKey -> active bridge의 animKey 또는 GetVisualStage(visualData, skillStage).animationKey
def.animPlaySpeed -> GetVisualStage(visualData, skillStage).playbackSpeed
def.lockDurationSec -> gameData.stage.lockDurationSec[GetSkillStageIndex(skillStage)]
def.rotate -> GetFacingMode(gameData.facing, skillStage)
def.onCastAcceptedHookId -> FindVisualEvent(GetVisualStage(visualData, skillStage), eVisualEventKind::CastAccepted)->hookId
```

기존 코드:

```cpp
            m_ActiveSkillDefStorage = def;
            m_ActiveSkillCommandStorage = cmd;
            m_pActiveSkillDef = &m_ActiveSkillDefStorage;
            m_fActivePrevFrame = 0.f;
            m_bCastFrameFired = false;
            m_bRecoveryFrameFired = false;
            ResetLocalSkillRuntimeState();
            m_pLastDispatchedSkill = &m_ActiveSkillDefStorage;
```

아래로 교체:

```cpp
            BeginActiveSkillRuntime(cmd, gameData, visualData, legacyDef, skillStage);
            ResetLocalSkillRuntimeState();
```

기존 코드:

```cpp
    m_bCastFrameFired = false;
    m_bRecoveryFrameFired = false;
    ResetLocalSkillRuntimeState();

    m_pActiveSkillDef = &m_ActiveSkillDefStorage;
    m_fActivePrevFrame = 0.f;
```

아래로 교체:

```cpp
    ResetLocalSkillRuntimeState();
    BeginActiveSkillRuntime(cmd, gameData, visualData, legacyDef, skillStage);
```

본질 의심 결과:
- local prediction이 active runtime을 시작한다.
- local prediction은 더 이상 `SkillDef`를 active truth로 저장하지 않는다.
- hook accepted path는 아직 legacy hook context가 있으므로 `legacyHookBridge` 또는 `legacyDef`를 넣되, stage는 반드시 `skillStage`로 전달한다.

1-11. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

```cpp
void CScene_InGame::RotatePlayerToward(eRotateMode mode, const CastSkillCommand& cmd)
```

아래로 교체:

```cpp
void CScene_InGame::RotatePlayerToward(
    const SkillFacingSpec& facingSpec,
    u8_t skillStage,
    const CastSkillCommand& cmd)
{
    const eRotateMode mode = ToLegacyRotateMode(GetFacingMode(facingSpec, skillStage));
```

함수 본문 나머지는 기존 `RotatePlayerToward(eRotateMode mode, ...)`의 본문을 그대로 사용한다.

본질 의심 결과:
- facing의 소유자는 `SkillFacingSpec`이다.
- `eRotateMode`는 기존 구현을 재사용하기 위한 로컬 변수일 뿐 함수 경계가 아니다.

2. 검증

미검증:
- active skill runtime을 `SkillDef*`에서 atom runtime으로 바꾼 뒤 빌드 미검증.
- basic attack, Q/W/E/R, stage2 강제 입력, Kalista passive dash, end transition 회귀 미검증.
- legacy hook들이 `legacyHookBridge`를 통해 기존처럼 동작하는지 미검증.

검증 명령:
- `Shared\Schemas\run_codegen.bat`
- `rg -n "m_pActiveSkillDef|m_ActiveSkillDefStorage|m_ActiveSkillCommandStorage|m_pLastDispatchedSkill|m_bCastFrameFired|m_bRecoveryFrameFired|m_fActivePrevFrame|SkillDef s2" Client/Public/Scene/Scene_InGame.h Client/Private/Scene/Scene_InGame.cpp`
- `rg -n "def->stageCount|def->cooldownSec|def->stageWindowSec|def->targetMode|def->rotate|BuildCastCommand\\(\\*def|ApplyLocalPrediction\\(cmd, \\*def" Client/Private/Scene/Scene_InGame.cpp`
- `rg -n "GameObject/SkillVisualData|GameObject/ChampionVisualData|Client/Public|Client/" Shared/GameSim Server -S --glob "!**/Bin/**" --glob "!**/Generated/**"`
- `git diff --check`
- `MSBuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m:1`
- `.\Tools\Bin\Debug\SimLab.exe 300 42`

확인 필요:
- `Scene_InGame`의 active skill 상태가 `m_ActiveSkill` 하나로 모였는지 확인한다.
- `SkillDef`가 active truth로 저장되지 않고 `legacyHookBridge`로만 남았는지 확인한다.
- stage2가 `SkillDef s2` 복사본이 아니라 `stage = 2`와 atom index로 처리되는지 확인한다.
- visual event frame/hook이 `SkillVisualData.events`에서 bridge로 흘러가는지 확인한다.
- Shared/GameSim과 Server가 Client visual atom을 include하지 않는지 확인한다.

후속 동기화:
- 이번 변경이 통과하면 다음 계획에서 `SkillHookContext`, `VisualHookContext`, `GameplayHookContext`의 `pDef` 의존을 `SkillHookFacts` 같은 작은 hook input으로 줄인다.
- 그 다음에 `SkillDef`, `SkillTable`의 남은 사용처를 registry import/compatibility layer로만 격리한다.
