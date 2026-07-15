Session - Claude S014 Active packet의 소유 파일은 건드리지 않은 채 WRPL 호환·Designer control 권한·authoritative linked clock을 병합 게이트로 고정하고, 이후 ToolDocument/AI Decision Lab/WFX·Animation·Model·UI·Map·Boss/Chrono branch를 `WINTERS_LIVE_AUTHORING_CHRONOBREAK_ARCHITECTURE.md`의 단일 제작 루프로 연결한다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Shared/Replay/ReplayFormat.h

Claude S014 handoff 뒤 아래 기존 코드를:

```cpp
	inline constexpr u16_t kReplayVersion = 2;
```

아래로 교체:

```cpp
	inline constexpr u16_t kReplayMinSupportedVersion = 1;
	inline constexpr u16_t kReplayVersion = 2;
```

`IsReplayMagic` 바로 아래에 추가:

```cpp
	inline bool_t IsSupportedReplayVersion(u16_t version)
	{
		return version >= kReplayMinSupportedVersion &&
			version <= kReplayVersion;
	}

	inline bool_t IsReplayRecordTypeSupported(u16_t version, u8_t type)
	{
		if (type == static_cast<u8_t>(eReplayRecordType::Snapshot) ||
			type == static_cast<u8_t>(eReplayRecordType::Event))
		{
			return true;
		}

		return version >= 2u &&
			type == static_cast<u8_t>(eReplayRecordType::Command);
	}
```

`ReplayCommandPayload::reserved0`는 Chrono re-sim 전에 command domain으로 교체해야 한다. S014 P0가 아직 Active이므로 아래 변경은 `CONFIRM_NEEDED`다. S014 handoff 시 v2 파일이 이미 생성됐는지 먼저 검사하고, 생성됐다면 version 3 migration으로 올린다.

```cpp
enum class ReplayCommandDomain : u8_t
{
	PlayerInput = 1,
	SimulationMutation = 2,
	AuthoringPatch = 3,
	ControlPlane = 4,
	ObservationOnly = 5,
};
```

분류 규칙은 다음으로 고정한다.

```text
Move/BA/Skill/Item                 -> PlayerInput
Teleport/Spawn/HP/Level/Scenario   -> SimulationMutation
Balance/AI/Boss revision patch     -> AuthoringPatch
Pause/Step/TimeScale/Rewind/Branch -> ControlPlane
Trace subscribe/export/breakpoint  -> ObservationOnly
```

### 1-2. C:/Users/user/Desktop/Winters/Client/Private/Replay/ReplayPlayer.cpp

anonymous namespace의 아래 기존 함수를 삭제:

```cpp
	bool_t IsValidReplayRecordType(u8_t type)
	{
		return type == static_cast<u8_t>(Winters::Replay::eReplayRecordType::Snapshot) ||
			type == static_cast<u8_t>(Winters::Replay::eReplayRecordType::Event) ||
			type == static_cast<u8_t>(Winters::Replay::eReplayRecordType::Command);
	}
```

`LoadFromFile`의 아래 기존 코드를:

```cpp
	if (!Winters::Replay::IsReplayMagic(player->m_Header) ||
		player->m_Header.version != Winters::Replay::kReplayVersion ||
		player->m_Header.headerSize != Winters::Replay::kReplayHeaderSize)
```

아래로 교체:

```cpp
	if (!Winters::Replay::IsReplayMagic(player->m_Header) ||
		!Winters::Replay::IsSupportedReplayVersion(player->m_Header.version) ||
		player->m_Header.headerSize != Winters::Replay::kReplayHeaderSize)
```

record header 검증의 아래 기존 코드를:

```cpp
		if (record.header.headerSize != Winters::Replay::kReplayRecordHeaderSize ||
			record.header.payloadSize == 0 ||
			record.header.payloadSize > kMaxReplayPayloadBytes ||
			!IsValidReplayRecordType(record.header.type))
```

아래로 교체:

```cpp
		if (record.header.headerSize != Winters::Replay::kReplayRecordHeaderSize ||
			record.header.payloadSize == 0 ||
			record.header.payloadSize > kMaxReplayPayloadBytes ||
			!Winters::Replay::IsReplayRecordTypeSupported(
				player->m_Header.version,
				record.header.type))
```

### 1-3. C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h

`CONFIRM_NEEDED`: 이 파일은 Claude work packet `2026-07-12_s014_sim_time_control_command_journal`의 Active owned path다. handoff 전에 수정하지 않는다.

handoff 뒤 `TickPausedControlLane` 아래에 다음 책임을 가진 private helper를 추가하는 code-preview를 다시 확정한다.

```cpp
void RecordReplayCommand(u64_t tick, const PendingCommand& pending);
bool_t TryHandleAIDebugControl(const TickContext& tc, const GameCommand& cmd);
```

Simulation time 상태 아래에는 다음 revision을 추가한다.

```cpp
u64_t m_toolRevision = 0;
u64_t m_lastReplayToolRevision = 0;
u32_t m_timelineEpoch = 1;
u32_t m_branchId = 0;
```

확인할 항목:

- `PendingCommand`가 header에서 완전형으로 보이는 현재 `Game/CommandIngress.h` include 유지.
- accepted mutation만 `m_toolRevision`을 증가시키는 단일 owner.
- ControlPlane command는 revision/audit에는 남겨도 Chrono re-sim 입력에서는 제외.
- rewind 뒤 transport command ack는 감소시키지 않음.

### 1-4. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomCommands.cpp

`CONFIRM_NEEDED`: Claude S014 Active hunk가 끝난 뒤 현재 본문을 다시 읽고 exact replacement를 작성한다. 다음 네 조건은 병합 전 필수다.

`SetEnabled(false)`의 아래 기존 블록:

```cpp
		if (!bEnable)
		{
			ClearPracticeSpawns();
```

안에서 `ClearPracticeSpawns()` 바로 위에 추가:

```cpp
			m_bSimPaused = false;
			m_simStepBudget = 0;
			m_simSpeedMul.store(1.f, std::memory_order_relaxed);
```

그 외 필수 변경:

- `Phase_DrainCommands`의 Replay payload 작성 블록을 `RecordReplayCommand` 한 곳으로 추출.
- `TickPausedControlLane`에서 실제 실행하는 Practice/AIDebug command도 같은 helper로 기록.
- pause 중 void 처리한 일반 gameplay input은 Simulation re-sim journal에 넣지 않음.
- same-tick accepted mutation 뒤 `m_toolRevision`을 증가시키고 최종 snapshot을 기록.
- AIDebugControl도 Practice와 같은 host/capability gate를 거친 뒤 executor에 전달.
- target AI 유효성, tuning id/value range를 GameRoom/Shared validator가 각각 자기 경계에서 확인.

### 1-5. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomReplication.cpp

`CONFIRM_NEEDED`: Claude S014 Active hunk 종료 후 아래 replay snapshot guard를 tick 하나만으로 판단하지 않도록 교체한다.

현재 기준 코드:

```cpp
	if (m_pReplayRecorder && !m_bReplayFinalized && m_pSnapBuilder &&
		tc.tickIndex != m_lastReplaySnapshotTick)
```

교체 조건:

```text
record snapshot when
  tick changed
  OR accepted tool revision changed at the same paused tick
```

snapshot 기록 성공 뒤 `m_lastReplaySnapshotTick`과 `m_lastReplayToolRevision`을 함께 갱신한다. 이 변경 전에는 pause 중 Teleport/AI tuning의 최종 상태가 forward replay snapshot에 남는다고 주장하지 않는다.

### 1-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

`CDefaultCommandExecutor::ExecuteCommand` 시작의 아래 기존 코드 위에 추가:

```cpp
	if (cmd.kind == eCommandKind::AIDebugControl)
	{
		HandleAIDebugControl(world, tc, cmd);
		return;
	}
```

기준 기존 코드:

```cpp
	if (cmd.issuerEntity == NULL_ENTITY || !world.IsAlive(cmd.issuerEntity))
		return;
```

이렇게 하면 authorized Designer control이 플레이어 issuer의 사망 때문에 막히지 않는다. Server GameRoom host gate를 먼저 통과한 명령만 이 경로에 도달해야 한다.

main command switch의 아래 코드는 삭제:

```cpp
	case eCommandKind::AIDebugControl:
		HandleAIDebugControl(world, tc, cmd);
		break;
```

`ApplyChampionAITuningOverride`의 아래 코드를 삭제:

```cpp
	ai.tuning.bOverrideProfile = true;
```

single parameter edit는 해당 `ChampionAITuningParam::bOverride`만 켠다. Whole-profile replace는 모든 값을 검증한 별도 atomic patch command로만 제공한다.

### 1-7. C:/Users/user/Desktop/Winters/Shared/Schemas/Snapshot.fbs

`table Snapshot`의 마지막 기존 코드:

```text
    blueBarons:ushort;
    redBarons:ushort;
}
```

아래로 교체:

```text
    blueBarons:ushort;
    redBarons:ushort;

    simulationPaused:bool;
    simulationTimeScale:float = 1.0;
    toolRevision:ulong;
    timelineEpoch:uint = 1;
    branchId:uint;
}
```

이 값은 요청 버튼의 optimistic state가 아니라 Server authoritative room state다.

### 1-8. C:/Users/user/Desktop/Winters/Server/Public/Game/SnapshotBuilder.h

`Build`의 마지막 두 인자 아래에 room tool state를 추가한다.

기존 코드:

```cpp
		u32_t lastAckedSeq,
		NetEntityId yourNetId);
```

아래로 교체:

```cpp
		u32_t lastAckedSeq,
		NetEntityId yourNetId,
		bool_t simulationPaused,
		f32_t simulationTimeScale,
		u64_t toolRevision,
		u32_t timelineEpoch,
		u32_t branchId);
```

### 1-9. C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp

`CSnapshotBuilder::Build` 정의를 1-8의 signature와 동일하게 교체한다.

파일 끝 `Shared::Schema::CreateSnapshot` 호출의 기존 마지막 인자:

```cpp
		matchScore.Red.iBarons);
```

아래로 교체:

```cpp
		matchScore.Red.iBarons,
		simulationPaused,
		simulationTimeScale,
		toolRevision,
		timelineEpoch,
		branchId);
```

### 1-10. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomReplication.cpp

1-5의 Active hunk handoff 뒤 replay/session 두 `m_pSnapBuilder->Build` 호출 마지막에 아래를 추가:

```cpp
		m_bSimPaused,
		m_simSpeedMul.load(std::memory_order_relaxed),
		m_toolRevision,
		m_timelineEpoch,
		m_branchId
```

### 1-11. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/SnapshotApplier.h

`GetLastAppliedServerTick` 바로 아래에 추가:

```cpp
	bool_t IsSimulationPaused() const { return m_bSimulationPaused; }
	f32_t GetSimulationTimeScale() const { return m_fSimulationTimeScale; }
	u64_t GetToolRevision() const { return m_toolRevision; }
	u32_t GetTimelineEpoch() const { return m_timelineEpoch; }
	u32_t GetBranchId() const { return m_branchId; }
```

`m_lastServerTick` 바로 아래에 추가:

```cpp
	bool_t m_bSimulationPaused = false;
	f32_t m_fSimulationTimeScale = 1.f;
	u64_t m_toolRevision = 0;
	u32_t m_timelineEpoch = 1;
	u32_t m_branchId = 0;
```

### 1-12. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

`m_lastServerTick = snapshot->serverTick();` 바로 아래에 추가:

```cpp
	m_bSimulationPaused = snapshot->simulationPaused();
	m_fSimulationTimeScale = snapshot->simulationTimeScale();
	if (!std::isfinite(m_fSimulationTimeScale) ||
		m_fSimulationTimeScale < 0.1f ||
		m_fSimulationTimeScale > 8.f)
	{
		m_fSimulationTimeScale = 1.f;
	}
	m_toolRevision = snapshot->toolRevision();
	m_timelineEpoch = snapshot->timelineEpoch();
	m_branchId = snapshot->branchId();
```

`<cmath>` include 존재 여부를 확인하고 없으면 include 영역에 추가한다.

### 1-13. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

`Update`에서 camera/input을 갱신하기 전, authoritative presentation delta를 한 번 계산한다.

```cpp
	f32_t presentationDt = dt;
	if (m_bNetworkAuthoritativeGameplay && m_pSnapshotApplier)
	{
		presentationDt = m_pSnapshotApplier->IsSimulationPaused()
			? 0.f
			: dt * m_pSnapshotApplier->GetSimulationTimeScale();
	}
```

Champion animation과 FX 3종의 아래 호출만 `dt`에서 `presentationDt`로 교체한다.

```cpp
	m_World.ForEach<ChampionComponent, RenderComponent>(
		[presentationDt](EntityID, ChampionComponent&, RenderComponent& rc)
		{
			if (rc.bSceneManaged) return;
			if (!rc.pRenderer || !rc.bAnimated) return;
			if (!rc.pRenderer->HasSkeleton()) return;
			rc.pRenderer->Update(presentationDt);
		}
	);

	if (m_pFxSystem)     m_pFxSystem->Update(m_World, presentationDt);
	if (m_pFxBeamSystem) m_pFxBeamSystem->Update(m_World, presentationDt);
	if (m_pFxMeshSystem) m_pFxMeshSystem->Update(m_World, presentationDt);
```

Camera, ImGui, mouse, network pump은 `dt`를 계속 사용한다. Ambient prop, UI floater, audio의 linked/freeze 정책은 이 packet에서 임의로 바꾸지 않고 `CONFIRM_NEEDED`로 남긴다.

### 1-14. C:/Users/user/Desktop/Winters/Engine/Public/Tools/ToolDocument.h

`CONFIRM_NEEDED`: 새 파일이다. 구현 전 아래 파일의 generic stack과 product-specific command 결합을 다시 분리해 전체 파일 본문을 작성한다.

```text
EldenRingEditor/Public/World/EditorTransaction.h
EldenRingEditor/Private/World/EditorTransaction.cpp
Client/Public/GameObject/FX/WfxDocument.h
Client/Private/UI/ChampionTuner.cpp
Engine/Private/Manager/UI/ActorHUDPanel.cpp
```

필수 계약:

```text
DocumentId / CurrentRevision / SavedRevision / AppliedRevision
Dirty / CanUndo / CanRedo
BeginTransaction / Commit / Cancel / Undo / Redo
Validation / Autosave / Recovery
```

complete body가 확정되기 전 placeholder header를 만들지 않는다.

### 1-15. C:/Users/user/Desktop/Winters/Engine/Public/Tools/AtomicFileWriter.h

`CONFIRM_NEEDED`: 새 파일이다. temp write -> flush -> reparse/validate -> backup -> same-volume replace -> last-good recovery의 complete body와 Windows error handling을 code-preview packet에서 작성한다. WFX, HUD, map이 각자 새 writer를 만들지 않는다.

### 1-16. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ChampionAIComponent.h

`CONFIRM_NEEDED`: AI Decision Lab packet에서 현재 16행 결과 trace를 다음 evidence로 확장한다.

```text
retreat score
observation/policy/tuning revision hash
candidate fixed array
hard gate/reject reason
feature id/value/weight/contribution fixed array
decision kind
commitment before/after
executor result
```

모든 bot의 전체 trace를 매 snapshot에 싣지 않는다. Server의 selected-bot subscription 계약과 Snapshot schema를 같은 packet에서 완성한 뒤 exact struct body를 작성한다.

### 1-17. C:/Users/user/Desktop/Winters/Client/Public/GameObject/FX/WfxDocument.h

`CONFIRM_NEEDED`: Gate 1의 ToolDocument/AtomicFileWriter가 확정된 뒤 S012 방향으로 dirty/revision/undo/redo/transaction을 연결한다. Graph UI를 먼저 추가하거나 WFX v1 scalar writer 옆에 두 번째 FX document/runtime을 만들지 않는다.

### 1-18. C:/Users/user/Desktop/Winters/Engine/Public/Resource/Animator.h

`CONFIRM_NEEDED`: Model/Animation packet에서 exact clip identity와 `EvaluateAtTime` semantics를 먼저 확정한다. `CSequencePlayer::Seek`가 Anim/FX pose를 재구성하도록 resolver 계약까지 함께 수정하며, 임시 `SetCurrentTime`만 추가하지 않는다.

### 1-19. C:/Users/user/Desktop/Winters/Engine/Public/Renderer/ModelRenderer.h

`CONFIRM_NEEDED`: clip/submesh/bone inspector view와 per-instance material override API를 complete code로 작성한다. 현재 shared cached `CModel`을 `LoadTexture/LoadMeshTexture`로 preview instance가 변조하는 경로는 사용하지 않는다.

### 1-20. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameImGui.cpp

`CONFIRM_NEEDED`: DesignerHub packet에서 F7/F8/F9/F10 bool/call을 panel registry로 옮긴다. 기존 단축키와 normal F5 panel entry는 유지하고, Live Authoritative Lab과 Isolated Preview Workspace를 명시적으로 나눈 complete code-preview를 작성한다.

### 1-21. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_Editor.cpp

`CONFIRM_NEEDED`: map packet에서 document transaction/gizmo/atomic save를 연결한다. M 키 scene swap을 곧바로 “한 실행의 live editor”로 간주하지 않는다. authoritative map reload는 Server room reset + timeline epoch/full snapshot 계약과 함께 작성한다.

### 1-22. C:/Users/user/Desktop/Winters/EldenRingEditor/Public/AI/BossDebugBridge.h

`CONFIRM_NEEDED`: 현 경로와 타입은 아직 존재하지 않는다. `.md/plan/EldenRingEditor/07_BOSS_BLACKBOARD_HFSM_BT_TUNING.md`의 `CONFIRM_NEEDED`를 먼저 해소한 뒤 새 파일 complete body를 작성한다. 공용 shell/evidence/transaction UI만 공유하고 LoL ChampionAI gameplay type을 include하지 않는다.

## 2. 검증

협업 게이트:

```text
Plan/S014... 및 .md/collab/work-packets/2026-07-12_s014... Status가 Active인 동안
1-1~1-5, 1-10 파일을 수정하지 않는다.
handoff 후 git diff로 Claude 소유 hunk와 S009/S011/S013 기존 hunk를 분리한다.
파일 전체 rollback은 금지하고 소유 hunk만 역패치한다.
```

Gate 0 검증 명령:

```powershell
Tools/Bin/flatc.exe --cpp --scoped-enums -o Shared/Schemas/Generated/cpp Shared/Schemas/Snapshot.fbs
git diff --check
MSBuild Shared/GameSim/Include/GameSim.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
MSBuild Tools/SimLab/SimLab.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
Tools/Bin/Debug/SimLab.exe 1800 42
MSBuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
MSBuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

자동 probe 완료 기준:

```text
PASS existing WRPL v1 files load and play
PASS v1 rejects Command records, v2 accepts them
PASS disabling Practice while paused restores running 1.0x lifecycle
PASS paused accepted mutation increments revision and is journal/snapshot visible
PASS ControlPlane commands are excluded from Chrono re-sim input
PASS non-host AIDebug mutation is rejected
PASS authorized AIDebug works even when the host champion is dead
PASS one AI tuning parameter changes only that parameter
PASS same-tick frozen snapshots are still applied by the client
```

수동 인게임 Gate 0:

```text
1. F10 Enable -> Pause. World, champion animation, FX age, F9 trace tick이 정지한다.
2. Camera, mouse, ImGui, network 상태는 계속 응답한다.
3. Step 1/30에서 server tick, animation/FX authoritative phase가 각각 정확히 전진한다.
4. 0.25x/4x에서 gameplay fDt 의미는 불변이고 linked presentation만 같은 wall scale을 따른다.
5. Pause 중 Teleport와 AI tuning을 실행하고 replay/final snapshot에서 결과를 확인한다.
6. Pause 중 Disable을 눌러도 즉시 running 1.0x로 복귀한다.
7. 두 번째 LAN Client의 AIDebug mutation은 reject되고 authoritative 상태는 변하지 않는다.
8. 기존 Replay/*.wrpl v1 네 개와 새 v2 replay를 모두 연다.
```

Gate 1 이후 공통 검증:

```text
Document save 실패 -> 기존 파일 hash 불변
한 drag gesture -> Undo transaction 1건
server patch partial apply 없음
requested/applied/canonical revision UI 구분
WFX preview와 normal F5가 같은 CFxCuePlayer path 사용
preview material edit가 같은 model의 다른 live instance를 변경하지 않음
```

Chrono golden probe:

```text
run N -> end-of-tick checkpoint -> run M -> complete hash A
restore checkpoint -> same eligible journal -> run M -> complete hash B
A == B

restore checkpoint -> apply branch patch before AI phase -> run M
only declared patch/revision and downstream consequences may diverge
```

30% ceiling 인게임 산출물:

```text
AI: 같은 Perception에서 weight A/B re-score 표와 JSONL trace 1개
VFX: Annie Q timeline/curve/scrub/undo before-after capture 1개
```

이 두 산출물 전에는 full node graph, 150 champion 일괄 migration, 대규모 GPU particle rewrite를 시작하지 않는다.
