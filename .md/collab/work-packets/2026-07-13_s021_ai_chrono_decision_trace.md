# Work Packet: S021 AI 선택 근거 + Chrono branch timeline

## Metadata

- ID: `2026-07-13_s021_ai_chrono_decision_trace`
- Status: `Handoff`
- Agent: `Codex`
- Owner: Desktop
- Branch: `main` (working tree, 미커밋)
- Base: `9110091`

## Owned Paths

- `.md/plan/2026-07-13_S021_AI_CHRONO_DECISION_TRACE_PLAN.md`
- `.md/build/2026-07-13_S021_AI_CHRONO_DECISION_TRACE_REPORT.md`
- `.md/collab/ACTIVE_WORK_PACKETS.md`
- `.md/collab/work-packets/2026-07-13_s021_ai_chrono_decision_trace.md`
- `Shared/GameSim/Components/ChampionAIComponent.h`
- `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`
- `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`
- `Shared/Schemas/Snapshot.fbs`
- `Shared/Schemas/Generated/cpp/Snapshot_generated.h`
- `Shared/Schemas/Generated/go/Shared/Schema/AIDebugTraceRow.go` (동일 codegen 실행의 기존 dirty 산출물은 보존)
- `Server/Private/Game/SnapshotBuilder.cpp`
- `Server/Private/Game/GameRoomCommands.cpp`
- `Client/Private/Network/Client/SnapshotApplier.cpp`
- `Client/Private/UI/AIDebugPanel.cpp`
- `Tools/SimLab/main.cpp`

## Read-Only Paths

- S020 Claude lane 문서와 누적 dirty 변경 중 위 Owned Paths 밖의 전체 파일
- `Server/Public/Game/GameRoom.h`
- `Server/Private/Game/GameRoomTick.cpp`
- `Client/Public/Scene/Scene_InGame.h`
- `Client/Public/Network/Client/SnapshotApplier.h`
- `Client/Public/Network/Client/CommandSerializer.h`
- `Client/Private/UI/ChampionTuner.cpp`
- `Shared/GameSim/Core/Checkpoint/**`
- `Tools/AIResearch/**`

## Validation

- S020 관련 diff의 기능 단위 정적 감사와 기존 dirty 변경 보존 확인
- SimLab recall/command outcome/keyframe/determinism probes
- GameSim, SimLab, Server, Client Debug x64 빌드
- `git diff --check` (S021 owned paths)

## Handoff Notes

- S015의 30-tick keyframe/transactional restore와 timeline epoch/branch를 재사용한다.
- F9 기록은 패널이 열린 동안 선택 bot만 client-side bounded sampling한다. 별도 30 Hz server history나 gameplay state owner를 추가하지 않는다.
- `AiEpisodeV1`의 Retreat/Fight/Farm/Siege 계약은 변경하지 않는다. Recall은 deterministic lifecycle 회귀로 고친다.
- 기존 S020과 S017 이후 누적 dirty 변경은 reset/revert하지 않고 exact anchor에만 수술한다.
- 구현/자동 검증 완료. 전체 결과와 잔여 exact Chrono/WRPL/DecisionLedger 경계는 `.md/build/2026-07-13_S021_AI_CHRONO_DECISION_TRACE_REPORT.md` 참조.
