Session - Champion AI 통합 시스템을 현재 코드베이스 기준으로 5-1부터 5-6까지 세션 단위로 완성한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h

Session 5-1은 이미 반영되었다.

현재 기준:
- `eChampionAIDebugControlMode`, `ChampionAITuningParam`, `ChampionAITuning`이 추가되어 있다.
- `attackChampionChance`, `lastDecisionRoll` 기반 랜덤 선택은 제거되어 있다.
- `retreatHpRatio` 기본값은 `0.10f`, `reengageHpRatio` 기본값은 `0.25f`이다.
- `fChampionDecisionScore`, `fFarmDecisionScore`, `fStructureDecisionScore`, `fDecisionSelfHpRatio`, `fDecisionEnemyHpRatio`, `fDecisionEnemyDistance`, `fDecisionAttackRange`, `fDecisionTurretDanger`가 AI 판단 근거 저장 필드로 존재한다.
- `bCanAttackChampion`, `bPostComboBAAllowed`, `fPostComboBATimer`가 존재한다.
- `ChampionAIDebugComponent`에는 일부 판단 근거 필드가 이미 추가되어 있지만 아직 snapshot으로 채워지지는 않는다.

Session 5-2에서 아래 enum을 `eChampionAIDebugControlMode` 아래에 추가한다.

아래에 추가:

```cpp
enum class eChampionAIDebugOp : u8_t
{
	ForceAction = 0,
	ClearOverride = 1,
	SetTuning = 2,
	ResetTuning = 3,
};

enum class eChampionAITuningId : u8_t
{
	ChampionScanRange = 0,
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
	Count,
};
```

Session 5-3에서 `ChampionAIDebugComponent`에 snapshot으로 받은 튜닝 표시값을 추가한다.

아래에 추가:

```cpp
	f32_t fRetreatHpRatio = 0.10f;
	f32_t fReengageHpRatio = 0.25f;
	f32_t fChampionScoreMargin = 0.10f;
	f32_t fTurretDangerThreshold = 0.85f;
	f32_t fPostComboBASelfHpMinRatio = 0.10f;
	f32_t fPostComboBAEnemyHpMargin = 0.f;
	f32_t fPostComboBAWindow = 0.80f;
	bool_t bTuningOverrideProfile = false;
```

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

Session 5-1은 이미 반영되었다.

현재 기준:
- `ShouldContinueBasicAttackAfterCombo`가 존재한다.
- `CompleteChampionAICombo(ai, ctx)`가 콤보 완료 후 BA 가능 여부와 window를 기록한다.
- `ApplyChampionAIProfileAndTuning`이 profile과 runtime tuning을 합성한다.
- `UpdateChampionAIDecisionEvidence`가 점수와 판단 근거를 갱신한다.
- `SampleLaneCombatIntent`는 랜덤 roll이 아니라 `fChampionDecisionScore >= fFarmDecisionScore + fChampionScoreMargin`으로 intent를 고른다.
- `TryEmitAttackChampion`은 post-combo BA window 동안 새 콤보보다 BA를 우선한다.
- `TryExecuteStructureAttack`은 아군 미니언이 없으면 구조물 공격을 하지 않는다.
- `ExecuteLaneCombat`의 turret danger 기준은 `ai.fTurretDangerThreshold`를 사용한다.

Session 5-2 이후 런타임 튜닝 command가 들어오면 이 파일은 직접 수정하지 않고 `ChampionAIComponent::tuning`만 바뀌어도 다음 tick의 `ApplyChampionAIProfileAndTuning`에서 자동 반영된다.

1-3. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Command.fbs

Session 5-2에서 기존 `AIDebugControl = 9` command를 유지하고 payload만 확장한다.

기존 코드:

```text
table CommandPacket {
    kind:CommandKind;
    sequenceNum:uint;
    clientTick:ulong;
    slot:ubyte;
    targetNet:uint;
    groundPos:Vec3;
    direction:Vec3;
    itemId:ushort;
    pad:ushort;
}
```

아래로 교체:

```text
table CommandPacket {
    kind:CommandKind;
    sequenceNum:uint;
    clientTick:ulong;
    slot:ubyte;
    targetNet:uint;
    groundPos:Vec3;
    direction:Vec3;
    itemId:ushort;
    pad:ushort;
    aiDebugOp:ubyte = 0;
    aiDebugParam:ubyte = 0;
    aiDebugFlags:ushort = 0;
    aiDebugValue:float = 0;
}
```

Session 5-2 검증 시 `Shared/Schemas/run_codegen.bat` 또는 Server/Client 빌드의 `FlatcCodegen`으로 generated header를 갱신한다.

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h

Session 5-2에서 `GameCommandWire`와 `GameCommand`에 AI debug payload를 추가한다.

기존 코드:

```cpp
struct GameCommandWire
{
    eCommandKind kind = eCommandKind::None;
    uint64_t clientTick = 0;
    uint32_t sequenceNum = 0;
    uint8_t slot = 0;
    NetEntityId targetNet = NULL_NET_ENTITY;
    Vec3 groundPos{};
    Vec3 direction{};
    uint16_t itemId = 0;
};
```

아래로 교체:

```cpp
struct GameCommandWire
{
    eCommandKind kind = eCommandKind::None;
    uint64_t clientTick = 0;
    uint32_t sequenceNum = 0;
    uint8_t slot = 0;
    NetEntityId targetNet = NULL_NET_ENTITY;
    Vec3 groundPos{};
    Vec3 direction{};
    uint16_t itemId = 0;
    uint8_t aiDebugOp = 0;
    uint8_t aiDebugParam = 0;
    uint16_t aiDebugFlags = 0;
    f32_t aiDebugValue = 0.f;
};
```

기존 코드:

```cpp
struct GameCommand
{
    eCommandKind kind = eCommandKind::None;
    EntityID issuerEntity = NULL_ENTITY;
    uint64_t issuedAtTick = 0;
    uint32_t sequenceNum = 0;
    u64_t rewindTicks = 0;
    uint8_t slot = 0;
    EntityID targetEntity = NULL_ENTITY;
    Vec3 groundPos{};
    Vec3 direction{};
    uint16_t itemId = 0;
};
```

아래로 교체:

```cpp
struct GameCommand
{
    eCommandKind kind = eCommandKind::None;
    EntityID issuerEntity = NULL_ENTITY;
    uint64_t issuedAtTick = 0;
    uint32_t sequenceNum = 0;
    u64_t rewindTicks = 0;
    uint8_t slot = 0;
    EntityID targetEntity = NULL_ENTITY;
    Vec3 groundPos{};
    Vec3 direction{};
    uint16_t itemId = 0;
    uint8_t aiDebugOp = 0;
    uint8_t aiDebugParam = 0;
    uint16_t aiDebugFlags = 0;
    f32_t aiDebugValue = 0.f;
};
```

1-5. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

Session 5-2에서 `OnCommandBatch`가 FlatBuffers packet의 새 AI debug 필드를 `GameCommandWire`로 복사한다.

기존 코드:

```cpp
        wire.itemId = packet->itemId();

        EnqueueCommand(sessionId, wire, acceptedTick, recvMs);
```

아래로 교체:

```cpp
        wire.itemId = packet->itemId();
        wire.aiDebugOp = packet->aiDebugOp();
        wire.aiDebugParam = packet->aiDebugParam();
        wire.aiDebugFlags = packet->aiDebugFlags();
        wire.aiDebugValue = packet->aiDebugValue();

        EnqueueCommand(sessionId, wire, acceptedTick, recvMs);
```

1-6. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

Session 5-2에서 `HandleAIDebugControl`을 force/clear/tuning으로 분기한다.

기존 anchor:

```cpp
void CDefaultCommandExecutor::HandleAIDebugControl(CWorld& world,
    const TickContext& tc, const GameCommand& cmd)
```

반영 방향:
- `cmd.aiDebugOp == eChampionAIDebugOp::ClearOverride`이면 기존 clear 동작을 수행한다.
- `cmd.aiDebugOp == eChampionAIDebugOp::ForceAction`이면 기존 force action 동작을 수행한다.
- `cmd.aiDebugOp == eChampionAIDebugOp::SetTuning`이면 `cmd.aiDebugParam`을 `eChampionAITuningId`로 해석해서 `ai.tuning`의 해당 param에 `fCurrent`, `bOverride`를 설정한다.
- `cmd.aiDebugOp == eChampionAIDebugOp::ResetTuning`이면 해당 param만 reset하거나 전체 reset flag일 때 `ai.tuning = ChampionAITuning{}`으로 되돌린다.
- 모든 tuning command는 `_DEBUG` 안에서만 동작한다.
- command 실행 후 `ai.decisionTimer = 0.f`로 다음 tick 판단을 즉시 갱신한다.

Session 5-2에서 `BuildServerCommand`에도 새 payload 복사를 추가한다.

기존 코드:

```cpp
    cmd.itemId = wire.itemId;
    return cmd;
```

아래로 교체:

```cpp
    cmd.itemId = wire.itemId;
    cmd.aiDebugOp = wire.aiDebugOp;
    cmd.aiDebugParam = wire.aiDebugParam;
    cmd.aiDebugFlags = wire.aiDebugFlags;
    cmd.aiDebugValue = wire.aiDebugValue;
    return cmd;
```

1-7. C:/Users/tnest/Desktop/Winters/Client/Public/Network/Client/CommandSerializer.h

Session 5-2에서 AI tuning 전송 API를 추가한다.

기존 코드:

```cpp
	void SendAIDebugControl(CClientNetwork& net, NetEntityId targetNet,
		eChampionAIAction action, u8_t skillSlot = 0);
	void SendAIDebugClear(CClientNetwork& net, NetEntityId targetNet);
```

아래에 추가:

```cpp
	void SendAIDebugSetTuning(CClientNetwork& net, NetEntityId targetNet,
		eChampionAITuningId tuningId, f32_t value);
	void SendAIDebugResetTuning(CClientNetwork& net, NetEntityId targetNet,
		eChampionAITuningId tuningId = eChampionAITuningId::Count);
```

1-8. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/CommandSerializer.cpp

Session 5-2에서 기존 `SendAIDebugControl`과 `SendAIDebugClear`는 `aiDebugOp`를 채우도록 보강한다.

기존 anchor:

```cpp
void CCommandSerializer::SendAIDebugControl(CClientNetwork& net, NetEntityId targetNet,
    eChampionAIAction action, u8_t skillSlot)
```

반영 방향:
- force action은 `wire.aiDebugOp = static_cast<u8_t>(eChampionAIDebugOp::ForceAction)`을 설정한다.
- clear는 `wire.aiDebugOp = static_cast<u8_t>(eChampionAIDebugOp::ClearOverride)`을 설정한다.
- `SendAIDebugSetTuning`은 `kind=AIDebugControl`, `targetNet`, `aiDebugOp=SetTuning`, `aiDebugParam=tuningId`, `aiDebugValue=value`를 채운다.
- `SendAIDebugResetTuning`은 `aiDebugOp=ResetTuning`, `aiDebugParam=tuningId`를 채운다.
- `BuildCommandBatch`에서 FlatBuffers `CreateCommandPacket` 호출에 새 필드를 넘긴다.

1-9. C:/Users/tnest/Desktop/Winters/Shared/Schemas/Snapshot.fbs

Session 5-3에서 `EntitySnapshot` 끝에 AI debug evidence 필드를 추가한다.

기존 anchor:

```text
    abilityHaste:float;
}
```

아래로 교체:

```text
    abilityHaste:float;
    aiDebugFlags:uint = 0;
    aiDebugChampionScore:float = 0;
    aiDebugFarmScore:float = 0;
    aiDebugStructureScore:float = 0;
    aiDebugSelfHpRatio:float = 1;
    aiDebugEnemyHpRatio:float = 1;
    aiDebugEnemyDistance:float = 999;
    aiDebugAttackRange:float = 1.5;
    aiDebugTurretDanger:float = 0;
    aiDebugPostComboBATimer:float = 0;
    aiDebugRetreatHpRatio:float = 0.10;
    aiDebugReengageHpRatio:float = 0.25;
    aiDebugChampionScoreMargin:float = 0.10;
    aiDebugTurretDangerThreshold:float = 0.85;
    aiDebugPostComboBASelfHpMinRatio:float = 0.10;
    aiDebugPostComboBAEnemyHpMargin:float = 0;
    aiDebugPostComboBAWindow:float = 0.80;
}
```

`aiDebugFlags` bit 계획:
- bit 0: `bCanAttackChampion`
- bit 1: `bPostComboBAAllowed`
- bit 2: `ai.tuning.bOverrideProfile`

Session 5-3 검증 시 `Shared/Schemas/run_codegen.bat` 또는 Server/Client 빌드의 `FlatcCodegen`으로 generated header를 갱신한다.

1-10. C:/Users/tnest/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

Session 5-3에서 기존 `ChampionAIComponent` snapshot block에서 AI evidence 값을 지역 변수에 채운다.

기존 anchor:

```cpp
        if (world.HasComponent<ChampionAIComponent>(entity))
        {
            const auto& ai = world.GetComponent<ChampionAIComponent>(entity);
```

반영 방향:
- `aiDebugFlags`, `aiDebugChampionScore`, `aiDebugFarmScore`, `aiDebugStructureScore`, `aiDebugSelfHpRatio`, `aiDebugEnemyHpRatio`, `aiDebugEnemyDistance`, `aiDebugAttackRange`, `aiDebugTurretDanger`, `aiDebugPostComboBATimer` 지역 변수를 추가한다.
- `ai.retreatHpRatio`, `ai.reengageHpRatio`, `ai.fChampionScoreMargin`, `ai.fTurretDangerThreshold`, `ai.fPostComboBASelfHpMinRatio`, `ai.fPostComboBAEnemyHpMargin`, `ai.fPostComboBAWindow`도 snapshot에 싣는다.
- `CreateEntitySnapshot` 호출의 마지막 인자들로 새 필드를 전달한다.

1-11. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

Session 5-3에서 기존 AI debug apply block에 evidence와 tuning 표시값을 복사한다.

기존 anchor:

```cpp
                debug.bOverridePending = (es->stateFlags() & kChampionAIDebugOverrideFlag) != 0u;
                debug.moveSpeed = es->moveSpeed();
                debug.snapshotPos = Vec3{ es->posX(), es->posY(), es->posZ() };
```

아래에 추가:

```cpp
                debug.bCanAttackChampion = (es->aiDebugFlags() & (1u << 0)) != 0u;
                debug.bPostComboBAAllowed = (es->aiDebugFlags() & (1u << 1)) != 0u;
                debug.bTuningOverrideProfile = (es->aiDebugFlags() & (1u << 2)) != 0u;
                debug.fChampionDecisionScore = es->aiDebugChampionScore();
                debug.fFarmDecisionScore = es->aiDebugFarmScore();
                debug.fStructureDecisionScore = es->aiDebugStructureScore();
                debug.fSelfHpRatio = es->aiDebugSelfHpRatio();
                debug.fEnemyHpRatio = es->aiDebugEnemyHpRatio();
                debug.fEnemyDistance = es->aiDebugEnemyDistance();
                debug.fAttackRange = es->aiDebugAttackRange();
                debug.fTurretDanger = es->aiDebugTurretDanger();
                debug.fPostComboBATimer = es->aiDebugPostComboBATimer();
                debug.fRetreatHpRatio = es->aiDebugRetreatHpRatio();
                debug.fReengageHpRatio = es->aiDebugReengageHpRatio();
                debug.fChampionScoreMargin = es->aiDebugChampionScoreMargin();
                debug.fTurretDangerThreshold = es->aiDebugTurretDangerThreshold();
                debug.fPostComboBASelfHpMinRatio = es->aiDebugPostComboBASelfHpMinRatio();
                debug.fPostComboBAEnemyHpMargin = es->aiDebugPostComboBAEnemyHpMargin();
                debug.fPostComboBAWindow = es->aiDebugPostComboBAWindow();
```

1-12. C:/Users/tnest/Desktop/Winters/Client/Private/UI/AIDebugPanel.cpp

Session 5-4에서 selected AI 영역을 evidence 중심으로 확장한다.

기존 anchor:

```cpp
		ImGui::Text("Risk: retreat %.0f%%   reengage %.0f%%   aggression %.2f   kite %.2f",
			profile.retreatHpRatio * 100.f,
			profile.reengageHpRatio * 100.f,
			profile.aggression,
			profile.kiteBias);
```

반영 방향:
- profile risk 표시 아래에 server evidence 표시를 추가한다.
- `Score: champion / farm / structure`
- `HP ratio: self / enemy`
- `Range: enemy distance / attack range`
- `Turret danger`, `CanAttackChampion`, `PostComboBAAllowed`, `PostComboBATimer`

Session 5-4에서 selected AI 영역에 runtime tuning slider를 추가한다.

반영 방향:
- `RetreatHpRatio`, `ReengageHpRatio`, `ChampionScoreMargin`, `TurretDangerThreshold`, `PostComboBASelfHpMinRatio`, `PostComboBAEnemyHpMargin`, `PostComboBAWindow`를 `ImGui::SliderFloat`로 노출한다.
- 값 변경 시 `SendAIDebugSetTuning`을 호출한다.
- `Reset Tuning` 버튼은 `SendAIDebugResetTuning`을 호출한다.
- 슬라이더는 snapshot에서 받은 현재값을 seed로 사용하고, UI local cache는 selected netId가 바뀌면 snapshot 값으로 다시 초기화한다.

Session 5-4에서 debug draw toggle을 AI Debug Panel 안에 둔다.

반영 방향:
- `pScene->SetDbgShowNavGrid`
- `pScene->SetDbgShowPathNavGrid`
- `pScene->SetDbgShowMinionMovement`
- 새로 추가할 `SetDbgShowChampionAIRange`
- 새로 추가할 `SetDbgShowChampionAIText`

1-13. C:/Users/tnest/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

Session 5-5에서 champion AI debug draw 전용 toggle을 추가한다.

기존 anchor:

```cpp
    bool_t   IsDbgShowMinionMovement() const { return m_bDbgShowMinionMovement; }
    void     SetDbgShowMinionMovement(bool_t b) { m_bDbgShowMinionMovement = b; }
```

아래에 추가:

```cpp
    bool_t   IsDbgShowChampionAIRange() const { return m_bDbgShowChampionAIRange; }
    void     SetDbgShowChampionAIRange(bool_t b) { m_bDbgShowChampionAIRange = b; }
    bool_t   IsDbgShowChampionAIText() const { return m_bDbgShowChampionAIText; }
    void     SetDbgShowChampionAIText(bool_t b) { m_bDbgShowChampionAIText = b; }
```

기존 anchor:

```cpp
    bool_t m_bDbgShowMinionMovement = true;
```

아래에 추가:

```cpp
    bool_t m_bDbgShowChampionAIRange = true;
    bool_t m_bDbgShowChampionAIText = true;
```

1-14. C:/Users/tnest/Desktop/Winters/Client/Public/UI/DebugDrawSystem.h

Session 5-5에서 AI range/text draw 함수를 추가한다.

기존 anchor:

```cpp
        static void DrawChampions(CWorld& w, CScene_InGame* s, ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
        static void DrawMinionMovement(CWorld& w, CScene_InGame* s, ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
```

아래에 추가:

```cpp
        static void DrawChampionAIDebug(CWorld& w, CScene_InGame* s, ImDrawList* pDraw, const DirectX::XMMATRIX& mVP);
```

1-15. C:/Users/tnest/Desktop/Winters/Client/Private/UI/DebugDrawSystem.cpp

Session 5-5에서 render entry에 AI debug draw를 연결한다.

기존 코드:

```cpp
        if (pScene->IsDbgShowChampions())  DrawChampions(world, pScene, pDraw, mVP);
        if (pScene->IsDbgShowMinionMovement()) DrawMinionMovement(world, pScene, pDraw, mVP);
```

아래에 추가:

```cpp
        if (pScene->IsDbgShowChampionAIRange() || pScene->IsDbgShowChampionAIText())
            DrawChampionAIDebug(world, pScene, pDraw, mVP);
```

Session 5-5에서 `DrawChampionAIDebug` 구현을 추가한다.

반영 방향:
- `ChampionComponent`, `TransformComponent`, `ChampionAIDebugComponent`를 순회한다.
- range toggle이 켜져 있으면 `debug.fAttackRange` 원을 그린다.
- detection range는 snapshot에 아직 scan range가 없으면 5-3에서 추가한 tuning/snapshot 필드를 보고 그린다.
- text toggle이 켜져 있으면 미니언 text와 비슷하게 머리 위에 state/intent/action/score/postBA를 표시한다.
- 표시 문구 예: `AI LaneCombat / AttackChampion / score C0.82 F0.55 / HP 0.71>0.42 / BA yes`
- 텍스트는 `WorldToScreen` 성공 시에만 그린다.

1-16. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

Session 5-5에서 render debug 기본값이 켜지는 개발 경로를 점검한다.

기존 anchor:

```cpp
            m_bDbgShowColliders = true;
            m_bDbgShowChampions = true;
```

아래에 추가:

```cpp
            m_bDbgShowChampionAIRange = true;
            m_bDbgShowChampionAIText = true;
```

1-17. Session 5-6 runtime smoke

새 코드 파일 반영은 없다.

수동 검증 시나리오:
- AI Debug Panel에서 champion AI를 선택한다.
- selected AI의 state/intent/action, score, HP ratio, enemy distance, attack range가 snapshot으로 갱신되는지 확인한다.
- attack range debug circle이 실제 BA 가능 거리와 맞는지 확인한다.
- championScanRange 또는 attack range text가 world position과 크게 어긋나지 않는지 확인한다.
- `RetreatHpRatio`를 0.10으로 두고 self HP가 10% 이하일 때 retreat 후 tower/anchor 도착 시 recall로 이어지는지 확인한다.
- 콤보 완료 후 `self HP > 10%`이고 `enemy HP < self HP`일 때 post-combo BA가 나가는지 확인한다.
- 같은 조건에서 enemy HP가 self HP보다 높으면 BA 대신 farm/follow/retreat 판단으로 빠지는지 확인한다.
- 적 타워는 아군 미니언이 있고 타워 탱킹 조건이 참일 때만 공격하는지 확인한다.
- AI Debug Panel에서 minion navgrid, minion movement debug draw를 켜고 끌 수 있는지 확인한다.

2. 검증

정적 검증:
- `rg "attackChampionChance|lastDecisionRoll|MakeChampionAIRoll" Shared/GameSim`
- `git diff --check`

스키마 검증:
- `Shared/Schemas/run_codegen.bat`
- generated header가 `Command_generated.h`, `Snapshot_generated.h`에 반영되는지 확인한다.
- `.vcxproj` 직접 수정은 기본적으로 하지 않는다. 현재 Server/Client 빌드가 `FlatcCodegen`을 실행한다.

빌드 검증:
- `& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
- `& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`

현재 5-1 완료 검증 결과:
- Server Debug 빌드 성공, 오류 0개.
- Client Debug 빌드 성공, 오류 0개.
- 기존 EngineSDK DLL interface warning은 남아 있다.
- `rg "attackChampionChance|lastDecisionRoll|MakeChampionAIRoll" Shared/GameSim` 결과 없음.

런타임 검증:
- 서버 권위 흐름은 반드시 `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual`을 유지한다.
- AI는 gameplay truth를 직접 클라이언트에서 바꾸지 않는다.
- AIDebugPanel tuning 조작은 command로 서버에 도착하고, 서버 AI component 값이 바뀐 뒤 snapshot evidence로 다시 클라이언트에 돌아와야 한다.
- debug draw는 client visual/debug 계층에서만 수행한다.
