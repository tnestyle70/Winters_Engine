Session - Champion AI Debug Draw, Blackboard, HFSM, runtime tuning completion

1. 반영해야 하는 코드

이번 세션의 기준 흐름은 `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual`이다. 이미 1차로 서버 권한 `F Flash`, Jax `ExecuteDive`, `UseFlashEscape`, AI debug snapshot, 인게임 AI text/range draw가 들어갔으므로 이 계획은 그 위에 남은 "현업식 디버깅/튜닝 완성"만 다룬다.

목표는 네 가지다.

- 서버 AI blackboard에 현재 판단 근거, 차단 사유, HFSM 전이, 최근 decision trace를 남긴다.
- 클라이언트 AI Debug 패널에서 런타임 튜닝 값을 서버에 안전하게 보낸다.
- Snapshot으로 현재 튜닝값/차단 사유/trace를 내려서 인게임에서 재현 가능한 상태로 본다.
- Jax dive가 실패했을 때 "왜 실패했는지"를 바로 볼 수 있게 만든다.

새 `.h/.cpp` 파일은 추가하지 않는다. 따라서 `.vcxproj` / `.filters` 변경은 없다.

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h

`enum class eChampionAIDivePhase : u8_t` 바로 아래에 아래 코드를 추가:

```cpp
enum class eChampionAIDecisionBlockReason : u8_t
{
	None,
	NoTarget,
	TargetDead,
	TargetUntargetable,
	TargetOutOfRange,
	SelfLowHp,
	TurretDanger,
	SkillCooldown,
	FlashNotReady,
	ActionLocked,
	StateBlocked,
	InvalidPath,
	CommandRejected,
};

enum class eChampionAITuningId : u8_t
{
	ChampionScanRange,
	MinionScanRange,
	StructureScanRange,
	LeashRange,
	RetreatHpRatio,
	ReengageHpRatio,
	ChampionScoreMargin,
	TurretDangerThreshold,
	PostComboBASelfHpMinRatio,
	PostComboBAEnemyHpMargin,
	PostComboBAWindow,
	LowHpExecuteThreshold,
	DiveScanRange,
	DiveExtraBAWindow,
	Count,
};

struct ChampionAIDecisionTraceEntry
{
	u64_t tick = 0;
	eChampionAIState state = eChampionAIState::MoveToOuterTurret;
	eChampionAIIntent intent = eChampionAIIntent::FarmMinion;
	eChampionAIAction action = eChampionAIAction::MoveToSafeAnchor;
	eChampionAIDivePhase divePhase = eChampionAIDivePhase::None;
	eChampionAIDecisionBlockReason blockReason = eChampionAIDecisionBlockReason::None;
	u8_t commandKind = 0;
	u8_t commandSlot = 0;
	EntityID target = NULL_ENTITY;
	Vec3 commandPos{ 0.f, 0.f, 0.f };
	f32_t championScore = 0.f;
	f32_t farmScore = 0.f;
	f32_t structureScore = 0.f;
	f32_t selfHpRatio = 1.f;
	f32_t enemyHpRatio = 1.f;
	f32_t enemyDistance = 999.f;
	f32_t turretDanger = 0.f;
};
```

기존 코드:

```cpp
inline constexpr u16_t kChampionAIDebugClearOverrideItemId = 0xFFFFu;
inline constexpr u8_t kChampionAIDebugForceActionSkillSlot = 0xFFu;
```

아래로 교체:

```cpp
inline constexpr u16_t kChampionAIDebugClearOverrideItemId = 0xFFFFu;
inline constexpr u16_t kChampionAIDebugTuneRuntimeItemId = 0xFFFEu;
inline constexpr u16_t kChampionAIDebugResetTuningItemId = 0xFFFDu;
inline constexpr u8_t kChampionAIDebugForceActionSkillSlot = 0xFFu;
inline constexpr u8_t kChampionAIDebugTraceCapacity = 16u;
```

`struct ChampionAIComponent` 안에서 기존 코드:

```cpp
	u8_t debugLastCommandKind = 0;
	u8_t debugLastCommandSlot = 0;
	EntityID debugLastCommandTarget = NULL_ENTITY;
	Vec3 debugLastCommandPos{};
	u32_t nextCommandSequence = 1;
```

아래로 교체:

```cpp
	u8_t debugLastCommandKind = 0;
	u8_t debugLastCommandSlot = 0;
	EntityID debugLastCommandTarget = NULL_ENTITY;
	Vec3 debugLastCommandPos{};
	eChampionAIDecisionBlockReason debugLastBlockReason = eChampionAIDecisionBlockReason::None;
	ChampionAIDecisionTraceEntry debugDecisionTrace[kChampionAIDebugTraceCapacity] = {};
	u8_t debugDecisionTraceHead = 0u;
	u8_t debugDecisionTraceCount = 0u;
	u32_t nextCommandSequence = 1;
```

`struct ChampionAIDebugComponent` 안에서 기존 코드:

```cpp
	u8_t lastCommandKind = 0;
	u8_t lastCommandSlot = 0;
	eChampionAIDivePhase divePhase = eChampionAIDivePhase::None;
	u32_t availableActionMask = 0;
```

아래로 교체:

```cpp
	u8_t lastCommandKind = 0;
	u8_t lastCommandSlot = 0;
	eChampionAIDivePhase divePhase = eChampionAIDivePhase::None;
	eChampionAIDecisionBlockReason lastBlockReason = eChampionAIDecisionBlockReason::None;
	u32_t availableActionMask = 0;
```

`struct ChampionAIDebugComponent` 안에서 기존 코드:

```cpp
	f32_t fChampionScanRange = 0.f;
	f32_t fDiveScanRange = 0.f;
	f32_t fFlashRange = 0.f;
	f32_t fPostComboBATimer = 0.f;
	f32_t moveSpeed = 0.f;
	Vec3 snapshotPos{ 0.f, 0.f, 0.f };
	Vec3 lastCommandPos{ 0.f, 0.f, 0.f };
```

아래로 교체:

```cpp
	f32_t fChampionScanRange = 0.f;
	f32_t fMinionScanRange = 0.f;
	f32_t fStructureScanRange = 0.f;
	f32_t fLeashRange = 0.f;
	f32_t fRetreatHpRatio = 0.f;
	f32_t fReengageHpRatio = 0.f;
	f32_t fChampionScoreMargin = 0.f;
	f32_t fTurretDangerThreshold = 0.f;
	f32_t fPostComboBASelfHpMinRatio = 0.f;
	f32_t fPostComboBAEnemyHpMargin = 0.f;
	f32_t fPostComboBAWindow = 0.f;
	f32_t fLowHpExecuteThreshold = 0.f;
	f32_t fDiveScanRange = 0.f;
	f32_t fDiveExtraBAWindow = 0.f;
	f32_t fFlashRange = 0.f;
	f32_t fPostComboBATimer = 0.f;
	f32_t moveSpeed = 0.f;
	Vec3 snapshotPos{ 0.f, 0.f, 0.f };
	Vec3 lastCommandPos{ 0.f, 0.f, 0.f };
```

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

`void SetChampionAIIntent(` 함수 바로 아래에 아래 코드를 추가:

```cpp
    void SetChampionAIState(
        ChampionAIComponent& ai,
        eChampionAIState state,
        eChampionAIDecisionBlockReason reason = eChampionAIDecisionBlockReason::None)
    {
        if (ai.state != state)
            ai.debugLastBlockReason = reason;
        ai.state = state;
    }

    void SetChampionAIBlockReason(
        ChampionAIComponent& ai,
        eChampionAIDecisionBlockReason reason)
    {
        ai.debugLastBlockReason = reason;
    }

    void PushChampionAIDecisionTrace(
        ChampionAIComponent& ai,
        const TickContext& tc,
        EntityID target)
    {
        ChampionAIDecisionTraceEntry entry{};
        entry.tick = tc.tickIndex;
        entry.state = ai.state;
        entry.intent = ai.intent;
        entry.action = ai.lastAction;
        entry.divePhase = ai.divePhase;
        entry.blockReason = ai.debugLastBlockReason;
        entry.commandKind = ai.debugLastCommandKind;
        entry.commandSlot = ai.debugLastCommandSlot;
        entry.target = target;
        entry.commandPos = ai.debugLastCommandPos;
        entry.championScore = ai.fChampionDecisionScore;
        entry.farmScore = ai.fFarmDecisionScore;
        entry.structureScore = ai.fStructureDecisionScore;
        entry.selfHpRatio = ai.fDecisionSelfHpRatio;
        entry.enemyHpRatio = ai.fDecisionEnemyHpRatio;
        entry.enemyDistance = ai.fDecisionEnemyDistance;
        entry.turretDanger = ai.fDecisionTurretDanger;

        ai.debugDecisionTrace[ai.debugDecisionTraceHead] = entry;
        ai.debugDecisionTraceHead =
            static_cast<u8_t>((ai.debugDecisionTraceHead + 1u) % kChampionAIDebugTraceCapacity);
        if (ai.debugDecisionTraceCount < kChampionAIDebugTraceCapacity)
            ++ai.debugDecisionTraceCount;
    }
```

`RecordChampionAICommandDebug` 기존 코드:

```cpp
    void RecordChampionAICommandDebug(
        ChampionAIComponent& ai,
        EntityID target,
        eCommandKind kind,
        u8_t slot,
        const Vec3& commandPos)
    {
        ai.debugLastCommandKind = static_cast<u8_t>(kind);
        ai.debugLastCommandSlot = slot;
        ai.debugLastCommandTarget = target;
        ai.debugLastCommandPos = commandPos;
    }
```

아래로 교체:

```cpp
    void RecordChampionAICommandDebug(
        ChampionAIComponent& ai,
        EntityID target,
        eCommandKind kind,
        u8_t slot,
        const Vec3& commandPos)
    {
        ai.debugLastCommandKind = static_cast<u8_t>(kind);
        ai.debugLastCommandSlot = slot;
        ai.debugLastCommandTarget = target;
        ai.debugLastCommandPos = commandPos;
        ai.debugLastBlockReason = eChampionAIDecisionBlockReason::None;
    }
```

`EmitMoveCommand`, `EmitBasicAttackCommand`, `EmitSkillCommand`, `EmitFlashCommand`, `EmitRecall` 안에서 `RecordChampionAICommandDebug(...)` 바로 아래에 각각 아래 코드를 추가:

```cpp
        PushChampionAIDecisionTrace(ai, tc, target);
```

대상이 없는 명령은 `target` 대신 `NULL_ENTITY`를 전달한다.

`EmitBasicAttackCommand` 기존 reject 조건:

```cpp
        if (!IsSkillReady(world, self, static_cast<u8_t>(eSkillSlot::BasicAttack)) ||
            target == NULL_ENTITY ||
            !IsAliveTarget(world, target) ||
            !GameplayStateQuery::CanBeTargetedBy(world, self, target))
        {
            return false;
        }
```

아래로 교체:

```cpp
        if (!IsSkillReady(world, self, static_cast<u8_t>(eSkillSlot::BasicAttack)))
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::SkillCooldown);
            return false;
        }
        if (target == NULL_ENTITY)
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::NoTarget);
            return false;
        }
        if (!IsAliveTarget(world, target))
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::TargetDead);
            return false;
        }
        if (!GameplayStateQuery::CanBeTargetedBy(world, self, target))
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::TargetUntargetable);
            return false;
        }
```

`EmitBasicAttackCommand` 기존 사거리 실패 코드:

```cpp
        if (attackRange > 0.f &&
            WintersMath::DistanceSqXZ(selfPos, targetPos) > attackRange * attackRange)
        {
            return false;
        }
```

아래로 교체:

```cpp
        if (attackRange > 0.f &&
            WintersMath::DistanceSqXZ(selfPos, targetPos) > attackRange * attackRange)
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::TargetOutOfRange);
            return false;
        }
```

`EmitSkillCommand`에도 같은 방식으로 `SkillCooldown`, `NoTarget`, `TargetDead`, `TargetUntargetable`, `TargetOutOfRange` block reason을 세팅한다.

`EmitFlashCommand` 기존 코드:

```cpp
        if (!IsFlashReady(world, self))
            return false;
```

아래로 교체:

```cpp
        if (!IsFlashReady(world, self))
        {
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::FlashNotReady);
            return false;
        }
```

`ApplyChampionAIProfileAndTuning` 기존 코드 끝부분:

```cpp
        ai.fDiveExtraBAWindow = ResolveChampionAITuningParam(
            ai.tuning.diveExtraBAWindow, 1.80f, bOverrideProfile);
```

아래에 추가:

```cpp
        ai.fDecisionChampionScanRange = ai.championScanRange;
        ai.fDecisionDiveScanRange = ai.fDiveScanRange;
```

`UpdateChampionAIDecisionEvidence` 기존 코드:

```cpp
        ai.fDecisionChampionScanRange = ai.championScanRange;
        ai.fDecisionDiveScanRange = ai.fDiveScanRange;
        ai.fDecisionFlashRange =
            ChampionGameDataDB::ResolveSummonerSpellRange(ChampionScoreComponent::kSummonerSpellFlash);
```

아래로 교체:

```cpp
        ai.fDecisionChampionScanRange = ai.championScanRange;
        ai.fDecisionDiveScanRange = ai.fDiveScanRange;
        ai.fDecisionFlashRange =
            ChampionGameDataDB::ResolveSummonerSpellRange(ChampionScoreComponent::kSummonerSpellFlash);
        if (ctx.enemyChampion == NULL_ENTITY && ctx.lowHpEnemyChampion == NULL_ENTITY)
            SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::NoTarget);
```

`TryExecuteJaxDive` 기존 시작 실패 코드:

```cpp
            if (ctx.lowHpEnemyChampion == NULL_ENTITY ||
                !IsFlashReady(world, self))
            {
                return false;
            }
```

아래로 교체:

```cpp
            if (ctx.lowHpEnemyChampion == NULL_ENTITY)
            {
                SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::NoTarget);
                return false;
            }
            if (!IsFlashReady(world, self))
            {
                SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::FlashNotReady);
                return false;
            }
```

`TryExecuteJaxDive` 기존 코드:

```cpp
        ai.state = eChampionAIState::Diving;
        SetChampionAIIntent(ai, eChampionAIIntent::ExecuteDive, true);
```

아래로 교체:

```cpp
        SetChampionAIState(ai, eChampionAIState::Diving);
        SetChampionAIIntent(ai, eChampionAIIntent::ExecuteDive, true);
```

`ExecuteLaneCombat` 기존 retreat gate:

```cpp
        if (ctx.selfHpRatio <= ai.retreatHpRatio ||
            (ctx.bInsideEnemyTurretDanger && ctx.enemyChampion != NULL_ENTITY) ||
            (ctx.turretDanger > ai.fTurretDangerThreshold && !ctx.bStructureWaveTanking))
```

아래로 교체:

```cpp
        const bool_t bSelfLowHp = ctx.selfHpRatio <= ai.retreatHpRatio;
        const bool_t bChampionInsideTurret =
            ctx.bInsideEnemyTurretDanger && ctx.enemyChampion != NULL_ENTITY;
        const bool_t bTooMuchTurretDanger =
            ctx.turretDanger > ai.fTurretDangerThreshold && !ctx.bStructureWaveTanking;
        if (bSelfLowHp || bChampionInsideTurret || bTooMuchTurretDanger)
```

그리고 해당 블록 안의 `EmitRetreat` 바로 위에 아래 코드를 추가:

```cpp
            SetChampionAIBlockReason(ai,
                bSelfLowHp
                    ? eChampionAIDecisionBlockReason::SelfLowHp
                    : eChampionAIDecisionBlockReason::TurretDanger);
```

`CChampionAISystem::Execute` 기존 코드:

```cpp
            if (ai.decisionTimer > 0.f && !bHasDebugOverride)
                return;

            if (IsChampionAIActionLocked(world, self, champion.id, tc))
                return;
```

아래로 교체:

```cpp
            if (ai.decisionTimer > 0.f && !bHasDebugOverride)
                return;

            if (IsChampionAIActionLocked(world, self, champion.id, tc))
            {
                SetChampionAIBlockReason(ai, eChampionAIDecisionBlockReason::ActionLocked);
                PushChampionAIDecisionTrace(ai, tc, ai.debugLastCommandTarget);
                return;
            }
```

1-3. C:/Users/tnest/Desktop/Winters/Client/Public/Network/Client/CommandSerializer.h

기존 코드:

```cpp
	void SendAIDebugControl(CClientNetwork& net, NetEntityId targetNet,
		eChampionAIAction action, u8_t skillSlot = 0);
	void SendAIDebugClear(CClientNetwork& net, NetEntityId targetNet);
	void SendLevelSkill(CClientNetwork& net, u8_t slot);
```

아래로 교체:

```cpp
	void SendAIDebugControl(CClientNetwork& net, NetEntityId targetNet,
		eChampionAIAction action, u8_t skillSlot = 0);
	void SendAIDebugTune(CClientNetwork& net, NetEntityId targetNet,
		u8_t tuningId, f32_t value);
	void SendAIDebugResetTuning(CClientNetwork& net, NetEntityId targetNet);
	void SendAIDebugClear(CClientNetwork& net, NetEntityId targetNet);
	void SendLevelSkill(CClientNetwork& net, u8_t slot);
```

1-4. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/CommandSerializer.cpp

`SendAIDebugControl` 함수 바로 아래에 아래 코드를 추가:

```cpp
void CCommandSerializer::SendAIDebugTune(CClientNetwork& net, NetEntityId targetNet,
    u8_t tuningId, f32_t value)
{
    if (targetNet == NULL_NET_ENTITY)
        return;

    GameCommandWire wire{};
    wire.kind = eCommandKind::AIDebugControl;
    wire.clientTick = m_clientTick++;
    wire.sequenceNum = m_nextSequenceNum++;
    wire.targetNet = targetNet;
    wire.slot = tuningId;
    wire.itemId = kChampionAIDebugTuneRuntimeItemId;
    wire.groundPos = Vec3{ value, 0.f, 0.f };

    SendSingle(net, wire);
}

void CCommandSerializer::SendAIDebugResetTuning(CClientNetwork& net, NetEntityId targetNet)
{
    if (targetNet == NULL_NET_ENTITY)
        return;

    GameCommandWire wire{};
    wire.kind = eCommandKind::AIDebugControl;
    wire.clientTick = m_clientTick++;
    wire.sequenceNum = m_nextSequenceNum++;
    wire.targetNet = targetNet;
    wire.itemId = kChampionAIDebugResetTuningItemId;

    SendSingle(net, wire);
}
```

1-5. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

`namespace` 내부의 `TryHandleViegoSoulBasicAttack` 함수 뒤, `CDefaultCommandExecutor::Create()` 앞에 아래 helper를 추가:

```cpp
    ChampionAITuningParam* ResolveChampionAITuningParamById(
        ChampionAIComponent& ai,
        eChampionAITuningId id)
    {
        switch (id)
        {
        case eChampionAITuningId::ChampionScanRange: return &ai.tuning.championScanRange;
        case eChampionAITuningId::MinionScanRange: return &ai.tuning.minionScanRange;
        case eChampionAITuningId::StructureScanRange: return &ai.tuning.structureScanRange;
        case eChampionAITuningId::LeashRange: return &ai.tuning.leashRange;
        case eChampionAITuningId::RetreatHpRatio: return &ai.tuning.retreatHpRatio;
        case eChampionAITuningId::ReengageHpRatio: return &ai.tuning.reengageHpRatio;
        case eChampionAITuningId::ChampionScoreMargin: return &ai.tuning.championScoreMargin;
        case eChampionAITuningId::TurretDangerThreshold: return &ai.tuning.turretDangerThreshold;
        case eChampionAITuningId::PostComboBASelfHpMinRatio: return &ai.tuning.postComboBASelfHpMinRatio;
        case eChampionAITuningId::PostComboBAEnemyHpMargin: return &ai.tuning.postComboBAEnemyHpMargin;
        case eChampionAITuningId::PostComboBAWindow: return &ai.tuning.postComboBAWindow;
        case eChampionAITuningId::LowHpExecuteThreshold: return &ai.tuning.lowHpExecuteThreshold;
        case eChampionAITuningId::DiveScanRange: return &ai.tuning.diveScanRange;
        case eChampionAITuningId::DiveExtraBAWindow: return &ai.tuning.diveExtraBAWindow;
        default: return nullptr;
        }
    }

    void ApplyChampionAITuningOverride(
        ChampionAIComponent& ai,
        eChampionAITuningId id,
        f32_t value)
    {
        ChampionAITuningParam* pParam = ResolveChampionAITuningParamById(ai, id);
        if (!pParam)
            return;

        pParam->fCurrent = std::clamp(value, pParam->fMin, pParam->fMax);
        pParam->bOverride = true;
        ai.tuning.bOverrideProfile = true;
    }
```

`HandleAIDebugControl` 기존 코드:

```cpp
    auto& ai = world.GetComponent<ChampionAIComponent>(cmd.targetEntity);
    if (cmd.itemId == kChampionAIDebugClearOverrideItemId)
```

아래로 교체:

```cpp
    auto& ai = world.GetComponent<ChampionAIComponent>(cmd.targetEntity);
    if (cmd.itemId == kChampionAIDebugTuneRuntimeItemId)
    {
        if (cmd.slot < static_cast<u8_t>(eChampionAITuningId::Count))
            ApplyChampionAITuningOverride(
                ai,
                static_cast<eChampionAITuningId>(cmd.slot),
                cmd.groundPos.x);
        ai.decisionTimer = 0.f;
        return;
    }

    if (cmd.itemId == kChampionAIDebugResetTuningItemId)
    {
        ai.tuning = ChampionAITuning{};
        ai.decisionTimer = 0.f;
        return;
    }

    if (cmd.itemId == kChampionAIDebugClearOverrideItemId)
```

1-6. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Snapshot.fbs

`table EntitySnapshot {` 위에 아래 table을 추가:

```fbs
table AIDebugTraceRow {
    tick:ulong;
    state:ubyte;
    intent:ubyte;
    action:ubyte;
    divePhase:ubyte;
    blockReason:ubyte;
    commandKind:ubyte;
    commandSlot:ubyte;
    targetNet:uint;
    posX:float;
    posY:float;
    posZ:float;
    championScore:float;
    farmScore:float;
    structureScore:float;
    selfHpRatio:float;
    enemyHpRatio:float;
    enemyDistance:float;
    turretDanger:float;
}
```

기존 코드:

```fbs
    aiDebugLastCommandPosX:float;
    aiDebugLastCommandPosY:float;
    aiDebugLastCommandPosZ:float;
}
```

아래로 교체:

```fbs
    aiDebugLastCommandPosX:float;
    aiDebugLastCommandPosY:float;
    aiDebugLastCommandPosZ:float;
    aiDebugLastBlockReason:ubyte;
    aiDebugMinionScanRange:float;
    aiDebugStructureScanRange:float;
    aiDebugLeashRange:float;
    aiDebugRetreatHpRatio:float;
    aiDebugReengageHpRatio:float;
    aiDebugChampionScoreMargin:float;
    aiDebugTurretDangerThreshold:float;
    aiDebugPostComboBASelfHpMinRatio:float;
    aiDebugPostComboBAEnemyHpMargin:float;
    aiDebugPostComboBAWindow:float;
    aiDebugLowHpExecuteThreshold:float;
    aiDebugDiveExtraBAWindow:float;
    aiDebugTrace:[AIDebugTraceRow];
}
```

1-7. C:/Users/tnest/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

`if (world.HasComponent<ChampionAIComponent>(entity))` 블록 안에서 기존 코드:

```cpp
            aiDebugLastCommandKind = ai.debugLastCommandKind;
            aiDebugLastCommandSlot = ai.debugLastCommandSlot;
            aiDebugDivePhase = static_cast<u8_t>(ai.divePhase);
```

아래로 교체:

```cpp
            aiDebugLastCommandKind = ai.debugLastCommandKind;
            aiDebugLastCommandSlot = ai.debugLastCommandSlot;
            aiDebugDivePhase = static_cast<u8_t>(ai.divePhase);
            aiDebugLastBlockReason = static_cast<u8_t>(ai.debugLastBlockReason);
            aiDebugMinionScanRange = ai.minionScanRange;
            aiDebugStructureScanRange = ai.structureScanRange;
            aiDebugLeashRange = ai.leashRange;
            aiDebugRetreatHpRatio = ai.retreatHpRatio;
            aiDebugReengageHpRatio = ai.reengageHpRatio;
            aiDebugChampionScoreMargin = ai.fChampionScoreMargin;
            aiDebugTurretDangerThreshold = ai.fTurretDangerThreshold;
            aiDebugPostComboBASelfHpMinRatio = ai.fPostComboBASelfHpMinRatio;
            aiDebugPostComboBAEnemyHpMargin = ai.fPostComboBAEnemyHpMargin;
            aiDebugPostComboBAWindow = ai.fPostComboBAWindow;
            aiDebugLowHpExecuteThreshold = ai.fLowHpExecuteThreshold;
            aiDebugDiveExtraBAWindow = ai.fDiveExtraBAWindow;
```

`CreateEntitySnapshot` 호출 직전의 vector offset 생성부에서 아래를 추가:

```cpp
        std::vector<flatbuffers::Offset<Shared::Schema::AIDebugTraceRow>> aiDebugTraceRows;
        if (world.HasComponent<ChampionAIComponent>(entity))
        {
            const auto& ai = world.GetComponent<ChampionAIComponent>(entity);
            aiDebugTraceRows.reserve(ai.debugDecisionTraceCount);
            for (u8_t i = 0; i < ai.debugDecisionTraceCount; ++i)
            {
                const u8_t index = static_cast<u8_t>(
                    (ai.debugDecisionTraceHead + kChampionAIDebugTraceCapacity -
                        ai.debugDecisionTraceCount + i) % kChampionAIDebugTraceCapacity);
                const auto& row = ai.debugDecisionTrace[index];
                aiDebugTraceRows.push_back(Shared::Schema::CreateAIDebugTraceRow(
                    fbb,
                    row.tick,
                    static_cast<u8_t>(row.state),
                    static_cast<u8_t>(row.intent),
                    static_cast<u8_t>(row.action),
                    static_cast<u8_t>(row.divePhase),
                    static_cast<u8_t>(row.blockReason),
                    row.commandKind,
                    row.commandSlot,
                    row.target != NULL_ENTITY ? entityMap.ToNet(row.target) : NULL_NET_ENTITY,
                    row.commandPos.x,
                    row.commandPos.y,
                    row.commandPos.z,
                    row.championScore,
                    row.farmScore,
                    row.structureScore,
                    row.selfHpRatio,
                    row.enemyHpRatio,
                    row.enemyDistance,
                    row.turretDanger));
            }
        }
        const auto aiDebugTraceOffset = fbb.CreateVector(aiDebugTraceRows);
```

`CreateEntitySnapshot` 인자 목록 끝에 Snapshot.fbs에 추가한 필드 순서대로 아래 인자를 추가:

```cpp
            aiDebugLastBlockReason,
            aiDebugMinionScanRange,
            aiDebugStructureScanRange,
            aiDebugLeashRange,
            aiDebugRetreatHpRatio,
            aiDebugReengageHpRatio,
            aiDebugChampionScoreMargin,
            aiDebugTurretDangerThreshold,
            aiDebugPostComboBASelfHpMinRatio,
            aiDebugPostComboBAEnemyHpMargin,
            aiDebugPostComboBAWindow,
            aiDebugLowHpExecuteThreshold,
            aiDebugDiveExtraBAWindow,
            aiDebugTraceOffset
```

1-8. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

`ChampionAIDebugComponent` 적용 블록에서 기존 코드:

```cpp
                debug.divePhase = static_cast<eChampionAIDivePhase>(es->aiDebugDivePhase());
                debug.bCanAttackChampion =
```

아래로 교체:

```cpp
                debug.divePhase = static_cast<eChampionAIDivePhase>(es->aiDebugDivePhase());
                debug.lastBlockReason =
                    static_cast<eChampionAIDecisionBlockReason>(es->aiDebugLastBlockReason());
                debug.fMinionScanRange = es->aiDebugMinionScanRange();
                debug.fStructureScanRange = es->aiDebugStructureScanRange();
                debug.fLeashRange = es->aiDebugLeashRange();
                debug.fRetreatHpRatio = es->aiDebugRetreatHpRatio();
                debug.fReengageHpRatio = es->aiDebugReengageHpRatio();
                debug.fChampionScoreMargin = es->aiDebugChampionScoreMargin();
                debug.fTurretDangerThreshold = es->aiDebugTurretDangerThreshold();
                debug.fPostComboBASelfHpMinRatio = es->aiDebugPostComboBASelfHpMinRatio();
                debug.fPostComboBAEnemyHpMargin = es->aiDebugPostComboBAEnemyHpMargin();
                debug.fPostComboBAWindow = es->aiDebugPostComboBAWindow();
                debug.fLowHpExecuteThreshold = es->aiDebugLowHpExecuteThreshold();
                debug.fDiveExtraBAWindow = es->aiDebugDiveExtraBAWindow();
                debug.bCanAttackChampion =
```

확인 필요:

- `ChampionAIDebugComponent`에 trace 배열을 클라이언트도 보관할지, AIDebugPanel에서 FlatBuffer row를 바로 쓰지 못하므로 컴포넌트에 `debugDecisionTrace`와 동일한 lightweight 배열을 추가하는 쪽이 좋다.
- 이 경우 `ChampionAIComponent.h`의 `ChampionAIDebugComponent`에도 `ChampionAIDecisionTraceEntry debugDecisionTrace[16]`, `debugDecisionTraceCount`를 추가하고 SnapshotApplier에서 net id는 `EntityID` 대신 `target`에 0으로 두거나 별도 `u32_t targetNet` 필드를 추가한다.

1-9. C:/Users/tnest/Desktop/Winters/Client/Private/UI/AIDebugPanel.cpp

`ChampionAICommandKindName` 함수 아래에 아래 helper를 추가:

```cpp
	const char* ChampionAIBlockReasonName(eChampionAIDecisionBlockReason reason)
	{
		switch (reason)
		{
		case eChampionAIDecisionBlockReason::None: return "None";
		case eChampionAIDecisionBlockReason::NoTarget: return "NoTarget";
		case eChampionAIDecisionBlockReason::TargetDead: return "TargetDead";
		case eChampionAIDecisionBlockReason::TargetUntargetable: return "Untargetable";
		case eChampionAIDecisionBlockReason::TargetOutOfRange: return "OutOfRange";
		case eChampionAIDecisionBlockReason::SelfLowHp: return "SelfLowHp";
		case eChampionAIDecisionBlockReason::TurretDanger: return "TurretDanger";
		case eChampionAIDecisionBlockReason::SkillCooldown: return "Cooldown";
		case eChampionAIDecisionBlockReason::FlashNotReady: return "FlashNotReady";
		case eChampionAIDecisionBlockReason::ActionLocked: return "ActionLocked";
		case eChampionAIDecisionBlockReason::StateBlocked: return "StateBlocked";
		case eChampionAIDecisionBlockReason::InvalidPath: return "InvalidPath";
		case eChampionAIDecisionBlockReason::CommandRejected: return "CommandRejected";
		default: return "Unknown";
		}
	}
```

`RenderForceActionButton` 함수 아래에 아래 helper를 추가:

```cpp
	void RenderTuningSlider(
		CScene_InGame* pScene,
		u32_t targetNetId,
		const char* pLabel,
		eChampionAITuningId tuningId,
		f32_t current,
		f32_t minValue,
		f32_t maxValue,
		const char* pFormat = "%.2f")
	{
		f32_t value = current;
		ImGui::PushID(static_cast<int>(tuningId));
		if (ImGui::SliderFloat(pLabel, &value, minValue, maxValue, pFormat) &&
			targetNetId != NULL_NET_ENTITY &&
			CanSendAIDebugCommand(pScene))
		{
			pScene->GetCommandSerializer()->SendAIDebugTune(
				*pScene->GetNetworkView(),
				targetNetId,
				static_cast<u8_t>(tuningId),
				value);
		}
		ImGui::PopID();
	}
```

기존 코드:

```cpp
		ImGui::Text("Dive: phase=%s target=%u   lastCmd=%s slot=%u target=%u pos=(%.1f, %.1f, %.1f)",
```

아래로 교체:

```cpp
		ImGui::Text("Dive: phase=%s target=%u   lastCmd=%s slot=%u target=%u block=%s pos=(%.1f, %.1f, %.1f)",
```

그리고 해당 `ImGui::Text` 인자 중 `debug.lastCommandTargetNetId,` 바로 아래에 추가:

```cpp
			ChampionAIBlockReasonName(debug.lastBlockReason),
```

`Skills` 섹션이 끝난 뒤, `ImGui::SeparatorText("Server Minions");` 바로 위에 아래 코드를 추가:

```cpp
		ImGui::SeparatorText("Runtime Tuning");
		RenderTuningSlider(pScene, debug.netId, "Champion scan", eChampionAITuningId::ChampionScanRange,
			debug.fChampionScanRange, 1.f, 40.f, "%.1f");
		RenderTuningSlider(pScene, debug.netId, "Minion scan", eChampionAITuningId::MinionScanRange,
			debug.fMinionScanRange, 1.f, 40.f, "%.1f");
		RenderTuningSlider(pScene, debug.netId, "Structure scan", eChampionAITuningId::StructureScanRange,
			debug.fStructureScanRange, 1.f, 60.f, "%.1f");
		RenderTuningSlider(pScene, debug.netId, "Leash", eChampionAITuningId::LeashRange,
			debug.fLeashRange, 1.f, 60.f, "%.1f");
		RenderTuningSlider(pScene, debug.netId, "Retreat HP", eChampionAITuningId::RetreatHpRatio,
			debug.fRetreatHpRatio, 0.01f, 0.90f, "%.2f");
		RenderTuningSlider(pScene, debug.netId, "Reengage HP", eChampionAITuningId::ReengageHpRatio,
			debug.fReengageHpRatio, 0.01f, 1.f, "%.2f");
		RenderTuningSlider(pScene, debug.netId, "Turret danger", eChampionAITuningId::TurretDangerThreshold,
			debug.fTurretDangerThreshold, 0.f, 1.f, "%.2f");
		RenderTuningSlider(pScene, debug.netId, "Low HP execute", eChampionAITuningId::LowHpExecuteThreshold,
			debug.fLowHpExecuteThreshold, 0.01f, 0.50f, "%.2f");
		RenderTuningSlider(pScene, debug.netId, "Dive scan", eChampionAITuningId::DiveScanRange,
			debug.fDiveScanRange, 1.f, 40.f, "%.1f");
		RenderTuningSlider(pScene, debug.netId, "Dive extra BA window", eChampionAITuningId::DiveExtraBAWindow,
			debug.fDiveExtraBAWindow, 0.f, 5.f, "%.2f");

		ImGui::BeginDisabled(!bCanSend || debug.netId == NULL_NET_ENTITY);
		if (ImGui::Button("Reset Tuning"))
			pScene->GetCommandSerializer()->SendAIDebugResetTuning(*pScene->GetNetworkView(), debug.netId);
		ImGui::EndDisabled();

		ImGui::SeparatorText("Decision Trace");
		ImGui::TextDisabled("Shows the latest server-side decisions for the selected AI.");
```

Decision trace table은 `ChampionAIDebugComponent`에 trace 배열을 추가한 뒤 아래 형태로 붙인다.

```cpp
		if (ImGui::BeginTable("ChampionAIDecisionTrace", 8,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("Tick");
			ImGui::TableSetupColumn("State");
			ImGui::TableSetupColumn("Intent");
			ImGui::TableSetupColumn("Action");
			ImGui::TableSetupColumn("Cmd");
			ImGui::TableSetupColumn("Block");
			ImGui::TableSetupColumn("Target");
			ImGui::TableSetupColumn("Score");
			ImGui::TableHeadersRow();

			for (u8_t i = 0; i < selectedAI.debug.debugDecisionTraceCount; ++i)
			{
				const auto& row = selectedAI.debug.debugDecisionTrace[i];
				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(row.tick));
				ImGui::TableNextColumn(); ImGui::TextUnformatted(ChampionAIStateName(row.state));
				ImGui::TableNextColumn(); ImGui::TextUnformatted(ChampionAIIntentName(row.intent));
				ImGui::TableNextColumn(); ImGui::TextUnformatted(ChampionAIActionName(row.action));
				ImGui::TableNextColumn(); ImGui::Text("%s/%u", ChampionAICommandKindName(row.commandKind), row.commandSlot);
				ImGui::TableNextColumn(); ImGui::TextUnformatted(ChampionAIBlockReasonName(row.blockReason));
				ImGui::TableNextColumn(); ImGui::Text("%u", static_cast<u32_t>(row.target));
				ImGui::TableNextColumn(); ImGui::Text("%.2f/%.2f/%.2f", row.championScore, row.farmScore, row.structureScore);
			}
			ImGui::EndTable();
		}
```

1-10. C:/Users/tnest/Desktop/Winters/Client/Private/UI/DebugDrawSystem.cpp

`ChampionAICommandLabel` 함수 아래에 아래 helper를 추가:

```cpp
    const char* ChampionAIBlockReasonLabel(eChampionAIDecisionBlockReason reason)
    {
        switch (reason)
        {
        case eChampionAIDecisionBlockReason::None: return "None";
        case eChampionAIDecisionBlockReason::NoTarget: return "NoTarget";
        case eChampionAIDecisionBlockReason::TargetDead: return "Dead";
        case eChampionAIDecisionBlockReason::TargetUntargetable: return "Untargetable";
        case eChampionAIDecisionBlockReason::TargetOutOfRange: return "OutOfRange";
        case eChampionAIDecisionBlockReason::SelfLowHp: return "SelfLowHp";
        case eChampionAIDecisionBlockReason::TurretDanger: return "TurretDanger";
        case eChampionAIDecisionBlockReason::SkillCooldown: return "Cooldown";
        case eChampionAIDecisionBlockReason::FlashNotReady: return "FlashNotReady";
        case eChampionAIDecisionBlockReason::ActionLocked: return "ActionLocked";
        case eChampionAIDecisionBlockReason::StateBlocked: return "StateBlocked";
        case eChampionAIDecisionBlockReason::InvalidPath: return "InvalidPath";
        case eChampionAIDecisionBlockReason::CommandRejected: return "Rejected";
        default: return "Unknown";
        }
    }
```

`DrawChampionAIDebug`의 label `sprintf_s` 기존 코드:

```cpp
                    "AI e%u %s\nstate=%s intent=%s\naction=%s cmd=%s s%u\nscore c/f/s %.2f/%.2f/%.2f\nscan c=%.1f d=%.1f flash=%.1f\nlowHP net=%u %.0f%% d=%.1f\nphase=%s dive=%u target=%u",
```

아래로 교체:

```cpp
                    "AI e%u %s\nstate=%s intent=%s\naction=%s cmd=%s s%u block=%s\nscore c/f/s %.2f/%.2f/%.2f\nscan c=%.1f m=%.1f d=%.1f flash=%.1f\nlowHP net=%u %.0f%% d=%.1f\nphase=%s dive=%u target=%u",
```

그리고 `ChampionAICommandLabel(debug.lastCommandKind),` 바로 아래에 추가:

```cpp
                    ChampionAIBlockReasonLabel(debug.lastBlockReason),
```

`debug.fChampionScanRange, debug.fDiveScanRange, debug.fFlashRange` 인자 부분은 아래로 교체:

```cpp
                    debug.fChampionScanRange,
                    debug.fMinionScanRange,
                    debug.fDiveScanRange,
                    debug.fFlashRange,
```

1-11. C:/Users/tnest/Desktop/Winters/Client/Private/UI/RenderDebug.cpp

기존 토글은 유지한다. 이번 세션에서는 새 파일/새 렌더러를 만들지 않는다.

확인 필요:

- 선택된 AI만 range draw하는 토글이 필요하면 `Scene_InGame`에 `m_bDbgShowSelectedChampionAIOnly`와 selected net id 공유가 필요하다. 현재는 `AIDebugPanel.cpp`의 `s_SelectedAINetId`가 translation unit local이라 `DebugDrawSystem`에서 접근할 수 없다. 이 기능은 별도 세션으로 분리한다.

1-12. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Generated/cpp/Snapshot_generated.h

수동 수정 금지.

아래 명령으로 생성:

```text
Shared/Schemas/run_codegen.bat
```

1-13. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Generated/go/Shared/Schema/*.go

수동 수정 금지.

아래 명령으로 생성:

```text
Shared/Schemas/run_codegen.bat
```

2. 검증

미검증:

- 현재 dirty worktree에는 AI/Jax/Flash 1차 구현이 이미 들어갔지만, 사용자가 다른 곳에서 빌드를 돌리는 중이라 이 세션에서 최종 MSBuild 검증은 멈춘 상태다.
- 위 계획 반영 후에는 반드시 schema codegen, diff check, Debug x64 build 순서로 검증한다.

검증 명령:

```text
Shared/Schemas/run_codegen.bat
git diff --check
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" Winters.sln /p:Configuration=Debug /p:Platform=x64 /m
```

수동 확인:

- 사람 플레이어가 `F`로 서버 권한 Flash를 사용하고, Flash 쿨다운이 Snapshot으로 내려온다.
- AI Debug 패널에서 Jax AI를 선택하면 state/intent/action/dive phase/last command/block reason이 갱신된다.
- AI Debug 패널의 Runtime Tuning slider를 움직이면 서버 `ChampionAIComponent.tuning` override가 적용되고, 다음 Snapshot에서 range/threshold 값이 변경된다.
- `Reset Tuning`을 누르면 profile 기본값으로 돌아간다.
- 인게임 DebugDraw에서 champion scan, minion scan, dive scan, attack range, flash range가 서로 다른 색 원으로 보인다.
- Jax 주변 `diveScanRange` 안에 10% 이하 적 챔피언을 만들면 `ExecuteDive -> EngageQ -> ArmW -> BasicAttack -> ExtraBasicAttack -> FlashExit -> ExitMove` 순서가 trace에 남는다.
- Jax가 다이브하지 않는 경우 trace의 block reason이 `NoTarget`, `FlashNotReady`, `ActionLocked`, `TurretDanger` 등으로 설명 가능해야 한다.
- 야스오 AI도 같은 공용 DebugDraw/Blackboard/Trace에 의해 Q/E/R 판단 근거가 보인다.
- 외부 빌드가 진행 중이면 MSBuild는 추가 실행하지 않고, 사용자가 빌드 완료를 알려준 뒤 재검증한다.

프로젝트 파일 확인:

- 새 `.h/.cpp` 파일을 추가하지 않으므로 `Client.vcxproj`, `Server.vcxproj`, `.filters` 변경은 없어야 한다.
- Snapshot schema 변경 후 generated cpp/go 파일은 `run_codegen.bat` 결과만 반영한다.
