# Winters Portfolio Completion Index - 2026-05-15

Session - Winters 포트폴리오 완성 축을 LoL, UDP, Replay, FX, Tooling, Elden, P4, Release로 나눈다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/.md/TODO/05-15/01_LOL_5CLIENT_SERVER_AUTHORITY_COMPLETION.md

목표:
- LoL 5-client normal flow에서 이동, 평타, 스킬, death/respawn, FX cue가 모두 server authority 경로로 왕복한다.

선행 문서:
- [Server authority brief](../../../CLAUDE_Legacy.md)
- [Server AI completion](../05-09/ServerAICompletion.md)
- [Champion server authority rules](../05-11/CHAMPION_SERVER_AUTHORITY_SUCCESS_RULES.md)

### 1-2. C:/Users/user/Desktop/Winters/.md/TODO/05-15/02_UDP_M1_M3_TRANSPORT_RELIABILITY_DELTA_AOI.md

목표:
- TCP prototype 이후 production 방향인 UDP transport, reliability, full snapshot, delta, AOI까지 M1-M3 범위를 구현한다.

선행 문서:
- [UDP LoL NetStack master v2](../../plan/sim/10_UDP_LOL_NETSTACK_MASTER_v2.md)
- [Ch7 Networking](../../문서/07_Ch7_Networking.md)

### 1-3. C:/Users/user/Desktop/Winters/.md/TODO/05-15/03_REPLAY_MVP_SERVER_SNAPSHOT_WRPL_UPLOAD_PLAYBACK.md

목표:
- server snapshot stream을 `.wrpl`로 저장하고, Go replay service 업로드와 client playback scene까지 연결한다.

선행 문서:
- [Replay service plan](../../plan/backend/Phase10_ReplayService.md)
- [Replay index](../05-09/Replay/00_REPLAY_INDEX.md)

### 1-4. C:/Users/user/Desktop/Winters/.md/TODO/05-15/04_FX_TOOL_MVP_WFX_SOA_RENDERER_PREVIEW.md

목표:
- `.wfx` JSON round-trip, runtime SoA, renderer 3종, ImGui preview를 MVP로 완성한다.

선행 문서:
- [Effect tool index](../../plan/EffectTool/00_EFFECT_TOOL_PLAN_INDEX.md)
- [Effect tool implementation index](../05-07/EffectTool/00_EFFECT_TOOL_IMPLEMENTATION_INDEX.md)

### 1-5. C:/Users/user/Desktop/Winters/.md/TODO/05-15/05_TOOLING_SPINE_BUILDTOOL_ASSETVALIDATOR_COOK_CI.md

목표:
- BuildTool, AssetValidator, Cook step, CI build를 추가해 손관리 `.vcxproj` 한계를 줄인다.

선행 문서:
- [Ch13 Tooling](../../문서/13_Ch13_Tooling.md)
- [Ch15 Data pipeline](../../문서/15_Ch15_Data_Pipeline.md)

### 1-6. C:/Users/user/Desktop/Winters/.md/TODO/05-15/06_ELDEN_RING_VERTICAL_SLICE_BOSS_ARENA_SEQUENCER_FX.md

목표:
- Elden Ring류 boss arena vertical slice를 world/sequence/camera/montage/notify/FX graph로 증명한다.

선행 문서:
- [Ch3 World Partition](../../문서/03_Ch3_WorldPartition_Streaming.md)
- [Ch11 Cinematics Sequencer](../../문서/11_Ch11_Cinematics_Sequencer.md)
- [Ch4 Animation](../../문서/04_Ch4_Animation.md)

### 1-7. C:/Users/user/Desktop/Winters/.md/TODO/05-15/07_PERFORCE_P4_WORKFLOW_DEMO.md

목표:
- binary asset lock, changelist, editor checkout, large asset workflow를 Perforce/P4 demo로 문서화한다.

선행 문서:
- [Ch16 Collaboration](../../문서/16_Ch16_Collaboration.md)
- [Ch12 Editor](../../문서/12_Ch12_Editor.md)

### 1-8. C:/Users/user/Desktop/Winters/.md/TODO/05-15/08_PUBLIC_DEMO_RELEASE_PIPELINE.md

목표:
- GitHub Releases, itch, Steam Playtest 중 하나로 공개 데모에 가까운 릴리즈 파이프라인을 만든다.

선행 문서:
- [Ch14 Services](../../문서/14_Ch14_Services.md)
- [Ch13 Tooling](../../문서/13_Ch13_Tooling.md)

## 2. 검증

문서 검증:
- 각 챕터 문서가 `Session - ...`, `1. 반영해야 하는 코드`, `2. 검증` 순서를 지키는지 확인.
- 각 문서가 기존 시스템의 server authority 원칙을 깨지 않는지 확인.
- 각 문서의 링크가 저장소 기준 상대 경로로 열리는지 확인.

실행 순서:
- 01 -> 02 -> 03은 LoL server/network/replay 증명 축이다.
- 04 -> 05는 엔진 제작 능력 증명 축이다.
- 06은 Elden Ring vertical slice 증명 축이다.
- 07 -> 08은 협업/출시 경험 보강 축이다.
