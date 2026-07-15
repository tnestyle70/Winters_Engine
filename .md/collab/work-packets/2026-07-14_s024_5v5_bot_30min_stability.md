# Work Packet: S024 5v5 Bot 30-Minute Stability

## Metadata

- ID: `2026-07-14_s024_5v5_bot_30min_stability`
- Status: `Handoff`
- Agent: `Codex`
- Owner: Desktop
- Branch: `main` (shared dirty working tree, no commit created)
- Base: current shared working tree after S023/S022 handoff
- Started/Finished: `2026-07-14`

## Objective

실제 Server `CGameRoom`에서 10개 ChampionAI bot과 gameplay objects, minion waves, snapshot/event/replay/keyframe 경로를 30 Hz 기준 54,000 tick 동안 실행하고 crash, hang, non-finite state, determinism drift, entity/resource leak, lifecycle/finalize 실패를 fail-closed로 검증한다.

## Owned Paths

- `Tools/Harness/GameRoomBotMatchSoak.cpp`
- `Tools/Harness/RunGameRoomBotMatchSoak.ps1`
- `Tools/Harness/ReplayCommandContractProbe.cpp` (기존 S014 probe에 sealed publish retry 계약 보강)
- `Server/Public/Game/ReplayRecorder.h`
- `Server/Private/Game/ReplayRecorder.cpp`
- `Server/Private/Game/GameRoomReplication.cpp`
- `Server/Public/Game/GameRoom.h`
- `Server/Public/Game/LobbyAuthority.h`
- `Server/Private/Game/GameRoom.cpp`
- `Shared/GameSim/Systems/Recall/RecallSystem.cpp`
- `Shared/GameSim/Systems/Turret/TurretAISystem.cpp`
- `Shared/GameSim/Systems/Move/MoveSystem.cpp`
- `Shared/GameSim/Systems/AttackChase/AttackChaseSystem.cpp`
- `Shared/GameSim/Systems/Combat/CombatActionSystem.cpp`
- `Shared/GameSim/Systems/WaypointPatrol/WaypointPatrolSystem.cpp`
- `Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp`
- `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`
- `Shared/GameSim/Components/MoveTargetComponent.h`
- `Shared/GameSim/Core/Checkpoint/WorldKeyframe.cpp`
- S024 deterministic-padding 대상 `Shared/GameSim/Components/{ActionStateComponent,AnnieSimComponent,AsheSimComponent,BuffComponent,ChampionAIComponent,GameplayComponents,LeeSinSimComponent,PoseStateComponent,StatComponent,ViegoSimComponent,YoneSimComponent,ZedSimComponent}.h`
- `Engine/Public/ECS/Components/{CoreComponents,NavigationThrottleComponent,SpatialAgentComponent,VisionComponents}.h`
- 동일 `EngineSDK/inc/ECS/Components` mirror 4종
- `.md/build/2026-07-14_S024_5V5_BOT_30MIN_STABILITY_REPORT.md`
- `.md/collab/work-packets/2026-07-14_s024_5v5_bot_30min_stability.md`
- `.md/collab/ACTIVE_WORK_PACKETS.md`의 S024/S025 행

## Excluded Paths

- `Tools/AIResearch/**`, `Tools/SimLab/**` S025 lane
- Shared/Server/Client network와 UDP transport
- Engine JobSystem/Chase-Lev/Fiber
- Client runtime/render path

## Final Validation

- Release Server/GameSim freshness build `/m:1`: PASS
- SharedBoundary: PASS
- Replay command/sealed publish contract: PASS
- Release 1,800-tick same-seed A/B: PASS
  - replay: `9D4F43DF5201865E`
  - world: `D789194186577787`
- v4 이전 Release 20,000-tick 기능·liveness regression: PASS
- Release 54,000-tick seed 42 A/B: PASS/PASS
  - replay/world both: `F172FA227ACA7576` / `3B12304F5110E999`
  - p50/p95/p99/max: 1.868/3.043/3.440/40.926 ms; 1.979/3.294/3.551/22.644 ms
  - deadline miss: 6/54,000; 0/54,000
  - entities initial/peak/final: 53/130/97
  - deaths/respawns: 20/20
  - command-active/inactive bots: 10/0
  - private growth/post-stop: 2.902/27.750 MiB; 2.594/27.879 MiB
  - replay each: 143,018 records, 1,420,783,432 bytes, finalize 0.666/0.639 sec
  - final-state SHA-256 both: `EA8CBF81223AB0A31B785596CD31562F5973B100AFBC62BDD3AAD9765982C19F`
- SimLab Debug x64 keyframe v4 restore/determinism: PASS
- Evidence: `.md/build/evidence/s024_bot_soak/release_ticks_54000_seed_42_20260714_032420_855_acca44c0`

## Handoff Notes

- 이 결과는 54,000 `CGameRoom::Tick`의 30분 simulation-time accelerated headless soak다.
- Client, socket/IOCP, TCP/UDP/dual, JobSystem/Fiber 실행은 비범위다.
- 54,000-tick 장시간 A/B 두 회와 별도 1,800-tick A/B를 수행했다.
- v3 장기 cross-run에서 발견한 26-byte raw padding drift는 `VisibilityComponent`와 `ZedSimComponent`의 명시적 zero-reserved field 및 keyframe v4로 닫았다.
- 외부 service CPU 경합으로 성능이 FAIL한 v3 54,000-tick run도 별도 evidence로 보존했다.
- 기존 dirty 변경을 reset/revert/stage/commit하지 않았다.
- S025 AI lane은 별도 Handoff이며 build lock을 직렬화했다.
