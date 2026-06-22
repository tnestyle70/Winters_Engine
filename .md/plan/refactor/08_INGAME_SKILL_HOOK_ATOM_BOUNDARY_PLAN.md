Session - gameplay/visual/local hook 호출 경계에서 SkillDef 포인터를 제거하고 실행 순간에 필요한 원자 값만 전달한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h

삭제할 코드:

```cpp
#include "Shared/GameSim/Definitions/SkillDef.h"
```

삭제할 코드:

```cpp
	const SkillDef* pDef = nullptr;
```

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

`DispatchGameplayHookIfAvailable` 안의 기존 코드를:

```cpp
        SkillDef def{};
        def.champ = champion;
        def.slot = cmd.slot;
        def.skillId = static_cast<u16_t>((static_cast<u32_t>(champion) << 8) | cmd.slot);

        GameplayHookContext ctx{};
        ctx.pWorld = &world;
        ctx.casterEntity = cmd.issuerEntity;
        ctx.casterTeam = ResolveTeam(world, cmd.issuerEntity);
        ctx.casterChampion = champion;
        ctx.skillRank = rank;
        ctx.pDef = &def;
        ctx.pCommand = &cmd;
```

아래로 교체:

```cpp
        GameplayHookContext ctx{};
        ctx.pWorld = &world;
        ctx.casterEntity = cmd.issuerEntity;
        ctx.casterTeam = ResolveTeam(world, cmd.issuerEntity);
        ctx.casterChampion = champion;
        ctx.skillRank = rank;
        ctx.pCommand = &cmd;
```

1-3. C:/Users/tnest/Desktop/Winters/Client/Public/GamePlay/SkillHookContext.h

삭제할 코드:

```cpp
#include "GameObject/SkillDef.h"
```

기존 코드:

```cpp
#include "GameContext.h"
```

아래로 교체:

```cpp
#include "GameContext.h"
#include "Shared/GameSim/Definitions/SkillCommand.h"
```

기존 코드:

```cpp
	const SkillDef* pDef = nullptr;
	const CastSkillCommand* pCommand = nullptr;
	u8_t skillStage = 1;
```

아래로 교체:

```cpp
	u8_t skillSlot = 0;
	u8_t skillStage = 1;
	f32_t fSkillLockDurationSec = 0.f;
	f32_t fSkillStageWindowSec = 0.f;
	const CastSkillCommand* pCommand = nullptr;
```

1-4. C:/Users/tnest/Desktop/Winters/Client/Public/GamePlay/VisualHookRegistry.h

삭제할 코드:

```cpp
#include "Shared/GameSim/Definitions/SkillDef.h"
```

기존 코드:

```cpp
#include "GameContext.h"
```

아래로 교체:

```cpp
#include "GameContext.h"
#include "Shared/GameSim/Definitions/SkillCommand.h"
```

기존 코드:

```cpp
	const SkillDef* pDef = nullptr;
	const CastSkillCommand* pCommand = nullptr;
	u8_t skillStage = 1;
```

아래로 교체:

```cpp
	u8_t skillSlot = 0;
	u8_t skillStage = 1;
	f32_t fSkillLockDurationSec = 0.f;
	f32_t fSkillStageWindowSec = 0.f;
	f32_t fSkillRangeMax = 0.f;
	const CastSkillCommand* pCommand = nullptr;
```

1-5. C:/Users/tnest/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

기존 코드:

```cpp
        SkillGameAtomBundle game{};
        SkillVisualData visual{};
        SkillDef legacyHookBridge{};
        f32_t prevFrame = 0.f;
```

아래로 교체:

```cpp
        SkillGameAtomBundle game{};
        SkillVisualData visual{};
        f32_t prevFrame = 0.f;
```

기존 코드:

```cpp
    bool_t ValidateLocalSkillStart(const SkillDef& def);
```

아래로 교체:

```cpp
    bool_t ValidateLocalSkillStart(const SkillSlotBinding& slotBinding);
```

기존 코드:

```cpp
    void ApplyLocalPrediction(
        const CastSkillCommand& cmd,
        const SkillGameAtomBundle& gameData,
        const SkillVisualData& visualData,
        const SkillDef& legacyDef,
        u8_t skillStage = 1);
```

아래로 교체:

```cpp
    void ApplyLocalPrediction(
        const CastSkillCommand& cmd,
        const SkillGameAtomBundle& gameData,
        const SkillVisualData& visualData,
        u8_t skillStage = 1);
```

기존 코드:

```cpp
    void BeginActiveSkillRuntime(
        const CastSkillCommand& cmd,
        const SkillGameAtomBundle& gameData,
        const SkillVisualData& visualData,
        const SkillDef& legacyDef,
        u8_t skillStage);
```

아래로 교체:

```cpp
    void BeginActiveSkillRuntime(
        const CastSkillCommand& cmd,
        const SkillGameAtomBundle& gameData,
        const SkillVisualData& visualData,
        u8_t skillStage);
```

1-6. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

```cpp
    SkillVisualStageData GetVisualStage(const SkillVisualData& visual, u8_t skillStage)
    {
        return visual.stages[GetSkillStageIndex(skillStage)];
    }
```

아래로 교체:

```cpp
    const SkillVisualStageData& GetVisualStage(
        const SkillVisualData& visual,
        u8_t skillStage)
    {
        return visual.stages[GetSkillStageIndex(skillStage)];
    }
```

삭제할 범위:
`eRotateMode ToLegacyRotateMode(eSkillFacingMode mode)` 줄부터
`SkillDef BuildLegacyHookBridge(...)` 함수의 닫는 중괄호까지 삭제.
그 아래 `f32_t SmoothStep01(f32_t t)` 함수는 남긴다.

`FindVisualEvent` 바로 아래에 추가:

```cpp
    u32_t FindVisualHookId(
        const SkillVisualStageData& stage,
        eVisualEventKind kind)
    {
        const VisualEventData* pEvent = FindVisualEvent(stage, kind);
        return pEvent ? pEvent->hookId : 0u;
    }

    f32_t ResolveSkillStageLockSec(
        const SkillGameAtomBundle& gameData,
        u8_t skillStage)
    {
        return gameData.stage.lockDurationSec[GetSkillStageIndex(skillStage)];
    }

    void ApplySkillHookAtoms(
        SkillHookContext& ctx,
        const SkillGameAtomBundle& gameData,
        u8_t skillStage)
    {
        ctx.skillSlot = gameData.slot.slot;
        ctx.skillStage = skillStage;
        ctx.fSkillLockDurationSec = ResolveSkillStageLockSec(gameData, skillStage);
        ctx.fSkillStageWindowSec = gameData.stage.stageWindowSec;
    }

    void ApplySkillHookAtoms(
        VisualHookContext& ctx,
        const SkillGameAtomBundle& gameData,
        u8_t skillStage)
    {
        ctx.skillSlot = gameData.slot.slot;
        ctx.skillStage = skillStage;
        ctx.fSkillLockDurationSec = ResolveSkillStageLockSec(gameData, skillStage);
        ctx.fSkillStageWindowSec = gameData.stage.stageWindowSec;
        ctx.fSkillRangeMax = gameData.range.rangeMax;
    }
```

기존 코드:

```cpp
    bool_t ShouldLoopLocalSkillAnimation(const SkillDef& def, u8_t skillStage)
    {
        if (def.champ == eChampion::JAX &&
            def.slot == static_cast<u8_t>(eSkillSlot::E) &&
            skillStage == 1u)
        {
            return true;
        }

        return !def.bOneShot;
    }
```

아래로 교체:

```cpp
    bool_t ShouldLoopLocalSkillAnimation(
        const SkillSlotBinding& slotBinding,
        const SkillVisualStageData& visualStage,
        u8_t skillStage)
    {
        if (slotBinding.champion == eChampion::JAX &&
            slotBinding.slot == static_cast<u8_t>(eSkillSlot::E) &&
            skillStage == 1u)
        {
            return true;
        }

        return visualStage.bLoop;
    }
```

1-7. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

```cpp
bool_t CScene_InGame::ValidateLocalSkillStart(const SkillDef& def)
{
    if (def.champ == eChampion::YASUO
        && def.slot == static_cast<uint8_t>(eSkillSlot::R))
```

아래로 교체:

```cpp
bool_t CScene_InGame::ValidateLocalSkillStart(const SkillSlotBinding& slotBinding)
{
    if (slotBinding.champion == eChampion::YASUO
        && slotBinding.slot == static_cast<uint8_t>(eSkillSlot::R))
```

기존 코드:

```cpp
    if (!ValidateLocalSkillStart(*def))
        return false;
```

아래로 교체:

```cpp
    if (!ValidateLocalSkillStart(gameData.slot))
        return false;
```

기존 코드:

```cpp
        ApplyLocalPrediction(cmd, gameData, visualData, *def, 2);
```

아래로 교체:

```cpp
        ApplyLocalPrediction(cmd, gameData, visualData, 2);
```

기존 코드:

```cpp
        ApplyLocalPrediction(cmd, gameData, visualData, *def, 1);
```

아래로 교체:

```cpp
        ApplyLocalPrediction(cmd, gameData, visualData, 1);
```

기존 코드:

```cpp
    ApplyLocalPrediction(cmd, gameData, visualData, *def);
```

아래로 교체:

```cpp
    ApplyLocalPrediction(cmd, gameData, visualData);
```

기존 코드:

```cpp
    Winters::DevSmoke::Log(
        "[SkillDispatch] accepted slot=%u champ=%u hook=0x%08X anim=%s\n",
        static_cast<u32_t>(slot),
        static_cast<u32_t>(champ),
        m_ActiveSkill.legacyHookBridge.castFrameHookId,
        m_ActiveSkill.legacyHookBridge.animKey
        ? m_ActiveSkill.legacyHookBridge.animKey
        : "(null)");
```

아래로 교체:

```cpp
    const SkillVisualStageData& acceptedVisualStage =
        GetVisualStage(visualData, 1u);
    Winters::DevSmoke::Log(
        "[SkillDispatch] accepted slot=%u champ=%u hook=0x%08X anim=%s\n",
        static_cast<u32_t>(slot),
        static_cast<u32_t>(champ),
        FindVisualHookId(acceptedVisualStage, eVisualEventKind::Cast),
        acceptedVisualStage.animationKey
        ? acceptedVisualStage.animationKey
        : "(null)");
```

1-8. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

```cpp
void CScene_InGame::BeginActiveSkillRuntime(
    const CastSkillCommand& cmd,
    const SkillGameAtomBundle& gameData,
    const SkillVisualData& visualData,
    const SkillDef& legacyDef,
    u8_t skillStage)
```

아래로 교체:

```cpp
void CScene_InGame::BeginActiveSkillRuntime(
    const CastSkillCommand& cmd,
    const SkillGameAtomBundle& gameData,
    const SkillVisualData& visualData,
    u8_t skillStage)
```

삭제할 코드:

```cpp
    m_ActiveSkill.legacyHookBridge =
        BuildLegacyHookBridge(gameData, visualData, legacyDef, stage);
```

1-9. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

```cpp
                m_ActiveSkill.legacyHookBridge.castFrameHookId);
```

아래로 교체:

```cpp
                FindVisualHookId(
                    GetVisualStage(m_ActiveSkill.visual, m_ActiveSkill.stage),
                    eVisualEventKind::Cast));
```

active skill frame 처리 블록의 기존 코드를:

```cpp
            const SkillDef& d = m_ActiveSkill.legacyHookBridge;
            const CastSkillCommand& activeCommand = m_ActiveSkill.command;

            const bool bCastHit =
                !m_ActiveSkill.bCastFrameFired
                && d.castFrame > 0.f
                && pAnim->HasFramePassed(d.castFrame, m_ActiveSkill.prevFrame);
            const bool bRecoveryHit =
                !m_ActiveSkill.bRecoveryFrameFired
                && d.recoveryFrame > 0.f
                && pAnim->HasFramePassed(d.recoveryFrame, m_ActiveSkill.prevFrame);
```

아래로 교체:

```cpp
            const CastSkillCommand& activeCommand = m_ActiveSkill.command;
            const SkillVisualStageData& visualStage =
                GetVisualStage(m_ActiveSkill.visual, m_ActiveSkill.stage);
            const VisualEventData* pCastEvent =
                FindVisualEvent(visualStage, eVisualEventKind::Cast);
            const VisualEventData* pRecoveryEvent =
                FindVisualEvent(visualStage, eVisualEventKind::Recovery);

            const bool bCastHit =
                !m_ActiveSkill.bCastFrameFired
                && pCastEvent
                && pCastEvent->frame > 0.f
                && pAnim->HasFramePassed(pCastEvent->frame, m_ActiveSkill.prevFrame);
            const bool bRecoveryHit =
                !m_ActiveSkill.bRecoveryFrameFired
                && pRecoveryEvent
                && pRecoveryEvent->frame > 0.f
                && pAnim->HasFramePassed(pRecoveryEvent->frame, m_ActiveSkill.prevFrame);
```

같은 active skill frame 처리 블록에서 아래 기존 식별자를 교체:

```cpp
d.slot
d.animKey
d.castFrameHookId
d.recoveryHookId
```

아래로 교체:

```cpp
m_ActiveSkill.slot
visualStage.animationKey
pCastEvent->hookId
pRecoveryEvent->hookId
```

기존 코드:

```cpp
            if (d.recoveryFrame > 0.f && curF >= d.recoveryFrame)
                ClearActiveSkillRuntime();
```

아래로 교체:

```cpp
            if (pRecoveryEvent &&
                pRecoveryEvent->frame > 0.f &&
                curF >= pRecoveryEvent->frame)
            {
                ClearActiveSkillRuntime();
            }
```

같은 블록에서 모든 `GameplayHookContext`의 아래 코드를 삭제:

```cpp
gameCtx.pDef = &m_ActiveSkill.legacyHookBridge;
```

같은 블록에서 `VisualHookContext`와 `SkillHookContext`를 만든 직후 아래 코드를:

```cpp
ctx.pDef = &m_ActiveSkill.legacyHookBridge;
ctx.pCommand = &activeCommand;
ctx.skillStage = m_ActiveSkill.stage;
```

아래로 교체:

```cpp
ApplySkillHookAtoms(ctx, m_ActiveSkill.game, m_ActiveSkill.stage);
ctx.pCommand = &activeCommand;
```

`visualCtx` 이름을 쓰는 블록에는 동일하게 아래 코드를 사용:

```cpp
ApplySkillHookAtoms(visualCtx, m_ActiveSkill.game, m_ActiveSkill.stage);
visualCtx.pCommand = &activeCommand;
```

1-10. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

```cpp
    const SkillDef* pDef = CSkillRegistry::Instance().Find(eChampion::KALISTA, slot);
    if (!pDef)
        pDef = FindSkillDef(eChampion::KALISTA, slot);
    if (!pDef)
        return false;

    SkillHookContext ctx{};
```

아래로 교체:

```cpp
    SkillGameAtomBundle gameData{};
    if (!CSkillRegistry::Instance().ResolveGameAtoms(eChampion::KALISTA, slot, gameData))
    {
        const SkillDef* pLegacyDef = FindSkillDef(eChampion::KALISTA, slot);
        if (!pLegacyDef)
            return false;
        gameData = SkillDefAdapters::BuildSkillGameAtomBundle(*pLegacyDef);
    }

    SkillHookContext ctx{};
```

기존 코드:

```cpp
    ctx.pDef = pDef;
    ctx.pCasterRenderer = m_pPlayerRenderer;
```

아래로 교체:

```cpp
    ApplySkillHookAtoms(ctx, gameData, 1u);
    ctx.pCasterRenderer = m_pPlayerRenderer;
```

1-11. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

`ApplyLocalPrediction` 함수 시작부의 아래 기존 코드를 교체한다.

기존 코드:

```cpp
void CScene_InGame::ApplyLocalPrediction(
    const CastSkillCommand& cmd,
    const SkillGameAtomBundle& gameData,
    const SkillVisualData& visualData,
    const SkillDef& legacyDef,
    u8_t skillStage)
{
    const SkillDef bridge = BuildLegacyHookBridge(gameData, visualData, legacyDef, skillStage);
```

아래로 교체:

```cpp
void CScene_InGame::ApplyLocalPrediction(
    const CastSkillCommand& cmd,
    const SkillGameAtomBundle& gameData,
    const SkillVisualData& visualData,
    u8_t skillStage)
{
    const SkillVisualStageData& visualStage = GetVisualStage(visualData, skillStage);
    const u32_t keySwapHookId =
        FindVisualHookId(visualStage, eVisualEventKind::KeySwap);
    const u32_t acceptedHookId =
        FindVisualHookId(visualStage, eVisualEventKind::CastAccepted);
    const f32_t lockDurationSec = ResolveSkillStageLockSec(gameData, skillStage);
```

함수 안에서 아래 기존 식별자를 교체:

```cpp
bridge.slot
bridge.champ
bridge.animKey
bridge.keySwapHookId
bridge.onCastAcceptedHookId
bridge.lockDurationSec
```

아래로 교체:

```cpp
gameData.slot.slot
gameData.slot.champion
visualStage.animationKey
keySwapHookId
acceptedHookId
lockDurationSec
```

기존 코드:

```cpp
                ShouldLoopLocalSkillAnimation(bridge, skillStage));
```

아래로 교체:

```cpp
                ShouldLoopLocalSkillAnimation(gameData.slot, visualStage, skillStage));
```

함수 안의 모든 `GameplayHookContext`에서 삭제할 코드:

```cpp
gameCtx.pDef = &bridge;
```

함수 안의 모든 `VisualHookContext`와 `SkillHookContext`에서 아래 기존 코드를:

```cpp
ctx.pDef = &bridge;
ctx.pCommand = &cmd;
ctx.skillStage = skillStage;
```

아래로 교체:

```cpp
ApplySkillHookAtoms(ctx, gameData, skillStage);
ctx.pCommand = &cmd;
```

`visualCtx` 이름을 쓰는 블록에는 동일하게 아래 코드를 사용:

```cpp
ApplySkillHookAtoms(visualCtx, gameData, skillStage);
visualCtx.pCommand = &cmd;
```

기존 코드:

```cpp
    BeginActiveSkillRuntime(cmd, gameData, visualData, legacyDef, skillStage);
```

아래로 교체:

```cpp
    BeginActiveSkillRuntime(cmd, gameData, visualData, skillStage);
```

1-12. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Feedback/GameplayFeedback.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Definitions/SkillDefGameDataAdapter.h"
#include "Shared/GameSim/Feedback/GameplayFeedback.h"
```

`ResolveGameAtomsForVisualHook` 바로 위에 추가:

```cpp
    eTargetMode ToLegacyTargetModeForVisualHook(eTargetShape shape)
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
```

기존 코드:

```cpp
    const SkillDef* FindSkillDefForVisualHook(
        CWorld& world,
        EntityID caster,
        eChampion hookChampion,
        u8_t slot)
    {
        const eChampion champion = hookChampion != eChampion::NONE
            ? hookChampion
            : ResolveChampionForVisualHook(world, caster);
        if (champion == eChampion::NONE || champion == eChampion::END)
            return nullptr;

        const SkillDef* def = CSkillRegistry::Instance().Find(champion, slot);
        if (!def)
            def = FindSkillDef(champion, slot);
        return def;
    }
```

아래로 교체:

```cpp
    bool_t ResolveGameAtomsForVisualHook(
        CWorld& world,
        EntityID caster,
        eChampion hookChampion,
        u8_t slot,
        SkillGameAtomBundle& outData)
    {
        const eChampion champion = hookChampion != eChampion::NONE
            ? hookChampion
            : ResolveChampionForVisualHook(world, caster);
        if (champion == eChampion::NONE || champion == eChampion::END)
            return false;

        if (CSkillRegistry::Instance().ResolveGameAtoms(champion, slot, outData))
            return true;

        const SkillDef* pLegacyDef = FindSkillDef(champion, slot);
        if (!pLegacyDef)
            return false;

        outData = SkillDefAdapters::BuildSkillGameAtomBundle(*pLegacyDef);
        return true;
    }
```

기존 코드:

```cpp
        const SkillDef* pDef = FindSkillDefForVisualHook(
            world,
            source,
            hookChampion,
            slot);
        if (pDef)
        {
            command.resolvedTargetMode = static_cast<u8_t>(
                (skillStage >= 2u && pDef->stageCount >= 2)
                    ? pDef->stage2TargetMode
                    : pDef->targetMode);
        }

        VisualHookContext ctx{};
        ctx.pWorld = &world;
        ctx.casterEntity = source;
        ctx.pDef = pDef;
        ctx.pCommand = &command;
        ctx.skillStage = skillStage;
```

아래로 교체:

```cpp
        SkillGameAtomBundle gameData{};
        const bool_t bHasGameData = ResolveGameAtomsForVisualHook(
            world,
            source,
            hookChampion,
            slot,
            gameData);
        if (bHasGameData)
        {
            const u8_t stageIndex = skillStage >= 2u ? 1u : 0u;
            command.resolvedTargetMode = static_cast<u8_t>(
                ToLegacyTargetModeForVisualHook(gameData.target.shape[stageIndex]));
        }

        VisualHookContext ctx{};
        ctx.pWorld = &world;
        ctx.casterEntity = source;
        ctx.skillSlot = slot;
        ctx.skillStage = skillStage;
        if (bHasGameData)
        {
            const u8_t stageIndex = skillStage >= 2u ? 1u : 0u;
            ctx.fSkillLockDurationSec = gameData.stage.lockDurationSec[stageIndex];
            ctx.fSkillStageWindowSec = gameData.stage.stageWindowSec;
            ctx.fSkillRangeMax = gameData.range.rangeMax;
        }
        ctx.pCommand = &command;
```

Irelia debug trace의 기존 코드를:

```cpp
                    "[IreliaReplayCue][Client] slot=%u stage=%u effect=0x%08X source=%u target=%u def=%u visual=%u pos=(%.2f,%.2f,%.2f)\n",
```

아래로 교체:

```cpp
                    "[IreliaReplayCue][Client] slot=%u stage=%u effect=0x%08X source=%u target=%u atoms=%u visual=%u pos=(%.2f,%.2f,%.2f)\n",
```

기존 코드:

```cpp
                    pDef ? 1u : 0u,
```

아래로 교체:

```cpp
                    bHasGameData ? 1u : 0u,
```

1-13. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/Champion/Garen/Garen_Registration.cpp

기존 코드:

```cpp
        skillCtx.pDef = visualCtx.pDef;
        skillCtx.pCommand = visualCtx.pCommand;
        skillCtx.skillStage = visualCtx.skillStage;
```

아래로 교체:

```cpp
        skillCtx.skillSlot = visualCtx.skillSlot;
        skillCtx.skillStage = visualCtx.skillStage;
        skillCtx.fSkillLockDurationSec = visualCtx.fSkillLockDurationSec;
        skillCtx.fSkillStageWindowSec = visualCtx.fSkillStageWindowSec;
        skillCtx.pCommand = visualCtx.pCommand;
```

1-14. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/Champion/Kalista/Kalista_Registration.cpp

기존 코드:

```cpp
        skillCtx.pDef = visualCtx.pDef;
        skillCtx.pCommand = visualCtx.pCommand;
        skillCtx.skillStage = visualCtx.skillStage;
```

아래로 교체:

```cpp
        skillCtx.skillSlot = visualCtx.skillSlot;
        skillCtx.skillStage = visualCtx.skillStage;
        skillCtx.fSkillLockDurationSec = visualCtx.fSkillLockDurationSec;
        skillCtx.fSkillStageWindowSec = visualCtx.fSkillStageWindowSec;
        skillCtx.pCommand = visualCtx.pCommand;
```

1-15. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/Champion/Zed/Zed_Registration.cpp

기존 코드:

```cpp
        skillCtx.pDef = visualCtx.pDef;
        skillCtx.pCommand = visualCtx.pCommand;
        skillCtx.skillStage = visualCtx.skillStage;
```

아래로 교체:

```cpp
        skillCtx.skillSlot = visualCtx.skillSlot;
        skillCtx.skillStage = visualCtx.skillStage;
        skillCtx.fSkillLockDurationSec = visualCtx.fSkillLockDurationSec;
        skillCtx.fSkillStageWindowSec = visualCtx.fSkillStageWindowSec;
        skillCtx.pCommand = visualCtx.pCommand;
```

1-16. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/Champion/Garen/Garen_Skills.cpp

기존 코드:

```cpp
        if (!ctx.pWorld || !ctx.pDef)
            return;

        const u8_t slot = ctx.pDef->slot;
```

아래로 교체:

```cpp
        if (!ctx.pWorld)
            return;

        const u8_t slot = ctx.skillSlot;
```

1-17. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/Champion/Kalista/Kalista_Skills.cpp

기존 코드:

```cpp
        if (!ctx.pDef)
            return;
        if (ctx.pDef->slot != 0 && ctx.pDef->slot != 1)
            return;
```

아래로 교체:

```cpp
        if (ctx.skillSlot != 0 && ctx.skillSlot != 1)
            return;
```

기존 코드:

```cpp
        const i32_t slot = ctx.pDef->slot;
```

아래로 교체:

```cpp
        const i32_t slot = ctx.skillSlot;
```

1-18. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/Champion/Riven/Riven_Skills.cpp

기존 코드:

```cpp
        if (!ctx.pWorld || !ctx.pDef)
            return;
```

아래로 교체:

```cpp
        if (!ctx.pWorld)
            return;
```

이 파일의 두 곳에서 기존 코드를:

```cpp
        const u8_t slot = ctx.pDef->slot;
```

아래로 교체:

```cpp
        const u8_t slot = ctx.skillSlot;
```

1-19. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp

기존 코드:

```cpp
            const f32_t duration = ctx.pDef ? ctx.pDef->lockDurationSec : 0.3f;
```

아래로 교체:

```cpp
            const f32_t duration =
                ctx.fSkillLockDurationSec > 0.f ? ctx.fSkillLockDurationSec : 0.3f;
```

기존 코드:

```cpp
        const f32_t lifetime =
            (ctx.pDef && ctx.pDef->stageWindowSec > 0.f) ? ctx.pDef->stageWindowSec + 0.5f : 4.5f;
```

아래로 교체:

```cpp
        const f32_t lifetime =
            ctx.fSkillStageWindowSec > 0.f ? ctx.fSkillStageWindowSec + 0.5f : 4.5f;
```

기존 코드:

```cpp
            skillCtx.pDef = ctx.pDef;
            skillCtx.pCommand = ctx.pCommand;
            skillCtx.skillStage = ctx.skillStage;
```

아래로 교체:

```cpp
            skillCtx.skillSlot = ctx.skillSlot;
            skillCtx.skillStage = ctx.skillStage;
            skillCtx.fSkillLockDurationSec = ctx.fSkillLockDurationSec;
            skillCtx.fSkillStageWindowSec = ctx.fSkillStageWindowSec;
            skillCtx.pCommand = ctx.pCommand;
```

1-20. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/Champion/Zed/Zed_Skills.cpp

기존 코드:

```cpp
        if (!ctx.pWorld || !ctx.pDef)
            return;

        const u8_t slot = ctx.pDef->slot;
```

아래로 교체:

```cpp
        if (!ctx.pWorld)
            return;

        const u8_t slot = ctx.skillSlot;
```

기존 코드:

```cpp
            const f32_t shadowDuration = ctx.pDef->stageWindowSec > 0.f
                ? ctx.pDef->stageWindowSec
                : 5.f;
```

아래로 교체:

```cpp
            const f32_t shadowDuration = ctx.fSkillStageWindowSec > 0.f
                ? ctx.fSkillStageWindowSec
                : 5.f;
```

1-21. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/Champion/Sylas/SylasSkills.cpp

기존 코드:

```cpp
					const f32_t fRange = (ctx.pDef && ctx.pDef->rangeMax > 0.f)
						? ctx.pDef->rangeMax
						: 6.f;
```

아래로 교체:

```cpp
					const f32_t fRange = ctx.fSkillRangeMax > 0.f
						? ctx.fSkillRangeMax
						: 6.f;
```

1-22. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/SkillDefGameDataAdapter.h

`BuildSkillGameAtomBundle` 안의

기존 코드:

```cpp
        data.stage.lockDurationSec[0] = def.lockDurationSec;
        data.stage.lockDurationSec[1] = def.stage2LockSec;
```

아래로 교체:

```cpp
        data.stage.lockDurationSec[0] = def.lockDurationSec;
        data.stage.lockDurationSec[1] = def.stage2LockSec > 0.f
            ? def.stage2LockSec
            : def.lockDurationSec;
```

1-23. C:/Users/tnest/Desktop/Winters/Client/Public/GameObject/SkillDefVisualDataAdapter.h

`BuildSkillVisualData` 안의

기존 코드:

```cpp
        stage2.animationKey = def.stage2AnimKey;
        stage2.playbackSpeed = def.stage2PlaySpeed;
```

아래로 교체:

```cpp
        stage2.animationKey = def.stage2AnimKey
            ? def.stage2AnimKey
            : def.animKey;
        stage2.playbackSpeed = def.stage2PlaySpeed > 0.f
            ? def.stage2PlaySpeed
            : def.animPlaySpeed;
```

2. 검증

미검증:
- 계획서 작성 단계이므로 코드 반영, 빌드, 런타임 검증 미수행.

검증 명령:
- `rg -n "\.pDef|legacyHookBridge|BuildLegacyHookBridge" Client/Public/GamePlay/SkillHookContext.h Client/Public/GamePlay/VisualHookRegistry.h Shared/GameSim/Systems/GameplayHookRegistry Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp Client/Private/Scene/Scene_InGame.cpp Client/Private/Network/Client/EventApplier.cpp Client/Private/GameObject/Champion -S`
- `rg -n "GameObject/SkillVisualData|GameObject/ChampionVisualData|Client/Public|Client/" Shared/GameSim Server -S --glob "!**/Bin/**" --glob "!**/Generated/**"`
- `git diff --check`
- `Shared/Schemas/run_codegen.bat`
- `"C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Winters.sln /p:Configuration=Debug /p:Platform=x64 /m:1`
- `Tools/Bin/Debug/SimLab.exe 300 42`

수동 확인:
- 로컬 연습 경로에서 Jax E 1단계 반복 애니메이션과 2단계 종료가 기존과 동일한지 확인.
- Kalista 기본 공격/Q recovery에서 passive dash가 기존 slot 판정과 동일하게 발생하는지 확인.
- Irelia Q dash duration과 W hold lifetime이 원자 timing 값으로 동일하게 유지되는지 확인.
- Zed W shadow lifetime과 Sylas E2 fallback range가 기존 값과 동일한지 확인.
- 서버 effect cue가 client `VisualHookContext`에 slot/stage/range/timing을 채운 뒤 한 번만 재생되는지 확인.
