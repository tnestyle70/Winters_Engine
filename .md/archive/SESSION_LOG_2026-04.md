# Session Log 2026-04 (Compressed from CLAUDE.md, 2026-05-01)

> **출처**: `CLAUDE.md` 압축 (2026-05-01) 시 분리된 4월 세션 누적 로그.
> 압축 원본: `.md/archive/CLAUDE_MD_2026-05-01_PRE_COMPRESSION.md` (1285 lines).
> 신규 CLAUDE.md (운영 브리프) 의 Current Focus 는 본 로그의 시간순 누적과 분리됨.

---

## 2026-04-30 — 네트워크 마스터 + TCP MVP + Ezreal FX 계획서 박제

- **Phase Sim-10 v2 (UDP NetStack 마스터)**: TCP→UDP 전환 + 자체 Reliability Layer + Snapshot Delta + AOI + Lag Compensation + Client Prediction + 결정성 보강. 6 마일스톤 (M1 Transport / M2 Reliability / M3 Delta+AOI / M4 LagComp / M5 Prediction / M6 Determinism) + Sim-11 Encryption 별도. Codex 7건 보정 (`ackSeq=newest` / Fragment `u16+messageId` / `SnapshotEnvelope` root / `stable_sort+/fp:precise` M1 부터 / Encryption 분리 / Prediction sim-only subset / grep allowlist). LoL 본체 5%→30% 도약 로드맵. **위치**: `.md/plan/sim/10_UDP_LOL_NETSTACK_MASTER_v2.md`
- **Phase 04a v2 (TCP MVP 2-client demo)**: 현 sketch (TCP IOCP) 마무리 → 1 server + 2 client Move/HP sync. Codex 11건 보정 (C1-C8 + repo 실측 C9 SkillStateComponent slots / C10 MoveTargetComponent 신규 / C11 Event.fbs 기존 확장) + 사용자 A' transport boundary 채택 (6 transport-aware vs 6 transport-neutral — UDP 마이그 시 28h 분량 그대로 재사용). Layer 1 필수 (~58h) + Layer 2 선택 (Cast event echo, +15-25h). **D-0 신규** — ServerSimSubset + MoveTargetComponent 신규 + Shared cpp 명시 편입. SnapshotApplier `OnNewEntity` callback 패턴. `Server.vcxproj` `/fp:precise` + `Mswsock.lib` 적용 (Codex). Layer 1 의 CastSkill/BasicAttack 은 reject/log only — 실 발동은 Layer 2 의 `ActiveCastComponent` + Event emit. **위치**: `.md/plan/sim/04a_MVP_2CLIENT_TCP_DEMO_v2.md`
- **Phase B-11d-FX v2 (Ezreal FX presets)**: Q/W/E/R/BA FX 안 보이는 문제 — `Ezreal_FxPresets.cpp` 6 stub 함수 본격 박제. Codex 6건 보정 + 5 파일 수정. `vRotation` Kalista 패턴 + 월드 FX `vWorldPos.y` 직박 + Visual hook 빈 채 유지 (이중 호출 방지) + Visual/Gameplay speed 일치. **위치**: `.md/plan/Champion/08_EZREAL_FX_PRESETS_BAKE_v2.md`

### Sub-plan (각 h/cpp 전문 박제됨)

1. `.md/plan/sim/04a_v2_D0_SHARED_LINK_AND_SUBSET.md` — D-0A vcxproj XML diff + D-0B ServerSimSubset 6 파일 (MoveTargetComponent.h + Move/SkillCooldown/DamageQueue/Death h+cpp + CommandExecutor.cpp) 전문
2. `.md/plan/sim/04a_v2_D1_SERVER_TRANSPORT.md` — D-1A~D-1J 10 작업, IOCPCore/FrameParser/Session/Manager/Dispatcher/GameRoom/SnapshotBuilder/main/Hello.fbs 전문
3. `.md/plan/sim/04a_v2_D2_CLIENT_TRANSPORT.md` — D-2A~D-2E 5 작업, ClientNetwork/CommandSerializer/SnapshotApplier (OnNewEntity callback 패턴) + Scene_InGame/InputSystem 통합
4. `.md/plan/sim/04a_v2_D3_VERIFICATION.md` — D-3A~D-3C 검증 + 디버깅 매트릭스 + 회귀 grep 4종 + branch 롤백 전략

### Gotcha 추가 (2026-04-30)

- **vc143.pdb lock**: VS (`devenv.exe`) 켜져 있으면 cl.exe 가 pdb 잡힘 → C1041. 빌드 전 VS 종료 또는 `/p:MultiProcessorCompilation=false /maxcpucount:1`. mspdbsrv leak 시 kill.
- **Engine 단독 빌드 → EngineSDK/inc 자동 동기화**: `Engine.vcxproj` PostBuild Event. Client 빌드 전 Engine 1회 필수.
- **Server.vcxproj 전제 (Sim-10 v2 M1)**: `<FloatingPointModel>Precise</FloatingPointModel>` (Debug+Release) + `Mswsock.lib` + Engine project reference. Codex 가 Precise + Mswsock 완료, Engine ref 는 D-1A.
- **Transport boundary (Sim-10 v2 + 04a v2)**: Server/Game/, Server/Security/, Shared/ 에서 `WSARecv`/`WSASend`/`recvfrom`/`sendto` 0 hit 강제. UDP 마이그 안전성의 핵심.

---

## 2026-04-28 — Phase B-9 자체 포맷 검증 + Phase B-10 제드 추가

- **Phase B-8 가렌 (4/27)**: 가렌 1체 풀 사이클 — 6 레이어 (자원/상태/정의/로직/연출/통합) 명시 분리 학습. ChampionTable + SkillTable 5행 + GarenFxPresets + castFrame hook + ApplyGarenHit. Codex 비판 7건 박제 (ChampionTable 누락 → ApplyLocalPrediction skip / Sync·Update·Render 가드 / W castFrame=1.f / vcxproj 등록 / animPrefix="" 트릭 / fMaximum 필드 / Mesh 0 더미 슬롯)
- **Phase B-9 wskel/wanim Stage3 (4/28, ★ 핵심)**: `.wskel` (rest_transform + GlobalInverseRoot + parent DFS) + `.wanim` (tick 단위 + skel_hash trailer) + WMeshWriter `--skel` 권위 모드 + Animator/CSkeleton 기존 인프라 재사용 + AssetConverter `skel/anim/info` 서브커맨드 + `BuildSkeletonFromStage3(ws, wm)` 합성 (matOffset=wmesh / matRestLocal=wskel) + tick 단위 박제 (sec 아님). **6 챔프 모두 fast-path 진입** (Irelia 149본/68anim, Yasuo 118/44, Sylas 138/80, Kalista 97/56, Garen 72/31). Codex 진단 — `.wmesh` skinned vertex layout (76B) 의 `BLENDINDICES uint32×4 @ 44 + BLENDWEIGHT @ 60` byte offset 이 런타임 Skinned3D IL 과 정확히 매칭 필수.
- **Phase B-10 제드 추가 (4/28)**: 가렌 패턴 미러 — `eChampion::ZED/EZREAL/YONE/JAX/MASTERYI/KINDRED 6개 일괄 enum 추가` + ChampionTable + Scene_InGame 6곳 + BanPick + SkillTable 5행 + ZedFxPresets + castFrame hook + ApplyZedHit + vcxproj 등록. **`zed.wskel` 72bones / `zed.wmesh` stride 76 / 37 wanim 산출** (`convert_all_assets.bat champions` → `OK=7 FAIL=0`)
- **신규 가이드**: `.md/guide/CHAMPION_WMESH_PIPELINE_GUIDE.md` (재사용 변환 절차서, 7절 구조)
- **신규 계획서**: `.md/plan/Champion/03_GAREN_PHASE_B8_PIPELINE.md` v2 / `04_GAREN_WSKEL_WANIM_VERIFICATION.md` v4 (Codex 3차 박제) / `04b_5CHAMP_BATCH_CONVERSION.md` / `05_ZED_PHASE_B10_PIPELINE.md`

### 다음 세션 (Phase B-10 잔여 4 챔프 + B-12 메쉬 분리)

- **목표 1 (B-10)**: 피오라/리븐/이즈리얼/요네 4 챔프 일괄 추가. 챔프당 30분 워크플로 (`.md/guide/CHAMPION_WMESH_PIPELINE_GUIDE.md` §5). 자원 (`fiora.fbx/riven.fbx/ezreal.fbx/yone.fbx`) + textured.glb 모두 존재. `convert_all_assets.bat` 에 4 줄 추가만으로 변환 진입.
- **요네 1차는 메쉬 분리 X** — 일반 방식 (Init/Update/Render 단일 인스턴스). 영혼/본체 분리는 B-12 본격.
- **목표 2 (B-12 메쉬 분리 파이프라인)**: `.wmesh` 서브메시 selective render + 본 마스킹 + 다중 ECS Entity 같은 ModelRenderer 공유. 적용 케이스: ① 요네 R 영혼 해방 (본체 idle + 영혼 = 같은 모델 별도 인스턴스 + 다른 anim) ② 엘든링 대비 (보스 부위 파괴, 무기 교체, 변신 메쉬 swap). 별도 계획서: `.md/plan/Champion/06_MESH_SEPARATION_PIPELINE.md` (예정).
- **선택 (B-11)**: AnimEvent → SkillDef.castFrame 박제 — wanim 의 `eAnimEventType::SkillCast/VfxSpawn` 박제로 데이터 드리븐 강화.
- **후속 (Phase C-0~C-8)**: PBR/GGX/Forward+ 그래픽스 파이프라인 (별도 7-Stage 사이클, `.md/plan/graphics/GGX+A/00_INDEX.md`).

---

## 2026-04-24 — Phase B-7 (이렐리아 기본 골격 완성 + 칼리스타 가시성)

### 1차 세션 종합

- **SDK 헤더 폴더-포함 경로 정규화** (14개 파일) + CDX11Device.h ComPtr FQN 전환
- **`CGameInstance` Tier-2 RHI 게터 4개** (`Get_RHIDevice/Get_MeshShader/Get_MeshPipeline/Get_BlendStateCache`) — CEngineApp 엔진 내부 유지
- **`CPlaneRenderer` + AttackRange PNG 렌더링** (Scene_InGame OnEnter/OnRender)
- **Profiler 인프라 완성** — F3 오버레이 / `CCPUProfiler::Create`/`CProfilerOverlay::Create` 생명주기 / `Profiler_Begin/End/Toggle/DrawOverlay` 4개 메서드 구현 / `ProfilerAPI.h __LINE__` 매크로 2단계 CONCAT 수정 / Counter 시스템 (`ProfilerCounter` + `AddCounter` + `WINTERS_PROFILE_COUNT` 매크로 + Overlay Counters 섹션)
- **MinionAI / Nav / Minion_Manager 계측** — 스코프 11곳 + 카운터 4종 (`Nav::RepathCalls/PathNodes/AStar::NodesVisited/Anim::UpdateCalls/Skipped`). **중대 수정**: CEngineApp.cpp 프레임당 Update/Render 2번 호출 중복 제거 / NavigationSystem `Find_Path` 중복 호출 제거
- **Part C `.wmesh` 포맷 완전 구현** — `WintersFileHeader` + `CBinaryReader/Writer` + `WMeshFormat`(POD 5종, static_assert 전수) + `CWMeshLoader`(zero-copy) + `CWMeshWriter`(Assimp 변환, gotcha 3개 박제: 전치/stride 통일/weight fallback)
- **`WintersAssetConverter.exe` 별도 프로젝트** — `Tools/WintersAssetConverter/` (Solution 4번째 vcxproj) + `WINTERS_STATIC_BUILD` 매크로 신설 / **27개 FBX/GLB → .wmesh 전수 변환 성공** (Irelia 60MB → 1.2MB = 50× 압축)
- **런타임 .wmesh 통합 (hybrid)** — `CModel::LoadModel` 에서 `.wmesh` 존재 시 Assimp postprocess 0 + 메시는 zero-copy GPU 업로드. 정적 메시(bone_count=0) 만 fast-path, 챔프는 Assimp 유지
- **프레임 드랍 해결** — `RenderComponent::bAnimated` 플래그 신설 + `HasSkeleton()` 자동 폴백. 맵/구조물/정글몹 `bAnimated=false` 스폰. **확인된 효과**: Frame 17.8ms → 9ms (107 anim updates 규모 감소)
- **수업 컨벤션 통합** — `CEntityBlueprint::AddArgs/Spawn(void*)` 2단계 초기화 (수업 `Initialize_Prototype + Initialize(void*)` 관례 흡수) + `Registry::Clone_Entity(void*)` 오버로드 + `Clear_Scene` + `CGameInstance::Clear_Resources` 실구현
- **3대 버그 수정**: PNG AttackRange 경로 (`UI/` 누락) / Viego FBX 경로 (`Character/` 누락) / CPUProfiler.cpp 가 ProfilerAPI.h 미포함으로 export 속성 누락

### 2차 세션 (이렐리아 기본 골격 완성 + 칼리스타 가시성)

- **핵심 버그 6건 수정**: ① 적 챔프 인식 (`m_bSylasHovered` 플래그 제거 → `IsEnemyOfPlayer(m_HoveredEntity)` 일반화) ② `GetEntityTeam` const 충돌 해결 (CWorld::GetComponent 가 non-const) ③ 애니 배속 (`Animator.cpp:27 += dt * GetTicksPerSecond() * m_fPlaySpeed` 복구, 틱 변환 누락 시 24~30배 감속) ④ SkillTimingPanel `.rdata` 크래시 (`static const SkillDef s_SkillTable[]` → `static`) ⑤ UI PNG 경로 이스케이프 (`\U`/`\u` → forward slash) ⑥ 칼리스타 Spear 가시성 (봇 idle 미기동 → bind pose 렌더)
- **PreemptAction 시스템** — 이동↔스킬↔공격 우선순위 교체. `m_fLastActionTimer/pActiveSkillDef/pPendingEndAnim/fEndTransitionTimer` 일괄 리셋. 이동 블록에 `!bActionLocked` 가드 (스킬 시전 중 이동 자동 정지). 우클릭 실패 분기에 `PreemptAction("Move")` 호출로 공격 중 지면 이동 가능
- **W 홀드형 전환** — `IsKeyPressed('W')` + `IsKeyReleased('W')` 분리. stage1=spell2 (방어 자세 루프), stage2=spell2_2 (원뿔 공격 해제 시). `stageWindowSec=4.0` (원작 최대 홀드)
- **SkillDef 5 필드 확장** — `animPlaySpeed` / `stage2PlaySpeed` (스킬별 애니 배속, lockDurationSec 와 독립) / `endTransitionIdleAnim` / `endTransitionRunAnim` / `endTransitionDuration` (공중→지면 전환 같은 연결 애니)
- **R 애니 연계 확정** — `spell4 → spell4_to_idle/to_run → idle/run` 자연 전환. castFrame=14 기준. `m_pLastDispatchedSkill` 별도 포인터 (recoveryFrame 이후에도 유지 — `m_pActiveSkillDef` nullptr 리셋 시점과 분리)
- **Q Dash 시스템** — `UpdateDash(dt)` 선형 보간 + 타겟 방향 접근 (meleeRange 1.5m 남기기) + 완료 시 BA 쿨 0 리셋 (원작 Q 패시브). `m_bDashActive` / `m_vDashStart/End` / `m_fDashDuration` Scene_InGame 멤버
- **CAnimator 배속 튜닝** — `SetPlaySpeed` / `GetPlaySpeed` / `GetCurrentAnimationDurationSec` (FBX 실측 길이, `dur/ticksPerSec`). ModelRenderer non-const `GetAnimator()` 오버로드. `lockDurationSec` 실측값 이렐리아: 평타=1.0 / Q=0.5 / W1=4.0 / E=1.0 / R=5.0
- **W 애니 키 매칭 수정** — `"spell2_1"` → `"spell2"`. Substring 매칭 원리 활용 (FBX 내 `irelia_spell2` 실재, `spell2_1` 부재)
- **ChampionTunerPanel 신규** — `Champion Tuner` 윈도우: Attack Speed Multiplier / Global Anim Speed / Basic Attack Range / Q Dash Duration / Q Dash Melee Gap 실시간 슬라이더. Debug State (ActionLocked / Remaining Lock) 표시
- **SkillTimingPanel 확장** — lockDuration / animPlaySpeed / stg2 playSpeed 슬라이더 추가
- **`WINTERS_MIN_SCENE` 매크로** — 이렐리아+칼리스타만 로드, 맵/미니언/야스오/사일러스/비에고 스킵. 빌드-실행 루프 5초 이내. `Scene_InGame.cpp:3` 에 `#define WINTERS_MIN_SCENE 1`. 7개 `#if` 가드
- **UI PNG 3건 수정** — ① `UI_Manager::Load_TextureSRV` 경로 forward slash 전환 (single_bar_blue/red) + WIC context 인자 `m_pContext` 주입 (mipmap 자동 생성) ② `MSG_BOX` 누락된 닫는 괄호 수정 ③ 2D 노란 링 블록 L937-961 제거 (3D AttackRange 쿼드만 유지)
- **이렐리아 FX 변환 완료** — `Client/Bin/Resource/Texture/FX/Irelia/` 에 100+ PNG (q_mark_pulse_erode / e_stun_beam_dark / disarm_ring 등). `.tex` → `.png` 사전 변환으로 Phase D 즉시 진입 가능
- **칼리스타 Spear 손 부착** — `m_Kalista.PlayAnimationByName("kalista_idle1")` 봇 idle 명시 기동 → bind pose 탈출 → armature skinning 으로 자동 부착. 3 submesh 텍스처 분리 (Body / Spear / Altar_Spear)

### 미결 (다음 세션 1순위)

- **UI_Manager HP 팀별 바 PNG 로드 실패** — `single_bar_blue.png` / `single_bar_red.png` 로드 안 됨. 인프라 완성 (SRV 2개, `UI_Resolve_Team` 헬퍼 조회, per-entity 팀별 텍스처 분기). 다음: DirectXTK WIC 로드 실패 원인 파악 (파일 실존 / WIC 초기화 / PNG 포맷)
- **Tools/AssetConverter main.cpp 통합** — Engine.vcxproj 에서 `ExcludedFromBuild=true` 상태. Tools 프로젝트에서만 컴파일됨 (정상 작동)
- **절대 경로 문자열 이스케이프 주의** — `"C:\Users\..."` 에서 `\U`/`\u`/`\W` 가 C++ 이스케이프 시퀀스 → 경로 깨짐

---

## 2026-04-19~23 — Phase B-6.6 / B-6.7 (맵 에디터 + Stage Data + Skill Dispatch)

### 직전 완료

- Phase B-6.7 완료 (맵 에디터 + Stage Data 시스템)
  - Structure/Jungle Manager 2개 싱글턴 (`DECLARE_SINGLETON`, `CGameApp::OnInit` 에서 Initialize + Stage1.dat 로드)
  - `Client/Public/Map/` — `MapDataFormats.h` + `MapDataIO.h/.cpp` (바이너리 `.dat`, STAGE_VERSION=3)
  - `Scene_Editor` 신규 씬 (M 키 진입, 좌클릭 배치, Hierarchy/Inspector/Palette, Ctrl+S 저장)
  - StructureEntry 에 `tier` (Outer/Inner/Inhib/Nexus) + `lane` (Top/Mid/Bot/Base) 필드
  - 미니언은 **에디터 제외** — 런타임 스폰 (Phase 4) 대상
  - Scene_InGame 1354 → 약 1000줄로 슬림화
- Phase B-6.6 (SkillDispatch + 2-stage + Cooldown + Rotation + FMOD)
- Phase B-6 (호버 타겟팅 + 우클릭 평타/QWER + 맵 튜너)
- Phase B-5 (ImGui DX11 통합, IScene/SceneManager)

---

## 2026-04-15~17 — Phase 1a / B-4 / B-5 산출물

### Phase 1a 산출물 (2026-04-15~16)

- ✅ `ECS/Components/TransformComponent.h` — 부모/자식 계층 + Dirty Propagation
- ✅ `ECS/Systems/TransformSystem.h/.cpp` — `CTransformSystem`
- ✅ `RHI/IBuffer.h` — Buffer 추상 인터페이스
- ✅ `RHI/DX11/DX11VertexBuffer.h/.cpp` — VB 전담
- ✅ `RHI/DX11/DX11IndexBuffer.h/.cpp` — IB 전담 (16/32bit)
- ✅ `RHI/DX11/DX11StructuredBuffer.h` — 템플릿 + `BoneMatrix` alias
- ✅ `RHI/DX11/SamplerStateCache.h/.cpp` — 6개 프리셋

### Phase B-4 산출물 (2026-04-16)

- ✅ 소환사의 협곡 glb 로딩 (Layer 노드 스킵으로 Z-fighting 해결)
- ✅ 맵 오브젝트 21종 렌더링
- ✅ DX11 ComPtr 마이그레이션 (수업 1일차 컨벤션 통합)

### Phase B-5 진행 중 (2026-04-17~)

- ✅ `Engine/Public/Editor/ImGuiLayer.h/cpp` — DX11 + Win32 백엔드
- ✅ `Engine/Include/IScene.h` — Scene 인터페이스
- ✅ `Engine/Public/Scene/Scene_Manager.h/cpp` — Change_Scene/Update/Render/ImGui 래핑
- ✅ `Client/Public/Scene/Scene_InGame.h` + `Scene_InGame.cpp` — 기존 인게임 렌더링 이관
- ✅ `CEngineApp::Render` 에서 ImGui BeginFrame / EndFrame 체인 연결
- 🔄 Scene_Loading 스캐폴드 (백그라운드 쓰레드 로더, ImGui ProgressBar) — 작업 중
- 🔄 Perf Overlay (FPS, draw call metrics) — 작업 중
- 🔄 `IWintersApp::OnImGui()` 글로벌 훅 연결 — 작업 중
- ⏭️ Scene_InGame 에디터 UI 확장 (엔티티 인스펙터, 파라미터 슬라이더)

---

## 완료된 챔피언 렌더링 (Phase B-4 ~ B-10 누적)

| 챔프 | Phase | 비고 |
|---|---|---|
| 이렐리아 | B-7 (4/24) | 68 anim, 4 mesh, body+blades 텍스처 + 풀 스킬 시스템 (PreemptAction/Q Dash/W 2-stage/R 연계) |
| 야스오 | B-7 | 44 anim, 4 mesh, body 텍스처 |
| 사일러스 | B-7 | 80 anim, 6 mesh, body+chain 텍스처 |
| 비에고 | B-7 | 83 anim, 6 mesh, body+crown_sword+wraith 텍스처 |
| 칼리스타 | B-7 | body+spear 텍스처, 봇 idle 명시 기동 |
| 가렌 | B-8 (4/27) | 72 bones / 31 wanim. 6 레이어 분리 학습 (자원/상태/정의/로직/연출/통합) |
| 제드 | B-10 (4/28) | 72 bones / 37 wanim. 가렌 패턴 미러 |

다음 (B-10 잔여): 피오라/리븐/이즈리얼/요네 4 챔프 일괄 추가.

---

## Reference

- 압축 전 원본: `.md/archive/CLAUDE_MD_2026-05-01_PRE_COMPRESSION.md`
- 신규 운영 브리프: `CLAUDE.md` (워크트리 루트)
- codex 압축 검토 계획서: `.md/plan/engine/CLAUDE_COMPRESSION_REVIEW_PLAN_2026_05_01.md`
- 진행 트랙 (오늘 이후): 신규 CLAUDE.md `Current Focus` 섹션 참조
