Session - 기획자/디자이너 인게임 툴 스위트의 전체 설계를 고정하고, P0(시뮬 시간 제어 Pause/Step/TimeScale + WRPL 명령 저널 = 크로노 브레이크 기반층)를 반영한다.

작성: Claude 레인 (도메인 조사 에이전트 7종, 전 앵커 rg 검증) / 작성일 2026-07-12
선행: Plan/S009(연습 툴·밸런스 랩), .md/plan/2026-07-12_S010~S013 (Codex 계획, 미적용)
경계 원칙: **Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변조하지 않는다.** 모든 툴 명령은 클라 ImGui -> CommandSerializer -> 서버 host 게이트 -> 틱 내부 실행으로만 진실을 바꾼다 (S009 계약 유지).

---

## 0. 설계 (전체 이니셔티브)

### 0-1. Goal / Non-goals / Why this order

**Goal**: 한 번의 빌드·실행 안에서 — (T1) 결정론 시뮬 기반 크로노 브레이크(N초 되감기+동일 상황 재실행), (T2) 수치 데이터 즉시 적용, (T3) 봇 AI 틱 단위 의사결정 분해·튜닝, (T4) 적 챔피언 배치·시나리오, (D1~D4) FX 시뮬레이션·애니 보간 프리뷰·모델 분해·프리뷰 스테이지.

**Non-goals**: 멀티플레이어 룸 전체 리와인드(연습 룸 한정 MVP), 컴파일된 정의 팩의 런타임 교체(P2), 기존 WfxEffectToolPanel/EffectTuner 재작성.

**Why this order**: 시간 제어(Pause/Step)는 크로노 브레이크의 전제이면서 그 자체로 T3(트레이스를 멈춰놓고 읽기)·D1(이펙트 정지 관찰)의 즉시 가치가 있다. 명령 저널은 지금부터 쌓여야 P1 재시뮬이 과거 매치에도 성립한다. 나머지 트랙은 S009~S013 자산 위 확장이라 기반층과 독립 진행 가능.

### 0-2. 현재 자산 지도 (조사 확정, 전부 미커밋 작업 트리 기준)

| 자산 | 앵커 | 의미 |
| --- | --- | --- |
| 30Hz 고정 틱 단일 스레드 시뮬 | `Server/Private/Game/GameRoomTick.cpp:68-113` | 틱 게이트 한 곳이면 전체 시간 제어 가능 |
| 단일 시드 RNG Get/SetState | `Shared/GameSim/Core/Determinism/DeterministicRng.h:39` | 리와인드 시 RNG 복원 훅 존재 |
| SimLab 동일시드 해시 계약 | `Tools/SimLab/main.cpp:2` | 재시뮬 회귀 게이트로 재사용 |
| WRPL 리플레이(스냅샷+이벤트, 매 틱 풀스냅샷+rngState) | `Shared/Replay/ReplayFormat.h:29-33`, `Server/Private/Game/GameRoomReplication.cpp:124-140` | Command 레코드만 추가하면 저널 완성 |
| PracticeControl 명령 레인 (host+_DEBUG 게이트, 11 ops) | `Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h:85-99`, `Server/Private/Game/GameRoomCommands.cpp:216-` | 시간 제어 ops의 수송로 |
| F10 Practice Tool/Balance Lab + 스킬 파라미터 오버레이(14종) | `Client/Private/UI/ChampionTuner.cpp:97-`, `Shared/GameSim/Definitions/GameplayDefinitionQuery.cpp:282-298` | T2의 수직 슬라이스 완성 상태 |
| F9 AI 트레이스(16링 스냅샷 복제)+튜닝 슬라이더 14종 | `Shared/GameSim/Components/ChampionAIComponent.h:99-118`, `Client/Private/UI/AIDebugPanel.cpp` | T3의 70% |
| F7 WFX 이펙트 툴(.wfx 편집+핫리플레이스) | `Client/Private/UI/WfxEffectToolPanel.cpp`, `Engine/Public/FX/FxAsset.h:218-256` | D1 기반 |
| 서브메시 2048비트 VisibilityMask 전 렌더패스 반영 | `Engine/Public/ECS/Components/MeshGroupVisibilityComponent.h:7-59` | D3의 80% (UI만 없음) |
| 미사용 Cinematic 모듈(Seek 가능한 트랙 타임라인) | `Engine/Public/Cinematic/CSequencePlayer.h:10-20` | D4 타임라인 백본 |
| 클라 리플레이 플레이어(전진 전용) | `Client/Private/Replay/ReplayPlayer.cpp:104-151` | P3 스크럽 뷰어 후보 |

### 0-3. 트랙별 설계와 단계

**T1 크로노 브레이크** — 아키텍처 결정: *비트 정확 키프레임 + 외부 명령 저널 + 결정론 재시뮬* 하이브리드.
근거: ECS dense store 순서가 add/remove 이력 함수라(`Engine/Public/ECS/ComponentStore.h:24`) "정렬 재구축" 복원은 공간 쿼리 순서를 바꿔 조용히 발산한다. 복원은 raw dense 배열 단위 비트 복원이어야 하고, 봇 명령은 저널하지 않는다(상태+RNG에서 결정론 재생성 — 이것이 곧 "수치 바꾸고 같은 상황 재실행" 루프의 원리).
- P0(이 문서): 틱 게이트 Pause/Step/TimeScale + WRPL v2 Command 저널. TimeScale은 **틱 주기(wall-clock)만 조절, fDt 불변** — 결정론·SimLab 해시 보존.
- P1: CSimStateSerializer(컴포넌트 스토어 raw 덤프 + EntityManager 슬롯 + RNG + EntityIdMap + 비ECS 상태: CServerMinionWaveRuntime/TurretAI/practice 상태/브로드캐스트 캐시) + 등록 누락 기계 검사(CWorld 스토어 전수 = 레지스트리 전수 assert) + SimLab "save->restore->continue 해시 동일" 골든 프로브. Engine ECS 접근자 추가 필요(ABI 1회 배치).
- P2: 리와인드 오케스트레이션 — pause -> 최근접 키프레임 복원 -> 저널 틱 재실행(브로드캐스트 억제) -> 복제 캐시 리셋(m_lastBroadcastActionSeq 재구축, 세션 ack 시퀀스는 절대 후퇴 금지) -> 클라 보간/예측 플러시 이벤트 1발 -> resume.
- P3: 타임라인 스크럽 UI + 리플레이 뷰어 Seek.

**T2 데이터 즉시 튜닝** — S009 오버레이 확장이 정답, 팩 핫리로드는 마지막.
- P1: (a) 오버라이드 대상 확장(issuer 고정 -> targetNet 지정/팀 전체), (b) AI 가중치(aggression/farm/siege/turretRisk)를 eChampionAITuningId에 추가, (c) 레거시 하드코딩 스킬 리더의 ResolveSkillEffectParam 패리티 마이그레이션(S009 잔여 목록), (d) 튜닝 값 JSON 프로필 저장->정식 데이터 반영은 Build-LoLDefinitionPack.py 왕복 규칙 유지.
- P2: 룸 레벨 정의 오버레이(챔피언 기본 스탯·쿨다운, StatComponent bDirty 재스탬프 활용 — 적용점은 틱 상단 팩 포인터 캡처 지점) + Hello dataBuildHash 경고를 UI 표면화.
- 주의: ChampionAIProfile/ComboPlan의 데이터화(ChampionAIProfileDefs.json)는 S013(봇 기반) 설계와 병합해 Codex 레인에서.

**T3 AI 트레이스 심화** — **Codex-S013 레인과 조율 필수** (ChampionAI*/SnapshotBuilder/AIDebugPanel은 S013 계획이 선점).
설계 지침만 고정: 트레이스 엔트리에 (a) retreatScore(이미 계산됨, 미기록), (b) 후보 배열 Candidate[6]{id,score,rejectReason}, (c) 게이트 비트(bCanMove/Attack/Cast), (d) decisionKind(Emitted/NoAction/ActionLocked) 추가; CommandRejected를 executor 검증 실패에서 실제 배선(현재 선언만 존재); 전체 트레이스는 선택 봇 구독 게이트 뒤로(스냅샷 대역폭); ResetChampionAIState practice op(커밋/타이머 클리어). P0의 Pause/Step과 조합되면 "멈춰서 한 틱씩 분해"가 즉시 성립.

**T4 시나리오/적 배치** — ePracticeOperation 확장(append-only): SpawnChampion(챔피언 id=practiceFlags, 팀=practiceValue, groundPos, PracticeSpawnedTag), Teleport의 targetNet 존중(임의 유닛 재배치), SaveScenario/LoadScenario JSON(포지션+HP+CD+레벨+오버라이드 묶음). 클라 로스터 UI의 비로스터 챔피언 허용 검증 필요.

**D1 FX 정밀 시뮬** — **Codex-S012 레인과 조율**: 클라 FX 시스템 3종 Update에 fFxTimeScale/pause/step 주입(`Scene_InGame.cpp:1206-1208`), F7 패널 auto-respawn-on-edit(0.3s 디바운스), LoopUntilSignal 루프 프리뷰. 파라미터 커브는 노드 그래프 계획(.md/plan/EffectTool)으로 — .wfx v1 스키마 동결.

**D2 애니메이션 프리뷰 패널** — ModelRenderer ABI 배치 1회(GetAnimationNameByIndex/GetSubmeshCount/GetSubmeshInfo/본 열거)를 묶어 Engine 변경 + UpdateLib 동기화. CAnimator::EvaluateAtTime 스크럽(재생 불요 평가), castFrame/recoveryFrame 마커를 스크럽 바에 표시(SkillTimingPanel과 통일). 인스턴스 애니메이터만 조작(공유 CModel 불변).

**D3 모델 분해 인스펙터** — 서브메시 체크박스/Solo/All/None + 머티리얼 인덱스/해시 표시 + AABB 플래시. 렌더 경로는 이미 마스크 반영 — UI+접근자만. 발견한 파트 인덱스는 Yone_MeshGroups.h 패턴의 네임드 마스크 헤더로 박제.

**D4 프리뷰 스테이지** — Scene_AssetPreview(Scene_Editor 골격: 자체 CWorld+카메라, 서버 무접속) + 궤도 카메라 + Cinematic CSequencePlayer 바인딩(Anim+Fx 트랙, Play/Seek 스크럽). 라이브 월드 내 프리뷰 오염 금지(2026-05-14 gotcha).

**공통 인프라** — (a) Designer Hub: DockSpace + 패널 레지스트리{이름, 카테고리, bool*, Render fn}로 Scene_InGameImGui.cpp의 수기 토글 삼중항 대체, F키는 단축키로 유지. (b) Release 게이트 정책 결정 필요: PracticeControl은 _DEBUG 밖에서 조용히 삼켜짐(`GameRoomCommands.cpp:223-226`) — 디자이너 빌드용 `--practice-server` 명시 플래그로 전환 검토(조용한 무시는 에러 정책 위반). (c) 스냅샷 디버그 페이로드 구독 게이트.

### 0-4. 레인 배분 (충돌 회피)

| 트랙 | 레인 | 근거 |
| --- | --- | --- |
| T1 P0/P1/P2 (틱 제어·저널·직렬화) | **Claude(이 세션 P0) -> 이후 packet 단위** | S010~S013 미접점 파일 |
| T2 P1 (오버레이 확장) | Codex 후속 세션 | S009 연장선, ChampionTuner/GameRoomCommands 소유 연속성 |
| T3 (트레이스 심화) | **Codex-S013에 병합** | ChampionAI*/SnapshotBuilder/AIDebugPanel 선점 |
| T4 (시나리오) | Codex 후속 세션 | GameRoomSpawn/Commands 연속성 |
| D1 (FX) | **Codex-S012에 병합** | VFX 어서링 계획 선점 |
| D2/D3/D4 | 신규 packet (Engine ABI 배치 포함) | Always-Lock vcxproj 단일 소유 필요 |

### 0-5. P0 범위 (이 세션 반영분)

Pause/Step/TimeScale 서버 게이트 + 일시정지 중 컨트롤 레인(Practice/AIDebug만 실행, 게임플레이 입력 void+ack) + WRPL v2 Command 저널 + 프리즈 틱 스냅샷 중복 기록 가드 + F10 패널 Simulation Time 섹션. 신규 파일 없음, Engine 무변경, .fbs 재생성 1회.

---

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h

`enum class ePracticeOperation` 안에서:

기존 코드:

```cpp
    ApplySkillEffectOverride = 10,
    ClearSkillEffectOverrides = 11,
};
```

아래로 교체:

```cpp
    ApplySkillEffectOverride = 10,
    ClearSkillEffectOverrides = 11,
    SetSimulationPaused = 12,
    StepSimulationTicks = 13,
    SetSimulationTimeScale = 14,
};
```

### 1-2. C:/Users/user/Desktop/Winters/Shared/Schemas/Command.fbs

기존 코드:

```text
    ApplySkillEffectOverride = 10,
    ClearSkillEffectOverrides = 11
}
```

아래로 교체:

```text
    ApplySkillEffectOverride = 10,
    ClearSkillEffectOverrides = 11,
    SetSimulationPaused = 12,
    StepSimulationTicks = 13,
    SetSimulationTimeScale = 14
}
```

### 1-3. C:/Users/user/Desktop/Winters/Shared/Replay/ReplayFormat.h

기존 코드:

```cpp
	inline constexpr u16_t kReplayVersion = 1;
```

아래로 교체:

```cpp
	inline constexpr u16_t kReplayVersion = 2;
```

기존 코드:

```cpp
	enum class eReplayRecordType : u8_t
	{
		Snapshot = 1,
		Event = 2,
	};
```

아래로 교체:

```cpp
	enum class eReplayRecordType : u8_t
	{
		Snapshot = 1,
		Event = 2,
		Command = 3,
	};

#pragma pack(push, 1)
	// v2 Command record payload: one externally-issued session command,
	// keyed by the record header's serverTick (= execution tick).
	// Bot AI commands are NOT journaled - they regenerate deterministically.
	struct ReplayCommandPayload
	{
		u32_t sourceSessionId = 0;
		u32_t sequenceNum = 0;
		u8_t kind = 0;
		u8_t slot = 0;
		u16_t itemId = 0;
		u32_t targetNetId = 0;
		f32_t groundPos[3]{};
		f32_t direction[3]{};
		u16_t practiceOperation = 0;
		u16_t reserved0 = 0;
		f32_t practiceValue = 0.f;
		u32_t practiceFlags = 0;
		u64_t clientTick = 0;
	};
#pragma pack(pop)

	static_assert(sizeof(ReplayCommandPayload) == 60, "ReplayCommandPayload size fixed");
```

### 1-4. C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h

기존 코드:

```cpp
    bool_t TryHandlePracticeControl(const TickContext& tc, const GameCommand& cmd);
    void TickPracticeControls(const TickContext& tc);
    void ClearPracticeSpawns();
```

아래로 교체:

```cpp
    bool_t TryHandlePracticeControl(const TickContext& tc, const GameCommand& cmd);
    void TickPracticeControls(const TickContext& tc);
    void ClearPracticeSpawns();
    void TickPausedControlLane();
```

기존 코드:

```cpp
    bool_t m_bPracticeModeEnabled = false;
    std::vector<EntityID> m_PracticeSpawnedEntities;
```

아래로 교체:

```cpp
    bool_t m_bPracticeModeEnabled = false;
    std::vector<EntityID> m_PracticeSpawnedEntities;

    // Simulation time control (designer/practice; sim dt stays kFixedDt)
    bool_t m_bSimPaused = false;
    u32_t m_simStepBudget = 0;
    std::atomic<f32_t> m_simSpeedMul{ 1.f };
    u64_t m_lastReplaySnapshotTick = 0;
```

### 1-5. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomTick.cpp

기존 코드:

```cpp
void CGameRoom::TickThread()
{
	using clock = std::chrono::steady_clock;
	auto next = clock::now();
	const auto period = std::chrono::microseconds(33333);

	while (m_bRunning.load(std::memory_order_relaxed))
	{
		Tick();
		next += period;
		std::this_thread::sleep_until(next);
	}
}
```

아래로 교체:

```cpp
void CGameRoom::TickThread()
{
	using clock = std::chrono::steady_clock;
	auto next = clock::now();

	while (m_bRunning.load(std::memory_order_relaxed))
	{
		Tick();

		f32_t speedMul = m_simSpeedMul.load(std::memory_order_relaxed);
		if (speedMul < 0.1f) speedMul = 0.1f;
		if (speedMul > 8.f) speedMul = 8.f;
		const auto period = std::chrono::microseconds(
			static_cast<long long>(33333.f / speedMul));

		next += period;
		const auto now = clock::now();
		if (next < now)
			next = now;
		std::this_thread::sleep_until(next);
	}
}
```

기존 코드:

```cpp
	std::lock_guard stateLock(m_stateMutex);

	if (!IsInGamePhase())
		return;

	++m_tickIndex;
```

아래로 교체:

```cpp
	std::lock_guard stateLock(m_stateMutex);

	if (!IsInGamePhase())
		return;

	if (m_bSimPaused)
	{
		if (m_simStepBudget == 0)
		{
			TickPausedControlLane();
			return;
		}
		--m_simStepBudget;
	}

	++m_tickIndex;
```

### 1-6. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

`Phase_DrainCommands` 안에서:

기존 코드:

```cpp
        m_pendingExecCommands.push_back(cmd);
        m_lastSimCommandSeqBySession[pending.sessionId] = pending.sequenceNum;
    }
}
```

아래로 교체:

```cpp
        m_pendingExecCommands.push_back(cmd);
        m_lastSimCommandSeqBySession[pending.sessionId] = pending.sequenceNum;

        if (m_pReplayRecorder && !m_bReplayFinalized)
        {
            Winters::Replay::ReplayCommandPayload journal{};
            journal.sourceSessionId = pending.sessionId;
            journal.sequenceNum = pending.sequenceNum;
            journal.kind = static_cast<u8_t>(pending.wire.kind);
            journal.slot = pending.wire.slot;
            journal.itemId = pending.wire.itemId;
            journal.targetNetId = static_cast<u32_t>(pending.wire.targetNet);
            journal.groundPos[0] = pending.wire.groundPos.x;
            journal.groundPos[1] = pending.wire.groundPos.y;
            journal.groundPos[2] = pending.wire.groundPos.z;
            journal.direction[0] = pending.wire.direction.x;
            journal.direction[1] = pending.wire.direction.y;
            journal.direction[2] = pending.wire.direction.z;
            journal.practiceOperation =
                static_cast<u16_t>(pending.wire.practiceOperation);
            journal.practiceValue = pending.wire.practiceValue;
            journal.practiceFlags = pending.wire.practiceFlags;
            journal.clientTick = pending.wire.clientTick;
            m_pReplayRecorder->RecordCommand(tc.tickIndex, journal);
        }
    }
}

void CGameRoom::TickPausedControlLane()
{
    const GameplayDefinitionPack& definitions =
        ServerData::GetLoLGameplayDefinitionPack();
    TickContext tc{
        m_tickIndex,
        DeterministicTime::kFixedDt,
        DeterministicTime::TickToSec(m_tickIndex),
        &m_rng, &m_entityMap, NULL_ENTITY, this
    };
    tc.pLagCompensation = m_pLagCompensation.get();
    tc.pDefinitions = &definitions;

    // While paused, only control-lane commands mutate state; gameplay
    // inputs are consumed as void but still acked so client prediction prunes.
    std::vector<PendingCommand> drained = m_commandIngress.DrainSorted();
    for (const auto& pending : drained)
    {
        m_lastSimCommandSeqBySession[pending.sessionId] = pending.sequenceNum;

        if (pending.wire.kind != eCommandKind::PracticeControl &&
            pending.wire.kind != eCommandKind::AIDebugControl)
        {
            continue;
        }

        const EntityID controlledEntity = m_sessionBinding.ResolveControlledEntity(
            pending.sessionId,
            m_world,
            m_entityMap,
            m_pLobbyAuthority.get());
        if (controlledEntity == NULL_ENTITY)
            continue;

        GameCommand cmd = BuildServerCommand(
            pending.wire, pending.sessionId, controlledEntity, m_entityMap);
        cmd.issuedAtTick = m_tickIndex;
        cmd.rewindTicks = 0;

        if (!TryHandlePracticeControl(tc, cmd))
            m_pExecutor->ExecuteCommand(m_world, tc, cmd);
    }

    Phase_BroadcastSnapshot(tc);
}
```

`TryHandlePracticeControl`의 switch 안에서 (`case ePracticeOperation::SetOptions:` 블록 위):

기존 코드:

```cpp
    switch (cmd.practiceOperation)
    {
    case ePracticeOperation::SetOptions:
```

아래로 교체:

```cpp
    switch (cmd.practiceOperation)
    {
    case ePracticeOperation::SetSimulationPaused:
    {
        if (!std::isfinite(cmd.practiceValue) ||
            (cmd.practiceValue != 0.f && cmd.practiceValue != 1.f))
        {
            return Finish(false, "paused-must-be-zero-or-one");
        }
        m_bSimPaused = cmd.practiceValue == 1.f;
        if (!m_bSimPaused)
            m_simStepBudget = 0;
        return Finish(true, m_bSimPaused ? "sim-paused" : "sim-resumed");
    }

    case ePracticeOperation::StepSimulationTicks:
    {
        if (!m_bSimPaused)
            return Finish(false, "step-requires-pause");
        if (!std::isfinite(cmd.practiceValue) ||
            cmd.practiceValue < 1.f || cmd.practiceValue > 300.f)
        {
            return Finish(false, "step-out-of-range");
        }
        m_simStepBudget += static_cast<u32_t>(cmd.practiceValue);
        return Finish(true, "step-scheduled");
    }

    case ePracticeOperation::SetSimulationTimeScale:
    {
        if (!std::isfinite(cmd.practiceValue) ||
            cmd.practiceValue < 0.1f || cmd.practiceValue > 8.f)
        {
            return Finish(false, "timescale-out-of-range");
        }
        m_simSpeedMul.store(cmd.practiceValue, std::memory_order_relaxed);
        return Finish(true, "timescale-set");
    }

    case ePracticeOperation::SetOptions:
```

### 1-7. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomReplication.cpp

`Phase_BroadcastSnapshot` 안에서 (일시정지 프리즈 틱 재브로드캐스트가 리플레이에 중복 기록되지 않게):

기존 코드:

```cpp
	if (m_pReplayRecorder && !m_bReplayFinalized && m_pSnapBuilder)
	{
```

아래로 교체:

```cpp
	if (m_pReplayRecorder && !m_bReplayFinalized && m_pSnapBuilder &&
		tc.tickIndex != m_lastReplaySnapshotTick)
	{
		m_lastReplaySnapshotTick = tc.tickIndex;
```

### 1-8. C:/Users/user/Desktop/Winters/Server/Public/Game/ReplayRecorder.h

기존 코드:

```cpp
	void RecordSnapshot(u64_t tick, const u8_t* bytes, u32_t len);
	void RecordEvent(u64_t tick, const u8_t* bytes, u32_t len);
```

아래로 교체:

```cpp
	void RecordSnapshot(u64_t tick, const u8_t* bytes, u32_t len);
	void RecordEvent(u64_t tick, const u8_t* bytes, u32_t len);
	void RecordCommand(u64_t tick, const Winters::Replay::ReplayCommandPayload& payload);
```

기존 코드:

```cpp
	u32_t m_iSnapshotCount = 0;
	u32_t m_iEventCount = 0;
```

아래로 교체:

```cpp
	u32_t m_iSnapshotCount = 0;
	u32_t m_iEventCount = 0;
	u32_t m_iCommandCount = 0;
```

### 1-9. C:/Users/user/Desktop/Winters/Server/Private/Game/ReplayRecorder.cpp

`RecordEvent` 구현 바로 아래에 추가 (기존 `Record(...)` 내부 헬퍼 재사용):

```cpp
void CReplayRecorder::RecordCommand(
	u64_t tick, const Winters::Replay::ReplayCommandPayload& payload)
{
	Record(Winters::Replay::eReplayRecordType::Command, tick,
		reinterpret_cast<const u8_t*>(&payload),
		static_cast<u32_t>(sizeof(payload)));
	++m_iCommandCount;
}
```

확인 필요: `Record()`가 snapshot/event 카운터를 내부에서 증가시키는 구조라면 Command 분기 추가 위치를 맞춘다 (구현 시 실제 함수 본문 기준).

### 1-10. C:/Users/user/Desktop/Winters/Client/Private/Replay/ReplayPlayer.cpp

기존 코드:

```cpp
		return type == static_cast<u8_t>(Winters::Replay::eReplayRecordType::Snapshot) ||
			type == static_cast<u8_t>(Winters::Replay::eReplayRecordType::Event);
```

아래로 교체:

```cpp
		return type == static_cast<u8_t>(Winters::Replay::eReplayRecordType::Snapshot) ||
			type == static_cast<u8_t>(Winters::Replay::eReplayRecordType::Event) ||
			type == static_cast<u8_t>(Winters::Replay::eReplayRecordType::Command);
```

(재생 루프는 Snapshot/Event 타입만 각각 적용하므로 Command 레코드는 자연 스킵 — 추가 변경 없음. kReplayVersion 상수 공유로 로더 버전 검사도 v2로 함께 이동.)

### 1-11. C:/Users/user/Desktop/Winters/Client/Private/UI/ChampionTuner.cpp

`struct PracticeToolState` 안에서:

기존 코드:

```cpp
		int minionTeam = 1;
		int minionRole = 0;
		int minionLane = 1;
	};
```

아래로 교체:

```cpp
		int minionTeam = 1;
		int minionRole = 0;
		int minionLane = 1;

		bool_t bSimPauseRequested = false;
		int simStepTicks = 1;
		f32_t simTimeScale = 1.f;
	};
```

`Enable/Disable Practice Session` 버튼 블록 아래(`ImGui::EndDisabled();`와 `if (ImGui::CollapsingHeader("Player Options"` 사이)에 추가:

```cpp
		if (ImGui::CollapsingHeader(
			"Simulation Time",
			ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::BeginDisabled(!bCanSend);
			if (ImGui::Button("Pause"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::SetSimulationPaused, 1.f);
				state.bSimPauseRequested = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Resume"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::SetSimulationPaused, 0.f);
				state.bSimPauseRequested = false;
			}
			ImGui::SameLine();
			ImGui::TextUnformatted(
				state.bSimPauseRequested ? "(pause requested)" : "(running)");

			ImGui::SetNextItemWidth(110.f);
			ImGui::InputInt("Ticks", &state.simStepTicks);
			if (state.simStepTicks < 1)
				state.simStepTicks = 1;
			if (state.simStepTicks > 300)
				state.simStepTicks = 300;
			ImGui::SameLine();
			if (ImGui::Button("Step"))
			{
				SendPracticeCommand(
					pScene,
					state,
					ePracticeOperation::StepSimulationTicks,
					static_cast<f32_t>(state.simStepTicks));
			}
			ImGui::SameLine();
			if (ImGui::Button("Step 1"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::StepSimulationTicks, 1.f);
			}
			ImGui::SameLine();
			if (ImGui::Button("Step 30 (1s)"))
			{
				SendPracticeCommand(
					pScene, state, ePracticeOperation::StepSimulationTicks, 30.f);
			}

			ImGui::SetNextItemWidth(220.f);
			ImGui::SliderFloat(
				"Time Scale", &state.simTimeScale, 0.1f, 8.f, "%.2fx",
				ImGuiSliderFlags_Logarithmic);
			ImGui::SameLine();
			if (ImGui::Button("Apply Scale"))
			{
				SendPracticeCommand(
					pScene,
					state,
					ePracticeOperation::SetSimulationTimeScale,
					state.simTimeScale);
			}
			ImGui::SameLine();
			if (ImGui::Button("1.0x"))
			{
				state.simTimeScale = 1.f;
				SendPracticeCommand(
					pScene, state, ePracticeOperation::SetSimulationTimeScale, 1.f);
			}
			ImGui::EndDisabled();
			ImGui::TextWrapped(
				"Pause freezes server ticks (world + bots). Gameplay input during "
				"pause is void (still acked); Practice/AI-debug commands keep working. "
				"Step runs N ticks then re-freezes. Time Scale changes wall-clock "
				"pacing only - sim dt stays fixed, determinism preserved.");
		}
```

## 2. 검증

미검증:
- 인게임 실검증(§아래 체크리스트)은 사용자 게이트로 위임.

검증 명령:

```powershell
# FlatBuffers 재생성 (Command.fbs 변경 반영)
Tools/Bin/flatc.exe --cpp --scoped-enums -o Shared/Schemas/Generated/cpp Shared/Schemas/Command.fbs
git diff -- Shared/Schemas/Generated/cpp/Command_generated.h   # enum 3종 추가만 확인

git diff --check
msbuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64
msbuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64
msbuild Tools/SimLab/SimLab.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64
Tools/SimLab의 산출물 실행 -> exit 0 (동일시드 해시 계약 유지 확인)
```

확인 필요:
- flatc 재생성 diff가 enum 값 3종 외 서식 변화를 만들면 flatc 플래그를 기존 생성물 스타일에 맞춘다.
- `CReplayRecorder::Record()` 내부 카운터 구조(1-9 참고).

인게임 검증 (Debug 서버 + 클라, F10 Practice 세션 Enable 후):
1. Pause -> 월드/봇/미니언 전부 정지, 카메라·ImGui·마우스는 생존, 타이틀 응답 없음 미발생 (일시정지 중에도 스냅샷이 흘러 HUD 유지).
2. Pause 상태에서 우클릭 이동 -> 아무 일 없음(입력 void), Resume 후에도 이동 명령 재생 안 됨.
3. Step 1 -> 정확히 한 틱 진행(F9 AI 트레이스 tick이 +1), Step 30 -> 1초 진행.
4. Pause 중 Teleport/Restore HP 등 Practice 명령 -> 즉시 반영(프리즈 틱 스냅샷으로 확인).
5. Time Scale 0.25x -> 슬로모션(애니/이동 전부), 4x -> 배속, 쿨다운·지속시간의 게임 시간 의미는 불변.
6. 매치 종료 후 Replay/*.wrpl 저장 -> 새 리플레이가 v2 헤더, 기존 뷰어 재생 정상(Command 레코드 스킵).
7. 회귀: 일시정지 미사용 일반 매치 진행 정상(게이트 기본값 false).

롤백 범위:
- 본 계획서의 11개 파일 diff만 원복하면 S009 직후 상태로 복귀. 신규 파일 없음, `.vcxproj`/`.filters`/EngineSDK 무변경.
- 구 버전(v1) .wrpl 4개는 버전 검사로 재생 불가가 되나 개발 산출물이므로 수용(필요 시 v1 허용 분기 추가).

다음 슬라이스:
- P1: CSimStateSerializer + 레지스트리 완전성 기계 검사 + SimLab save/restore 골든 프로브 (Engine ECS 접근자 ABI 배치 포함, 별도 packet).
- Codex 조율: T3 트레이스 심화는 S013에, D1 FX 타임스케일은 S012에 이 문서 §0-3 지침 전달.
