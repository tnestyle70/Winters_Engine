# Winters → AAA 스케일 엔진 확장 마스터 플랜

> **출력 모드**: handoff (코드 직접 수정 없음, AGENTS.md / `.md/architecture/IMPLEMENTATION_HANDOFF_OUTPUT_RULE.md` 준수)
> **현재 slice 위치**: S10 BotAIStage1 1차 smoke pass 직후 — Death/TargetInvalid/Respawn 안정화 진행 중
> **이 문서의 위상**: long-horizon 아키텍처 brief. S 시퀀스(S1~S10)와 직각 axis로, **엔진 capability layer**의 챕터 분해다. 이 brief는 현재 slice를 **대체하지 않는다.** S10 안정화는 그대로 가고, 본 brief는 그 위에 얹는다.
> **작성일**: 2026-05-13
> **참조 코드베이스**: `C:\Users\user\Desktop\Winters\` (Winters 실코드) + `C:\Users\user\Desktop\UnrealEngine\UnrealEngine\` (UE5 레퍼런스)

## Current sequence 다시 박제

```text
[지금 진행 중]
S10_BotAIStage1 후속: Death/TargetInvalid/Respawn → projectile/turret stale target 무효화
                      → champion respawn → tower aggro refinement
                      → tower→nexus→room reset/result 1판 루프

[엔진 capability axis — 본 brief의 챕터]
Ch1 RHI / Render Hardware (DX12/Vulkan/Metal/Console)
Ch2 RenderGraph / Lighting / GI / Virtual Texture / Nanite-tier
Ch3 World Partition / Streaming / Open World
Ch4 Animation (StateMachine, BlendTree, MotionMatching, IK, Physics-anim)
Ch5 Physics (Rigid/Cloth/Destruction/Fluid)
Ch6 Audio (3D, DSP graph, Wwise/FMOD interop)
Ch7 Networking (IOCP/AOI/Replication/Anti-cheat/Replay)
Ch8 GameplayAbilitySystem (GAS, Tags, Effects, Attributes)
Ch9 AI (BehaviorTree, EQS, Perception, NavMesh, Crowd)
Ch10 UI (UMG-tier, DataBinding, Localization)
Ch11 Cinematics / Sequencer / Camera
Ch12 Editor (DCC, Blueprint, AssetBrowser, World Editor)
Ch13 Tooling (AssetConverter, DDC, CookOnTheFly, BuildSystem)
Ch14 Services (Auth/Shop/Match/Profile/Leaderboard/Telemetry/LiveOps)
Ch15 Data pipeline (DataTable, Curve, ContentRegistry)
Ch16 Cross-discipline collaboration topology
```

---

## Why this order

UE5에서 챕터들은 서로 의존이 강하다. RenderGraph 없이 GI를 짤 수 없고, GAS 없이 챔피언 150 스케일 못 가고, World Partition 없이 GTA6/엘든링 스케일 못 간다. 그래서 챕터 순서는 **"이 챕터가 없으면 다음 챕터를 시작도 못 한다"** 기준으로 잡았다.

Winters의 현재 강점은 RHI 추상화 1차(`Engine/Public/RHI/`)와 ECS(`Engine/Public/ECS/`), GameSim 분리(`Shared/GameSim/`)다. UE5와 비교하면 가장 **앞서 있는 부분**이다 — UE5도 Iris/Mass/Niagara에서 데이터 지향으로 가는 중이다. 약점은 Editor, GAS, AnimGraph, RenderGraph 완성도, World Partition이다.

---

## Ch1. RHI / Render Hardware Interface 완성

### 현재 Winters
```text
Engine/Public/RHI/RHISurface.h, RHICapabilities.h
Engine/Public/Platform/{PlatformTypes,IPlatformWindow,IPlatformSurface}.h
Engine/Private/RHI/DX11/    (런타임 기준)
Engine/Private/RHI/DX12/    (clear/present + DX12SmokeHost 통과)
```

### UE5 대응
```text
UnrealEngine/Engine/Source/Runtime/RHI/                 RHI 추상 contract
UnrealEngine/Engine/Source/Runtime/D3D12RHI/            DX12 backend
UnrealEngine/Engine/Source/Runtime/VulkanRHI/           Vulkan backend
UnrealEngine/Engine/Source/Runtime/MetalRHI/            Metal backend
UnrealEngine/Engine/Source/Runtime/RenderCore/          GPU resource lifetime + RDG
```

### Winters에 추가할 것
```text
Engine/Public/RHI/
  RHIResource.h          공통 base (Texture/Buffer/Pipeline ref-counted)
  RHICommandList.h       record/submit 추상화 (현재는 device가 직접 들고있음)
  RHIPipelineState.h     PSO + binding layout
  RHIDescriptorHeap.h    DX12 heap / Vulkan descriptor set 통일
  RHIFence.h             cross-queue 동기화
  RHIQueue.h             graphics/compute/copy queue 명시
  RHIShaderCompiler.h    DXC/glslang/SpirV-Cross interop

Engine/Private/RHI/Vulkan/   (신규 backend, 모바일/리눅스/Switch)
Engine/Private/RHI/Metal/    (신규 backend, macOS/iOS)
Engine/Private/RHI/Null/     (CI/dedicated server용 no-op backend)
```

### 검증 기준
- DX12SmokeHost와 동일한 `VulkanSmokeHost` 8초 생존 smoke.
- 모든 backend에서 동일한 `RHITriangleSmoke` shader가 같은 color를 그린다.

---

## Ch2. RenderGraph / Lighting / GI

### 현재 Winters
```text
Engine/Public/Renderer/RenderGraph.h        (스켈레톤 존재)
Engine/Public/Renderer/GPUDrivenPipeline.h  (스켈레톤)
Engine/Public/Renderer/SSAOPass.h           (단일 pass)
Engine/Public/Renderer/FogOfWarRenderer.h   (LoL 스타일 FoW)
```

### UE5 대응
```text
UnrealEngine/Engine/Source/Runtime/Renderer/Private/
  RenderGraph.cpp / RDG*.cpp                    명시적 RG
  PostProcess/, DeferredShadingRenderer.cpp     deferred path
  Lumen/                                        GI
  VirtualTexturing/, VT/                        Streaming VT
  Nanite/                                       Virtualized geometry
```

### Winters에 추가할 챕터별 모듈
```text
Engine/Public/Renderer/RenderGraph/
  RDGBuilder.h               pass node DAG, transient resource
  RDGResource.h              auto-barrier, alias
  RDGShaderParameters.h      auto-binding
Engine/Public/Renderer/Passes/
  GBufferPass.h, ShadowPass.h, AmbientOcclusionPass.h,
  GlobalIlluminationPass.h (probe/voxel), TAAPass.h, BloomPass.h, TonemapPass.h
Engine/Public/Renderer/VirtualTexture/
  VTPageTable.h, VTStreamer.h, VTFeedback.h
Engine/Public/Renderer/MeshClusters/
  ClusterCulling.h           (Nanite 등가, GPU-driven)
Engine/Public/Renderer/Lighting/
  LightGrid.h, ClusteredShading.h, ShadowAtlas.h
```

### 왜 RG가 먼저인가
RenderGraph 없이 GI/VT/Cluster culling을 짜면, pass 간 dependency를 매번 손으로 잡아야 한다. UE5도 RDG 도입 이전과 이후 코드량 차이가 크다. **Ch1 RHI command list 추상화 → Ch2 RG → 나머지 pass** 순서가 강제다.

---

## Ch3. World Partition / Streaming / Open World

### 현재 Winters
```text
Client/Public/Map/                   stage 단일 맵 위주
Client/Private/Manager/Structure_Manager.cpp  단일 LoL map 기준
Engine/Public/ECS/SpatialIndex.h     2D grid (LoL용)
```

→ 엘든링/GTA6 스케일은 **불가**. 현재는 LoL 5v5 한 판 분량.

### UE5 대응
```text
UnrealEngine/Engine/Source/Runtime/Engine/Classes/WorldPartition/
UnrealEngine/Engine/Source/Editor/WorldPartitionEditor/
```

### Winters에 추가
```text
Engine/Public/World/
  WorldPartition.h          cell grid + HLOD + actor descriptor
  WorldCell.h               load/unload unit
  WorldStreamingSource.h    플레이어/카메라/네트 viewpoint
  DataLayer.h               gameplay layer 토글
  HLODBuilder.h             cell 단위 hierarchical LOD
Engine/Public/Streaming/
  AsyncLoader.h             cooperative scheduler, IO ring
  StreamingBudget.h         GPU/CPU/disk budget
  LevelInstance.h           재사용 가능한 sub-level
Tools/WorldPartitionBuilder/  cook-time HLOD/visibility 사전 계산
```

### 게임별 적용 예
- **GTA6 / 엘든링**: WorldPartition cell + HLOD + 비행/탈것 streaming source 다중화.
- **로스트아크**: 동일 World에 여러 instance(필드/레이드/카오스)를 LevelInstance로.
- **LoL**: 현재 단일 맵 그대로. WorldPartition은 cell 1개로 축퇴.

---

## Ch4. Animation Stack

### 현재 Winters
```text
Engine/Public/Resource/Animation.h, Animator.h, Bone.h, Skeleton.h
Client/Private/GameObject/Champion/*/   각 챔프 애니 호출
```

스켈레탈 mesh는 동작하지만 **StateMachine/BlendTree/MotionMatching이 없다.** 챔피언별 castFrame 직접 호출 구조 — 150 챔프 가면 무너진다.

### UE5 대응
```text
UnrealEngine/Engine/Source/Runtime/AnimGraphRuntime/
UnrealEngine/Engine/Source/Runtime/AnimationCore/
UnrealEngine/Engine/Source/Editor/AnimGraph/
UnrealEngine/Engine/Source/Editor/AnimationBlueprintEditor/
```

### Winters에 추가
```text
Engine/Public/Animation/
  AnimStateMachine.h        Locomotion / Combat / Stagger 등 상태
  AnimBlendTree.h           Idle/Walk/Run blend, additive
  AnimMontage.h             skill cast cue, notifies
  AnimNotifyTrack.h         hitFrame / fxCue / soundCue 데이터
  AnimCurve.h               IK weight, gameplay curve
  IKSolver.h                FABRIK / TwoBone / FullBodyIK
  MotionMatching.h          (엘든링/GTA6급 motion DB lookup)
  PoseSearch.h              motion DB
  AnimRetargeter.h          skeleton-to-skeleton

Shared/GameSim/Systems/AnimationCueSystem.h  서버 cue → 클라 montage
```

### S5와의 관계
현재 `S5_AnimationReplicationSingleSource`가 actionSeq 기준 1회 재생까지 왔다. **그 다음은 montage/notify 데이터화** — 각 챔프 `_Skills.cpp`에 박힌 frame 숫자를 `.anim` 데이터로 뽑아내야 한다.

---

## Ch5. Physics

### 현재 Winters
- 충돌은 LoL식 capsule + nav grid. 강체/천/파괴/유체 **전부 없음.**

### UE5 대응
```text
UnrealEngine/Engine/Source/Runtime/Experimental/Chaos/
UnrealEngine/Engine/Source/Runtime/Experimental/ChaosCloth/
UnrealEngine/Engine/Source/Runtime/Experimental/GeometryCollectionCore/   파괴
UnrealEngine/Engine/Source/Runtime/PhysicsCore/
```

### Winters에 추가
```text
Engine/Public/Physics/
  PhysicsWorld.h            broad-phase + narrow-phase
  RigidBody.h, Collider.h
  ConstraintSolver.h        joint, hinge
  CharacterController.h     gameplay-friendly capsule controller
  ClothSolver.h             망토/머리카락
  Destruction.h             geometry collection
Engine/Private/Physics/Jolt/   Jolt Physics 통합 (오픈소스, AAA-tier)
  또는 PhysX/Chaos 직접 포팅
```

**선택 가이드**: GTA6/엘든링 급은 Chaos 또는 PhysX. LoL/로아 급은 Jolt 또는 자체 lightweight면 충분.

---

## Ch6. Audio

### 현재 Winters
```text
Engine/External/FMOD          런타임 사용
Engine/Public/Sound/          간단한 wrapper
```

### UE5 대응
```text
UnrealEngine/Engine/Source/Runtime/AudioMixer/
UnrealEngine/Engine/Source/Runtime/AudioMixerCore/
UnrealEngine/Engine/Source/Runtime/AudioExtensions/
UnrealEngine/Engine/Source/Runtime/MetasoundEngine/  (DSP node graph)
```

### Winters에 추가
```text
Engine/Public/Audio/
  AudioEngine.h           submix bus, send/return
  AudioSource3D.h         attenuation, doppler
  AudioCueGraph.h         metasound 등가
  AudioOcclusion.h        portal/obstruction
  AudioStreaming.h        대용량 BGM/voice streaming
Shared/GameSim/Systems/AudioCueSystem.h   서버 cue → 클라 재생
```

Ch4 AnimNotifyTrack과 묶여서 **서버 actionSeq → notify → fx + sound + montage 동시 재생**이 1 데이터 소스로 처리되어야 한다.

---

## Ch7. Networking 고도화

### 현재 Winters
```text
Server/Public/Network/        IOCP는 일부 진행
Shared/Network/               PacketDef / Snapshot
Shared/Replay/                R0 골격
```

**memory 박제**: 사용자는 **Fiber + IOCP 통합**을 server side 주력으로 잡았다 (`project_fiber_mastery_session_2026_05_11.md`). 이 챕터의 server-side는 Fiber 기준이다.

### UE5 대응
```text
UnrealEngine/Engine/Source/Runtime/Online/
UnrealEngine/Engine/Source/Runtime/PacketHandlers/
UnrealEngine/Engine/Source/Runtime/Net/
UnrealEngine/Engine/Source/Runtime/NetCore/
UnrealEngine/Engine/Source/Runtime/Experimental/Iris/   (next-gen replication)
```

### Winters에 추가
```text
Server/Public/Network/
  IocpFiberServer.h       Fiber × IOCP 통합 (사용자 master plan)
  AOIManager.h            area-of-interest grid/octree
  ReplicationGraph.h      UE5 Replication Graph 등가
  CongestionControl.h     RTT/loss adaptive
Shared/Network/
  RpcSchema.h             FlatBuffers RPC 정의
  DeltaCompression.h      snapshot delta + bit-packing
  RollbackBuffer.h        예측/롤백
  AntiCheat.h             서버 sanity (속도/위치/쿨다운 위반 검출)
Shared/Replay/              현재 R0~R3 계획대로
```

### 챔프 150 / 100인 PvP / 오픈월드별 적용
- LoL 5v5: AOI 없이도 OK, 현재 구조로 충분.
- 로아 레이드 8인: AOI cell + Replication Graph로 actor priority 다층화.
- GTA6/오픈월드: AOI 필수 + Replication Graph + Iris-tier dormancy.

---

## Ch8. GameplayAbilitySystem (GAS)

여기가 **150 챔프 / 보스 패턴 / 직업 다수 스킬**을 가능하게 하는 핵심.

### 현재 Winters
```text
Client/Public/GameObject/SkillTable.h   현재 챔프별 hard-coded
Client/Private/GamePlay/SkillRegistry.cpp
Shared/GameSim/Components/SkillState*   서버 권위 cooldown
```

memory 박제 `project_phase_b11d_v31_ezreal_pending.md`에서 Registry 3종 + hookId 4분할 + ChampionRegistry BanPick 보정안이 박혀있다. → **GAS의 Winters 등가가 막 자리잡는 중.**

### UE5 대응
```text
UnrealEngine/Engine/Source/Runtime/GameplayAbilities/  (Plugins/Runtime 아래에 있을 수도)
UnrealEngine/Engine/Source/Runtime/GameplayTags/
UnrealEngine/Engine/Source/Runtime/GameplayTasks/
```

### Winters에 추가
```text
Shared/GameSim/Abilities/
  AbilitySystem.h           Activate / Cancel / Cooldown / Cost
  AbilityDef.h              데이터 자산 (.ability)
  AbilityTask.h             projectile, channel, dash 등 빌딩블록
  GameplayTag.h             "Status.Stunned", "Damage.Magic" 계층 태그
  GameplayEffect.h          버프/디버프/dot
  AttributeSet.h            HP/MP/AD/AP/CDR 자동 dirty 추적
  GameplayCue.h             fx + sound + montage 묶음 (서버→클라)

Tools/AbilityEditor/        스킬 그래프 에디터 (Phase G EffectTool 위에 얹기)
```

**Bot AI 불변식 재확인** (AGENTS.md / CLAUDE.md 2026-05-12 박제):
> AI는 `GameplayEffect.Apply` / `AbilityTask.Execute`를 직접 호출하지 않는다. AI는 `GameCommand`를 만들고, executor가 `AbilitySystem::TryActivate`를 호출한다. AttributeSet 변경은 서버 GameSim에서만.

---

## Ch9. AI

### 현재 Winters
```text
Shared/GameSim/Systems/BotLaneAISystem.cpp   S10 Stage1 smoke pass
Engine/Public/AI/                            (스켈레톤)
```

### UE5 대응
```text
UnrealEngine/Engine/Source/Runtime/AIModule/
UnrealEngine/Engine/Source/Editor/BehaviorTreeEditor/
UnrealEngine/Engine/Source/Runtime/NavigationSystem/
```

### Winters에 추가
```text
Engine/Public/AI/
  BehaviorTree.h            BT node, decorator, service
  Blackboard.h              key-value 공유 상태
  EQS.h                     Environment Query System (위치 탐색)
  Perception.h              sight/hearing stimuli
  CrowdManager.h            ORCA/RVO
Engine/Public/Navigation/
  NavMesh.h                 recast/detour 등가
  NavLink.h                 점프/벽 넘기
```

S10 BotLaneAISystem는 **BT의 root behavior 1개**로 흡수된다. 보스/몹 패턴/오픈월드 NPC 전부 BT + Blackboard로 통일.

---

## Ch10. UI

### 현재 Winters
```text
Client/Public/UI/             씬별 UI
Engine/Public/Renderer/UIRenderer.h
```

### UE5 대응
```text
UnrealEngine/Engine/Source/Runtime/UMG/
UnrealEngine/Engine/Source/Runtime/Slate/
UnrealEngine/Engine/Source/Runtime/SlateCore/
```

### Winters에 추가
```text
Engine/Public/UI/
  Widget.h, WidgetTree.h         UMG-tier 트리
  LayoutPanel.h                  Horizontal/Vertical/Grid/Canvas
  DataBinding.h                  Attribute → UI 자동 바인딩
  WidgetAnimation.h
  Localization.h                 .loc 테이블, 키 기반
Tools/UIEditor/                  Designer 협업 entry point
```

---

## Ch11. Cinematics / Sequencer

### 현재 Winters
- 카메라는 `Client/Public/DynamicCamera.h` 하나. **시네마틱 트랙/시퀀서 없음.**

### UE5 대응
```text
UnrealEngine/Engine/Source/Runtime/MovieScene/
UnrealEngine/Engine/Source/Runtime/MovieSceneTracks/
UnrealEngine/Engine/Source/Editor/Sequencer/
```

### Winters에 추가
```text
Engine/Public/Cinematic/
  Sequence.h, Track.h, Section.h
  CameraTrack.h, TransformTrack.h, AudioTrack.h, EventTrack.h
  SequencePlayer.h
Tools/SequencerEditor/         시네마틱 컷씬 편집
```

엘든링/GTA6 컷씬, LoL 챔프 처형 모션, 로아 보스 페이즈 컷에 필수.

---

## Ch12. Editor

여기가 **기획자/디자이너가 들어오는 입구**다. 현재 Winters Editor는 ImGui 기반 in-game tuner 수준. UE5 Editor는 거대한 별도 앱.

### 현재 Winters
```text
Engine/Public/Editor/         (ImGui 기반 in-game)
Client/Private/Scene/Scene_Editor.cpp  맵 에디터
```

### UE5 대응
```text
UnrealEngine/Engine/Source/Editor/   ~200 모듈
  UnrealEd/                          코어 에디터
  LevelEditor/                       월드 편집
  ContentBrowser/                    에셋 브라우저
  BlueprintGraph/                    노드 그래프
  PropertyEditor/                    DetailsPanel
  AssetTools/                        import/export
```

### Winters 단계별 확장 (현실적으로)
```text
[Stage A — 현재] ImGui 기반 in-game tuner (B-6.7 / EffectTuner 완료)
[Stage B] Tools/WintersEditor/   별도 exe로 분리 (ImGui + docking + asset 트리)
  ├── ContentBrowser              .wmesh / .anim / .ability / .fx asset 탐색
  ├── WorldEditor                 Scene_Editor 확장
  ├── DetailsPanel                reflection 기반 property 편집
  ├── BlueprintLite               비주얼 스크립팅 (Ability/AI/Sequence 한정)
  └── AssetImportPipeline         drag&drop 자동 wmesh/wanim 변환
[Stage C] Reflection 시스템      runtime + editor 공통 type DB
[Stage D] Hot reload              C++ 변경 시 게임 안 끄고 반영
```

### Reflection이 먼저 필요한 이유
UE5 UPROPERTY/UCLASS 매크로처럼, Winters도 reflection 없이는 DetailsPanel/Blueprint/Serialization을 못 만든다.

```text
Engine/Public/Reflection/
  Type.h                       runtime type info
  Property.h                   name + offset + meta
  ReflectionMacros.h           WINTERS_CLASS / WINTERS_PROPERTY
  Serializer.h                 binary/json/yaml
Tools/HeaderTool/              .h 파싱 → reflection 코드 생성 (UE5 UHT 등가)
```

---

## Ch13. Tooling (Build, Cook, DDC)

### UE5 대응
```text
UnrealEngine/Engine/Source/Programs/UnrealBuildTool/
UnrealEngine/Engine/Source/Programs/UnrealHeaderTool/
UnrealEngine/Engine/Source/Runtime/DerivedDataCache/
```

### Winters에 추가
```text
Tools/WintersBuildTool/         .vcxproj 자동 생성 (현재는 손으로 .filters 관리)
Tools/WintersHeaderTool/        reflection 코드 생성
Tools/WintersCooker/            플랫폼별 asset cook (DX11/DX12/Vulkan/Metal)
Tools/DerivedDataCache/         shader/wmesh compile 결과 캐시 (팀 공유)
Tools/AssetValidator/           cook 전 sanity (누락 텍스처, 데드 ref)
Tools/Profiler/                 별도 desktop tool (현재 in-game ImGui 확장)
```

**현재 통증**: `.vcxproj`와 `.filters`를 손으로 관리하는 게 곧 한계. UE5 UBT 등가가 1년 안에 필요.

---

## Ch14. Backend Services 확장

### 현재 Winters (`Services/`)
```text
internal/auth/        로그인
internal/shop/        상점
internal/matchmaking/ 매칭
internal/profile/     프로필
internal/payment/     결제
internal/leaderboard/ 랭킹
```

Go monorepo + docker-compose. 좋은 출발.

### AAA 라이브 서비스에 추가
```text
Services/internal/
  inventory/          (로아: 캐릭별 인벤/창고/펫)
  guild/              (로아/엘든링 길드/연합)
  social/             친구/파티/우편/채팅
  liveops/            이벤트/패치노트/공지/CMS
  telemetry/          gameplay event ingest (BigQuery/ClickHouse)
  analytics/          DAU/MAU/retention/funnel
  antifraud/          결제 fraud, bot 탐지
  crashreport/        Sentry/Backtrace 등가
  notification/       push/이메일
  cdn/                패치/asset 다운로드 메타
  region/             멀티 region routing
  presence/           온라인 상태
  party/              파티 룸 매칭 직전
  chat/               실시간 chat (Redis Streams/Kafka)
Services/migrations/   Atlas/sqlc 기반 schema
Services/observability/ Prometheus + Grafana + Loki dashboard
```

### 게임별 적용
- **LoL**: matchmaking + shop + leaderboard + chat + party 정도.
- **로스트아크**: 위 + inventory + guild + liveops + auction-house.
- **GTA6 온라인**: 위 + region + cdn + antifraud(차/돈) + crew + heist 인스턴스.

---

## Ch15. Data Pipeline

### Winters에 추가
```text
Shared/Data/
  DataTable.h           CSV/JSON → struct 매핑
  CurveTable.h          레벨별 스탯 커브
  ContentRegistry.h     모든 ID(챔프/스킬/아이템) 단일 색인
  DataAssetTypes/
    .champion (id, basestat, abilities[])
    .ability  (id, cost, cooldown, effects[])
    .item     (id, slot, stats, abilities[])
    .quest    (id, objectives, rewards)
    .npc      (id, mesh, ai, dialogue)
Tools/DataEditor/       기획자가 .xlsx 또는 web에서 편집 → cook
```

기획자/디자이너 협업의 80%가 이 챕터에서 일어난다.

---

## Ch16. Cross-discipline 협업 토폴로지

### 역할별 entry point

```text
┌────────────────────────────────────────────────────────────────┐
│  기획자 (Game Designer)                                          │
│  in: Tools/DataEditor (web 또는 desktop)                         │
│      .xlsx / DataTable / CurveTable                             │
│      Tools/AbilityEditor (Blueprint Lite — Ch8/Ch12 의존)       │
│  out: cook → Client/Bin/Resource/Data/                          │
│  검증: in-editor PIE (Play-In-Editor) — Ch12 Stage B 필수       │
└────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│  레벨 디자이너 / 아티스트                                          │
│  in: Tools/WintersEditor (World/Sequencer/Effect)                │
│      Blender / Maya → .wmesh / .wanim (Tools/AssetConverter)     │
│      Photoshop / Substance → .png / .wtex                       │
│      Tools/EffectTool (Phase G)                                  │
│  out: AssetRegistry 자동 색인 → Content/                         │
│  검증: Asset Validator, RenderDoc                               │
└────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│  클라 개발자                                                      │
│  in:  Client/Private/Scene/, Client/Private/GameObject/         │
│       Client/Private/UI/, Client/Private/Network/               │
│  out: Snapshot/Event 소비 + Visual + Input → GameCommand        │
│  금지: gameplay 결과 직접 수정 (HP/Cooldown/Position)            │
└────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│  엔진 개발자                                                      │
│  in:  Engine/Public, Engine/Private                             │
│  out: 모든 챕터 capability 제공                                  │
│  검증: SmokeHost (DX12/Vulkan), Unit test, Benchmark            │
└────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│  서버 / GameSim 개발자                                            │
│  in: Server/, Shared/GameSim/, Shared/Network/                  │
│      Fiber × IOCP 인프라 (memory: project_fiber_mastery...)      │
│  out: gameplay truth, simulation tick, AOI, replication         │
│  금지: 클라용 visual/animation/sound 직접 호출                    │
└────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│  백엔드 개발자 (Go)                                               │
│  in: Services/internal/*                                        │
│  out: Auth/Shop/Match/Profile/Telemetry/LiveOps                 │
│  검증: Postman, docker-compose, k6 load test                    │
└────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│  QA / Live Ops                                                  │
│  in: Tools/Profiler (desktop), Telemetry dashboard              │
│      Replay 시스템 (R0~R3 — `.md/TODO/05-09/Replay/`)            │
│  out: 버그 리포트 + replay 첨부, A/B 테스트 설정                  │
└────────────────────────────────────────────────────────────────┘
```

### 협업이 깨지지 않는 4가지 단일 진실원(Single Source of Truth)

| 도메인 | SSoT 위치 | 누가 owner | 누가 consumer |
|--------|-----------|-----------|--------------|
| Gameplay state | `Shared/GameSim/` ECS World | 서버/GameSim 개발자 | 클라(visual), AI(producer) |
| 데이터 자산 (스탯/스킬/아이템) | `Content/` (cook) + `Tools/DataEditor` | 기획자 | 클라/서버/엔진 |
| Animation cue / FX cue | actionSeq → notify track | 애니메이터 + 클라 | 서버 cue 송신, 클라 재생 |
| Asset binary | `Content/.wmesh` `.wanim` `.wtex` | 아티스트 | AssetCache → 런타임 |

### 디렉토리 차원에서 강제하는 의존성 방향

```text
Services/  ──→ DB / 외부                  (게임 클라/서버 모름)
   │
   ↓ REST/gRPC
Server/Shared/GameSim/  ──→ Shared/Network ──→ Shared/Schemas
   │                                              ↑
   ↓ snapshot/event                               │
Client/  ──→ Engine/  ──→ RHI                     │
   ↓                                              │
   └──→ GameCommand ──────────────────────────────┘

UE5에도 같은 방향:
  Programs/  Editor/  Developer/   ← Runtime/ 의존
  Runtime/   ← 다른 Runtime 모듈만 의존 (Editor 모름)
```

**위반 시 빌드 깨짐**으로 강제해야 한다 (UBT 등가 Tools/WintersBuildTool이 해줘야 할 일).

---

## 챕터 도입 우선순위 (실용적 6년 로드맵)

```text
[Year 1: 지금 ~ S10 안정화 + 격투/AOS 한 판 완결]
  S10 후속 (Death/Respawn/Tower/Nexus)
  Ch1 RHI 완성 (DX12 Client visual parity)
  Ch4 Animation StateMachine 최소
  Ch7 Networking IOCP×Fiber 본격
  Ch8 GAS 1차 (Ezreal Q vertical slice 확장)

[Year 2: 챔프 30 → 150 + 협동 콘텐츠]
  Ch2 RenderGraph + Cluster + GI 1차
  Ch8 GAS 완성
  Ch9 AI BT/Blackboard
  Ch12 Editor Stage B (별도 exe + ContentBrowser)
  Ch13 Reflection + HeaderTool
  Ch14 Services 확장 (guild/social/telemetry)

[Year 3: 오픈월드 시도]
  Ch3 WorldPartition
  Ch2 Nanite/VT 등가
  Ch5 Physics (Jolt 통합)
  Ch11 Sequencer
  Ch12 Editor Stage C (Reflection 완성)

[Year 4-5: AAA-tier]
  모든 챕터 production 품질
  Vulkan/Metal/Console backend
  Live Ops / Telemetry / Anti-cheat 성숙
  Ch12 Editor Stage D (Hot reload)
```

---

## Rollback / 실패 시 되돌릴 범위

이 brief는 **문서 출력만**이다. 코드/파일 변경 없음. 되돌릴 것 없음.

## 다음 slice

원래 진행: **S10 후속 — Death/TargetInvalid/Respawn 안정화.** 본 brief는 그 위에 얹는 long-horizon 지도일 뿐, 현재 sequence를 대체하지 않는다.

특정 챕터(예: Ch8 GAS, Ch12 Editor)를 깊게 파고 싶으면 그 챕터만 따로 handoff 받겠다고 알려 달라. 그러면 해당 챕터의 **rg-검증 anchor + 신규 파일 전문 + 검증 명령**까지 내려간 구현 handoff로 출력한다.
