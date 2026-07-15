# Work Packet: S014 시뮬 시간 제어 + 명령 저널 (크로노 브레이크 기반층 P0)

## Metadata

- ID: `2026-07-12_s014_sim_time_control_command_journal`
- Status: `Active`
- Agent: `Claude`
- Owner: Desktop
- Branch: `main` (working tree, 미커밋)
- Base: `9110091`

## Owned Paths

- `Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h` (ePracticeOperation append 3)
- `Shared/Schemas/Command.fbs` + `Shared/Schemas/Generated/cpp/Command_generated.h` (flatc 재생성)
- `Shared/Replay/ReplayFormat.h` (v2, Command 레코드, ReplayCommandPayload)
- `Server/Public/Game/GameRoom.h`, `Server/Private/Game/GameRoomTick.cpp`, `Server/Private/Game/GameRoomCommands.cpp`, `Server/Private/Game/GameRoomReplication.cpp`
- `Server/Public/Game/ReplayRecorder.h` + `Server/Private/Game/ReplayRecorder.cpp`
- `Client/Private/Replay/ReplayPlayer.cpp` (Command 레코드 허용)
- `Client/Private/UI/ChampionTuner.cpp` (Simulation Time 섹션)
- `Plan/S014_DESIGNER_SIM_CONSOLE_AND_CHRONO_BREAK_FOUNDATION_SESSION_20260712.md`

## Read-Only Paths

- `Shared/GameSim/Systems/ChampionAI/**`, `Server/Private/Game/SnapshotBuilder.cpp`, `Client/Private/UI/AIDebugPanel.cpp` (Codex S013 선점)
- FX/애니 클라 툴 경로 (Codex S010/S012 선점)

## Validation

- flatc 재생성 diff 확인 (enum 3종 + 기존 S009 상태 재현)
- `git diff --check`
- Server/Client/SimLab Debug x64 빌드 + SimLab 실행 exit 0 (동일시드 해시 계약)
- 인게임 체크리스트: S014 §2

## Handoff Notes

- Bot AI = GameCommand 생산자 원칙 유지. 봇 명령은 저널하지 않음(재시뮬 시 결정론 재생성 = 튜닝 루프의 원리).
- TimeScale은 틱 주기(wall-clock)만 변경, fDt 불변 — SimLab 해시·리플레이 계약 보존.
- 다음 슬라이스 P1 = CSimStateSerializer + SimLab save/restore 골든 프로브 (Engine ECS 접근자 ABI 배치 포함, 새 packet).
- T3 트레이스 심화는 S013에, D1 FX 타임스케일은 S012에 S014 §0-3 지침 전달.
