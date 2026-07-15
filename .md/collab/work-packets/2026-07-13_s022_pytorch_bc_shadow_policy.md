# Work Packet: S022 PyTorch BC Shadow Policy

## Metadata

- ID: `2026-07-13_s022_pytorch_bc_shadow_policy`
- Status: `Handoff`
- Agent: `Codex`
- Owner: Desktop
- Branch: `main` (working tree, 미커밋)
- Base: `9110091`

## Owned Paths

- `.md/plan/2026-07-13_S022_PYTORCH_BC_SHADOW_POLICY_PLAN.md`
- `.md/build/2026-07-13_S022_PYTORCH_BC_SHADOW_POLICY_REPORT.md`
- `.md/collab/ACTIVE_WORK_PACKETS.md`
- `.md/collab/work-packets/2026-07-13_s022_pytorch_bc_shadow_policy.md`
- `Tools/AIResearch/TrainImitationRankingBaseline.py`
- `Tools/AIResearch/tests/test_train_imitation_ranking_baseline.py`
- `Tools/AIResearch/tests/RunLiveAiEpisodeSmokeProbe.ps1`
- `Tools/AIResearch/RunValidation.ps1`
- `Tools/AIResearch/README.md`
- `Tools/SimLab/main.cpp`
- `Shared/GameSim/Components/JaxSimComponent.h`
- `Shared/GameSim/Components/ChampionAIComponent.h`
- `Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h`
- `Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.cpp`
- `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h`
- `Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp`
- `Shared/Schemas/Snapshot.fbs`
- `Shared/Schemas/Generated/cpp/Snapshot_generated.h`
- `Shared/Schemas/Generated/go/Shared/Schema/AIDebugTraceRow.go`
- `Server/Private/main.cpp`
- `Server/Public/Game/GameRoom.h`
- `Server/Private/Game/GameRoom.cpp`
- `Server/Public/Game/ServerAICommandProducer.h`
- `Server/Private/Game/ServerAICommandProducer.cpp`
- `Server/Private/Game/GameRoomChampionAI.cpp`
- `Server/Private/Game/SnapshotBuilder.cpp`
- `Client/Private/Network/Client/SnapshotApplier.cpp`
- `Client/Private/UI/AIDebugPanel.cpp`

## Read-Only Paths

- S021/S020/S017 누적 dirty 변경 중 위 Owned Paths 밖의 전체 파일
- `Shared/GameSim/Core/Checkpoint/**`
- `Shared/GameSim/Systems/CommandExecutor/**`
- `Server/Private/Game/GameRoomCommands.cpp`
- `Shared/Replay/**`
- `Client/Private/Replay/**`
- Engine AI/BT/MCTS/RL placeholder 전체

## Validation

- Python deterministic masked BC/report/binary tests와 두 실행 SHA equality
- 4 scenario x 2 side x 8 parameterized measured corpus repeat/promotion validation
- Python/C++ logit parity와 shadow non-interference/decision cadence/checkpoint probes
- schema codegen, Shared boundary, S021 SimLab 회귀
- GameSim, SimLab, Server, Client, Winters.sln Debug x64 빌드
- 실제 full-map Client F9/Chrono rewind 전후 눈검증과 PNG 증거
- scoped `git diff --check`

## Handoff Notes

- active gameplay는 기존 RuleBased/PlayerLike가 계속 소유한다. learned artifact는 S022에서 shadow-only다.
- Python/PyTorch는 offline trainer이며 GameSim transition truth나 Client runtime을 소유하지 않는다.
- 기존 240-tick live smoke는 contract smoke로 유지하고 measured corpus CLI를 별도로 둔다.
- `AiEpisodeV1` ABI, exact Chrono journal, PPO/DAgger/5v5 league는 이번 packet에서 확장하지 않는다.
- S021 owned path의 현재 dirty anchor를 인수하되 기존 필드와 동작을 보존하고 전체 S021 gate를 재실행한다.

## Final Handoff - 2026-07-13

- SimLab Debug x64 `/m:1` PASS. 최종 executable SHA-256: `91A9BA0A5EC2C04AFED66C68DA9D025E82355BEF32879ED4141593990C836BD7`.
- `SimLab.exe 1800 42` 연속 3/3 PASS. same seed `DB0DC85E451999AD`, seed+1 `57A9B2394575042A`.
- Shadow evaluated/disagreed를 실제 관찰하면서 300-tick command/state/raw-keyframe non-interference PASS.
- 최종 measured corpus A/B: 각 64 records, 32 mirrored groups, 네 class 각 16, byte-identical SHA `98498BF4E2E3EEFF85B5F8C164A14F9CA52144109BB0785AC93B0F43CF8A4268`.
- PyTorch BC A/B report와 WBC byte-identical. report SHA `77D1752F1D238E8DD00D1A9F3394EF2FFB74FE60317A56F43FDE411D5238AE34`, WBC SHA `AE3F6AEAF7D67D05038220866B375AC223F2072A6FA5B2123DE3E4D6ABF53B3D`.
- C++ parity max absolute logit delta `1.89092278724e-7`, threshold `1e-5` 이내.
- `Tools/AIResearch/RunValidation.ps1` exit 0, Python 71/71과 static/native contract/Shared boundary PASS.
- 최초 shadow 실패는 `JaxSimComponent` raw POD padding flake였다. 같은 header의 다섯 gap을 explicit zero-init reserved bytes로 바꾸고 ABI/offset static assert를 추가했으며 raw keyframe equality gate는 유지했다.
- S023 build-lock 합의에 따라 최종 Server/Client/solution build와 visible F9 capture는 통합 owner에게 넘긴다. 이 미실행 항목을 PASS로 주장하지 않는다.
- 결과 보고서: `.md/build/2026-07-13_S022_PYTORCH_BC_SHADOW_POLICY_REPORT.md`.
