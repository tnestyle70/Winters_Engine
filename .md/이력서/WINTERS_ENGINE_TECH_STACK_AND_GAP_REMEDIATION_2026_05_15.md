# Winters Engine 기술 스택 및 보완 로드맵 - 2026-05-15

이 문서는 이력서/포트폴리오 정리용이다. 현재 Winters Engine이 이미 증명한 기술 스택과, 채용자 관점에서 아직 보강하면 좋은 부족분을 링크 중심으로 정리한다.

## 현재 기술 스택

| 영역 | 현재 Winters 근거 | 이력서 표현 |
|---|---|---|
| 언어/플랫폼 | C++20, Win32, DLL 기반 Engine/Client/Server 분리 | C++20 기반 Windows game engine runtime 구현 |
| 렌더링 | DX11 runtime, DX12 smoke path, RHI abstraction, renderer pass, FX renderer | DX11/DX12 RHI abstraction 및 renderer pipeline 구현 |
| ECS/GameSim | `Engine/Public/ECS`, `Shared/GameSim` | ECS 기반 deterministic gameplay simulation 설계 |
| 서버 권위 | `Server/GameRoom`, `Shared/GameSim`, Snapshot/Event | Client Input -> Server GameSim -> Snapshot/Event -> Client Visual 구조 구현 |
| 네트워크 | TCP IOCP prototype, PacketEnvelope, FlatBuffers schema | IOCP 기반 multiplayer server prototype 및 FlatBuffers snapshot replication |
| 챔피언/스킬 | champion GameSim, SkillState, cooldown/rank/stat systems | MOBA champion skill system, cooldown, stat scaling, projectile/damage pipeline 구현 |
| AI | BotLaneAISystem, BT/MCTS/RL 계획 문서 | server-side command-producing bot AI 구조 설계 |
| FX | `Engine/Public/FX`, client FX systems, visual hook registry | data-driven VFX asset/runtime 기반 구축 |
| 에셋 포맷 | `.wmesh`, `.wskel`, `.wanim`, Assimp converter | custom binary asset format 및 import/conversion pipeline 구현 |
| 백엔드 | Go Services: auth, leaderboard, matchmaking, profile, payment, shop | Go microservice backend, PostgreSQL/Redis/Kafka/JWT 기반 서비스 구현 |
| 리플레이 | `Shared/Replay`, `Server/ReplayRecorder` | authoritative snapshot replay container 설계/구현 중 |
| UI/도구 | ImGui editor/tuner/debug panels | in-engine debug/editor tooling 구현 |
| 사운드 | FMOD wrapper | FMOD 기반 audio integration |

## 보완/보안 사항 링크

| 부족분 | 보완 문서 | 이력서에서 노릴 증명 |
|---|---|---|
| LoL 5-client server authority 완성 | [01_LOL_5CLIENT_SERVER_AUTHORITY_COMPLETION](../TODO/05-15/01_LOL_5CLIENT_SERVER_AUTHORITY_COMPLETION.md) | 5-client authoritative MOBA gameplay loop |
| UDP production netstack | [02_UDP_M1_M3_TRANSPORT_RELIABILITY_DELTA_AOI](../TODO/05-15/02_UDP_M1_M3_TRANSPORT_RELIABILITY_DELTA_AOI.md) | UDP reliability, delta snapshot, AOI replication |
| Replay MVP | [03_REPLAY_MVP_SERVER_SNAPSHOT_WRPL_UPLOAD_PLAYBACK](../TODO/05-15/03_REPLAY_MVP_SERVER_SNAPSHOT_WRPL_UPLOAD_PLAYBACK.md) | server-side replay recording and offline playback |
| FX Tool MVP | [04_FX_TOOL_MVP_WFX_SOA_RENDERER_PREVIEW](../TODO/05-15/04_FX_TOOL_MVP_WFX_SOA_RENDERER_PREVIEW.md) | Niagara-style data-driven VFX tool/runtime MVP |
| Build/Cook/CI tooling | [05_TOOLING_SPINE_BUILDTOOL_ASSETVALIDATOR_COOK_CI](../TODO/05-15/05_TOOLING_SPINE_BUILDTOOL_ASSETVALIDATOR_COOK_CI.md) | build tool, asset validator, cooker, CI pipeline |
| Elden Ring vertical slice | [06_ELDEN_RING_VERTICAL_SLICE_BOSS_ARENA_SEQUENCER_FX](../TODO/05-15/06_ELDEN_RING_VERTICAL_SLICE_BOSS_ARENA_SEQUENCER_FX.md) | boss arena, sequencer, camera, montage, FX graph vertical slice |
| Perforce/P4 협업 경험 | [07_PERFORCE_P4_WORKFLOW_DEMO](../TODO/05-15/07_PERFORCE_P4_WORKFLOW_DEMO.md) | binary asset lock, changelist, editor checkout workflow |
| 공개 데모 릴리즈 | [08_PUBLIC_DEMO_RELEASE_PIPELINE](../TODO/05-15/08_PUBLIC_DEMO_RELEASE_PIPELINE.md) | release packaging, crash log, patch notes, rollback checklist |
| 안티치트/보안 | [Security index](../plan/security/00_SECURITY_INDEX.md) | server authority, usermode anti-cheat, integrity, suspicious input detection |

## 기존 심화 문서 링크

| 주제 | 문서 |
|---|---|
| AAA scale engine overview | [WINTERS_AAA_SCALE_ENGINE_MASTER_BRIEF](../문서/WINTERS_AAA_SCALE_ENGINE_MASTER_BRIEF.md) |
| Networking | [Ch7 Networking](../문서/07_Ch7_Networking.md) |
| Gameplay ability / GAS | [Ch8 GAS](../문서/08_Ch8_GAS.md) |
| AI | [Ch9 AI](../문서/09_Ch9_AI.md), [AI plan index](../plan/ai/00_AI_PLAN_INDEX.md) |
| Editor | [Ch12 Editor](../문서/12_Ch12_Editor.md) |
| Tooling | [Ch13 Tooling](../문서/13_Ch13_Tooling.md) |
| Backend services | [Ch14 Services](../문서/14_Ch14_Services.md), [Backend plan index](../plan/backend/00_BACKEND_PLAN_INDEX.md) |
| Data pipeline | [Ch15 Data Pipeline](../문서/15_Ch15_Data_Pipeline.md) |
| Collaboration | [Ch16 Collaboration](../문서/16_Ch16_Collaboration.md) |
| World partition | [Ch3 World Partition](../문서/03_Ch3_WorldPartition_Streaming.md) |
| Sequencer | [Ch11 Cinematics Sequencer](../문서/11_Ch11_Cinematics_Sequencer.md) |
| Effect tool | [Effect tool index](../plan/EffectTool/00_EFFECT_TOOL_PLAN_INDEX.md) |

## 이력서 문장 후보

- C++20 기반 자체 게임 엔진 Winters에서 ECS, RHI, renderer, asset format, ImGui tooling, client/server runtime을 설계 및 구현.
- MOBA server authority 구조를 `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual` 흐름으로 구축.
- FlatBuffers 기반 snapshot/event replication과 IOCP server prototype을 구현하고 UDP reliability/delta/AOI netstack으로 확장 계획 수립.
- Go 기반 backend services(auth, matchmaking, profile, leaderboard, payment, shop)를 PostgreSQL/Redis/Kafka/JWT stack으로 구현.
- custom `.wmesh/.wskel/.wanim` asset format과 Assimp 기반 converter를 구현해 engine runtime asset loading과 연결.
- data-driven VFX runtime과 `.wfx` tool MVP를 통해 skill cue, renderer, particle pool, editor preview를 통합 중.
- replay container `.wrpl`와 authoritative snapshot recording/playback 구조를 설계해 debugging, spectator, portfolio demo에 활용.

## 보완 우선순위

1. LoL 5-client server authority를 먼저 닫는다. 이 축이 Winters의 gameplay/network 신뢰도를 만든다.
2. UDP M1-M3와 Replay MVP를 붙여 network/replay 증명을 만든다.
3. FX Tool MVP와 Tooling Spine으로 자체 엔진의 제작 파이프라인을 보여준다.
4. Elden Ring vertical slice로 LoL 외 장르 확장성과 cinematics/animation/FX 역량을 보여준다.
5. Perforce/P4 workflow와 공개 데모 릴리즈로 실무 협업/출시 경험 부족분을 보완한다.
