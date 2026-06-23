# UE5급 에디터 스위트 마스터 설계 + Codex 프롬프트

> 작성: 2026-06-13. 대상: Codex 및 EldenRing 팀.
> 목표: World Editor(UE5 Editor급) · FX Editor(Niagara급) · Sequencer · World Partition · Boss Pattern Testing Environment
> 5대 시스템을 **UE5를 reference로, 코드 복사 없이 Winters `.w*` contract + Winters runtime으로** 구축.
> 선행: `12`(UE5 빅픽처/Phase/게이트), `04`(월드파티션), `06`(FX/시퀀서/에디터), `07`(에셋로더),
> `03`(런타임 아키텍처), `15`(2달 바이너리화 플랜), plan/EldenRingEditor/01~09.

---

## 0. 핵심 원칙 (12문서 재확인 — 절대 위반 금지)

```text
UE5 source/editor = reference depot (개념·UX·책임분리 관찰)
  → 이름이 아니라 개념을 Winters식으로 재구성
  → 코드 복사·모듈 링크·UE object model 이식 금지
  → 모든 산출물은 Winters .w* 포맷 + Winters runtime을 통과
```

**구현 순서의 철칙**: "에디터 화면 먼저 크게" 금지. **runtime contract를 작게 증명 → 에디터가 그 contract를 편집**.
모든 패널의 완료 기준은 "`.w*`/JSON seed → runtime preview"가 보이는 것.

---

## 1. 현재 Winters 구조 스냅샷 (실측 2026-06-13)

| 영역 | 현재 상태 | 5대 시스템 관점 |
|---|---|---|
| **EldenRingEditor** | DX12 ImGui 셸 **빌드됨**(WintersEldenRingEditor.exe) | 에디터 셸 기반 존재 ✅ |
| FX 셰이더 | `FxSprite.hlsl`, `FxMesh.hlsl`, `FxStaticMeshRenderer`(DLL) | FX 렌더 기반 존재, 그래프/시뮬 없음 |
| 렌더러 | `ModelRenderer`, PBR/SSAO/GTAO 셰이더, DX11/DX12 RHI | 뷰포트 프리뷰 기반 |
| ECS | `GameSim`(Server/Shared) — 컴포넌트/시스템 스케줄러 | 보스 AI·hitbox의 권위 계층 |
| 네비/충돌 | `CNavGrid`(2D 512×512) + `CPathfinder`(A*) | 평지 슬라이스 재사용, 3D는 신규 |
| 사운드 | `CSound_Manager`(FMOD, 9채널) | Sequencer AudioTrack·FX cue |
| UI | `CUIAtlasManifest` + `CFont_Manager`(ImGui) | 에디터 패널 + 게임 HUD |
| **미구현** | `CWorldPartition`·`CSequencePlayer`·`CFxGraph`·`CAssetStreaming` | **5대 시스템 런타임 신규** |

---

## 2. UE5 설계 철학 → Winters 매핑 (5대 시스템)

### 2.1 World Editor (UE5 Editor급)

| UE5 개념 | 역할 | Winters 대응 |
|---|---|---|
| Slate UI | 에디터 위젯 프레임워크 | **ImGui 도크스페이스** (이미 셸 존재) |
| Editor Mode | 선택/변형/페인트 모드 | `eEldenEditorMode { Edit, Simulate, Play }` (06문서) |
| Transform Gizmo | translate/rotate/scale 핸들 | ImGui 3D gizmo(ImGuizmo 또는 자체) + ray-pick |
| Viewport | 퍼스펙티브/직교, grid snap | `PreviewViewport`(RHI 위) |
| Details Panel | 선택 객체 속성 인스펙터 | `AssetInspector` / `Details` 패널 |
| World Outliner | 액터 계층 트리 | `World Outliner`(cell/entity 트리) |
| Content Browser | 에셋 브라우저 | `CAssetCatalog`(`.w*`/PNG/JSON 스캔) |
| **Transaction(Undo/Redo)** | `BeginTransaction`/`EndTransaction` | **`CEditorTransaction` 스택** (command 패턴, redo 포함) |
| PIE(Play In Editor) | 에디터 내 게임 실행 | `Scene_EldenEditor`의 Play 모드(같은 런타임) |
| Drag-drop placement | 에셋 → 월드 배치 | Content Browser → 뷰포트 드롭 → cell descriptor 추가 |

**핵심 철학**: 에디터는 runtime을 우회하지 않는다. 에디터가 만든 transform/placement가 `.wcell` JSON으로 저장되고
**같은 데이터를 게임 런타임이 그대로 로드**. Undo/Redo는 모든 편집 동작이 command로 표현돼야 가능 → 처음부터 command 기반 편집.

### 2.2 FX Editor (Niagara급)

| UE5 Niagara 개념 | 역할 | Winters 대응 |
|---|---|---|
| System | 여러 Emitter 묶음 | `.wfx` document (emitters 배열) |
| Emitter | Module 스택 단위 | `FxEmitter`(nodes/edges) |
| **Module 스택** | Spawn/EmitterUpdate/ParticleSpawn/ParticleUpdate/Render | `Spawn→Init→Update→Render` 노드 스테이지(06문서) |
| Data Interface | 외부 데이터(mesh/texture/collision) 접근 | `FxDataBinding`(wmesh/wtex/navgrid 참조) |
| Parameter namespace | User/System/Emitter/Particle | `FxParam` 스코프 |
| Sim Stage | GPU 멀티패스 | P3: GPU compute 시뮬(초기엔 CPU) |
| CPU/GPU 선택 | VM(CPU) / HLSL 컴파일(GPU) | **초기 CPU execution plan**, GPU는 후순위 |
| Renderer(Sprite/Mesh/Ribbon) | 파티클 렌더 | `FxSprite.hlsl`/`FxMesh.hlsl` 재사용 + Ribbon 신규 |

**Winters 런타임**:
```text
CFxGraph → validation → compiled CPU execution plan → EmitterInstance → ParticlePool → FxRenderSystem
```
**결정**(06문서): visual-only FX는 클라 로컬 시드, **gameplay-affecting FX는 서버 event id+seed**,
raid telegraph FX는 서버 action state에서 파생. → FX 그래프는 presentation, 판정은 서버.

**Niagara 핵심 통찰 반영**: 노드 그래프를 **컴파일된 실행 계획**(execution plan)으로 변환하는 게 핵심.
에디터의 그래프 metadata와 런타임 emitter desc를 **함께 저장**(graph 없는 `.wfx`도 로드 가능 — 하위호환).

### 2.3 Sequencer (UE5 Sequencer)

| UE5 Sequencer 개념 | 역할 | Winters 대응 |
|---|---|---|
| Level Sequence | 시퀀스 에셋 | `.wseq` (tracks 배열) |
| Track / Section / Channel | 트랙 계층 | `SeqTrack`(Camera/Anim/Fx/Audio/Event/Visibility/TimeDilation) |
| Possessable | 기존 액터 바인딩 | `binding`(entity id/name) |
| Spawnable | 시퀀서가 생성하는 액터 | `spawnable` descriptor (P2) |
| **Evaluation Template** | 런타임 평가 구조 | `CSequencePlayer::Tick(dt)` — 시간→트랙 평가 |
| Keyframe + Curve | 보간 | key{time, value} + interp 모드 |
| Camera Cuts track | 카메라 전환 | CameraTrack의 cut 지원 |
| Sub-sequence / Shot | 중첩 시퀀스 | P3 |

**Winters 런타임**: `CSequencePlayer::Play/Stop/Tick/IsPlaying`. **Sequence는 gameplay truth를 직접 판정하지 않음**
— 컷신/연출/트리거만. EventTrack은 gameplay callback 후보를 발행하되 판정은 서버.

### 2.4 World Partition (UE5 World Partition)

| UE5 개념 | 역할 | Winters 대응 (04문서 상세) |
|---|---|---|
| Grid partitioning | 공간 grid 분할 | `cellSizeMeters`(64) grid, `x000_z000` cell id |
| **One File Per Actor** | 액터마다 파일 | 초기엔 cell당 1 `.wcell`(OFPA는 P3 — 머지 충돌 시) |
| Runtime hash grid | 위치→cell 조회 | `RuntimeHash`(position→cell id) |
| Streaming Source | 로딩 기준점 | `StreamingSourceComponent`(load/visible/unload radius, priority) |
| Data Layer | 상황별 레이어 | `DataLayerComponent`(layerMask: Base/Gameplay/RaidPhase/Cinematic/EditorOnly) |
| HLOD | 먼 거리 프록시 | `CHLODSystem`(near=entity, far=proxy mesh) |
| Cell 상태 머신 | 스트리밍 단계 | `eWorldCellState`(Unloaded→Queued→LoadingMetadata→LoadingAssets→CreatingEntities→LoadedHidden→Visible→Unloading) |

**Winters 런타임**: `CWorldPartitionSystem`{Descriptor, CellRuntime, StreamingSourceRegistry, DataLayerSystem,
HLODSystem, CellVisibilityResolver, CellStreamingScheduler}. **에셋 로더와 연동**(07문서):
cell load = 에셋 요청 묶음, Required(collision/near mesh/material fallback) 우선, Optional(high-res/decal/ambient FX) 후순위.

**서버 분리**(04문서): Client cell = render mesh/material/FX/UI. Server sim cell = collision/nav/spawn/authority.

### 2.5 Boss Pattern Testing Environment

UE5 직접 대응 없음 — Behavior Tree + Blackboard(AI) + Gauntlet(자동화 테스트) + AI Debugger를 조합한 **커스텀 환경**.

| 개념 | 역할 | Winters 대응 |
|---|---|---|
| Blackboard | AI 결정 상태 | `BossBlackboard`(target/distance/phase/cooldown) |
| HFSM / Behavior Tree | 행동 결정 | `BossHFSM`/`BossBT` → GameCommand 후보 발행 |
| Phase graph | 페이즈 전이 | `BossPatternEditor` phase graph(06문서) |
| Action + telegraph | 패턴 데이터 | action{range/cooldown/weight} + telegraph FX 연결 |
| **Hitbox Timeline** | active-frame 판정 | `HitboxTimelineEditor` + `HitboxInstance`(center/halfExtents/activeFrames) |
| Gauntlet(자동 테스트) | 반복 검증 | **자동 패턴 테스트 루프**(시드 고정, 패턴 N회 재생, hit/miss/timing 로깅) |
| AI Debugger | 결정 시각화 | 블랙보드/액션/사유 debug overlay |

**서버 권위 철칙**(12문서 Phase I): Boss AI는 client visual에서 gameplay 결과를 만들지 않는다.
blackboard/HFSM/BT 결정은 **GameCommand 후보로만** 서버에 전달, **hitbox 판정·damage·phase 전이는 서버 권위**.
client는 animation/telegraph FX/UI/camera shake 재생.

**테스트 환경 핵심**: 보스 패턴을 **격리된 아레나에서 반복 재생**하며 hitbox 타이밍·텔레그래프·페이즈 전이를
시각화+로깅. 더미 적/플레이어로 회피 윈도우 검증. 시드 고정으로 결정성 보장.

---

## 3. 통합 아키텍처 (12문서 mermaid 기반)

```text
EldenRingEditor (DX12 ImGui)                    Winters .w* contract
├── AssetBrowser ──────────────► .wtex .wmat .wmesh .wskel .wanim
├── MaterialResolver ──────────► .wmat (PBR 채널)
├── FxGraphEditor ─────────────► .wfx (graph + runtime emitter desc)
├── SequencerPanel ────────────► .wseq (tracks)
├── WorldPartitionEditor ──────► .wmap .wcell .wnav
├── BossPatternEditor ─────────► .wboss .whitbox (또는 JSON seed)
└── HitboxTimelineEditor ──────► .whitbox
        │                              │
        └──────────────► CAssetStreamingSystem (handle/state, async, GPU budget)
                                       │
                              RenderWorldSnapshot
                                       │
                         CRHISceneRenderer / CRenderGraph → DX11/DX12

Server GameSim ──► Snapshot/Event/FX Cue ──► Client Visual (FX/anim/UI/camera)
```

**데이터 흐름 불변식**: 에디터가 만든 모든 데이터(`.wfx/.wseq/.wcell/.wboss`)는 `CAssetStreamingSystem`을 거쳐
런타임에 로드. 에디터 전용 우회 경로 금지. 게임플레이 판정은 항상 Server GameSim.

---

## 4. 5대 시스템 구현 순서 + 게이트 (12·04·06·07 종합)

전제: **RHI ladder(12문서 RHI-02~04)와 에셋 contract(15문서 Phase1)가 선행.** 텍스처+라이트+스태틱메시가
에디터 뷰포트에 보이는 G2 게이트 전에는 FX/Sequencer 패널을 크게 벌리지 않는다.

| 순서 | 세션 | 시스템 | 게이트 |
|---|---|---|---|
| ED-01 | DX12 ImGui editor bootstrap | World Editor | G3 도크스페이스+뷰포트 |
| ED-02~05 | Asset catalog / texture·material·mesh preview / resolver | World Editor | G4 카탈로그→프리뷰 |
| ED-06 | Asset handle/state registry | Asset Loader | G5 핸들/셀 상태 |
| ED-07 | Map assembly seed + **Transform Gizmo + Undo/Redo** | World Editor | 배치 편집 가능 |
| ED-08 | World partition seed (fake camera→cell state) | World Partition | WP cell 전이 |
| ED-09 | WFX graph minimum (burst billboard save/load/preview) | FX Editor | G6 graph→emitter bake |
| ED-10 | Sequencer camera track (key/scrub/play) | Sequencer | G7 카메라 트랙 재생 |
| ED-11 | Hitbox timeline (attack clip에 window) | Boss Testing | hitbox window 표시 |
| ED-12 | Boss blackboard/pattern tool + **자동 테스트 루프** | Boss Testing | G8/G9 서버 권위 판정 |

**게이트 우선 원칙**(12문서): 각 게이트 통과 전 다음 큰 범위로 안 넘어간다. G2(스태틱 에셋) 막히면
월드파티션 배치 중단. G6(WFX bake) 막히면 gameplay FX cue 연결 중단. G7(Sequencer) 막히면 cinematic 연결 중단.

---

## 5. 저장 포맷 전략 (모두 JSON → binary 승격)

| 시스템 | 초기 | 중기 | 후기 |
|---|---|---|---|
| World | `world.json`+`cell.json` | `.wmap`+`.wcell`+`.wnav` | `Content.winters` |
| FX | `.wfx.json` | `.wfx` | bundle |
| Sequencer | `.wseq.json` | `.wseq` | bundle |
| Boss | JSON seed | `.wboss`+`.whitbox` | bundle |

**이유**: JSON은 diff 가능·빠른 수정·schema 변경 용이. 에디터 안정화 후 binary 승격.

---

## 6. Codex 지시 프롬프트 (복붙용)

아래 코드블록을 Codex에 붙여넣는다. 한 명이 전체를 보거나, `SYSTEM=` 한 줄로 5대 중 하나를 담당.

```text
SYSTEM=WORLD_EDITOR   # WORLD_EDITOR | FX_NIAGARA | SEQUENCER | WORLD_PARTITION | BOSS_TESTING

너는 Winters 엔진에 UE5급 에디터 스위트를 구축하는 시니어 엔진/툴 엔지니어다.
목표: World Editor·FX(Niagara급)·Sequencer·World Partition·Boss Pattern Testing 5대 시스템을
UE5를 reference로만 삼아, 코드 복사 없이 Winters .w* contract + Winters runtime으로 구현한다.

[절대 원칙 — 위반 시 작업 무효]
1. UE5는 reference depot(개념·UX·책임분리 관찰)일 뿐. UE 코드 복사/모듈 링크/object model 이식 금지.
2. "에디터 화면 먼저 크게" 금지. runtime contract를 작게 증명 → 에디터가 그 contract를 편집.
   모든 패널의 완료 기준 = ".w*/JSON seed → runtime preview"가 실제로 보이는 것.
3. 에디터가 만든 데이터(.wfx/.wseq/.wcell/.wboss)는 CAssetStreamingSystem 거쳐 런타임 로드.
   에디터 전용 우회 경로 금지. 게임플레이 판정은 항상 Server GameSim(presentation/truth 분리).
4. Engine→Client 의존 역전 금지. Client/Public·Shared에 DX11/DX12 concrete type 노출 금지.
5. normal F5 LoL runtime을 editor/lab path로 우회·은폐 금지. LoL DX11 visual smoke 계속 검증.

[환경]
- 저장소: C:/Users/tnest/Desktop/Winters
- 에디터: EldenRingEditor/ (WintersEldenRingEditor.exe, DX12 ImGui 셸 이미 빌드됨)
- 런타임: EldenRingClient/ (--rhi=dx11), Engine/(공용 RHI/렌더러/리소스)
- FX 셰이더 존재: Shaders/FxSprite.hlsl, FxMesh.hlsl, FxStaticMeshRenderer(엔진)
- ECS: Shared/GameSim (서버 권위 gameplay)
- 미구현(신규): CWorldPartitionSystem, CSequencePlayer, CFxGraph, CAssetStreamingSystem

[먼저 읽을 문서 — 순서대로]
1. .md/EldenRing/17_UE5_GRADE_EDITOR_SUITE_MASTER.md  ← 이 설계(UE5↔Winters 매핑·순서·게이트)
2. .md/EldenRing/12_UE5_REFERENCE_DX12_RHI_EDITOR_BIG_PICTURE.md  ← Phase A~J·게이트 G0~G9·세션 ladder
3. .md/EldenRing/04(월드파티션) 06(FX/시퀀서/에디터) 07(에셋로더) 03(런타임아키텍처)
4. .md/plan/EldenRingEditor/01~09  ← 세션별 코드 지시서(bootstrap→boss)
5. .md/EldenRing/14(런타임 계약) 15(2달 바이너리화) + CLAUDE.md/.claude/gotchas.md

[너의 SYSTEM 작업 범위]
- WORLD_EDITOR: ImGui 도크스페이스+뷰포트(있음) 위에 Content Browser·Details·Outliner·Transform Gizmo·
  Undo/Redo(CEditorTransaction command 스택)·드래그배치. 편집 동작은 전부 command로 표현(Undo 가능).
  산출: 선택 .wmesh 뷰포트 프리뷰 + cell descriptor 편집/저장.
- FX_NIAGARA: .wfx 그래프(Spawn→Init→Update→Render 노드) + CPU execution plan 컴파일 +
  EmitterInstance/ParticlePool/FxRenderSystem + FxSprite/FxMesh 렌더 재사용. graph metadata와
  runtime emitter desc 함께 저장(graph 없는 .wfx도 로드). visual-only는 로컬, gameplay FX는 서버 event+seed.
- SEQUENCER: .wseq(Camera/Anim/Fx/Audio/Event/Visibility/TimeDilation 트랙) + CSequencePlayer::Tick 평가 +
  keyframe/curve. EventTrack은 gameplay 후보만 발행, 판정은 서버. camera track key/scrub/play 우선.
- WORLD_PARTITION: world.json+cell.json → CWorldPartitionSystem(cell state machine·streaming source·
  data layer·HLOD·runtime hash) + 에셋로더 연동(Required/Optional). fake camera→cell 전이 우선,
  .wmap/.wcell 승격은 안정화 후. 서버 sim cell(collision/nav/spawn)과 client cell(render) 분리.
- BOSS_TESTING: BossBlackboard+HFSM/BT(GameCommand 후보만) + 격리 아레나 자동 패턴 테스트 루프(시드 고정,
  패턴 N회 재생, hitbox 타이밍/회피윈도우 hit-miss 로깅) + HitboxTimeline(activeFrames) + 결정 debug overlay.
  hitbox 판정·damage·phase 전이는 서버 권위. client는 anim/telegraph FX/UI/camera shake.

[작업 루프 — 게이트 통과까지]
1. 선행 게이트 확인: RHI(텍스처+라이트+스태틱메시) G2와 에셋 contract(15 Phase1)가 됐는지.
   안 됐으면 내 시스템 큰 패널 확장 멈추고 선행부터.
2. runtime contract 먼저: 내 시스템의 .w*/JSON 포맷 + 런타임 로더/플레이어를 최소로 구현·검증.
3. 에디터가 그 contract를 편집하게: 패널 추가. 완료기준은 항상 "편집→저장→런타임 preview" 왕복.
4. 게이트 검증: 17문서 4절 게이트(G3~G9). 통과 전 다음 범위 금지.
5. presentation/truth 구분: 이번 변경이 gameplay truth면 Server/GameSim, presentation이면 client/editor.
6. 막히면 사유 보고(특히 서버 권위·의존 역전·게이트 미통과). 나머지는 계속.

[빌드 검증]
- Engine: MSBuild Winters.sln /t:Engine /p:Configuration=Debug /p:Platform=x64
- Editor: /t:EldenRingEditor /p:Configuration=Debug-DX12
- Elden: /t:EldenRingClient /p:Configuration=Debug-DX12 (또는 --rhi=dx11)
- LoL 영향: /t:Client (visual smoke 유지)
- git diff --check. Engine public header 변경 시 SDK sync(UpdateLib.bat) 확인.

[완료 기준 — 내 SYSTEM]
- WORLD_EDITOR: 카탈로그 선택 .wmesh 뷰포트 표시 + gizmo 배치 + Undo/Redo + cell 저장→런타임 로드.
- FX_NIAGARA: burst billboard 그래프 save/load/preview + graph→emitter bake + 서버 cue 1회 재생.
- SEQUENCER: camera track key/scrub/play + .wseq 저장→런타임 preview 적용.
- WORLD_PARTITION: fake camera 이동에 cell state 전이 + 에셋 dependency 패널 연결 + transform 없는 ref 미배치.
- BOSS_TESTING: 격리 아레나에서 보스 패턴 자동 N회 재생 + hitbox 타이밍 로깅 + 서버 권위 판정 + 회피윈도우 검증.

[시작]
지금: (1) 위 문서를 읽고, (2) 내 SYSTEM의 선행 게이트 충족 여부와 현재 코드 상태를 집계해 보고,
(3) runtime contract 최소 구현부터 시작하라. 막히면 사유 분류해 보고하고 나머지는 계속 진행하라.
```

---

## 7. 포트폴리오 메시지 (12문서)

```text
Winters는 자체 C++ 엔진 DLL, 자체 RHI, 자체 asset binary, 자체 editor tooling으로
MOBA(LoL)와 action RPG(EldenRing) runtime을 모두 구동한다.
UE5(World Partition/Niagara/Sequencer/Editor)는 참고 기준이고, Winters는 독립 구현이다.
```

면접 질문 대비(06문서): ① Sequencer 같은 툴의 runtime/editor asset 분리, ② particle graph를 CPU/GPU 실행계획으로 변환,
③ boss action data+hitbox timeline의 서버 권위 연결, ④ 에디터 저장 data의 runtime+network 사용.
