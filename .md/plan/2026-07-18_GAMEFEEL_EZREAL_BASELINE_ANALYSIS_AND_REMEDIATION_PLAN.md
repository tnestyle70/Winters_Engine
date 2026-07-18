# 2026-07-18 이즈리얼 기준 전 챔피언 조작감 정합화 계획서

```text
Session - 이즈리얼 조작감 우위의 원인을 서버 타격·입력 락·클라이언트 연출 시계로 분해하고 전 챔피언 공통 경로를 정합화한다.
좌표: 신규 좌표 후보 · 축: C5 이산화와 오차 · C7 권위와 정합성 · C8 검증이 병목
관련: 2026-07-18_SUSTAINED_BASIC_ATTACK_CHASE_RESULT.md, 2026-07-18_IRELIA_VIEGO_EZREAL_STAGE_INPUT_REGRESSION_RECOVERY_RESULT.md
```

## 0. Codex/Claude 교차 분석 결론

두 분석은 이즈리얼의 우위가 애니메이션 블렌딩이나 네트워크 예측 때문이 아니라는 데 일치한다. 네트워크 모드의 스킬 애니메이션은 서버 `ActionStart` 뒤 시작하고, 평타 윈드업은 현재 전 챔피언 공통 `lockDurationSec * 0.35`이며, 클라이언트 액션 종료는 `min(lockDurationSec, clipDurationSec)`만 사용한다.

Codex가 전수 계산한 추가 근거는 다음과 같다.

- 이즈리얼 BA: 서버 윈드업 약 227ms, 저작 cast frame 약 200ms로 오차가 약 27ms다.
- 가렌/제드/리븐/피오라/잭스 BA: 서버 350ms, 저작 cast frame 200ms로 오차가 150ms다.
- 81개 비-BA stage 중 57개는 `lockDurationSec - commandLockSec > 200ms`, 38개는 시각 recovery와 lock 오차가 200ms를 넘는다.
- 공격 수락 지형 세그먼트 면제는 `CommandExecutor`에서 이즈리얼만, impact에서는 애쉬/이즈리얼/칼리스타만 하드코딩돼 시작·타격 판정이 서로 다르다.

Claude 초안의 `lockDurationSec` 일괄 단축은 평타 서버 impact 시점과 액션 길이를 동시에 바꾸고 아이덴티티 채널까지 건드릴 수 있다. 본 계획은 세 시계를 분리한다: 서버 BA impact는 새 저작값, 서버 command lock은 기존값, 클라이언트 복귀는 visual recovery marker를 사용한다.

## 1. 결정 기록

```text
① 문제·제약: BA 저작 타격 프레임과 서버 35% 윈드업이 최대 150ms 어긋나고, 스킬 recovery marker가 네트워크 복귀에 소비되지 않으며, 원거리 세그먼트 정책이 챔피언 하드코딩으로 분기된다. 서버 권위와 채널/차지 아이덴티티는 유지해야 한다.
② 순진한 해법의 실패: 모든 lockDurationSec를 0.25~0.65초로 줄이면 impact/cancel window/액션 수명이 함께 변하고, Garen E·Irelia W·Zed R 같은 의도된 지속 행동을 절단한다. 전 챔피언 switch도 두 번째 데이터 권위가 된다.
③ 메커니즘: champion stats에 `basicAttackWindupSec`를 17종 모두 명시해 BA impact만 시각 cast marker에 맞춘다. 네트워크 presentation은 `max(commandLock, recoveryMarker)`를 authored lock으로 clamp한다. 원거리 여부는 현재 StatComponent의 attackRange로 한 곳에서 판정한다.
④ 대조: 이즈리얼 기준은 BA impact 200ms, Q/W/E 짧은 command lock, recovery와 lock 근접이다. 변경 후 긴 BA 액션/쿨다운은 유지하면서 impact만 133~200ms로 정렬하고, 일반 스킬은 server unlock 전에는 끝나지 않되 불필요한 시각 backswing만 줄인다.
⑤ 대가·예산: 30% 천장 일은 17챔피언 교차 체험표·캡처와 수치 리포트, 70% 바닥 일은 데이터/공통 경로/자동 회귀다. 신규 약한 예측·스킬 입력 버퍼는 command ACK/reconcile 설계가 필요하므로 이번 slice에서 만들지 않는다.
```

## 2. 반영 코드

### 2-1. `AGENTS.md` — PLAN/RESULT 세션 계약

기존 `## Plan Document Placement` 아래에 추가한다.

```md
- Every session that authors, resumes, or applies a dated implementation plan must read and create or update the corresponding `*_PLAN.md` before source edits. Once implementation starts, the same session must create or update the same-name `*_RESULT.md` before handoff; chat-only plans and implementation-only handoffs are not complete.
- Resume the existing plan/result pair for the same slice instead of creating a duplicate, and follow `.md/계획서작성규칙.md` for both files. `2026-07-18_IRELIA_E_UNIT_HIT_MARK_Q_RESET_RECALL_6S_PLAN.md` is the reference example for this mandatory session contract.
```

### 2-2. `Shared/GameSim/Definitions/ChampionStatsDef.h`

기존 코드:

```cpp
    f32_t attackSpeedPerLevel = 0.025f;
    f32_t baseAttackRange = 5.5f;
```

아래로 교체:

```cpp
    f32_t attackSpeedPerLevel = 0.025f;
    f32_t basicAttackWindupSec = 0.f;
    f32_t baseAttackRange = 5.5f;
```

### 2-3. `Shared/GameSim/Definitions/ChampionGameplayDef.h`

기존 코드:

```cpp
    f32_t attackSpeedPerLevel = 0.025f;
    f32_t baseAttackRange = 5.5f;
```

아래로 교체:

```cpp
    f32_t attackSpeedPerLevel = 0.025f;
    f32_t basicAttackWindupSec = 0.f;
    f32_t baseAttackRange = 5.5f;
```

### 2-4. `Tools/ChampionData/build_champion_game_data.py`

기존 `STAT_FIELDS` 코드:

```python
    "attackSpeedPerLevel": 0.025,
    "baseAttackRange": 5.5,
```

아래로 교체:

```python
    "attackSpeedPerLevel": 0.025,
    "basicAttackWindupSec": 0.0,
    "baseAttackRange": 5.5,
```

### 2-5. `Data/Gameplay/ChampionGameData/champions.json`

각 챔피언 `stats.attackSpeedPerLevel` 바로 아래에 다음 형식으로 추가한다.

```json
                "attackSpeedPerLevel": 0.025,
                "basicAttackWindupSec": 0.2,
                "baseAttackRange": 5.5,
```

저작값은 현재 `ChampionVisualDefs.json` BA `castFrame / 30 / animationPlaybackSpeed`이며 cast frame이 0인 Yasuo만 기존 35%를 보존한다.

```text
IRELIA 0.160000 · YASUO 0.175000 · KALISTA 0.200000 · GAREN 0.200000
ZED 0.200000 · RIVEN 0.200000 · EZREAL 0.200000 · FIORA 0.200000
JAX 0.200000 · LEESIN 0.133333 · KINDRED 0.133333 · MASTERYI 0.133333
ANNIE 0.200000 · ASHE 0.166667 · VIEGO 0.200000 · YONE 0.196078
SYLAS 0.133333
```

### 2-5a. `Data/LoL/Schemas/champions.json.schema.json`

`stats.required`의 `attackSpeedPerLevel` 뒤와 `stats.properties`의 같은 필드 뒤에 `basicAttackWindupSec`를 필수 non-negative number로 추가한다.

```json
        "attackSpeedPerLevel", "basicAttackWindupSec",
        "baseAttackRange", "baseMoveSpeed", "navArriveRadius",
```

```json
        "attackSpeedPerLevel": {"type": "number"},
        "basicAttackWindupSec": {"$ref": "#/$defs/nonNegativeNumber"},
        "baseAttackRange": {"$ref": "#/$defs/nonNegativeNumber"},
```

### 2-6. `Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp`

기존 `GetDefaultChampionBasicAttackTiming`의 35% 계산 블록:

```cpp
    const f32_t rawWindup = timing.fActionDurationSec * 0.35f;
    const f32_t maxWindup = (std::max)(0.05f, timing.fActionDurationSec - 0.03f);
    timing.fWindupSec = std::clamp(rawWindup, 0.12f, maxWindup);
```

아래로 교체:

```cpp
    const ChampionStatsDef stats = BuildDefaultChampionStatsDef(champion);
    const f32_t maxWindup = (std::max)(0.05f, timing.fActionDurationSec - 0.03f);
    if (std::isfinite(stats.basicAttackWindupSec) &&
        stats.basicAttackWindupSec > 0.f)
    {
        timing.fWindupSec = std::clamp(
            stats.basicAttackWindupSec,
            0.05f,
            maxWindup);
    }
    else
    {
        timing.fWindupSec = std::clamp(
            timing.fActionDurationSec * 0.35f,
            0.12f,
            maxWindup);
    }
```

### 2-7. `Server/Private/Data/RuntimeGameplayDefinitionOverlay.cpp`

`kChampionStatFields`에서 기존 코드:

```cpp
        { "attackSpeedPerLevel", &ChampionStatBlock::attackSpeedPerLevel },
        { "baseAttackRange", &ChampionStatBlock::baseAttackRange },
```

아래로 교체:

```cpp
        { "attackSpeedPerLevel", &ChampionStatBlock::attackSpeedPerLevel },
        { "basicAttackWindupSec", &ChampionStatBlock::basicAttackWindupSec },
        { "baseAttackRange", &ChampionStatBlock::baseAttackRange },
```

같은 배열 주석의 `전 20필드`는 `전 21필드`로 교체한다.

### 2-8. `Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h/.cpp`

헤더의 `IsAttackSegmentGateExemptTarget` 선언 바로 위에 추가:

```cpp
    bool_t ShouldApplyBasicAttackSegmentGate(CWorld& world, EntityID source);
```

CPP의 `IsAttackSegmentGateExemptTarget` 바로 위에 추가:

```cpp
    bool_t ShouldApplyBasicAttackSegmentGate(CWorld& world, EntityID source)
    {
        if (source == NULL_ENTITY || !world.HasComponent<StatComponent>(source))
            return true;

        constexpr f32_t kRangedAttackThreshold = 3.f;
        const f32_t attackRange = world.GetComponent<StatComponent>(source).attackRange;
        return !std::isfinite(attackRange) || attackRange < kRangedAttackThreshold;
    }
```

### 2-9. `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`

기존 코드:

```cpp
    if (tc.pWalkable && champion != eChampion::EZREAL &&
        !GameplayStateQuery::IsAttackSegmentGateExemptTarget(world, cmd.targetEntity))
```

아래로 교체:

```cpp
    if (tc.pWalkable &&
        GameplayStateQuery::ShouldApplyBasicAttackSegmentGate(world, cmd.issuerEntity) &&
        !GameplayStateQuery::IsAttackSegmentGateExemptTarget(world, cmd.targetEntity))
```

### 2-10. `Shared/GameSim/Systems/Combat/CombatActionSystem.cpp`

기존 코드:

```cpp
        const eChampion resolvedChampion = ResolveChampion(world, source);
        if (resolvedChampion != eChampion::ASHE &&
            resolvedChampion != eChampion::EZREAL &&
            resolvedChampion != eChampion::KALISTA &&
            tc.pWalkable &&
```

아래로 교체:

```cpp
        const eChampion resolvedChampion = ResolveChampion(world, source);
        if (tc.pWalkable &&
            GameplayStateQuery::ShouldApplyBasicAttackSegmentGate(world, source) &&
```

### 2-11. `Engine/Public/Renderer/ModelRenderer.h`와 `EngineSDK/inc/Renderer/ModelRenderer.h`

기존 코드:

```cpp
    f32_t GetAnimationDurationSecondsByName(const std::string& strKeyword) const;
```

아래로 교체:

```cpp
    f32_t GetAnimationDurationSecondsByName(const std::string& strKeyword) const;
    f32_t GetAnimationTimeSecondsByFrameByName(
        const std::string& strKeyword,
        f32_t fFrame) const;
```

### 2-12. `Engine/Private/Renderer/ModelRenderer.cpp`

`GetAnimationDurationSecondsByName` 바로 아래에 추가:

```cpp
f32_t ModelRenderer::GetAnimationTimeSecondsByFrameByName(
    const string& strKeyword,
    f32_t fFrame) const
{
    if (!m_pImpl || !m_pImpl->pSharedModel ||
        !std::isfinite(fFrame) || fFrame < 0.f)
    {
        return 0.f;
    }

    const i32_t idx = m_pImpl->pSharedModel->FindAnimationIndex(strKeyword);
    if (idx < 0)
        return 0.f;

    const auto* pAnim = m_pImpl->pSharedModel->GetAnimation((u32_t)idx);
    if (!pAnim || pAnim->GetTicksPerSecond() <= 0.0)
        return 0.f;

    const f64_t frame = (std::min)(
        static_cast<f64_t>(fFrame),
        pAnim->GetDuration());
    return static_cast<f32_t>(frame / pAnim->GetTicksPerSecond());
}
```

### 2-13. `Client/Private/Scene/Scene_InGameNetwork.cpp`

`ResolveNetworkActionPlaySpeed` 아래에 visual recovery와 command lock을 읽는 helper를 추가하고, `ResolveNetworkActionDurationSec`의 마지막 clip-duration 반환을 아래 정책으로 교체한다.

```cpp
    f32_t ResolveNetworkActionRecoveryFrame(
        eChampion champion,
        const SkillDef* pDef,
        u16_t actionId,
        u8_t stage)
    {
        const u8_t slot = NetworkActionToSkillSlot(actionId);
        const u8_t stageIndex = stage > 0u ? static_cast<u8_t>(stage - 1u) : 0u;
        if (const ClientData::ChampionVisualDefinition* pVisual =
            ClientData::FindChampionVisualDefinition(champion))
        {
            if (slot < ClientData::kVisualSkillSlotCount &&
                stageIndex < pVisual->skills[slot].stageCount &&
                stageIndex < ClientData::kVisualSkillStageCount)
            {
                return pVisual->skills[slot].stages[stageIndex].recoveryFrame;
            }
        }

        if (!pDef)
            return 0.f;
        return stage >= 2u
            ? pDef->stage2VisualRecoveryFrame
            : pDef->visualRecoveryFrame;
    }

    f32_t ResolveNetworkActionCommandLockSec(const SkillDef* pDef, u8_t stage)
    {
        if (!pDef)
            return 0.f;
        const f32_t seconds = stage >= 2u
            ? pDef->stage2CommandLockSec
            : pDef->commandLockSec;
        return std::isfinite(seconds) && seconds > 0.f ? seconds : 0.f;
    }
```

기존 `ResolveNetworkActionDurationSec`의 `bLoopAction`은 `SkillDef`의 stage별 presentation loop 값으로 교체한다.

```cpp
        const bool_t bLoopAction = pDef && (stage >= 2u
            ? pDef->bStage2PresentationLoopWhileActive
            : pDef->bPresentationLoopWhileActive);
```

기존 마지막 코드:

```cpp
        return (std::min)(scaledLockDurationSec, visualDurationSec);
```

아래로 교체:

```cpp
        const f32_t recoveryFrame = ResolveNetworkActionRecoveryFrame(
            champion, pDef, actionId, stage);
        const f32_t recoveryDurationSec =
            render.pRenderer->GetAnimationTimeSecondsByFrameByName(
                animName, recoveryFrame) / effectivePlaySpeed;
        if (!std::isfinite(recoveryDurationSec) || recoveryDurationSec <= 0.01f)
            return (std::min)(scaledLockDurationSec, visualDurationSec);

        const f32_t commandLockDurationSec =
            ResolveNetworkActionCommandLockSec(pDef, stage) / fAttackSpeedScale;
        return std::clamp(
            (std::max)(commandLockDurationSec, recoveryDurationSec),
            0.01f,
            scaledLockDurationSec);
```

### 2-14. `Tools/SimLab/main.cpp`

`RunAttackSpeedLabMatrixProbe` 위에 `RunBasicAttackGameFeelContractProbe`를 추가한다. 17개 expected windup을 exact table로 검사하고, `FlatWalkable.bSegmentWalkable=false`에서 Irelia는 segment gate 적용, Annie/Ezreal은 면제되는 것을 검사한다. 기존 main probe 목록에 bool을 추가하고 최종 `bPass`에 포함한다.

```cpp
    struct BasicAttackWindupExpectation
    {
        eChampion champion;
        f32_t seconds;
    };

    static constexpr BasicAttackWindupExpectation kExpected[] =
    {
        { eChampion::IRELIA, 0.16f },
        { eChampion::YASUO, 0.175f },
        { eChampion::KALISTA, 0.2f },
        { eChampion::GAREN, 0.2f },
        { eChampion::ZED, 0.2f },
        { eChampion::RIVEN, 0.2f },
        { eChampion::EZREAL, 0.2f },
        { eChampion::FIORA, 0.2f },
        { eChampion::JAX, 0.2f },
        { eChampion::LEESIN, 0.133333f },
        { eChampion::KINDRED, 0.133333f },
        { eChampion::MASTERYI, 0.133333f },
        { eChampion::ANNIE, 0.2f },
        { eChampion::ASHE, 0.166667f },
        { eChampion::VIEGO, 0.2f },
        { eChampion::YONE, 0.196078f },
        { eChampion::SYLAS, 0.133333f },
    };
```

### 2-15. 생성 파일

아래 파일은 직접 편집하지 않고 생성기로 갱신한다.

```text
Shared/GameSim/Generated/ChampionGameData.generated.cpp
Client/Private/Data/Generated/LoLVisualDefinitions.generated.cpp (변경 예상 없음)
Data/LoL/ServerPrivate/Gameplay/ChampionGameplayDefs.json
Data/LoL/ServerPrivate/Gameplay/SkillGameplayDefs.json
Server/Private/Data/Generated/LoLGameplayDefinitions.generated.cpp
Data/LoL/SharedContract/DefinitionManifest.json
```

## 3. 검증

```text
예측:
- 17챔피언 BA 저작 윈드업 exact table이 PASS하고, 기존 action duration/cooldown은 변하지 않는다.
- Garen/Zed/Riven/Fiora/Jax BA 서버 impact는 350ms -> 200ms, LeeSin/Kindred/MasterYi/Sylas는 210ms -> 133ms로 이동한다.
- 일반 non-loop skill은 command lock 전에는 복귀하지 않고 recovery marker 이후에는 idle/run으로 전환한다. 이즈리얼 Q/W/E는 authored lock 0.25초 clamp로 기존 체감을 유지한다.
- 원거리 5종(Annie/Ashe/Ezreal/Kindred/Kalista)은 동일한 segment 정책을 사용하고, 근접/중거리 12종은 기존 gate를 유지한다.

검증 명령:
python Tools/ChampionData/build_champion_game_data.py
python Tools/ChampionData/build_champion_game_data.py --check
python Tools/LoLData/Build-LoLDefinitionPack.py
python Tools/LoLData/Build-LoLDefinitionPack.py --check
msbuild Engine/Include/Engine.vcxproj Debug x64 /m:1
msbuild Server/Include/Server.vcxproj Debug x64 /m:1
msbuild Client/Include/Client.vcxproj Debug x64 /m:1
msbuild Tools/SimLab/SimLab.vcxproj Debug x64 /m:1
Tools/Bin/Debug/SimLab.exe --attack-speed-lab-only (지원 여부 확인 후, 없으면 full)
git diff --check -- <본 계획의 코드/데이터/문서 파일>

미검증/CONFIRM_NEEDED:
- 실제 FBX의 ticks-per-second가 visual frame 저작 기준 30과 다른 자산은 인게임 capture에서 다시 맞춰야 한다.
- 네트워크 RTT를 숨기는 약한 action prediction과 hard-block 중 skill input buffer는 이번 slice 밖이다.
- 최종 완료 선언 전 각 챔피언 BA 3회 + Q/W/E + 이동 cancel의 정상 F5 교차 체험이 필요하다.
```
