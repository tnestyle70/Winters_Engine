# Winters Engine

**범용 DX11 C++20 게임 엔진** — 하나의 엔진 DLL 로 LoL 모작과 엘든링 모작을 구동.

```
WintersEngine.dll
├── WintersLOL.exe      ← 첫 타겟: LoL 30일 모작 (풀스택 MOBA)
└── WintersElden.exe    ← 두 번째 타겟: 엘든링 모작 (액션RPG)
```

**★ 최신 세션 (2026-04-30) — 네트워크 마스터 + TCP MVP + Ezreal FX 계획서 박제**:
  - **Phase Sim-10 v2 (UDP NetStack 마스터)**: TCP→UDP 전환 + 자체 Reliability Layer + Snapshot Delta + AOI + Lag Compensation + Client Prediction + 결정성 보강. 6 마일스톤 (M1 Transport / M2 Reliability / M3 Delta+AOI / M4 LagComp / M5 Prediction / M6 Determinism) + Sim-11 Encryption 별도. Codex 7건 보정 (`ackSeq=newest` / Fragment `u16+messageId` / `SnapshotEnvelope` root / `stable_sort+/fp:precise` M1 부터 / Encryption 분리 / Prediction sim-only subset / grep allowlist). LoL 본체 5%→30% 도약 로드맵. **위치**: `.md/plan/sim/10_UDP_LOL_NETSTACK_MASTER_v2.md`
  - **Phase 04a v2 (TCP MVP 2-client demo)**: 현 sketch (TCP IOCP) 마무리 → 1 server + 2 client Move/HP sync. Codex 11건 보정 (C1-C8 + repo 실측 C9 SkillStateComponent slots / C10 MoveTargetComponent 신규 / C11 Event.fbs 기존 확장) + 사용자 A' transport boundary 채택 (6 transport-aware vs 6 transport-neutral — UDP 마이그 시 28h 분량 그대로 재사용). Layer 1 필수 (~58h) + Layer 2 선택 (Cast event echo, +15-25h). **D-0 신규** — ServerSimSubset + MoveTargetComponent 신규 + Shared cpp 명시 편입. SnapshotApplier `OnNewEntity` callback 패턴. `Server.vcxproj` `/fp:precise` + `Mswsock.lib` 적용 (Codex). Layer 1 의 CastSkill/BasicAttack 은 reject/log only — 실 발동은 Layer 2 의 `ActiveCastComponent` + Event emit. **위치**: `.md/plan/sim/04a_MVP_2CLIENT_TCP_DEMO_v2.md`
  - **Phase B-11d-FX v2 (Ezreal FX presets)**: Q/W/E/R/BA FX 안 보이는 문제 — `Ezreal_FxPresets.cpp` 6 stub 함수 본격 박제. Codex 6건 보정 + 5 파일 수정 (`SkillHookContext` + `Scene_InGame` ctx 주입 + `Ezreal_FxPresets.h/.cpp` 시그니처 + `Ezreal_Skills.cpp`). `vRotation` Kalista 패턴 (`atan2f(dir.x, dir.z) + XM_PI`) + 월드 FX `vWorldPos.y` 직박 + Visual hook 빈 채 유지 (이중 호출 방지) + Visual/Gameplay speed 일치. `particles/fbx/` 5개 + `particles/` 17개 자산 매핑. **위치**: `.md/plan/Champion/08_EZREAL_FX_PRESETS_BAKE_v2.md`

**★ 다음 세션 즉시 진입 명령 (2026-04-30 기준 — D-0~D-3 sub-plan 박제 완료)**:

**진입 sub-plan (각 h/cpp 전문 박제됨)**:
1. `.md/plan/sim/04a_v2_D0_SHARED_LINK_AND_SUBSET.md` — D-0A vcxproj XML diff + D-0B ServerSimSubset 6 파일 (MoveTargetComponent.h + Move/SkillCooldown/DamageQueue/Death h+cpp + CommandExecutor.cpp) 전문
2. `.md/plan/sim/04a_v2_D1_SERVER_TRANSPORT.md` — D-1A~D-1J 10 작업, IOCPCore/FrameParser/Session/Manager/Dispatcher/GameRoom/SnapshotBuilder/main/Hello.fbs 전문
3. `.md/plan/sim/04a_v2_D2_CLIENT_TRANSPORT.md` — D-2A~D-2E 5 작업, ClientNetwork/CommandSerializer/SnapshotApplier (OnNewEntity callback 패턴) + Scene_InGame/InputSystem 통합
4. `.md/plan/sim/04a_v2_D3_VERIFICATION.md` — D-3A~D-3C 검증 + 디버깅 매트릭스 + 회귀 grep 4종 + branch 롤백 전략

**즉시 진입 명령**:
```
"04a v2 D-0 sub-plan 부터 시작. .md/plan/sim/04a_v2_D0_SHARED_LINK_AND_SUBSET.md §1 Server.vcxproj XML diff 적용 → §2 ServerSimSubset 6 파일 박제 → 빌드 검증. 합격 시 D-1 sub-plan (04a_v2_D1_SERVER_TRANSPORT.md) D-1A 진입 → D-1J 까지 → D-2 → D-3 Layer 1 합격."
```

**진입 전 체크리스트**:
- [ ] devenv.exe (Visual Studio) 종료 — vc143.pdb lock 회피
- [ ] git: `feature/04a-v2-d0` branch 생성
- [ ] Engine 단독 빌드 1회 — EngineSDK/inc 동기화 확인
- [ ] 빌드 명령: `MSBuild Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:MultiProcessorCompilation=false /maxcpucount:1 /v:minimal`

**Gotcha 추가 (2026-04-30)**:
- **vc143.pdb lock** (중요): Visual Studio (`devenv.exe`) 가 켜져 있으면 cl.exe 가 `vc143.pdb` 잡힘 → `C1041` 빌드 실패. 빌드 전 VS 종료 또는 `/p:MultiProcessorCompilation=false /maxcpucount:1` 옵션. mspdbsrv 도 leak 시 kill.
- **Engine 단독 빌드 → EngineSDK/inc 자동 동기화**: `Engine.vcxproj` PostBuild Event 가 헤더 복사. Client 빌드 전에 Engine 빌드 1회 필수.
- **Server.vcxproj 전제 (Sim-10 v2 M1)**: `<FloatingPointModel>Precise</FloatingPointModel>` (Debug+Release) + `Mswsock.lib` 링크 의존성 + Engine project reference (M1 진입 시 추가). Codex 가 Precise + Mswsock 적용 완료, Engine ref 는 D-1A.
- **Transport boundary (Sim-10 v2 + 04a v2)**: Server/Game/, Server/Security/, Shared/ 에서 `WSARecv`/`WSASend`/`recvfrom`/`sendto` 호출 0 hit 강제 (grep 검증). UDP 마이그 안전성의 핵심.

---

**현재 진행**: **Phase B-9 자체 포맷 검증 + Phase B-10 제드 추가 완료** — 2026-04-28
  - **Phase B-8 가렌 (4/27)**: 가렌 1체 풀 사이클 — 6 레이어 (자원/상태/정의/로직/연출/통합) 명시 분리 학습. ChampionTable + SkillTable 5행 + GarenFxPresets + castFrame hook + ApplyGarenHit. Codex 비판 7건 박제 (ChampionTable 누락 → ApplyLocalPrediction skip / Sync·Update·Render 가드 / W castFrame=1.f / vcxproj 등록 / animPrefix="" 트릭 / fMaximum 필드 / Mesh 0 더미 슬롯)
  - **Phase B-9 wskel/wanim Stage3 (4/28, ★ 핵심)**: `.wskel` (rest_transform + GlobalInverseRoot + parent DFS) + `.wanim` (tick 단위 + skel_hash trailer) + WMeshWriter `--skel` 권위 모드 + Animator/CSkeleton 기존 인프라 재사용 + AssetConverter `skel/anim/info` 서브커맨드 + `BuildSkeletonFromStage3(ws, wm)` 합성 (matOffset=wmesh / matRestLocal=wskel) + tick 단위 박제 (sec 아님). **6 챔프 모두 fast-path 진입** (Irelia 149본/68anim, Yasuo 118/44, Sylas 138/80, Kalista 97/56, Garen 72/31). Codex 진단 — `.wmesh` skinned vertex layout (76B) 의 `BLENDINDICES uint32×4 @ 44 + BLENDWEIGHT @ 60` byte offset 이 런타임 Skinned3D IL 과 정확히 매칭 필수 (가렌 사고: writer 가 `tan float4 → weights → packed uint8 idx` 순서로 write → fast-path 진입했는데 화면 안 보임 → CLAUDE.md gotcha 박제)
  - **Phase B-10 제드 추가 (4/28)**: 가렌 패턴 미러 — `eChampion::ZED/EZREAL/YONE/JAX/MASTERYI/KINDRED 6개 일괄 enum 추가` + ChampionTable + Scene_InGame 6곳 + BanPick + SkillTable 5행 + ZedFxPresets + castFrame hook + ApplyZedHit + vcxproj 등록. **`zed.wskel` 72bones / `zed.wmesh` stride 76 / 37 wanim 산출** (`convert_all_assets.bat champions` → `OK=7 FAIL=0`)
  - **신규 가이드**: `.md/guide/CHAMPION_WMESH_PIPELINE_GUIDE.md` (재사용 변환 절차서, 7절 구조)
  - **신규 계획서**: `.md/plan/Champion/03_GAREN_PHASE_B8_PIPELINE.md` v2 / `04_GAREN_WSKEL_WANIM_VERIFICATION.md` v4 (Codex 3차 박제) / `04b_5CHAMP_BATCH_CONVERSION.md` / `05_ZED_PHASE_B10_PIPELINE.md`

**★ 다음 세션 (Phase B-10 잔여 4 챔프 + B-12 메쉬 분리)**:
  - **목표 1 (B-10)**: 피오라/리븐/이즈리얼/요네 4 챔프 일괄 추가. 챔프당 30분 워크플로 (`.md/guide/CHAMPION_WMESH_PIPELINE_GUIDE.md` §5). 자원 (`fiora.fbx/riven.fbx/ezreal.fbx/yone.fbx`) + textured.glb 모두 존재. `convert_all_assets.bat` 에 4 줄 추가만으로 변환 진입.
  - **요네 1차는 메쉬 분리 X** — 일반 방식 (Init/Update/Render 단일 인스턴스). 영혼/본체 분리는 B-12 본격.
  - **목표 2 (B-12 메쉬 분리 파이프라인)**: `.wmesh` 서브메시 selective render + 본 마스킹 + 다중 ECS Entity 같은 ModelRenderer 공유. 적용 케이스: ① 요네 R 영혼 해방 (본체 idle + 영혼 = 같은 모델 별도 인스턴스 + 다른 anim) ② 엘든링 대비 (보스 부위 파괴, 무기 교체, 변신 메쉬 swap). 별도 계획서: `.md/plan/Champion/06_MESH_SEPARATION_PIPELINE.md` (예정).
  - **선택 (B-11)**: AnimEvent → SkillDef.castFrame 박제 — wanim 의 `eAnimEventType::SkillCast/VfxSpawn` 박제로 데이터 드리븐 강화. PBR 진입 직전 마무리.
  - **후속 (Phase C-0~C-8)**: PBR/GGX/Forward+ 그래픽스 파이프라인 (별도 7-Stage 사이클, `.md/plan/graphics/GGX+A/00_INDEX.md`). 챔프 다 끝낸 후 진입 결정.

**다음 세션 즉시 진입 명령**:
```
"Phase B-10 잔여 4 챔프 추가. 05_ZED_PHASE_B10_PIPELINE.md §12 워크플로 + CHAMPION_WMESH_PIPELINE_GUIDE.md §5 따라 피오라부터 진행"
```

---

**이전 진행**: Phase B-6.7 완료 (맵 에디터 + Stage Data 시스템)
  - Structure/Jungle Manager 2개 싱글턴 (`DECLARE_SINGLETON`, `CGameApp::OnInit` 에서 Initialize + Stage1.dat 로드)
  - `Client/Public/Map/` — `MapDataFormats.h` + `MapDataIO.h/.cpp` (바이너리 `.dat`, STAGE_VERSION=3)
  - `Scene_Editor` 신규 씬 (M 키 진입, 좌클릭 배치, Hierarchy/Inspector/Palette, Ctrl+S 저장)
  - StructureEntry 에 `tier` (Outer/Inner/Inhib/Nexus) + `lane` (Top/Mid/Bot/Base) 필드
  - 미니언은 **에디터 제외** — 런타임 스폰 (Phase 4) 대상
  - Scene_InGame 1354 → 약 1000줄로 슬림화 (defs[44] / ObjectLayout.txt / SaveObjectLayout 제거)
**직전 완료 (2026-04-24 세션 종합)**:
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

**직전 완료 (2026-04-24 세션 2차 — 이렐리아 기본 골격 완성 + 칼리스타 가시성)**:
- **핵심 버그 6건 수정**: ① 적 챔프 인식 (`m_bSylasHovered` 플래그 제거 → `IsEnemyOfPlayer(m_HoveredEntity)` 일반화) ② `GetEntityTeam` const 충돌 해결 (CWorld::GetComponent 가 non-const) ③ 애니 배속 (`Animator.cpp:27 += dt * GetTicksPerSecond() * m_fPlaySpeed` 복구, 틱 변환 누락 시 24~30배 감속) ④ SkillTimingPanel `.rdata` 크래시 (`static const SkillDef s_SkillTable[]` → `static`) ⑤ UI PNG 경로 이스케이프 (`\U`/`\u` → forward slash) ⑥ 칼리스타 Spear 가시성 (봇 idle 미기동 → bind pose 렌더)
- **PreemptAction 시스템** — 이동↔스킬↔공격 우선순위 교체. `m_fLastActionTimer/pActiveSkillDef/pPendingEndAnim/fEndTransitionTimer` 일괄 리셋. 이동 블록에 `!bActionLocked` 가드 (스킬 시전 중 이동 자동 정지). 우클릭 실패 분기에 `PreemptAction("Move")` 호출로 공격 중 지면 이동 가능
- **W 홀드형 전환** — `IsKeyPressed('W')` + `IsKeyReleased('W')` 분리. stage1=spell2 (방어 자세 루프), stage2=spell2_2 (원뿔 공격 해제 시). `stageWindowSec=4.0` (원작 최대 홀드)
- **SkillDef 5 필드 확장** — `animPlaySpeed` / `stage2PlaySpeed` (스킬별 애니 배속, lockDurationSec 와 독립) / `endTransitionIdleAnim` / `endTransitionRunAnim` / `endTransitionDuration` (공중→지면 전환 같은 연결 애니)
- **R 애니 연계 확정** — `spell4 → spell4_to_idle/to_run → idle/run` 자연 전환. castFrame=14 기준. `m_pLastDispatchedSkill` 별도 포인터 (recoveryFrame 이후에도 유지 — `m_pActiveSkillDef` nullptr 리셋 시점과 분리)
- **Q Dash 시스템** — `UpdateDash(dt)` 선형 보간 + 타겟 방향 접근 (meleeRange 1.5m 남기기) + 완료 시 BA 쿨 0 리셋 (원작 Q 패시브). `m_bDashActive` / `m_vDashStart/End` / `m_fDashDuration` Scene_InGame 멤버
- **CAnimator 배속 튜닝** — `SetPlaySpeed` / `GetPlaySpeed` / `GetCurrentAnimationDurationSec` (FBX 실측 길이, `dur/ticksPerSec`). ModelRenderer non-const `GetAnimator()` 오버로드. `lockDurationSec` 실측값 이렐리아: 평타=1.0 / Q=0.5 / W1=4.0 / E=1.0 / R=5.0
- **W 애니 키 매칭 수정** — `"spell2_1"` → `"spell2"`. Substring 매칭 원리 활용 (FBX 내 `irelia_spell2` 실재, `spell2_1` 부재)
- **ChampionTunerPanel 신규** — `Champion Tuner` 윈도우: Attack Speed Multiplier / Global Anim Speed / Basic Attack Range / Q Dash Duration / Q Dash Melee Gap 실시간 슬라이더. Debug State (ActionLocked / Remaining Lock) 표시
- **SkillTimingPanel 확장** — lockDuration / animPlaySpeed / stg2 playSpeed 슬라이더 추가 (슬라이더 드래그로 런타임 튜닝)
- **`WINTERS_MIN_SCENE` 매크로** — 이렐리아+칼리스타만 로드, 맵/미니언/야스오/사일러스/비에고 스킵. 빌드-실행 루프 5초 이내. `Scene_InGame.cpp:3` 에 `#define WINTERS_MIN_SCENE 1`. 7개 `#if` 가드
- **UI PNG 3건 수정** — ① `UI_Manager::Load_TextureSRV` 경로 forward slash 전환 (single_bar_blue/red) + WIC context 인자 `m_pContext` 주입 (mipmap 자동 생성) ② `MSG_BOX` 누락된 닫는 괄호 수정 ③ 2D 노란 링 블록 L937-961 제거 (3D AttackRange 쿼드만 유지). 미니언 강제 활성화 (HP바 검증용)
- **이렐리아 FX 변환 완료** — `Client/Bin/Resource/Texture/FX/Irelia/` 에 100+ PNG (q_mark_pulse_erode / e_stun_beam_dark / disarm_ring 등 핵심 에셋 전부). `.tex` → `.png` 사전 변환으로 Phase D 즉시 진입 가능
- **칼리스타 Spear 손 부착** — `m_Kalista.PlayAnimationByName("kalista_idle1")` 봇 idle 명시 기동 → bind pose 탈출 → armature skinning 으로 자동 부착. 3 submesh 텍스처 분리 (Body / Spear / Altar_Spear)

**미결 (다음 세션 1순위)**:
- **UI_Manager HP 팀별 바 PNG 로드 실패** — `single_bar_blue.png` / `single_bar_red.png` 로드 안 됨. 인프라 완성 (SRV 2개, `UI_Resolve_Team` 헬퍼 조회, per-entity 팀별 텍스처 분기). 다음: DirectXTK WIC 로드 실패 원인 파악 (파일 실존 / WIC 초기화 / PNG 포맷)
- **Tools/AssetConverter main.cpp 통합** — Engine.vcxproj 에서 `ExcludedFromBuild=true` 상태. Tools 프로젝트에서만 컴파일됨 (정상 작동)
- **절대 경로 문자열 이스케이프 주의** — `"C:\Users\..."` 에서 `\U`/`\u`/`\W` 가 C++ 이스케이프 시퀀스 → 경로 깨짐. `/` 슬래시 또는 `\\` 이중화 필수 (gotchas 추가 필요)

**직전 완료**: Phase B-6.6 (SkillDispatch + 2-stage + Cooldown + Rotation + FMOD) / B-6 (호버 타겟팅 + 우클릭 평타/QWER + 맵 튜너) / B-5 (ImGui DX11 통합, IScene/SceneManager)

**★ 다음 (2026-04-25~ 예정)**:
1. **UI HP 팀색 바 PNG 로드 수복** — WIC 실패 원인 진단 후 정상 동작. 실패 시 Rect fallback 으로도 팀색 구분 됨 (파/빨 테두리).
2. **Phase 5-B JobSystem race 정식 수정** — Chase-Lev Main-push 위반 해결 → AnimUpdate 병렬화로 추가 2~3ms 절감. `Set_JobSystem` 주석/`#if 0` 복구. 메모리: `project_session_2026_04_23.md`.
3. **.wmesh Stage 3 확장** — `.wanim` / `.wskel` 분리 저장 → 챔프/미니언도 fast-path 전환 (스키닝 메시 .wmesh 활성). Stage 3 계획서: `.md/plan/WintersFormat/04_STAGE3_WANIM_WSKEL.md`.
4. **Quadtree View Frustum Culling** — 카메라 밖 엔티티 AnimUpdate/Render 스킵. LoL 탑뷰 2D 공간 인덱스.
5. **학원 컨벤션 매트릭스 CLAUDE.md 반영** — `.md/architecture/CLASS_DAY8_VS_WINTERS.md` 의 결정 표 CLAUDE.md §483 직전 삽입 (CGameObject/CLayer/CObject_Manager 미도입 명문화, CLoader 흡수 예정).

**이후 (B-7a 계열, 원래 트랙)**: ModelRenderer 분해 (AssetRegistry + 인스턴스별 CAnimator + MeshRefComponent)
  → B-7b (ChampionSpawnSystem), B-8 (UI 패널 분리), B-9 (Input/Dispatch 시스템화),
  B-10 (SkillHook + Champions/ 폴더 self-register), B-11 (BuffSystem),
  B-12 (레거시 CTransform 제거), Phase 4 (네트워크 — Scene 수정 0)
**★ 아키텍처 개요 (처음 읽기)**: `.md/architecture/WINTERS_GAMEPLAY_ARCHITECTURE.md` (11 섹션, 150 챔프 타겟)

**마스터 문서**:
- 마스터 플랜: `.md/roadmap/LOL_30DAY_MASTER_PLAN.md`
- 아키텍처: `.md/architecture/WINTERS_ENGINE_ARCHITECTURE_FINAL.md`
- 코딩 컨벤션: `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md`
- 백엔드: `.md/plan/backend/00_BACKEND_PLAN_INDEX.md`
- 보안: `.md/plan/security/00_SECURITY_INDEX.md`
- C++/CS 지식베이스: `C:\Users\user\Desktop\.markdown\C++&CS\00_CPP_CS_KNOWLEDGE_INDEX.md`

---

## 기술 스택

DX11 + RHI · ECS · JobSystem (Fiber 계획) · **자체 Physics** (PBD + Rigid Body + Constraint + CCD)
· **자체 고급 렌더링** (BRDF + PBR + Monte Carlo + Path Tracing + 실시간 GI + FFT + TAA)
· **자체 AI 봇** (HFSM + Behavior Tree + GOAP + Utility AI + Influence Map + MCTS + RL 확장)
· **FMOD (ThirdPartyLib 편입 완료)** · Lua 5.4 · FlatBuffers · Go 백엔드 · Kafka · 커널 레벨 안티치트.

**서드파티 의존성**: Assimp / DirectXTK / FMOD 전부 `Engine/ThirdPartyLib/` 자립 구조 (vcpkg 미사용).
새 라이브러리 편입 표준 절차는 `.md/build/THIRDPARTY_INTEGRATION_GUIDE.md` 참고.

**외부 엔진/라이브러리 의존 제거**:
- **Jolt Physics 폐기** — PBD·강체·조화 진동·그래픽스 물리 수학을 포트폴리오급으로 직접 구현 (Phase D).
- **상용 렌더링 프레임워크 미사용** — BRDF, 몬테카를로 적분, Path Tracing, Real-Time GI, FFT (Ocean/Bloom), TAA 전부 이론부터 직접 구현 (Phase E).
- **AI 외부 라이브러리 미사용** — FSM / BT / GOAP / Utility / Influence Map / MCTS 전부 직접 구현. 실제 LoL 봇 수준 이상의 고도화된 지능 목표 (Phase F).

---

## 게임 콘텐츠 목표 (LoL 모작)

### 메타 게임 플로우

```
[로고] → [로그인] → [메인 메뉴]
                         │
                         ├──→ [상점] → RP 충전, 스킨 구매 (영속)
                         ├──→ [프로필] → 전적, 인벤토리, 장착 스킨
                         └──→ [매칭 큐]
                                  │ 매칭 성사
                                  ↓
                             [챔피언 선택]
                                  │
                                  ↓
                            [로딩 씬] ← 서버로부터 각자 장착 스킨 수신
                                  │
                                  ↓
                              [인게임]
                                  │ 넥서스 파괴
                                  ↓
                            [결과 화면] → [메인 메뉴]
```

### 상점 시스템

- **RP (Riot Points)** 충전 + 스킨 구매
- 백엔드 연동: **Shop Service (8086)** + **Payment Service (8085)**
- 구매 내역은 PostgreSQL 에 영속 → **프로세스 종료 후 다음 로그인 시 그대로 유지**
- 구매 이벤트 플로우: Client → Shop 서비스 → DB 저장 → Kafka `SkinPurchased` 발행
  → Profile 서비스 Consumer 가 인벤토리 업데이트
- 인게임 진입 시 각 플레이어 장착 스킨 SkinID 를 서버에서 내려받아 `CModel` 교체

### 매칭 시스템

- **Matchmaking Service (8083)**
- 큐 입장 → 10명 모집 완료 → 챔피언 선택 페이즈 진입
- 챔피언 선택 초기 구현: 밴픽 없이 단순 픽
- 확장: 포지션 지정 / 밴픽 / 재픽

### 게임 모드 2종

#### A. 봇전 (Co-op vs AI)

- 구성: 아군 플레이어 5명 + **적 봇 5명 (고도화 AI)**
- 필드 요소 전부 구현:
  - 정글 몬스터 (블루/레드/크루그/늑대/새/드래곤/바론/리프트헤럴드)
  - 미니언 3라인 (근접/원거리/공성)
  - 포탑 9개 (외곽 3 · 내곽 3 · 억제기 3)
  - 억제기 3개 + 넥서스 1개
- **종료 조건**: 어느 한쪽 넥서스 파괴 → 결과 화면 → 메인 메뉴 복귀
- 서버 권위 (Phase 4 UDP/KCP + 서버사이드 검증)
- **AI 봇 수준** — 실제 LoL "Intermediate" 봇 이상. 목표는 **Master 난이도 (인간 골드~플래티넘 체감)**.
  상세 Stage 는 Phase F (자체 AI 봇) 참조. 정글 몹 AI 는 단순 Aggro 반경 + 리쉬 로직 (Phase F Stage 0 수준).

#### B. 연습모드 (Custom + Editor 통합)

- **에디터급 커스터마이징**:
  - 챔피언 위치/체력/마나/쿨다운 실시간 조작
  - 오브젝트 배치 (정글 몬스터 리스폰, 미니언 웨이브 생성 트리거)
  - 스킬 레벨, 아이템 프리셋, 골드 증감
- 실제 적 플레이어 입장 가능 → 전투 시연/연구
- **1v1 서브 모드를 연습모드에 통합** — 사용자 설정 (맵 영역/챔피언/조건) 로 1v1 세팅
- ImGui 에디터 UI 와 게임 로직이 한 화면에서 공존 (Phase B-5 ImGui 전면 연동의 궁극 목표)

### 인게임 스킨 장착

1. 로딩 씬 진입 시 서버에서 10명 전원의 선택 챔피언 + 장착 SkinID 수신
2. `CModel::LoadWithSkin(championID, skinID)` 호출 → 해당 경로의 FBX/텍스처 로드
3. 스키닝 애니메이션은 공통 본 구조 재사용 (같은 챔피언의 모든 스킨이 본 이름 통일)

---

## Phase 로드맵

### 엔진 Phase — ECS 전환 주 흐름 (B-7a~B-12 → Phase 4)

| Phase | 내용 | 상태 |
|---|---|---|
| B-4 | 맵 + 21개 오브젝트 + 챔피언 5체 렌더링 (DX11 ComPtr 마이그레이션) | ✅ |
| B-5 | ImGui DX11 통합 + Scene 시스템 + Editor UI | ✅ |
| B-6 | 호버 타겟팅(실린더 피킹) + 우클릭 평타/QWER + 맵 튜너 + CInput 에지 API + 액션 락 | ✅ |
| B-6.5 | eTargetMode 5종 + SkillDef 테이블 + DispatchSkillInput + YasuoStateComponent Conditional | ✅ |
| **B-6.6** | **ChampionDef + SkillDef 확장(rotate/lock/oneShot/2-stage) + SkillStateComponent + RotatePlayerToward + ThirdPartyLib 자립 + FMOD + CSound_Manager** | ✅ |
| **B-7a** | **ModelRenderer 분해 (AssetRegistry + CAnimator 인스턴스별 + RenderComponents) — ECS 소비 가능한 컴포넌트로 쪼갬** | ⏭️ 다음 |
| B-7b | ChampionSpawnSystem + RenderSystem — Scene 의 수동 m_Irelia 멤버 제거, 런타임 스폰 | ⏭️ |
| B-8 | UI 패널 분리 (CombatDebugPanel / MapTunerPanel / SkillStatePanel) | ⏭️ |
| B-9 | InputSystem + SkillDispatchSystem + SkillSimulationSystem + **InputIntent Pulse/State 규약** | ⏭️ |
| B-10 | SkillHook (`uint32_t hookId` + Registry) + Champions/{Irelia,Yasuo}/ self-register | ⏭️ |
| B-11 | BuffSystem (`BuffComponent` + `BuffInstance` 타이머, 첫 적용 Irelia W 방어막) | ⏭️ |
| B-12 | 레거시 `CTransform` 제거, TransformComponent 단일 원천 (침습적, 네트워크 직전) | ⏭️ |
| **Phase 4** | **NetworkInput / StateReplication / Prediction 시스템 — Scene 수정 0** | ⏭️ |
| **D** | **자체 Physics (Stage 1~7) — 포트폴리오급** | 장기 |
| **E** | **자체 고급 렌더링 (BRDF→PBR→MC→Path Tracing→GI→FFT→TAA→DXR)** | 장기 |
| **F** | **자체 AI 봇 (Aggro→HFSM→BT→GOAP→Utility→InfluenceMap→MCTS→모방학습→RL)** | 장기 |

**아키텍처 상세**: `.md/architecture/WINTERS_GAMEPLAY_ARCHITECTURE.md` — 150 챔프 타겟의 7-레이어 구조와 현재 vs 목표 비교, 확정된 6 결정.

### 학원 DX11 수업 흡수 항목 (참조용, 주 흐름은 위의 B-7a~)

| 항목 | 내용 | 비고 |
|---|---|---|
| C-1 | CGameInstance 확장 + CRenderer 렌더 큐 + Scene_Logo/MainMenu | B-7b RenderSystem 과 통합 |
| C-2 | Font + Font_Manager + DamageFontRenderer (BMFont/SDF 3D 빌보드) + DebugDraw | B-8 UI 패널 이후 |
| C-3 | Socket (Bone Attach) + Hitbox/Hurtbox + AnimationEvent | B-10 SkillHook 와 묶어 설계 |
| C-4 | Collider + Bounding(AABB/OBB/Sphere/Capsule) + Picking + Navigation/Cell | B-11 이후 |
| C-5 | VIBuffer 용도별 파생 (Terrain/Cell/Particle_*) | Phase D 진입 전 |

### 게임 콘텐츠 Phase (LoL_30DAY_MASTER_PLAN.md)

| Phase | 일차 | 내용 | 상태 |
|---|---|---|---|
| 0 | D0~D2 | 모델링 추출 & 에셋 파이프라인 | ✅ |
| 1 | D3~D5 | JobSystem & 코어 강화 | 🔄 |
| 2 | D6~D10 | RenderGraph & Deferred Pipeline | ⏭️ |
| 3 | D11~D13 | GPU-Driven Rendering | ⏭️ |
| 4 | D14~D19 | 네트워크 & 게임 서버 (UDP/KCP, IOCP, AOI) | ⏭️ |
| 5 | D20~D23 | Go 백엔드 (Auth/Shop/Match/Profile/Payment) | ✅ Phase 0~7 완료 |
| 6 | D24~D26 | 안티치트 | ⏭️ |
| 7 | D27~D28 | 에디터(ImGui) + 게임 콘텐츠 (맵/챔피언 Lua) | 🔄 |
| 8 | D29 | 통합 테스트 & 폴리싱 | ⏭️ |

---

## 병합 철학 — 학원 DX11 × Winters

**Winters 방식 유지 + 학원 수업 개념 흡수**.

| 축 | 결정 | 이유 |
|---|---|---|
| 메모리 관리 | `unique_ptr`/`shared_ptr` RAII | 모던 C++. 학원 `CBase` 참조카운트 미도입 |
| 오브젝트 모델 | **ECS** (Entity + ComponentStore SoA) | 성능·데이터 지향. 학원 `CGameObject+multimap<Component*>` 미도입 |
| 씬 계층 | `IScene + Scene_Manager` | 단순화. 학원 Management→Scene→Layer→GameObject 중 Layer 제거 |
| 싱글턴 | **`CGameInstance` 하나만** | Tier1·2 정책. 학원 다중 싱글턴 불채택 |
| 셰이더 | raw HLSL + `DX11Shader` | Effects11 제거됨. 학원 `.fx` 미사용 |
| 리소스 캐싱 | `CResourceCache` | 경로 기반. 학원 `CProtoMgr` 역할 흡수 |
| 렌더 큐 | **`CRenderer` + `RENDERID`** 도입 (C-1) | 학원 구조 흡수 |
| 씬 전환 | **Scene_Logo/Loading/MainMenu/InGame** 도입 | 학원 Level_* 흡수 |
| Font | **Font + Font_Manager + DamageFont** 신규 | ImGui 로 불가능한 3D 빌보드 용 |
| Collider | **Collider + Bounding_* + Socket** 신규 | 애니메이션 싱크 포함 |
| Physics | **자체 구현** (Jolt 폐기) | 포트폴리오 목적 |
| 고급 렌더링 | **자체 구현** (BRDF·MC·Path Tracing·GI·FFT·TAA) | 포트폴리오 목적 |
| AI 봇 | **자체 구현** (HFSM·BT·GOAP·Utility·InfluenceMap·MCTS·RL) | 봇전 메인 콘텐츠. 실제 LoL 봇 이상 목표 |
| Navigation | **CNavigation + CCell** 도입 | MOBA 필수 |
| VIBuffer 파생 | **Terrain/Cell/Particle_*** 도입 | 학원 구조 그대로 |
| DebugDraw | **신규 도입** | 콜라이더/본/Ray/Frustum 시각화 |

---

## 학원 DX11 파일 × Winters 매핑 테이블

| 학원 파일 | Winters 대응 | 상태 |
|---|---|---|
| Base (CBase) | ❌ 미도입 (unique_ptr RAII) | N/A |
| GameInstance | ✅ 이미 있음 (Timer/Scene 포워딩). C-1 에서 확장 | 🔄 |
| Timer / Timer_Manager | ✅ CTimer, CTimer_Manager | ✅ |
| Prototype_Manager | 🟡 CResourceCache 로 흡수 확장 | 🔄 |
| Object_Manager | ❌ ECS CWorld 로 대체 | N/A |
| Renderer | 🔴 C-1 에서 CRenderer 신규 | ⏭️ |
| PipeLine | ✅ DX11Pipeline (InputLayout/RS) — 역할 재해석 | ✅ |
| Shader | ✅ DX11Shader (raw HLSL) | ✅ |
| Texture | ✅ CTexture + ResourceCache | ✅ |
| Target_Manager + RenderTarget | ⏭️ Phase 2 G-Buffer | ⏭️ |
| Shadow | ⏭️ Phase 2 | ⏭️ |
| DebugDraw | 🔴 C-2 신규 | ⏭️ |
| Font + Font_Manager | 🔴 C-2 신규 (BMFont/SDF + DamageFont) | ⏭️ |
| Component | ❌ ECS 로 대체 | N/A |
| GameObject | ❌ ECS Entity 로 대체 | N/A |
| ContainerObject / PartObject | 🟡 Socket 시스템으로 대체 (C-3) | ⏭️ |
| UIObject | 🟡 ImGui + TextRenderer 조합 | ⏭️ |
| Transform | ✅ TransformComponent + TransformSystem | ✅ |
| Camera | ✅ CCamera + DynamicCamera | ✅ |
| Animation / Bone / Channel | ✅ Assimp 스키닝 완비 | ✅ |
| Model | ✅ CModel | ✅ |
| VIBuffer | ✅ DX11VertexBuffer / IndexBuffer | ✅ |
| VIBuffer_Rect/Cube | 🟡 CubeGeometry 있음, Rect 는 C-5 | 🔄 |
| VIBuffer_Terrain | 🔴 C-5 신규 | ⏭️ |
| VIBuffer_Cell | 🔴 C-5 신규 | ⏭️ |
| VIBuffer_Particle_* | 🔴 C-5 신규 | ⏭️ |
| Collider | 🔴 C-4 신규 | ⏭️ |
| Bounding_AABB/OBB/Sphere | 🔴 C-4 + Phase D Stage 1 | ⏭️ |
| Navigation + Cell | 🔴 C-4 신규 | ⏭️ |
| Picking | 🔴 C-4 신규 | ⏭️ |

범례: ✅ 완료 / 🔄 진행 / ⏭️ 예정 / ❌ 미도입 / 🔴 신규 필요

---

## 디렉토리

```
Engine/     Include/(공개API) Public/(내부헤더) Private/(구현) → WintersEngine.dll
            ThirdPartyLib/   서드파티 자립 (Assimp/DirectXTK/FMOD — vcpkg 미사용)
              Assimp/    Inc/assimp/, Lib/{Debug,Release}/, Bin/{Debug,Release}/  (+ transitives)
              DirectXTK/ Inc/directxtk/, Lib/{Debug,Release}/, Bin/{Debug,Release}/
              FMOD/      Inc/, Lib/fmod_vc.lib, Bin/fmod.dll (Debug/Release 공용)
Client/     Include/(vcxproj+rc) Public/(헤더) Private/(소스) Bin/(출력) → WintersGame.exe
              Bin/Resource/       원본 리소스 (Shader/Font/Sound/Texture 등)
              Bin/{Debug,Release}/ 빌드 산출물. PostBuild 가 Shaders/Resource 를 이리로 xcopy
Server/     Include/(vcxproj) Public/(헤더) Private/(소스) Bin/(출력)
Shared/     PacketDef.h → 향후 Schemas/ (FlatBuffers .fbs)
Shaders/    *.hlsl (raw HLSL, Effects11 미사용)
Services/   Go 백엔드 (cmd/ 엔트리, internal/ 비즈니스, pkg/ 공용)
AntiCheat/  [Phase 6] Driver/(커널 .sys) Service/(유저모드) Shared/(IOCTL)
Tools/      [Phase 0] blender_export.py, convert_textures.bat, WintersAssetConverter/
EngineSDK/  Engine 공개 헤더 flat 재배포 + .lib (UpdateLib.bat 자동 동기화)
```

향후 게임별 분리:
```
WintersLOL/     Client/(MOBA EXE) Server/(IOCP) Data/(챔피언Lua, 맵)
WintersElden/   Client/(액션RPG EXE) Data/(보스, 오픈월드)
```

---

## Engine 필터 (vcxproj.filters)

```
Include              공개 API (DLL 경계)
00. Manager          DX11/RHI (Device, Pipeline, Shader, Buffer, ConstantBuffer, Geometry)
01. Core             Timer, Input, Transform, Platform(Window), Paths, Allocator, EventBus, Profiler
02. Structure        Framework(게임루프), Entry(DLL 진입점)
03. Renderer         Camera, Cube, Model, Triangle → [C-1+] Renderer, VIBuffer_*, Font, DamageFont,
                                                       [Phase E] BRDF, PBR, GI, PostFX, FFT, Ocean, PathTracer
04. Editor           ImGui DX11 백엔드, Inspector, Console, Perf Overlay
05. ECS              Entity, ComponentStore, World, ISystem, SystemScheduler, CommandBuffer
06. Resource         Model(FBX), Texture, Animation, Bone, Skeleton, ResourceCache, [C-2] Font
07. Physics          [자체 구현] BoundingVolume, RigidBody, ConstraintSolver, PBD, BVH, CCD
08. Audio            CSound_Manager (FMOD) · eSoundChannel enum · Resource/Sound 재귀 UTF-8 로드
09. Network          UDP/KCP, Prediction, PacketSerializer, HttpClient
10. JobSystem        Counter 의존성, Fiber Pool, Work-Stealing Deque
11. Scene            IScene, Scene_Manager
12. Collision        [C-3+] Collider, Socket, Hitbox/Hurtbox, AnimationEvent
13. AI               [Phase F] FSM, BehaviorTree, GOAP, Utility, Blackboard, InfluenceMap, MCTS, Imitation, RL, Bots
```

번호 = 의존성 방향. 낮은 번호는 높은 번호에 의존 X.

## Client 필터

```
00. MainApp         main.cpp + CGameApp
01. Scene           Logo, Loading ✅, MainMenu, Shop, ChampSelect, InGame, PostGame
02. GameObject      Champion (Player/Bot), Minion, Turret, Monster (정글몹), Projectile, Tower, Nexus, Inhibitor
03. Manager         PathFinding, Inventory, Damage, FOW(Fog of War), Skin
04. Network         AuthClient, MatchClient, PaymentClient, ProfileClient, ShopClient (✅ WinHTTP)
05. GameMode        BotMatch, Practice (1v1 통합)
06. AI              [Phase F] BotProfile_Irelia/Yasuo/Sylas/Viego/Kalista — 챔피언별 봇 행동 정의
98. Default         resource.h, Client.rc, 아이콘
99. Defines         Defines.h
Shaders             HLSL
```

## Server 필터

```
00. Server          main.cpp
01. Network         IOCP, Session, PacketHandler
02. Game            GameRoom(5v5), GameLogic(서버 권위), ServerWorld(ECS), AOI(50m 그리드)
03. Security        AntiCheatServer(speedhack/cooldown/range/damage 검증), LagCompensation
Shared              PacketDef.h → Schemas/
```

---

## 코딩 컨벤션

### ★★★ C++ 코드 작성 전 반드시 읽을 것 (순서대로, 5~10분) ★★★

1. **`.md/architecture/WINTERS_ENGINE_CONVENTIONS.md`** — 네이밍/DLL 경계/팩토리/타입 alias 확정 규칙
2. **`winters-skills/code/SKILL.md`** — 코드 작성+리뷰 통합 사이클 (기존 인프라 식별 → 데이터 형태 → DLL 경계 → 검증 결정 포인트 → 최소 수정 → 엣지 케이스). Codex 코드/리뷰 패턴 흡수.
3. **`winters-skills/code-scaffolding/SKILL.md`** + `references/gotchas.md` — 공통 함정/스캐폴딩 패턴
4. **`winters-skills/debug-pipeline/SKILL.md`** — 디버깅 사이클 (셰이더 우선 Read + 데이터 직접 계측. "안 보임" / "한 케이스만 깨짐" 증상)
5. **본 파일 Gotchas 섹션** — 프로젝트 전역 함정

**CLAUDE.md 만 읽고 건너뛸 때 실제로 발생한 사고**:
- 2026-04-19 Phase B-6.7 (맵 에디터/Stage Data): 파일명 `CXxxMgr.h` (컨벤션은 `XxxMgr.h`), `#include` 전부 누락, `#pragma once` 누락, `final`/`NO_COPY` 누락 → Manager 재작성
- 2026-04-19 Scene 전환 (B-6.7): `Change_Scene` 이후 `return` 누락 → use-after-free → ntdll 디버그 break
- 2026-04-26 Phase 1 (이렐리아 FBX): render/PNG 가 sprite 캡처라 mesh UV 가 알파 0 영역 → clip 으로 전 픽셀 버려짐. 셰이더 한 번도 안 읽고 CPU 가설만 누적해 1.5시간 소모. 본 사례 이후 winters-skills/debug-pipeline 도입.
- 2026-04-26 Phase FX (FxSprite): v4 가 `eFxBlendMode` 신설하려 했으나 BlendStateCache 의 `eBlendPreset` (Additive/Premultiplied) 이미 존재 — 4 폴더 grep 누락 사례. 본 사례 이후 `winters-skills/code/SKILL.md` 의 "기존 인프라 식별 우선" 단계 강제.

### ★ C++ 코드 리뷰 시 필독
- `winters-skills/code-review/SKILL.md` — 컨벤션 체크리스트
- `winters-skills/code/SKILL.md` §B — 빌드 차단 vs 동작 차단 vs 엣지 케이스 분리, 매트릭스 분류 (✅ 적용 / ⚠️ 버그 / ❌ 미적용)

### ★ 새 기능 계획 전 의무
- `Engine/Public/Resource/`, `Engine/Public/Core/`, `Engine/Public/Framework/`, `Engine/Public/Renderer/` 4개 폴더 grep 전수 스캔 → 기존 클래스/유틸 중복 방지

신규 클래스/파일 생성 시 위 규칙 **반드시** 따를 것. 위반 = 리뷰 블록.

### ★ ImGui 정책

- ImGui 적극 사용 — 엔진 디버그/튜닝/에디터 UI 전부 ImGui
- `WINTERS_EDITOR` 매크로 Debug/Editor 빌드 활성화, Release `#ifdef` 완전 제거
- 신규 시스템 작성 시 "튜닝 파라미터는 ImGui 슬라이더 노출" 의무 — 하드코딩 금지
- 우선순위: Inspector → Material → Profiler → Animation → Physics → Console → Shader Reload → Network
- 원칙: "빌드 1번으로 모든 값 튜닝" = 기획/디자인 이터레이션 속도 = 게임 품질
- **연습모드가 최종 목표** — 에디터 UI 가 곧 게임 모드가 되는 구조

### 네이밍

- **클래스: `C` 접두사 필수** (`class CTimer`, `class CCamera`)
- **struct (POD): C 접두사 금지** (`struct TransformComponent`)
- **인터페이스: `I` 접두사** (`class IBuffer`, `class ISystem`, `class IScene`)
- **파일명: C 접두사 없이 (확정 규칙)** — `Transform.h` (클래스명 `CTransform`), `IBuffer.h`
  (인터페이스 `IBuffer`). 기존 `C` 접두사 파일은 점진 리네임.
- 헤더: `#pragma once`
- COM 객체: `ComPtr` 필수 (raw `ID3D11*` 포인터 금지)
- HRESULT: 반드시 체크 + errorBlob 출력

### 타입 별칭 (확정 규칙)

- **신규 코드는 반드시** `f32_t` / `f64_t` / `i32_t` / `u32_t` / `wstring_t` / `tchar_t` / `bool_t` 사용
- **금지**: `float32` / `float64` / `int32` / `uint32` / `WString` / `WStr` / raw `float` / raw `int`
  — 레거시 호환 alias, 신규 코드 사용 금지
- **예외 (그대로 사용)**: Win32 타입 (`HWND`/`LPARAM`/`DWORD`/`UINT` 등), DirectXMath
  (`XMVECTOR`/`XMFLOAT3` 등), 서드파티 (Assimp/FMOD)
- **공개 API 헤더** (`WintersMath.h`/`WintersTypes.h`/`EngineConfig.h`/`IWintersApp.h`/`WintersPaths.h`):
  Phase 4 별도 일괄 정리 예정, 그 전까지 현행 유지 (신규 추가 시에만 컨벤션 적용)
- `unique_ptr` 등 STL 은 `std::` 생략 가능 (`Engine_Defines.h` 에서 `using namespace std`)
- 상세: `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md §1.4.1`

### Include 컨벤션 (★ 2026-04-24 확정)

- **Client 헤더**: `#include "Defines.h"` → STL + Engine 타입 전부 해결
- **Engine 헤더**: `#include "WintersAPI.h"` + `"WintersTypes.h"` + `"WintersMath.h"` (Engine_Defines.h 미포함)
- `Defines.h` 에 `WintersMath.h` 미포함 → Vec3/Mat4 필요 시 별도 include

#### 폴더-포함 경로 원칙 (필수)

- 서브디렉토리 헤더는 **반드시 폴더 경로 포함**: `"ECS/Entity.h"`, `"Resource/Texture.h"`, `"Sound/SoundChannel.h"`, `"RHI/CDX11Device.h"`
- **금지**: `"Entity.h"` (경로 생략) — Engine 은 `AdditionalIncludeDirectories` 에 `..\Public\ECS` 등 서브폴더가 등록돼 있어 통과하지만 **Client 빌드 실패** (C1083 "포함 파일을 열 수 없습니다")
- 이유: `UpdateLib.bat` 이 `xcopy /S` 로 Engine/Public 폴더 구조를 **그대로** `EngineSDK/inc/` 에 복사. Client 는 `EngineSDK/inc` 하나만 등록 → unqualified 헤더 못 찾음.
- 같은 디렉토리의 형제 헤더는 예외 — MSVC quoted include 는 먼저 해당 파일 위치를 탐색하므로 `"ProfilerTypes.h"` (같은 폴더) 는 OK. 그러나 cross-folder 는 항상 full path.
- **리뷰 블록**: 신규 Engine 헤더가 unqualified subdirectory include 사용 시 즉시 거절. `Engine/Include/` 와 `Engine/Public/` 루트에 있는 헤더 (WintersAPI/Types/Math/IScene/GameContext/Engine_Defines 등) 만 경로 없이 가능.

#### ComPtr 규칙

- `Microsoft::WRL::ComPtr<T>` 를 사용하는 **SDK-노출 헤더** (`Engine/Public/` 또는 `Engine/Include/` 하위, SDK 로 복사되는 것) 는 **반드시 `#include <wrl/client.h>` 명시** + 완전 수식 `Microsoft::WRL::ComPtr<...>` 사용
- `WintersPCH.h` 의 `using Microsoft::WRL::ComPtr` 에 의존 금지 — Client 는 PCH 없이 SDK 헤더를 직접 파싱함. `ComPtr` alias 만 쓰면 C7568 에러 발생.
- 참고 구현: `Engine/Public/RHI/CDX11Device.h`, `Engine/Public/RHI/DX11/BlendStateCache.h` (FQN 방식)
- Engine/Private 소스 파일 (.cpp) 은 PCH 경유로 `ComPtr` alias 사용 가능.

### 클래스 설계 원칙

- **생성자: private** — 외부 `new` 금지. `Create()` 정적 팩토리 또는 팩토리 함수로만 생성
- **소멸자: public virtual** — 스마트 포인터로 수명 관리
- **멤버 변수: 전부 private** — 외부 접근은 public 멤버 함수로만 (파생 허용 시 protected + 주석)
- **변수 조작**: 해당 변수가 선언된 클래스의 함수 내부에서만 직접 조작
- Dirty Flag 패턴, RAII, cbuffer 16바이트 정렬

### Create 팩토리 패턴

```cpp
class CExample
{
public:
    ~CExample() = default;

    static unique_ptr<CExample> Create(/* params */)
    {
        auto pInstance = unique_ptr<CExample>(new CExample());
        // 초기화 ...
        return pInstance;
    }

    void DoSomething();
    i32_t GetValue() const { return m_Value; }

private:
    CExample() = default;
    i32_t m_Value = 0;
};
```

### 멤버 변수 접두사

- `m_f` float: `m_fSpeed`, `m_fFov`
- `m_v` Vec3: `m_vEye`, `m_vAt`
- `m_b` bool: `m_bDirty`, `m_bFix`
- `m_p` pointer: `m_pTargetTransform`
- `m_` 기타: `m_ViewMatrix`, `m_ProjMatrix`

### HLSL / 셰이더

- cbuffer 행렬: `row_major matrix` 또는 `row_major float4x4` 필수 (DirectXMath row-major 관례)
- mul 순서: `mul(vector, matrix)` — 행 벡터 × 행렬
- 셰이더 파일: `Shaders/` 디렉토리, 용도 기반 이름 (Mesh3D, Default3D, Skinned3D)
- 레지스터 슬롯: b0 = PerFrame(VP), b1 = PerObject(World), b2 = BoneMatrices

### 템플릿 / 메타프로그래밍 (2026-04-19 추가, CDPR/Unreal 참고)

**원칙**: 타입이 많고 공통 동작이면 템플릿. 도메인 특수 로직은 OOP 로 유지.

**✅ 템플릿 적극 권장 영역**
- 엔진 코어 컨테이너: `CObjectPool<T>`, `CHandle<T>`, `CSparseSet<T>` (STL `vector<T>` / `unordered_map<K,V>` 도 동일 맥락)
- RHI 버퍼: `DX11ConstantBuffer<T>` (이미 존재, `CBPerFrame`/`CBBoneMatrices` 등 타입별), `CDX11StructuredBuffer<T>`
- ResourceCache: `CResourceCache<T>` 로 Texture/Model/Sound/Font 통합 — 같은 로직에 타입만 다를 때
- JobSystem: `Job<TReturn>`, `TaskQueue<T>`, `IJobSystem::Submit<Fn>(Fn&& job)`
- ECS: `ComponentStore<TComponent>` — Transform/Minion/Combat 등 N 종 컴포넌트 동일 저장 로직
- 수학: DirectXMath 가 이미 타입 안전 템플릿 제공 (`XMVECTOR`/`XMMATRIX` 외 `XMFLOAT4X4` 등), 재발명 금지

**❌ 템플릿 남용 금지 영역**
- 게임플레이 도메인 타입: `CMinion`, `CChampion`, `CTurret`, `CNexus` 등은 구체 클래스 유지. `CUnit<Policy>` 같은 정책 기반 템플릿 금지 — 디버깅 지옥, 컴파일 시간 폭발
- 싱글턴 Manager: 하나뿐인 클래스에 템플릿 의미 없음 (`CStructure_Manager<T>` 금지)
- CRTP 남용: `class CDerived : public Base<CDerived>` 는 인터페이스 + `virtual` 로 충분한 곳에 쓰지 말 것. 이해도 비용만 늘어남
- 깊은 SFINAE / 복잡한 개념(concepts) 중첩: C++20 `concepts` 로 제한 **명시** 는 OK. 중첩 type_traits / `std::enable_if` 체인은 컴파일 에러 메시지 2000줄 유발 → 실기 비용 큼

**💡 판단 기준**
1. "같은 로직에 타입만 다른가?" → YES = 템플릿 후보
2. "타입마다 동작이 정말 다른가?" → 그러면 다형성 (`virtual`) 고려
3. "인터페이스 + `virtual` 로 충분한가?" → 그러면 템플릿 불필요 (과공학)
4. 컴파일 시간 2배 느려지면 추상화 한 단계 제거

**Unreal/CDPR 관찰 차용 요지**: 엔진 레벨 (리소스 관리 / 컨테이너 / 델리게이트 / 리플렉션 / RHI) 은 템플릿 적극. 게임플레이 레벨은 의외로 OOP + 인터페이스 조합이 많음. **균형** 이 핵심. "멋있어서 쓰는 템플릿" 은 유지보수 비용으로 돌아옴 — 동료(미래의 나 포함) 가 5분 안에 이해 못 하는 템플릿은 리팩터링 대상.

**C++20 `concepts` 활용 예** (권장):
```cpp
template<typename T>
concept CComponent = requires(T t) { { t.OnAttach() } -> std::same_as<void>; };

template<CComponent T>
class ComponentStore { /* ... */ };
```
→ 에러 메시지가 "`T` does not satisfy `CComponent` (missing `OnAttach()`)" 형태로 명확.

### GameInstance 경계 규칙

- **신규 싱글턴은 `CGameInstance` 하나뿐.** 기존 `CEngineApp::s_pInstance` / `CInput::Get()` 공존 (점진 이관)
- **Tier 1 (GameInstance 포워딩)**: Timer, Input, Scene, Resource 요청, Sound 고수준, Network 세션 — 저빈도 · 이름 기반
- **Tier 2 (GameInstance 바깥 직접 접근)**: JobSystem, ECS World, RHI draw, Physics step, GPU-driven — 고빈도 핫패스.
  `CGameInstance::Get_JobSystem() -> IJobSystem*` 같은 인터페이스 Getter 만 제공, Client 는 포인터 캐시 후 직접 호출
- **DLL export**: 신규 export 는 `CGameInstance` 만. 내부 매니저는 `ENGINE_DLL`/`WINTERS_API` 마크 금지
- **`DECLARE_SINGLETON` 호출 스타일**: Winters 는 **포인터 반환** — `CGameInstance::Get()->Method()` (`->` 사용)
- **`make_unique` 금지**: private ctor 때문에 `unique_ptr<T>(new T())` 직접 사용
- 상세: `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md §3, §6`

#### Tier-2 RHI 포워딩 게터 (★ 2026-04-24 신설)

- `CEngineApp` 은 **엔진 내부 전용**. Client 는 `CEngineApp::Get()` 을 **직접 호출 금지** — `CGameInstance` 경유.
- `CGameInstance` 가 CEngineApp 을 래핑한 4개 RHI 게터 제공:

  | 게터 | 반환 타입 | 용도 |
  |---|---|---|
  | `Get_RHIDevice()` | `CDX11Device*` | `GetDevice()`/`GetContext()` 호출 시 |
  | `Get_MeshShader()` | `DX11Shader*` | Mesh3D 공유 셰이더 (`CPlaneRenderer::Create` 등) |
  | `Get_MeshPipeline()` | `DX11Pipeline*` | Mesh3D 공유 파이프라인 |
  | `Get_BlendStateCache()` | `CBlendStateCache*` | `AlphaBlend` 프리셋 등 (`SetBlendCache`) |

- 구현: `Engine/Private/GameInstance.cpp` 에서 `&CEngineApp::Get().GetDevice()` 등 1줄 포워딩.
- 헤더 전방선언: `GameInstance.h` 에 `class CDX11Device; class DX11Shader; class DX11Pipeline; class CBlendStateCache;` — d3d11.h Client 오염 방지.
- Client 가 반환 포인터를 역참조할 때만 해당 헤더 include (`#include "RHI/CDX11Device.h"` 등).
- 사용 예: `Client/Private/Scene/Scene_InGame.cpp` OnEnter (AttackRange PNG 로드 블록).

### Sound 사용 규칙 (B-6.6 추가)

- Client 는 반드시 `CGameInstance::Get()->PlaySoundOn(...)` / `PlayEffect` / `PlayBGM` 등 **Tier1 포워딩** 으로만 접근
- `CSound_Manager` 를 직접 참조 금지 (EngineSDK 에 헤더 배포되지만 내부 매니저)
- 키는 `Resource/Sound/` 기준 상대경로 `wstring_t`. 예: `L"BGM/Title.wav"`, `L"Irelia/attack1.wav"`
- 채널: `eSoundChannel` enum 9 슬롯 (BGM / PlayerAction / PlayerVoice / UI / Ambient / Effect0~3)
  - 고정 채널 = 같은 채널에 재생 중이면 stop 후 교체 (`PlaySoundOn`)
  - 자동 채널 = 겹침 허용 (`PlayEffect`)
  - BGM = Loop 고정 (`PlayBGM`)
- 신규 .wav 추가 → `Client/Bin/Resource/Sound/<category>/` 에 배치 → Client 빌드 시 PostBuild 가 `$(OutDir)Resource\` 로 xcopy (/D 로 증분 복사)

---

## 서브시스템 설계 원칙

### Font / DamageFont (Phase C-2)

- **BMFont 또는 SDF 아틀라스** (오프라인 생성) — `.fnt` 메타 + `.png/.dds` 아틀라스
- `CFont` — 아틀라스 텍스처 + char→UV 딕셔너리 (char 범위는 ASCII + 한글 조합형 일부)
- `CFont_Manager` — 태그 기반 폰트 등록/조회 (학원 `CProtoMgr` 의 Font 전담 역할)
- `CTextRenderer` — 2D 스크린 고정 텍스트 (HUD, 미니맵 라벨)
- **`CDamageFontRenderer`** — 3D 월드 빌보드 숫자
  - `Spawn(worldPos, value, color, lifetime)` API
  - ECS `DamageFontComponent { text, pos, velocity, lifetime, scale, color }` 엔티티화
  - `DamageFontSystem` 매 프레임 위치 부유 + 알파 감쇠 + 카메라 빌보드
  - LoL 의 크리티컬/일반/힐 색 구분 지원
- **ImGui 폰트와 분리** — 에디터용 ≠ 게임 내 데미지 폰트

### Socket + Hitbox/Hurtbox + AnimationEvent (Phase C-3)

- 무기/콜라이더는 Transform 이 아닌 **본 월드 행렬에 부착**
- `SocketComponent { targetBoneIndex, localOffset, parentEntity }`
- `SocketSystem` — 매 프레임 `socketWorld = boneWorldMatrix × localOffset`
- `HitboxComponent { shape, active, damage, layerMask }` — 공격측
- `HurtboxComponent { shape, owner, layerMask }` — 피격측
- **AnimationEvent** — `.wanim` 에 이벤트 트랙 포함 (triggerTime + type + payload)
  - `HIT_START` → Hitbox active = true
  - `HIT_END` → Hitbox active = false
  - `FOOTSTEP` / `SFX` / `VFX_SPAWN` / `DAMAGE_NUMBER`
- `CAnimator` 가 currentTime 비교 후 이벤트 dispatch
- **CCD** (Phase D Stage 7) — 빠른 공격은 prevSocketPos→currSocketPos 캡슐 스윕

### 자체 Physics Stage (Phase D — 포트폴리오)

외부 물리 엔진 없이 강체·연체·구속·CCD 를 직접 구현. 참고 문헌:
Real-Time Collision Detection (Ericson), Game Physics Engine Development (Millington), Box2D/Bullet 소스,
Matthias Müller 2007 PBD 논문, Erin Catto GDC Sequential Impulse.

1. **프리미티브 충돌** (AABB / Sphere / Ray + Picking 공유)
2. **Narrow Phase** (SAT OBB-OBB, GJK+EPA Convex)
3. **Broad Phase** (Sweep and Prune → Dynamic AABB Tree)
4. **Rigid Body** (Semi-implicit Euler, 관성텐서, 외력 적분)
5. **Constraint Solver** (Erin Catto Sequential Impulse — Contact, Distance, Hinge, Slider)
6. **PBD** (Matthias Müller 2007 + XPBD — Cloth, Rope, Soft Body)
7. **CCD** (TOI — 빠른 공격/투사체 관통 방지)

**Engine 통합 지점**:
```
Engine/Public/Physics/
├── Collision/         BoundingVolume (AABB/Sphere/OBB/Capsule), NarrowPhase (SAT/GJK/EPA), BroadPhase (SAP/BVH)
├── Dynamics/          CRigidBody, CConstraintSolver, Integrator
├── PBD/               CParticleSystem, CClothSolver, CRopeSolver
└── CCD/               TOI, SweptCapsule
```

### 자체 고급 렌더링 Stage (Phase E — 포트폴리오)

Physics 와 동일 방향. 외부 렌더링 라이브러리/프레임워크 없이 **수학·그래픽스 이론을 직접 구현**.
참고 문헌: Real-Time Rendering 4th Ed., Physically Based Rendering 3rd Ed., GPU Gems, SIGGRAPH 논문.

1. **BRDF 이론 + 구현**
   - Lambertian / Phong / Blinn-Phong (기초)
   - **Cook-Torrance** (Microfacet, Fresnel-Schlick, Geometry G, NDF D)
   - **GGX / Trowbridge-Reitz** (업계 표준 NDF)
   - Disney Principled BSDF (Metallic/Roughness/Anisotropy/Clearcoat)
   - Energy Conservation 검증

2. **PBR 파이프라인**
   - Metallic / Roughness 워크플로우
   - Image-Based Lighting — Irradiance Map + Prefiltered Specular + BRDF LUT
   - Split-Sum Approximation (Epic Games UE4)

3. **몬테카를로 적분**
   - 확률 이론 + 기대값/분산
   - Importance Sampling (중요도 샘플링)
   - **Multiple Importance Sampling (MIS)** — Veach & Guibas
   - Low-Discrepancy Sequences (Halton, Sobol, Owen Scramble)
   - 분산 감소 (Stratified, Quasi-Monte Carlo)

4. **오프라인 Path Tracing (Reference 렌더러)**
   - Recursive Ray Tracing (Whitted 1980)
   - Path Tracing (Kajiya 1986) — GPU Compute Shader 구현
   - Russian Roulette 종료
   - Next Event Estimation (Direct Light Sampling)
   - 재질 샘플링 (Diffuse/Specular/Glass)
   - BVH (SAH 기반 빌드) — Physics Broad Phase 와 공유

5. **실시간 Global Illumination**
   - SSAO / HBAO+ (Screen Space Ambient Occlusion)
   - SSR (Screen Space Reflections) — Hi-Z Ray Marching
   - Voxel Cone Tracing (VXGI — Crassin 2011)
   - DDGI (Dynamic Diffuse GI — NVIDIA 2019, Probe 기반)
   - Light Propagation Volumes (LPV) — 비교 학습용

6. **FFT 기반 기법**
   - 이산 푸리에 변환 → **Cooley-Tukey FFT 직접 구현** (Compute Shader)
   - **Ocean Wave Simulation** (Tessendorf 2001) — Phillips Spectrum → IFFT → 변위/법선
   - **FFT Bloom / Convolution** (Large Kernel Bloom)
   - Depth of Field Circular Bokeh (FFT 컨볼루션)

7. **Temporal 기법**
   - **TAA** — Halton Jitter, Neighborhood Clamping, Variance Clipping
   - Temporal Upsampling (DLSS/FSR 원리 축소 학습)
   - Temporal Reprojection (GI/SSR 재활용)
   - Motion Vectors 생성 + 검증

8. **Hardware Ray Tracing (DXR) — 선택**
   - BLAS / TLAS Acceleration Structure
   - Ray Generation / Closest Hit / Miss 셰이더
   - Hybrid: Raster G-Buffer + DXR Shadow/Reflection

**구현 순서**: 1 (BRDF) → 2 (PBR+IBL) → 3 (MC 이론) → 4 (Path Tracing Ref) → 5 (실시간 GI)
→ 6 (FFT Ocean+Bloom) → 7 (TAA) → 8 (DXR, 선택).

**Engine 통합 지점**:
```
Engine/Public/Renderer/
├── BRDF/              Cook-Torrance, GGX, Disney
├── PBR/               IBL precompute, Material model
├── GI/                SSAO, SSR, VXGI, DDGI
├── PostFX/
│   ├── TAA.h
│   ├── Bloom_FFT.h
│   └── DoF_FFT.h
├── FFT/               Cooley-Tukey 직접 구현
├── Ocean/             Tessendorf + IFFT 변위
└── PathTracer/        오프라인 Reference (Compute)
```

### 자체 AI 봇 Stage (Phase F — 봇전 메인 콘텐츠, 포트폴리오급)

**목표**: 실제 LoL Intermediate/Intro 봇 수준을 **넘어서는** 고도화된 지능. 인간 골드~플래티넘 체감의
Master 난이도 프리셋을 최종 목표로. MOBA AI 연구 최신 흐름 (OpenAI Five, AlphaStar) 축소판 적용.

참고 문헌: Game AI Pro 1/2/3, Programming Game AI by Example (Buckland), Behavioral Mathematics for
Game AI, OpenAI Five 논문 (Dota 2 PPO), AlphaStar 논문 (StarCraft II LSTM + Self-Play),
DeepMind FTW (Quake III CTF).

#### 3-Layer 의사결정 계층

모든 봇은 3단 계층으로 의사결정:

```
┌──────────────────────────────────────────────────────┐
│  Strategic (매크로) — 팀 단위 목표 선택              │
│    "지금 드래곤 싸움? 바텀 갱? 바론? 백도어?"        │
│    업데이트 주기: 2~5초                             │
└────────────────────────┬─────────────────────────────┘
                         │
┌────────────────────────┴─────────────────────────────┐
│  Tactical (미들) — 개별 봇의 경로/포지션/교전 판단   │
│    "어느 라인·정글 진입 경로? 탱커 앞? 원딜 뒤?"     │
│    업데이트 주기: 0.2~0.5초                         │
└────────────────────────┬─────────────────────────────┘
                         │
┌────────────────────────┴─────────────────────────────┐
│  Operational (마이크로) — 즉각 실행                 │
│    "스킬 샷 예측 조준, 평타 무빙, 궁 캔슬링"         │
│    업데이트 주기: 매 프레임                         │
└──────────────────────────────────────────────────────┘
```

#### Stage 0: 단순 Aggro (정글몹/미니언용)

- Aggro 반경 + 어그로 우선순위 (거리 · HP · 피해량 · 마지막 공격자)
- 리쉬 (Leash) 반경: 초과 시 리셋 + 무적 귀환
- 단순 평타 + 간단한 스킬 사용 (예: 크루그 소환, 바론 침묵)

#### Stage 1: HFSM (Hierarchical FSM)

계층형 상태 머신. 루트 상태 밑에 서브 상태.

- **Root States**: `Laning` / `Farming` / `Ganking` / `TeamFighting` / `Pushing` / `Recalling` /
  `Defending` / `Objective` / `Dead`
- **Sub-States** 예시 (Laning): `Farming_Safe` / `Farming_Aggressive` / `Trading` / `Zoning` / `Freezing`
- 상태 전이 조건: HP / 마나 / 쿨다운 / 아군 거리 / 적 거리 / 미니언 웨이브 / 오브젝트 타이머

#### Stage 2: Behavior Tree (BT)

복잡한 행동은 FSM 대신 BT 로. Selector / Sequence / Parallel / Decorator 노드 조합.

- 예: `Attack Champion` 트리
  ```
  Selector
  ├── Sequence: [IsInRange → HasMana → Cast Q]
  ├── Sequence: [IsLowHP → HasFlash → Flash Away]
  └── Fallback: Basic Attack + Kite Move
  ```
- `.bt` 데이터 파일 + 런타임 인터프리터 (Lua 연동 고려)

#### Stage 3: GOAP (Goal-Oriented Action Planning)

여러 행동의 시퀀스를 자동 계획. A* 로 목표 상태까지 최소 비용 경로 탐색.

- WorldState: `{ hp, mana, gold, items, position, cooldowns, teamDistance }`
- Goal: `KillEnemy`, `PushLane`, `ClearJungle`, `TakeObjective`
- Action 라이브러리: `MoveTo`, `CastAbility`, `BuyItem`, `Recall`, `Ward` — 각각 precondition/effect/cost
- 동적 재계획: 월드 변경 시 (적 갱, 팀원 죽음) 플랜 재구성

#### Stage 4: Utility AI (점수 기반)

같은 상황에서 여러 행동 후보의 **유틸 점수** 계산 → 최고점 선택.

- 예: `Should Gank Bot?` 점수 =
  `(아군 CC 가능성) × (적 체력 낮음) × (시야 없음) × (1/거리) - (내 갱킹 후 손실)`
- Response Curve (Linear / Quadratic / Sigmoid) 로 입력 정규화
- Dave Mark "Behavioral Mathematics" 방법론

#### Stage 5: Influence Map + Threat/Opportunity Map

맵을 그리드 (또는 nav cell) 로 분할해 여러 레이어 점수 관리.

- **Team Influence Map**: 각 그리드 셀에 아군/적군 영향력 (거리 기반 감쇠 합산)
- **Threat Map**: 적 시야 + 사거리 + CC + 소환사 주문 쿨다운 고려
- **Opportunity Map**: 드래곤/바론/전령 리스폰 타이머 + 미니언 웨이브 클리어 가능성
- **Vision Map**: 와드/시야 차단물 반영 (전쟁의 안개 시뮬)
- 각 맵은 GPU Compute 또는 CPU 지연 업데이트 (100ms 간격)
- 봇 의사결정 입력: "Threat 낮고 Opportunity 높은 곳으로 이동"

#### Stage 6: Monte Carlo Tree Search (MCTS)

단기 전투 상황 시뮬레이션 — 교전 1:1 / 2:2 의 몇 수 앞 예측.

- 노드: 현재 월드 스냅샷
- 액션: 스킬/평타/이동 조합
- Rollout: 간이 시뮬레이터로 3~5초 미래 예측
- UCB1 선택 + 백프로파게이션
- Phase E 의 몬테카를로 적분과 수학 공유 (난수·확률·분산 감소)

#### Stage 7: 모방 학습 (Imitation Learning)

실제 플레이어 게임 로그 → 행동 분포 학습.

- 서버가 매칭에서 플레이어 입력/상태 로그 수집
- 특징 추출: 상태 벡터 → 액션 벡터 (CSV/FlatBuffers 저장)
- Behavior Cloning: 간이 MLP 또는 결정 트리 학습 (C++ 자체 구현 or ONNX Runtime)
- 점진적 난이도: 실수 로그까지 포함 → 인간다움

#### Stage 8: 강화학습 (선택 — RL)

Self-Play 환경에서 점진 강화. 연구 수준 도전 과제.

- PPO (Proximal Policy Optimization) — OpenAI Five 방식
- Reward: 킬 + 오브젝트 + 포탑 파괴 - 사망 - 시간 가중치
- 환경 추상화: `IBotEnv` — state, step, reset
- 학습은 파이썬 (PyTorch) 별도 프로세스, Inference 는 C++/ONNX 런타임
- 디플로이: Docker 컨테이너에서 병렬 Self-Play → 학습된 모델 `.onnx` 로 엔진에 로드

#### 팀 협동 (Blackboard)

봇팀원끼리 공유 메모리로 조율. 사람 플레이어와 팀인 경우 **핑 시스템만** 사용 (부정행위 방지).

- `Blackboard` 키: `CurrentObjective`, `GroupLeader`, `FlaggedEnemy`, `SafeRetreatPos`, `WardPriorities`
- 5명 봇이 같은 Blackboard 보고 역할 분담 (탑솔/정글/미드/원딜/서폿 자동 assign)

#### 난이도 프리셋

| 난이도 | APM 상한 | 반응 시간 | 실수 주입 | MCTS 깊이 | 사용 Stage |
|---|---|---|---|---|---|
| Intro | 60 | 400ms | 높음 | off | 0~2 |
| Beginner | 90 | 300ms | 중간 | off | 0~3 |
| Intermediate | 150 | 200ms | 낮음 | 3수 | 0~5 |
| **Master** | **250** | **100ms** | **미세** | **5수** | **0~7 전부** |
| Grandmaster (RL 전용) | 400 | 50ms | 없음 | 7수 | 0~8 |

#### 디버깅 / 에디터 통합

- **ImGui 봇 인스펙터**: 각 봇의 HFSM 현재 상태, BT 트리 순회 노드, GOAP 현재 플랜, Utility 점수 표 실시간
- **DebugDraw** (Phase C-2): Influence Map 히트맵 오버레이, 목표 라인, MCTS 탐색 트리
- **연습모드 통합**: 에디터에서 봇 추가/제거/난이도 변경/상태 강제 전이 가능
- **Replay 시스템**: 봇 의사결정 로그 파일로 저장 → 나중에 재생해서 버그 추적

#### Engine 통합 지점

```
Engine/Public/AI/
├── FSM/               HFSM (Hierarchical Finite State Machine)
├── BehaviorTree/      BT 인터프리터, 노드 라이브러리 (Selector/Sequence/Parallel/Decorator)
├── GOAP/              WorldState, Action, Goal, A* Planner
├── Utility/           Utility Score 계산, Response Curves
├── Blackboard/        팀 공유 메모리
├── InfluenceMap/      Team/Threat/Opportunity/Vision 레이어
├── Pathfinding/       A* + JPS (Navigation 과 공유)
├── MCTS/              Monte Carlo Tree Search (교전 시뮬)
├── Imitation/         Behavior Cloning 런타임 (ONNX)
├── RL/                PPO 추론 런타임 (ONNX) — 선택
└── Bots/              챔피언별 봇 프리셋 (Irelia/Yasuo/Sylas/Viego/Kalista 등)
```

ECS 컴포넌트: `BotComponent { difficulty, role, blackboardID }`, `DecisionComponent { currentGoal,
plan, utilityScores }`. `BotSystem` 이 FSM/BT/GOAP 계층을 매 틱 실행.

---

## Gotchas

- **vcxproj `/utf-8` 필수**: 한글 주석이 CP949 로 해석되면 C4819 + 가짜 파서 오류 (C1075 '{') 발생. 새 프로젝트 생성 시 `<AdditionalOptions>` 에 `/utf-8` 반드시 포함.
- **`.bat` 파일은 ASCII 전용**: cmd.exe 는 `.bat` 를 시스템 ANSI (한국어 Windows = CP949) 로 읽음. UTF-8 저장 시 한글 주석 바이트가 예약 문자를 만나면 **cmd 파싱이 중간에 실패해 for 루프/xcopy 가 부분적으로 죽음** (EngineSDK 동기화 실패 전형적 원인). 모든 `.bat` 주석은 영어만. `chcp 65001` 도 파일 첫 줄 이전에는 적용 안 됨.
- **Windows 절대 경로 문자열 이스케이프 금지** (2026-04-24 추가): `L"C:\Users\user\Desktop\..."` 같은 백슬래시 절대 경로는 C++ 이스케이프 시퀀스 지옥:
  - `\U` = 8자리 Unicode 이스케이프 (`\Users` → `\U` + `sers` 가 16진 해석 시도, 비16진 만나면 경고 혹은 경로 깨짐)
  - `\u` = 4자리 Unicode 이스케이프 (`\user` → `\u` + `ser...` 동일 문제)
  - `\W`, `\D`, `\B`, `\b`, `\t`, `\n` 등 — 인식 안 된 이스케이프는 MSVC 가 `\` 를 버리거나 유효 이스케이프 (`\b` = backspace) 로 치환
  - MSVC 는 보통 경고만 주고 빌드 통과 → **런타임 경로가 조용히 깨짐** (파일 로드 실패)
  - **해결**: ① `L"C:\\Users\\user\\..."` 이중 백슬래시, ② `L"C:/Users/user/..."` 슬래시, ③ **가장 권장: 상대 경로** `L"../Bin/Resource/..."` (PC 이동/다른 사용자 실행 시 안정). 실제 사고: UI_Manager HP 바 PNG 로드 실패 (`single_bar_blue.png`, 2026-04-24)
- **`WINTERS_PROFILE_SCOPE` 매크로는 같은 스코프 2개 허용** (2026-04-24 추가): 내부에서 2단계 CONCAT (`WINTERS_PROFILE_CAT_INNER(a,b) a##b` + `WINTERS_PROFILE_CAT` 래퍼) 로 `__LINE__` 지연 확장해 지역변수 이름 유일화. 단일 `##__LINE__` 은 토큰 리터럴로 고정되므로 같은 함수에 2개 이상 넣으면 중복 선언 에러. 신규 RAII 매크로 작성 시 반드시 2단계 CONCAT 패턴 적용.
- **ProfilerAPI.h 를 Engine .cpp 에서 include 누락 = DLL export 속성 누락**: `Winters_Profile_Push/Pop/Counter` 는 `WINTERS_ENGINE` (dllexport/dllimport) 로 선언. Engine 내부 TU (`CPUProfiler.cpp`) 가 ProfilerAPI.h 를 include 안 하면 함수 정의에 export 속성이 안 붙어 Engine.dll 이 심볼을 export 하지 않음 → Client LNK2019 (`__imp_Winters_Profile_Counter` unresolved). WINTERS_ENGINE 마크된 free function 을 Engine 내부에서 정의할 때 해당 헤더 include 필수.
- **절대 경로 하드코딩 지양**: PC 이동/빌드 서버/팀원 환경에서 전부 깨짐. Windows 폼은 상대 경로 (`../Bin/Resource/...`) 또는 실행 디렉토리 기준 resolver 사용. 2026-04-24 UI HP 바 로드 실패 + 이전 세션 Viego FBX `Character/` 누락은 경로 취급 일관성 부재가 원인.
- **Docker PostgreSQL 포트 충돌**: Windows 에 PostgreSQL 이 설치되어 있으면 5432 선점. Docker `ports` 를 `5433:5432` 로, `.env` 의 `DB_PORT=5433` 으로 맞출 것. 현재 프로젝트는 5433 사용 중.
- **Docker Kafka 이미지 환경변수**: `apache/kafka:3.7.0` 은 `KAFKA_CFG_` 가 아닌 `KAFKA_` 접두사. KRaft 모드에서 `KAFKA_NODE_ID`, `KAFKA_PROCESS_ROLES`, `KAFKA_CONTROLLER_QUORUM_VOTERS` 등으로 설정.
- **Go 서비스 `package main` 위치**: `internal/{service}/main.go` 에 `package main` 을 넣으면 같은 디렉토리의 `package {service}` 파일과 충돌. 반드시 `cmd/{service}/main.go` 에 분리.
- **Go `godotenv` + `getEnv` fallback**: `.env` 의 `KEY=` (빈값) 이어도 `getEnv(key, fallback)` 은 빈 문자열을 무시하고 fallback 반환. 빈값 의도하려면 `os.Getenv` 직접 사용.
- **Windows localhost IPv6**: Go `net.Dial("tcp", "localhost:5432")` 는 IPv6 `[::1]` 우선 시도. Docker 컨테이너가 IPv4만 리스닝하면 연결 실패. `.env` 의 `DB_HOST=127.0.0.1` 명시 권장.
- **HLSL row_major 필수**: DirectXMath 는 row-major 행렬 생성하지만 HLSL `matrix`/`float4x4` 는 기본 column-major 해석. cbuffer 행렬에 반드시 `row_major` 키워드. 누락 시 카메라 이동 따른 비대칭 클리핑/왜곡 (View 행렬 전치로 w 에 카메라 위치 의존 항). 예: `row_major matrix g_matViewProj;`
- **Assimp aiMatrix4x4 → XMFLOAT4X4 전치 필수**: Assimp 는 column-vector 관례 (Translation=4열), DirectX 는 row-vector 관례 (Translation=4행). `ConvertMatrix()` 에서 반드시 전치. 안 하면 본 오프셋/Rest Pose/GlobalInverseRoot 전부 틀려 스키닝 메시 폭발.
- **스켈레톤 모델 stride 혼재 금지**: 모델에 스켈레톤이 있으면 본 없는 서브메시도 VTXANIM (76바이트) 으로 생성. VTXMESH (44바이트) 와 섞이면 skinned InputLayout 이 44바이트 정점을 잘못 읽어 폭발. 본 없는 메시는 `weight[0]=1.0, index[0]=0`.
- **`.wmesh` skinned 정점 레이아웃 = 런타임 Skinned3D IL byte offset 매칭 필수 (B-9, 2026-04-28 추가, 중요!)**: `WMeshFormat::VertexSkinned` POD 의 필드 순서/크기를 셰이더 IL `D3D11_INPUT_ELEMENT_DESC` 의 `AlignedByteOffset` 과 **byte 단위로** 일치시킬 것. 확정 layout (76B, 한 줄도 변경 X):
  - `pos[3]` `0~11` (POSITION)
  - `nrm[3]` `12~23` (NORMAL)
  - `uv[2]` `24~31` (TEXCOORD)
  - `tan[3]` `32~43` (TANGENT, **float3** — handedness 없음, PBR 진입 시 별도 채널 추가)
  - `indices[4]` `44~59` (BLENDINDICES, **uint32×4** — uint8 packed 아님, bone>=256 자동 안전)
  - `weights[4]` `60~75` (BLENDWEIGHT, float×4)
  - 합 76B, `static_assert(sizeof(VertexSkinned) == 76)`
  WMeshWriter 의 정점 write 순서도 동일 offset 으로. 임의 추가 필드 (예: tangent.w handedness, reserved 패딩) 넣으면 GPU 가 다음 필드를 한 byte 씩 밀어 잘못 읽음 → 정점이 NaN/0 으로 collapse, 메시 화면에서 **소리 없이 사라짐** (애니/Transform 정상 로그 = 진단 어렵). 실제 사고 (2026-04-28): 가렌 `.wmesh` writer 가 `tangent float4 (16B) → weights → packed uint8 indices` 순서로 write → 런타임 IL 의 BLENDINDICES (offset 44) 가 weights 한가운데 float 를 uint 로 해석 → 본 인덱스 garbage → vertex skinning 폭발. 진단: `[CModel] .wmesh+.wskel fast-path` 로그 정상인데 화면 안 보임 = byte offset 미스매치 1순위 의심. **신규 vertex 포맷 추가 시 IL 부터 read off 후 POD 설계**.
- **Assimp 스키닝 표준 공식**: `Final = Offset × GlobalTransform × GlobalInverseRoot` (DX row-major). GlobalInverseRoot 빠지면 루트 노드 트랜스폼만큼 전체 메시 틀어짐. 채널 없는 본은 Identity 가 아닌 Rest Pose (aiNode::mTransformation) 초기화.
- **Engine 헤더 수정 후 EngineSDK 동기화 필수**: `Engine/Public/` 헤더 수정 후 `EngineSDK/inc/` 에 복사 안 하면 Client 에서 `'함수명': 멤버가 아닙니다` 에러. Post-Build Event 자동화 권장.
- **FBX 머티리얼 텍스처 경로 없을 수 있음 (LoL 추출)**: LoL FBX 는 머티리얼 Diffuse 경로가 없어 `LoadTextures()` 가 아무것도 로드 안 함. `SetOverrideTexture()` 대신 `LoadMeshTexture(meshIndex, path)` 로 서브메시별 수동 지정. 메시 이름 동일 (`Mesh_0`) 인 경우 정점 수 + 시각 확인으로 바디/칼날 구분.
- **Blender FBX Export 는 glTF PBR 텍스처 경로 기록 불가**: glTF/glb → Blender Import → FBX Export 시 FBX 에 텍스처 참조 0개. 해결: **glb 를 Assimp 으로 직접 로드** (`CTexture::CreateFromMemory()` + `pScene->GetEmbeddedTexture()`).
- **맵 glb Z-Fighting**: LoL mapgeo → glb 변환 시 Layer1~8 노드가 기본 지형과 동일 평면에 겹침. Near/Far 조정/DepthBias 전부 무효 (코플라나). **해결: `ProcessNode()` 에서 `"Layer"` 이름 노드 스킵** — Z-fighting 제거 + 메시 37% 감소. 상세: `.md/guide/LOL_MAP_EXTRACT_GUIDE.md`
- **lol2gltf 1.0.0 `--flipX` 옵션 무력화 (중요!)**: `lol2gltf.exe mapgeo2gltf` 의 `-x, --flipX` 가 `(Default: true)` 로 X 축 자동 반전하는데, **`-x false` / `--flipX false` / `--flipX=false` / `-x:false` 4가지 형식 모두 무시됨** (CLI parser 버그, md5 비교로 확정). 도구 단에서 X flip 끌 방법 없음 → 항상 좌우 반전된 glb 생성. **해결: 코드에서 맵 transform 에 `SetScale({-0.01f, 0.01f, 0.01f})` X 미러 우회.** Mesh3D 파이프라인이 `D3D11_CULL_NONE` ([Engine/Private/RHI/DX11/DX11Pipeline.cpp](Engine/Private/RHI/DX11/DX11Pipeline.cpp) `CreateMesh`) 라 winding 뒤집혀도 면 안 사라짐. 챔피언 fbx 는 lol2gltf 안 거치니 영향 0. 상세: `.md/guide/LOL_MAP_EXTRACT_GUIDE.md` Gotcha #8.
- **변환 도구 명령 후 결과 검증 필수 (중요!)**: lol2gltf 처럼 옵션이 silent 하게 무시되는 경우가 있음. 변환 명령 실행 후 **즉시 md5 / 정점 분포 등으로 옵션 효과 검증** 후 다음 단계로. 검증 없이 진행하면 "도구 옵션 잘못 사용" vs "도구 버그" 구분 못 해 시간 낭비. lol2gltf X-flip 사고가 그 예.
- **공개 헤더는 `std::` 명시 필수 (B-6.6, 중요)**: Engine/Public·Engine/Include 의 `.h` 는 `using namespace std;` 를 가정하지 말 것. Entity.h / ComponentStore.h / CCommandBuffer.h 가 unqualified `vector`/`function` 쓰다가 Client TU (SkillTable.cpp 등) 에서 파싱 실패. 공개 헤더는 `std::` prefix 명시, 내부 `.cpp` 만 `using namespace std` 허용.
- **namespace Engine 밖 공개 헤더는 `bool_t`/`i32_t` 등 `_t` alias 사용 금지 (B-6.6)**: `bool_t` 는 `Engine_Typedef.h` 의 `namespace Engine` 안에만 존재. `ModelRenderer.h` 처럼 namespace 밖 공개 API 는 `bool`/`int32_t` 또는 `WintersTypes.h` 의 `f32_t`/`f64_t` (global alias) 사용.
- **EngineSDK/inc 는 flat 구조 (B-6.6, 중요)**: UpdateLib.bat 이 모든 `.h` 를 한 폴더로 복사하므로 공개 헤더 안의 `#include "Sound/SoundChannel.h"` 같은 서브경로 include 는 Client TU 에서 실패. **공개 헤더는 flat include (`#include "SoundChannel.h"`) 사용**, Engine.vcxproj 의 AdditionalIncludeDirectories 에 필요한 Public 서브폴더 (`Sound/`, `ECS/`, `ECS/Components/`, `ECS/Systems/`) 를 추가해 Engine 내부 TU 도 flat 해소 가능하게 유지.
- **`WintersResolveContentPath` 는 파일 전용 (B-6.6)**: 내부 `FileExistsFile` 이 디렉토리를 거부 (`(a & FILE_ATTRIBUTE_DIRECTORY) == 0`). 폴더 경로를 해석해야 하는 경우 (사운드 재귀 로드 등) 는 `GetModuleFileNameW(nullptr, ...)` 로 exe dir 을 직접 구성하고 `FindFirstFileW` 로 순회.
- **Client PreBuildEvent 상대경로 주의 (B-6.6)**: `Client/Include/Client.vcxproj` 의 PreBuildEvent 에서 `../../../UpdateLib.bat` 는 데스크톱을 가리키는 실수. `$(SolutionDir)UpdateLib.bat` 로 통일.
- **vcpkg 탈피 시 vcxproj 설정 필수 (B-6.6)**: `<VcpkgTriplet>` 삭제만으로는 부족. `<VcpkgEnabled>false</VcpkgEnabled>` + `<VcpkgEnableManifest>false</VcpkgEnableManifest>` 명시해 MSBuild/IDE 가 vcpkg 통합 시도하지 않도록 박아둘 것. 새 서드파티 편입 시 `.md/build/THIRDPARTY_INTEGRATION_GUIDE.md` 절차 따를 것.
- **Resource 리소스는 OutDir 동기화 필수 (B-6.6)**: `Client/Bin/Resource/` 원본이 `Client/Bin/Debug|Release/Resource/` 로 복사되지 않으면 `WintersResolveContentPath` 가 exe 옆에서 못 찾음. Client PostBuild 에 `xcopy /Y /E /I /D "$(SolutionDir)Client\Bin\Resource\*.*" "$(OutDir)Resource\"` 필요. Shaders 와 동일 패턴.
- **LoL FX FBX 의 `render/*.png` 는 mesh diffuse 가 아니다 (Phase 1, 중요!)**: LoL 추출 시 `fbx/` 옆에 함께 나오는 `render/irelia_base_e_*.png` 같은 파일은 **LoL 클라이언트가 이펙트를 카메라로 렌더한 스프라이트 캡처** 이지 mesh 머티리얼 텍스처가 아니다. FBX 의 UV 좌표가 그 스프라이트의 **알파 0 영역만 가리켜** `Mesh3D.hlsl` 의 `clip(texColor.a - 0.05f)` 가 모든 픽셀을 버림 → DrawMesh / CModel::Render 는 정상 호출되는데 화면 출력 0 픽셀 = "스폰 안 됨" 처럼 보임. CPU 측 디버거 (BP/Output) 로 못 잡고 픽셀 파이프라인 안에서 죽는 패턴. **해결: LoL 본체 추출 시 함께 나오는 진짜 머티리얼 텍스처 (`irelia_base_blades_passive_4_texture.png` / `irelia_base_e_beam_mult.png` 같은 형태) 를 `SetMeshTexture` 로 바인딩.** `render/*.png` 는 빌보드 (FxBillboardComponent, UV 전체 0~1) 전용. Yasuo / Kalista / Yone 등 모든 챔프 FX FBX 도입 시 동일 패턴 재발 예상 — 텍스처 검증 절차 우선.
- **셰이더 (`Shaders/*.hlsl`) 수정 후 OutDir 강제 동기화 필수 (Phase FX-8, 중요!)**: `D3DCompileFromFile` 가 OutDir 복사본 (`Client/Bin/Debug/Shaders/`) 을 읽는데 **MSBuild incremental 은 `.hlsl` 변경을 감지 못 해 PostBuild xcopy 가 skip**. 셰이더 원본 수정 + 빌드 통과해도 OutDir 의 옛 .hlsl (이전 오타 포함) 그대로라 런타임 `D3DCompileFromFile` 실패 → `__debugbreak()` 발동. **신호**: Output 창의 셰이더 컴파일 에러 메시지에 `Bin/Debug/Shaders/` 경로가 찍히면 OutDir 복사본 의심. **해결**: 셰이더 수정 후 ① `cp Shaders/*.hlsl Client/Bin/Debug/Shaders/` 직접 동기화, ② Rebuild Solution, ③ Client.vcxproj PostBuild 의 xcopy 에 `/Y` 강제 옵션 (currently `/D` 라 timestamp 비교만), 셋 중 하나. 셰이더는 `.cpp` 와 달리 컴파일 단위가 아니라 빌드 dependency 무시. CLAUDE.md `winters-skills/debug-pipeline/SKILL.md` 의 "셰이더 우선 Read" 단계에 **"Output 경로가 OutDir 면 동기화 누락 의심"** 추가.
- **UpdateLib.bat 의 `_Manager.h` 제외 로직은 실제로 작동 안 함**: `findstr /I /E "_Manager.h"` 가 `for` 블록 내부 `errorlevel` 평가 시 의도와 다르게 동작하여 Timer_Manager.h / Scene_Manager.h / Sound_Manager.h 가 EngineSDK 에 배포됨. 현재 Client 가 이 헤더를 include 하지 않아 실해는 없음. 후일 경계 강제 필요 시 전체 재구조 (서브폴더 배포) 고려. **우선순위 낮음**.
- **F12 는 Visual Studio "Break into Debugger" 단축키 (B-6.7, 중요)**: 게임 내 키 바인딩으로 쓰면 디버깅 중 F12 누를 때마다 ntdll 에서 `__debugbreak` 걸리고 내 코드에 없는 콜스택 표시됨 → **힙 손상으로 오진 가능**. 에디터 진입 등 게임 입력은 **M/Tab/F1~F11** 중 선택. VK_F12 안 쓰기 권장.
- **Scene_Manager::Change_Scene 은 즉시 self-destruct (B-6.7, 중요)**: `Change_Scene(std::move(newScene))` 호출 시 내부 `m_pCurrentScene = std::move(pScene)` 실행되며 **이전 Scene unique_ptr 가 그 자리에서 소멸**. Scene 메서드 (OnUpdate/OnImGui) 안에서 `Change_Scene` 호출 후 코드 계속 실행하면 **이미 소멸된 this 에 대한 use-after-free** → ntdll 에서 break (콜스택이 Windows 내부로 사라짐). `Change_Scene` 호출 직후 **반드시 `return`**.
- **SkillDef.lockDurationSec ↔ FBX 애니 실측 길이 일치 (T-4, 중요!)**: `Scene_InGame::ApplyLocalPrediction` 이 `m_fLastActionTimer = def.lockDurationSec` 로 세팅 → 타이머 0 도달 시 L700 의 자동 교체 로직이 idle/run 으로 **강제 전환**. `lockDurationSec` 가 FBX 원본 애니 길이보다 **짧으면** 재생 완료 전에 idle 로 커트되어 스킬 애니가 중간에 잘린다. 증상은 AS=1.0 정속에서만 발현 — AS 를 올리면 애니가 lock 안에 우연히 완주해 **디버그 우회**되므로 놓치기 쉽다. 해결: `SkillTable.cpp` 의 각 스킬 `lockDurationSec` 를 FBX 원본 길이(초) 와 정확히 매칭. AS 연동은 `m_fLastActionTimer = lockDurationSec / m_fAttackSpeedMul` 역비례로 처리. 이렐리아 실측 레퍼런스: 평타=1.0, Q=0.5, W1=4.0, E=1.0, R=0.3. 확인법: ChampionTuner "Debug State" 의 `Remaining Lock` 초기값 == 해당 스킬 애니 한 번 재생 시간. 애니별 배속 고정이 필요하면 `SkillDef.animPlaySpeed` (T-4 추가) 필드로 개별 조정 — 전체 길이(`lockDurationSec`)와 독립.
- **enum class 이름 충돌 across namespace (B-6.7)**: `Winters::Map::eTeam` (u32_t) 과 `Engine::eTeam` (uint8_t, `GameplayComponents.h`) 처럼 동일 enum 이름이 다른 네임스페이스에 공존하면 `using namespace Winters::Map;` 만으로 해결 안 됨. **`Winters::Map::eTeam::Blue` fully qualify 필수**. 공개 헤더가 `GameplayComponents.h` 를 include 하는 경로에서 발생.
- **바이너리 POD 포맷 변경 시 VERSION bump + 기존 .dat 삭제 (B-6.7)**: `StructureEntry` 같은 고정 레이아웃 POD 에 필드 추가 시 `sizeof` 가 바뀌어 이전 포맷 파일이 호환 안 됨. **`STAGE_VERSION` 증가 + `static_assert(sizeof(...) == 기대값)` 업데이트 + 기존 `Data/StageN.dat` 삭제** 필수. 버전 불일치 파일을 읽으면 count 필드가 쓰레기값으로 해석되어 거대 할당 시도 → 힙 손상 가능.
- **`imgui.h` include 시 `new` 매크로 가드 필수 (_DEBUG 빌드, 중요!)**: [Engine_Defines.h:41](Engine/Public/Engine_Defines.h:41) 의 `#define new DBG_NEW` (`_CRTDBG_MAP_ALLOC` 메모리 누수 추적) 가 imgui.h 의 `void* operator new(size_t, ImGuiContext*)` placement new 선언과 치환 충돌 → **`'operator new': 재정의`** 컴파일 에러. 해결: imgui.h include 를 `#ifdef _DEBUG / #undef new / #endif` 와 `#ifdef _DEBUG / #define new DBG_NEW / #endif` 로 감싸기. [Scene_MatchLoading.cpp](Client/Private/Scene/Scene_MatchLoading.cpp) 참조. 추후 ImGui 쓰는 cpp 가 늘어나면 `WintersImGui.h` 같은 공통 래퍼 헤더로 중복 제거 고려.
- **`DECLARE_SINGLETON` 매크로 뒤 private ctor 명시 (B-6.7)**: 매크로가 `NO_COPY` + `public: Get() { static CXxx instance; }` + `private:` 순서로 전개되므로, 매크로 바로 다음 줄에 `CXxx() = default;` 를 **private 로 명시**해야 Meyers 싱글턴의 `static CXxx instance;` 가 컴파일. 이후 `public:` 로 바꾸고 API 선언. Winters `Engine_Macro.h` L57 참조.
- **`CInput` 은 키보드 에지만 제공, 마우스 LButton 없음 (B-6.7)**: `m_Keys[256]` 배열이 키보드만 WndProc 에서 기록됨. `IsKeyPressed(VK_LBUTTON)` 은 **영원히 false**. 마우스 좌클릭 에지가 필요하면 **`GetAsyncKeyState(VK_LBUTTON) & 0x8000`** 으로 직접 감지 + 멤버 bool 로 이전 상태 보관 (Scene_Editor `Handle_MousePlacement` 참조).
- **EngineSDK/inc 는 절대 직접 수정 금지 (CRITICAL — 반복 사례)**: UpdateLib.bat 이 `xcopy /Y /D` 로 **Engine → SDK 단방향 복사**. SDK 만 수정하면 "지금은" 보호되지만 누군가 Engine 원본을 touch 하는 순간 덮어씌워서 **수정 유실**. **항상 `Engine/Include` + `Engine/Public` 원본을 수정** 후 UpdateLib.bat 실행. Visual Studio 가 F12 점프 시 Client TU 에서는 `EngineSDK/inc` 로 점프하므로 **탭 제목 + Solution Explorer 로 실제 경로 확인 후 편집**. 검증 루틴: `for sdk in EngineSDK/inc/*.h; do name=$(basename "$sdk"); src=$(find Engine/Include Engine/Public -name "$name" | head -1); [ -n "$src" ] && cmp -s "$src" "$sdk" || echo "DIFF: $name"; done`
- **enum / struct 이름 변경 시 연쇄 참조 전수 검색 필수**: `CHAMPION_END` → `END` 같은 enum 이름 변경 후 **반드시 `grep -r "CHAMPION_END"` 로 소스 전체 전수 업데이트**. GameContext.h 만 고치고 GameplayComponents.h (L55) 참조 놓치면 **이 헤더를 include 하는 모든 Engine TU 컴파일 실패**. 리네임은 IDE Find&Replace in Files 사용 후 빌드 테스트 필수.
- **Engine/Include 공개 헤더는 flat include 만 (L795 재강조)**: `#include "ECS/Systems/EntityBlueprint.h"` 같은 서브경로는 **SDK flat 복사 후 Client TU 에서 해석 실패**. 공개 헤더는 항상 `#include "EntityBlueprint.h"` (파일명만) 으로 작성하고, Engine.vcxproj AdditionalIncludeDirectories 에 `Engine/Public/ECS/Systems` 등 서브폴더 추가해 Engine 내부 TU 도 flat 해소 가능하게. 공개 헤더가 내부 TU 때문에 서브경로 쓰면 SDK 복사 후 깨짐.
- **`WINTERS_ENGINE` dllexport 클래스 + `unique_ptr` 멤버 = copy ctor/assign 명시 `= delete` 필수 (Phase 5-A, 중요!)**: MSVC `__declspec(dllexport)` 는 클래스의 **모든 특수 멤버 함수** (copy/move ctor, copy/move assign) 를 강제 인스턴스화/export 하려 함. `unique_ptr<T>` 또는 `vector<unique_ptr<T>>` 또는 `map<K, vector<unique_ptr<T>>>` 멤버가 있으면 암묵적 copy ctor 는 deleted 가 되지만 **MSVC 는 먼저 인스턴스화 시도 후 `construct_at` SFINAE 로 실패** → `C2672 'std::construct_at': 일치하는 오버로드된 함수가 없습니다` + `unique_ptr::unique_ptr(const unique_ptr&): 삭제된 함수` 에러 체인. **해결: 특수 멤버 5개 명시 선언**:
  ```cpp
  class WINTERS_ENGINE CFoo {
  public:
      CFoo() = default;
      ~CFoo() = default;
      CFoo(const CFoo&) = delete;              // ← 필수
      CFoo& operator=(const CFoo&) = delete;   // ← 필수
      CFoo(CFoo&&) = default;
      CFoo& operator=(CFoo&&) = default;
  private:
      std::unique_ptr<Bar> m_pBar;   // 또는 map<K, vector<unique_ptr>> 등
  };
  ```
  선례: [CWorld.h:59~62](Engine/Public/ECS/World.h:59), [CSystemSchedular.h:14~22](Engine/Public/ECS/SystemScheduler.h:14). 증상이 `m_mapPhases[phase].push_back(move(system))` 같은 전혀 무관해 보이는 push_back 줄에서 나오므로 오진하기 쉬움 — **에러 체인 최상단의 "vector::vector(const vector&)" 컴파일 중 문구**를 보면 dllexport copy 인스턴스화가 트리거임을 알 수 있음.
- **wide string literal 의 `\U`/`\u`/`\W`/`\B`/`\R`/`\T` 이스케이프 충돌 (T-3, 중요!)**: `L"C:\Users\user\..."` 에서 `\U` 는 C++ universal-character-name 규칙 (8 hex digits 요구) → C4129/C4566 경고 + 엉뚱한 문자열로 컴파일. `\W`/`\B`/`\R`/`\T` 도 일부 escape 처리. 증상: PNG 로드 실패 (`CreateWICTextureFromFile` 의 경로 해석 실패). 해결: **forward slash** (`L"Client/Bin/Resource/Texture/UI/..."`) 또는 raw literal (`LR"(C:\Users\...)"`). Windows 경로는 모든 API 가 `/` 를 허용. CWD 는 vcxproj `LocalDebuggerWorkingDirectory=$(SolutionDir)` 이므로 `Client/Bin/Resource/...` 상대 경로가 올바른 기준. 이 버그로 UI_Manager.cpp L31 / Scene_InGame.cpp L266 두 곳에서 이미 당함.
- **PlayAnimationByName 매칭 실패는 조용히 무시 (T-4, 중요!)**: [Model.cpp:119-128](Engine/Private/Resource/Model.cpp:119) `FindAnimationIndex` 가 substring 검색 실패 시 -1 반환 → [ModelRenderer.cpp:244](Engine/Private/Renderer/ModelRenderer.cpp:244) `PlayAnimationByName` 이 조용히 early-return, **기존 애니 유지**. 증상: "락 타이머는 정상인데 스킬 애니만 idle/run 으로 유지" (유저는 스킬 버튼 눌러도 변화 없음 체감). LoL FBX 는 원작 애니 이름 그대로 (`spell1/spell2/spell3/spell4` 혹은 `spell2a/b/c`) 라서 개발자 임의 키 (`spell2_1` 등) 는 매칭 실패. 진단: `Model.cpp:233-238` 로드 시점 OutputDebugString 의 "animations=N" 이후 이름 덤프 확인. 방지: ModelRenderer.cpp L244 에 `if (idx < 0) { OutputDebugStringA(("[ModelRenderer] animation NOT FOUND: " + strKeyword + "\n").c_str()); return; }` 1줄 추가 권장. 이렐리아 W 의 `spell2_1` → `spell2` 매칭 사고 (2026-04-24) 선례.
- **봇 챔프도 OnEnter 에서 idle 애니 명시 기동 필수 (T-7, 중요!)**: CAnimator 는 PlayAnimation 호출 전엔 `m_bPlaying=false` 로 Update early-return → 본 매트릭스 identity → **bind pose 렌더**. Kalista/Viego 등 손에 무기 submesh 가진 챔프는 bind pose 에서 무기가 몸체 밖 손 본 로컬 좌표 `(-85, 128, -122)` 에 떠 있어 "안 보이는 것처럼" 보임 (0.01 스케일 시 월드 `(-0.85, 1.28, -1.22)` 몸 뒤쪽·왼쪽·아래). 플레이어 챔프는 Scene_InGame L215 에서 `m_pPlayerRenderer->PlayAnimationByName(m_pPlayerIdleAnim)` 호출로 자동 기동되지만 봇은 분기 자체 없음. 해결: OnEnter 끝에 **봇 전체** `m_Kalista.PlayAnimationByName("kalista_idle1")` / `m_Viego.PlayAnimationByName("viego_idle1")` 등 명시 호출. Yone 계획서 `.md/plan/Champion/01_MULTI_MATERIAL_CHAMPION_YONE.md` §6.5-2 상세.
- **SkillTimingPanel const_cast 는 `.rdata` 쓰기 크래시 (T-4, 중요!)**: `static const SkillDef s_SkillTable[]` 는 MSVC 에서 `.rdata` (read-only) 섹션에 배치됨. `const_cast<SkillDef&>(g_SkillTable[i])` 로 const 를 벗겨도 실제 메모리가 readonly 라 **write access violation**. 증상: ImGui 슬라이더 드래그 순간 크래시. 해결: SkillTable.cpp 소스 배열에서 `const` 제거 (`static SkillDef s_SkillTable[]`). 외부 노출 포인터 `g_SkillTable` 은 여전히 `const SkillDef*` 로 const-correctness 유지. const_cast 는 포인터 타입 const 벗기기 용으로만 유효.
- **스킬 애니 연계는 SkillDef.endTransition 3필드 (T-5)**: 락 해제 시점에 Scene_InGame 이 `m_bMoving` 기준으로 분기해 전환 애니 재생 후 idle/run 스냅. 필드: `endTransitionIdleAnim` (정지 시) / `endTransitionRunAnim` (이동 시) / `endTransitionDuration` (전환 재생 시간). 원작 LoL 의 `spell4_to_idle` / `spell4_to_run` 같은 공중→지면 자연 전환 재현. 핵심: `m_pActiveSkillDef` 는 `recoveryFrame` 지나면 nullptr 되므로 **별도 `m_pLastDispatchedSkill` 포인터 유지** (락 해제 시점에 endTransition 조회용). PreemptAction 호출 시 전환 타이머 (`m_fEndTransitionTimer`) 도 리셋 필수. 이렐리아 적용: R `spell4_to_idle/to_run` + Q `spell1_to_idle/into_runbase` + E `spell3_to_idle/run`.
- **`SkillDef.lockDurationSec = FBX 실측 / AS` 부등식 (T-4, 중요!)**: 애니가 끝까지 재생되려면 `애니 원본 길이 ≤ lockDurationSec × animPlaySpeed`. AS=1.0 기준 FBX 원본 애니 길이를 정확히 측정해 SkillTable 하드코딩. 런타임에 `m_fLastActionTimer = lockDurationSec / m_fAttackSpeedMul` 로 AS 역비례 스케일. 위배 시: 애니 재생 완료 전에 L700 자동 교체가 idle/run 으로 강제 전환 → 스킬 애니 중간 커트. AS=1.0 에서만 발현하고 AS 올리면 우연히 숨겨지므로 놓치기 쉬움. 확인법: ChampionTuner "Debug State" 의 `Remaining Lock` 초기값 == 해당 스킬 애니 한 번 재생 시간. 이렐리아 실측 레퍼런스: 평타=1.0 / Q=0.5 / W1=4.0 / E=1.0 / R=5.0. 애니별 배속 고정이 필요하면 `SkillDef.animPlaySpeed` 로 독립 조정.
- **Pathfinder empty path 시 silent fail 금지 (B-10c, 중요!)**: [CPathfinder::Find_Path](Engine/Private/Manager/Navigation/Pathfinder.cpp:42) 는 start/goal 셀이 unwalkable 이거나 도달 불가 시 빈 vector 반환. NavigationSystem 등 호출자가 그대로 `bHasGoal=false; vel.fSpeed=0` 만 처리하면 **Chase/Pursuit 유닛이 제자리 애니로 stuck** (Manager.Tick 의 Chase ForEach 가 `vel.fSpeed<=0` 이면 위치 업데이트 skip). NavGrid 셀 0.5m + 포탑/넥서스 unwalkable 반경 (2~4m) → **라인 클래시에서 적 미니언이 unwalkable 셀에 들어가는 빈도 매우 높음** = 빈번한 트리거. **해결**: 추적 의도 (Chase 류) 가 명확한 경우 NavSystem 의 path empty 분기에서 `target - selfPos` 직선 fallback + `bPathDirty=true` 다음 프레임 재시도. 클릭 이동/웨이포인트는 그대로 정지 (의도된 도달 불가). 진단 카운터: `Nav::PathEmpty`, `Nav::DirectFallback`, `Minion::StuckChase` (Profiler). 같은 NavGrid+A* 위에 올라갈 챔피언 추적, 정글몹 어그로, 펫/소환수, 추후 RL 환경 모두 같은 함정 — **silent fail 인 환경 모델은 학습/MCTS 시뮬레이션에서 transition 노이즈 = sim2real 격차**.
- **ECS Phase 순서 = data dependency 순서 (B-10c, 중요)**: 같은 phase 내 시스템은 [SystemScheduler.cpp:30-42](Engine/Private/ECS/SystemScheduler.cpp:30) 가 JobSystem Submit 으로 **병렬 실행** (race). 그리고 같은 프레임 안에 "set → consume" 의존이 있으면 Producer phase **<** Consumer phase 여야 1프레임 stall 없이 흐름. `MinionAISystem` 이 `nav.vTarget`/`bPathDirty` 를 set, `NavigationSystem` 이 read+write 하므로 AI(1) → Nav(2) 순서 필수. 거꾸로 두면 **첫 Chase 프레임에 stale velocity** (직전 LaneMove 의 웨이포인트 방향) 로 1프레임 잘못 추적 — 사용자에겐 "감지했는데 적 쪽으로 안 감"으로 체감. Phase 번호는 단순 정렬키지만 **data flow 그래프** 로 읽고, swap/추가 시 ① Producer/Consumer 의존 그래프 그리기 ② 같은 phase 시스템 간 컴포넌트 충돌 (race) 검증 두 가지 모두 통과해야. 현재 배치: Transform(0) → AI(1) → Nav(2) → Status(3) → Manager.Tick (Scheduler 외부, 위치 적용 + 미니언 LaneMove waypoint).

---

## 보안 / 안티치트 코딩 컨벤션 (★ Winters 필독)

> **철학**: "완벽한 보호는 불가능. 목표는 **치트 개발 비용 ≥ 게임 수익** 으로 만들기."
> Cheat Engine / Frida / DLL Injection / Kernel driver cheat 에 단계별 저항. Phase 6 커널 안티치트 (`.md/plan/security/`) 이전 단계에서도 **코드 작성 습관**만으로 난이도 최대화.
> 참고 사례: **엘든링 2022 CVE** — `PlayerManager` 가 Client EXE 에 있어서 멀티플레이 RCE 발생. FromSoft 도 Engine DLL 격리로 방향 전환.

### 1. 서버 권위 원칙 (CRITICAL — 1번)
- **Client 는 절대 진실의 원천이 아니다.** 모든 상태 변경은 서버 검증 후 반영.
- 데미지/이동/스킬/아이템 획득: Client prediction 허용, **서버 reconciliation 필수**
- `HealthComponent::fCurrent` 치트가 999 로 바꿔도 **서버 자체 값 기준** → 무시
- 네트워크 패킷 파싱 시 **범위 체크 필수** — `moveDelta > maxSpeed * dt * 1.1` 이면 치트 신고
- 🔴 금지: Client 만으로 `player.gold += 100` 같은 로직. 반드시 서버 경유.

### 2. Client EXE 표면적 최소화
- **민감한 로직은 Engine DLL 에** — Cheat Engine 메모리 스캔 저항. EXE 단일 베이스 vs DLL+offset 계산 = 한 단계 난이도↑
- Client EXE 직접 노출 금지: **체력 변경 / 데미지 계산 / 서버 토큰 처리 / 결제 검증**
- 매니저 포인터는 DLL 내부 static — Client EXE 에 고정 offset 배치 지양
- `CGameInstance::Get()` 진입점은 불가피 → **여러 매니저를 한 객체 뒤에 숨김** (현재 패턴 유지 ✓)

### 3. 심볼 가시성
- `WINTERS_ENGINE` / `ENGINE_DLL` export 는 **최소 세트만**. 공개할수록 공격면 확대
- **CLAUDE.md §6 원칙 유지**: 내부 매니저는 export 금지 (Timer_Manager, UI_Manager, BlueprintRegistry 등)
- Release 빌드: `/PDBALTPATH`, `/DEBUG:NONE` 으로 PDB 제거 → 함수 이름 역추적 불가
- Export 함수명 난독화: `WintersRun` 같은 의미 드러나는 이름 → 추후 난독화 매크로 도입 고려

### 4. 중요 값 이중 저장 + 무결성 검증 (Phase 6 패턴 선점)
- 치트는 `HP=600` 스캔 → 주소 찾아 수정. 방어 패턴:
  ```cpp
  struct ProtectedF32 {
      f32_t m_fValue;
      u32_t m_uXorMask;     // 난수 마스크
      u32_t m_uCheckSum;    // m_fValue XOR m_uXorMask
      // Get/Set 에서 체크섬 불일치 → Tamper 감지 → 서버 신고
  };
  ```
- **지금 HealthComponent 는 POD 여도 OK** — 단 "언젠가 래핑" 전제로 **직접 필드 수정 최소화** (헬퍼 함수 경유). Phase 6 에서 ProtectedF32 로 일괄 교체.

### 5. 로그 / 문자열 누출 방지
- **Release 에 `OutputDebugStringA` / `printf` 금지** — 치트 개발자에게 힌트 제공
- `#ifdef _DEBUG` 로 감싸거나 Logger 추상화 → Release 자동 제거
- 🔴 오류 메시지에 함수명/내부 상태 노출 금지: `"Skill cooldown check failed at line 142"`
- 🟢 대안: 해시 에러 코드 `"E0x7A2F"` (내부 매핑 테이블로만 해석)

### 6. 빌드 플래그 (Release — vcxproj 검증 필수)
- `/guard:cf` — Control Flow Guard (간접 호출 공격 차단)
- `/GS` — Buffer Security Check (스택 카나리, 기본 ON)
- `/sdl` — Additional Security Development Lifecycle checks
- `/DYNAMICBASE` — ASLR 활성 (주소 무작위화)
- `/NXCOMPAT` — DEP 활성 (데이터 영역 실행 방지)
- `/HIGHENTROPYVA` — 64bit ASLR 엔트로피 증가
- Release: `/O2 /GL /Gw /Gy` + `/OPT:ICF /OPT:REF /LTCG`

### 7. 네트워크 패킷 보안 (Phase 4+)
- **디시리얼라이저는 size-bounded** — FlatBuffers 기본 검증 사용
- 패킷 크기 상한: `if (pktSize > MAX_PACKET) disconnect`
- Replay attack 방어: 패킷 시퀀스 번호 + timestamp 검증
- KCP/UDP: 암호화 + HMAC (AES-GCM 또는 ChaCha20-Poly1305)

### 8. 인증 / 결제 (Phase 8 C++ Client SDK)
- **토큰은 메모리에 평문 금지** — `CryptProtectMemory` 로 보호
- JWT Access Token: 짧은 수명 (15분) + Refresh Token
- HTTPS 인증서 핀닝 (pinned cert) — MITM 방어
- 결제: 영수증은 **서버 검증** (Google Play / Apple receipt verification API)

### 9. 파일 무결성 (에셋 치트 방지)
- `.wmesh` / `.wanim` / `.wmat` 로드 시 **SHA256 검증** — 치트가 메시 바꿔 벽뚫기 금지
- 셰이더: HLSL 소스는 **Release 에 포함 금지** — 컴파일된 cso 만 + 해시 비교
- 에셋 번들 전체 해시를 서버 기대값과 매칭

### 10. 안티 디버그 (Phase 6 유저모드 레이어)
- `IsDebuggerPresent()` 는 쉽게 우회 → **PEB 직접 읽기**
- `NtQueryInformationProcess(ProcessDebugPort)` — 디버거 포트 존재 확인
- 타이밍 체크: `RDTSC` 로 간단 명령 수행 시간 측정 → 너무 느리면 단계 실행 중
- 개발 중엔 끄는 매크로: `#ifdef WINTERS_SHIP` 분기

### 11. DLL 주입 저항
- 서명 안 된 DLL 로드 차단: `SetProcessMitigationPolicy(ProcessSignaturePolicy)`
- `GetModuleHandle` 순회하여 화이트리스트 외 DLL 탐지
- Import Address Table (IAT) 후킹 탐지: 함수 포인터 예상값과 비교

### 12. 커널 안티치트 연계 (Phase 6)
- 유저모드 탐지 우회 쉬움 → 커널 드라이버 필수
- 드라이버: EasyAntiCheat / BattlEye / Vanguard 참조, 자체 구현 시 IOCTL 로 유저모드 통신
- **지금 단계 코드 작성 시**: 중요 함수 앞에 `WINTERS_CRITICAL` 매크로 표시 (미리 선언, 나중에 드라이버에서 후크 지점 인식)

### ❌ 즉시 적용 금지 사항

- `OutputDebugStringA("[Damage] Dealt %f to %s\n", dmg, target)` ← **Release 절대 금지**
- Client 에서 `player.gold += 100;` 직접 변경 (서버 경유 필수)
- `std::cout << "Secret key: " << key;` ← 평문 로그
- HLSL 원본을 Bin/Release 배포
- `std::string password` 평문 메모리 보관
- 매니저 raw 포인터 Client 노출 (`Get_Raw_Manager_Ptr` 류 API)
- Client 에 `g_iWinSizeX` 같은 Client 심볼을 Engine 에 **역방향 주입** (DLL 경계 위반 + 공격면 확대)

### 🟢 허용 사항 (Debug 빌드만)

- `#ifdef _DEBUG` 안의 OutputDebugString
- Cheat Engine 으로 자기 게임 디버깅 (이슈 재현용)
- Release Symbol 사용 안 하는 PDB 로컬 보관

### 아키텍처 설계 원칙 (언리얼 UEngineSubsystem + FromSoft 엔진 참조)

- **Engine 매니저는 Engine 이 초기화** (Client 가 UI_Initialize 호출 = 안티패턴)
- **Client 매니저는 Client 가 초기화** (Structure/Jungle/Minion 등 게임 특화)
- **Device 생성 직후 Engine 매니저 자동 Init** — `CEngineApp::Initialize` 내부에서 수행
- Client CGameApp 은 **순수 게임 로직만** — `CEngineApp::Get()` 접근 최소화

### /review 체크리스트 (신규 PR 필수 검증)

1. Client 파일에서 `Engine/Public/` 직접 include 최소인가?
2. `OutputDebugString` 이 `#ifdef _DEBUG` 안에 있는가?
3. 네트워크 수신 함수에 size 체크 있는가?
4. 민감 값 (HP/Gold/Mana) 의 Set 경로가 헬퍼 함수 경유인가?
5. export 마크가 정말 필요한가? (내부 매니저는 금지)

---

## Phase D Gotchas (2026-04-25 추가)

- **castFrame 감지 블록 분리 금지 (중요)**: `Scene_InGame::OnUpdate` 에 castFrame/recoveryFrame 체크 블록을 **여러 개 두면** 첫 블록 끝에서 `m_fActivePrevFrame = curF` 갱신이 박제되어 두 번째 블록의 `HasFramePassed(castFrame, prevFrame)` 가 **항상 false**. 로직은 있는데 실행 안 되는 침묵 버그. **해결: 단일 블록에 `bCastHit`/`bRecoveryHit` 플래그 캐싱 후 모든 반응 (로그 + 스폰 + 전환) 수행하고 맨 마지막에 prevFrame 갱신**. `.md/plan/Champion/02b_IRELIA_PHASE_D_REMEDIATION_PLAN.md` §7 참조.
- **애니 전환 시 castFrame 재발동 (중요)**: `PlayAnimationByName` 이 `m_dCurrentTime=0` 리셋 → 같은 castFrame 을 **여러 애니에서 반복 통과**. `spell4 → spell4_to_idle → idle_01` 3번 바뀌면 castFrame=7 이 3번 지남. **해결: `m_bCastFrameFired` / `m_bRecoveryFrameFired` bool 멤버 + `ApplyLocalPrediction` 에서 새 스킬 발동 시 리셋**. castFrame 도달 1회만 반응 보장.
- **`m_pActiveSkillDef` 댕글링 (Buffer Too Small 크래시)**: stage2 분기에서 `SkillDef s2 = *def;` 로컬 스택에 생성 후 `ApplyLocalPrediction(cmd, s2)` → 내부에서 `m_pActiveSkillDef = &def` 가 **스택 해제된 s2 주소 저장**. 이후 OnUpdate 에서 `d.animKey` 가 쓰레기 포인터 → `sprintf_s("%s", ...)` 가 128B 초과 → "Buffer Too Small" CRT abort. **해결: `SkillDef m_ActiveSkillDefStorage{}` 멤버 추가해 값 복사 후 포인터 저장** (`&m_ActiveSkillDefStorage`).
- **CPlaneRenderer 기본 CULL_BACK → 지면 퀘드 특정 각도 컬링**: DX11 기본 RasterizerState 는 뒷면 컬링. `bBillboard=false` 지면 퀘드가 `fYaw` 회전 시 카메라와의 각도에 따라 **뒷면 노출 → 사라짐**. **해결: `D3D11_CULL_NONE` TwoSided RS 생성 + Render 에 백업/바인딩/복원**. `Engine/Private/Renderer/PlaneRenderer.cpp` 참조. FX 빌보드는 기본적으로 양면 렌더가 기본값.
- **SolutionDir 기준 상대경로 — `Client/Bin/Resource/...`**: `LocalDebuggerWorkingDirectory=$(SolutionDir)` (Client.vcxproj). 따라서 런타임 리소스 경로는 `Client/Bin/Resource/Texture/...` 형식. **던전스 프로젝트의 `../Bin/...` 은 다른 WorkingDir 설정 때문에 작동**. `SetCurrentDirectoryW` 불필요. FX 텍스처 / AttackRange PNG 전부 이 규약으로 로드.
- **FxBillboard 회전 필요 시 `bBillboard=false` + `fYaw`**: `bBillboard=true` 는 카메라 바라보는 고정 빌보드. 월드 방향 (예: E beam 이 두 검 잇는 방향) 반영 불가. **해결: `bBillboard=false` (XZ 지면 퀘드) + `fYaw` (Y축 라디안) + `CFxSystem::Render` 에 `XMMatrixRotationY(fx.fYaw)` 적용**. `Scene_InGame` E beam 스폰: `fYaw = atan2f(dx, dz)`.
- **모델 yaw 의 +π 보정 (Dash 방향 버그)**: `UpdateDash` 의 `yaw = atan2f(dx, dz) + XM_PI` 는 모델이 **뒷면 = 정면** 이라 보정. 따라서 월드 forward 를 계산할 때는 **부호 뒤집기**: `Vec3 fwd{ -sinf(yaw), 0, -cosf(yaw) }`. 이 컨벤션은 Winters 전역이라 FX 스폰 (W stage2 / R 투사체) 도 동일 부호. 안 뒤집으면 "마우스 반대 방향" 버그.

---

## Phase B-7 Gotchas (2026-04-27 추가 — 칼리스타 + UI_Manager 확장 사이클)

- **BanPick 류 Scene 의 `Change_Scene` 직후 `ImGui::End()` + `return;` 필수 (★ Codex F-5)**: `ImGui::Begin("Champion Select")` 안의 버튼 클릭 분기에서 `Change_Scene(MatchLoading)` 호출 직후 그대로 함수 본체를 빠져나가야 함. 안 하면 (a) ImGui Begin/End 비대칭으로 다음 프레임 ImGui assert, (b) Phase D Gotcha 의 self-destruct 패턴 그대로 — 이미 소멸된 BanPick 의 `this` 로 같은 OnImGui 의 다음 버튼 접근. **3 버튼 모두 (Irelia/Yasuo/Kalista)** `Change_Scene(...)` → `ImGui::End();` → `return;` 3 줄 패턴 강제. 다른 Scene 의 `Change_Scene` 분기도 같은 규칙.

- **UI 신규 매니저 신설 금지 — 기존 Engine `CUI_Manager` 확장 (★ Codex F-1, F-2)**: 새 UI 기능 (마우스 커서 / 챔프 HUD / 미니맵 / 데미지 팝업 등) 추가 시 **Client 측 `CUIManager` 같은 신규 클래스 만들지 말 것**. Engine 의 `CUI_Manager` ([Engine/Public/Manager/UI/UI_Manager.h](Engine/Public/Manager/UI/UI_Manager.h)) 가 이미 raw `ID3D11ShaderResourceView*` 멤버 + `Load_TextureSRV(path, &pSRV)` 헬퍼 + ImGui DrawList 패턴을 갖춘 정식 위치. 신규 매니저는 `CGameInstance::Get_UI_Manager()` 와 중복되고, 의존성 (CTexture/CGraphicDev/Scene_InGame 멤버 incomplete type) 줄줄이 깨짐. **신규 SRV 멤버 추가 + `Draw_*` 함수 추가** 패턴으로 확장.

- **`CTexture::GetSRV()` 미공개 — ImGui::Image 시 raw `ID3D11ShaderResourceView*` 직접 보유 필요 (★ Codex F-1)**: [Engine/Public/Resource/Texture.h](Engine/Public/Resource/Texture.h) 의 `m_pSRV` 는 private + public getter 없음 (`Bind`/`Create`/`CreateFromMemory`/`CreateDefault` 만 노출). 따라서 `(ImTextureID)pTex->GetSRV()` 같은 호출은 **컴파일 실패**. UI/이펙트에서 `ImGui::Image` 또는 `pDraw->AddImage` 가 필요하면 `CTexture` 우회하고 **`Load_TextureSRV(path, &pSRV)` 로 raw SRV 직접 보유**해 `(ImTextureID)pSRV` 캐스팅. CUI_Manager 의 HP bar / cursor 패턴 참조. CTexture 는 메시 머티리얼 바인딩 (`Bind(ctx, slot)`) 전용으로 유지.

- **`CUI_Manager` 타입은 Client 에 절대 노출 X — `UI_*` 포워딩만 사용 (★ Codex F-7, GameInstance.h:66 경계 규칙)**: `CGameInstance::Get_UI_Manager()` 같은 raw 포인터 게터 추가 **금지**. Client 가 `CUI_Manager*` 를 직접 만지면 ENGINE_DLL 경계 흐려지고 (a) 매니저 수명 / 멤버 가시성 깨짐, (b) Tier1 패턴 (Timer/Sound/Scene) 일관성 깨짐. 새 UI 기능 노출 시 **`Engine/Include/GameInstance.h` 에 `UI_Set_Foo()` / `UI_Get_Foo()` 포워딩 메서드 1개씩 추가** 후 `.cpp` 에서 `m_pUI_Manager->Set_Foo()` 호출. 포워딩 인자에 enum 이 필요하면 GameInstance.h 에 `enum class eXxx : uint8_t;` forward declare (정의는 UI_Manager.h). EngineSDK 동기화 시 UI_Manager.h + GameInstance.h 둘 다 복사.

- **EntityBlueprint loading thread race (★ Phase B-7.2, 중요!)**: [Loader.cpp:13, 26](Client/Private/Scene/Loader.cpp:26) 의 `std::thread + .detach()` 가 `Register_Blueprints_InGame` 호출 → MatchLoading 3초 안에 완료 보장 X. Scene_InGame OnEnter 의 `Clone_Entity` 가 등록 전 호출되면 **NULL_ENTITY 반환** → 사일러스 entity 자체 미생성 → 호버/디버그 콜라이더 등 모든 검증 실패. **해결 2 단계 방어**: (a) OnEnter 진입 시 `Register_Blueprints_InGame()` 동기 재호출 (Add_Blueprint 가 같은 key 두번째 호출 시 E_FAIL 반환만, 무해), (b) Clone NULL_ENTITY 시 legacy `CreateChampionEntity` 직접 호출 fallback.

- **`lockDuration × animPlaySpeed ≥ recoveryFrame / FBX_FPS` 부등식 필수 (★ Phase B-7.3, 중요!)**: 위배 시 recoveryFrame hook 진입 전에 `m_pActiveSkillDef = nullptr` 처리 → bRecoveryHit fire X → recovery 시점 의존 로직 (Kalista cancel-windown dash 등) 발동 안 됨. 칼리스타 BA 검증값: `1.5 × 0.6 = 0.9s ≥ 14/24fps = 0.58s ✅`. 새 챔프 SkillTable 등록 시 즉시 점검. 증상: castFrame hook 은 fire 되는데 recoveryFrame hook 안 됨, 디버그 로그 `[KalistaPassive] pending` 만 출력되고 `[KalistaPassive] dash` 미출력.

- **flying spear ↔ stuck spear `visualEntity` 핸들 연결 (★ Codex 핵심, 중요!)**: 발사 시각 entity (`KalistaFx::SpawnQSpear` — `FxMeshComponent` lifetime 기반) 와 판정 entity (`KalistaProjectileSystem::Spawn` — invisible) 별개. 두 entity 사이 핸들 없으면 hit 시 flying spear destroy 불가 → 영원히 lifetime 끝까지 비행. **해결**: `KalistaProjectileComponent` 에 `EntityID visualEntity` 추가 + Spawn 인자 + `Execute` 의 hit/maxDist 분기에서 `world.GetComponent<FxMeshComponent>(visualEntity).bPendingDelete = true`. 동일 패턴: 미사일/투사체 + visual FX 분리 시스템 모두 적용.

- **Dash 후 `m_vPlayerDest` 동기화 필수 (★ Phase B-7.4, 중요!)**: `StartKalistaPassiveDash` 가 `m_pPlayerTransform->SetPosition` 으로 직접 위치 갱신해도, [Scene_InGame.cpp:1029](Client/Private/Scene/Scene_InGame.cpp:1029) 의 OnUpdate 매 프레임 player 직선 이동 로직이 `m_vPlayerDest - cur` delta 로 dist 계산 → dash 끝나면 `m_vPlayerDest`(시전 시작 시점 origin) 로 자동 복귀. **해결**: `StartKalistaPassiveDash` 끝에 `m_vPlayerDest = m_vKalistaPassiveDashEnd` + NavAgent goal 비활성 (`bHasGoal=false`). 추가 가드 — dash 중 `!m_bKalistaPassiveDashActive` 검사로 OnUpdate movement 차단.

- **Additive blend + `vColor` RGB ≥ 1.0 = 흰색 포화 (★ Phase B-7.4, 중요!)**: PNG 가 흰색 위주 (LoL FX glow_color 패턴 — RGB=1, alpha 그라데이션) + tint 의 G/B 가 1.0 초과 (예: `{0.7, 1.2, 1.6}`) → `Sampled × tint = (0.7, 1.2, 1.6)` clamp → 0.7/1.0/1.0. **Additive 공식 `Src + Dst`** 가 alpha 무시 + 픽셀 중첩 (mesh 두께) 시 채널 누적 → clamp 가속 → 흰색. **해결**: (a) vColor 모든 RGB ≤ 1.0 (예: `{0.3, 0.5, 0.9}`), (b) AlphaBlend 사용 (alpha 비례 mix 라 누적 X 단 글로우 효과 약함). 상세 분석: 칼리스타 mis_spear 색상 디버깅 사이클.

- **conditional 후속 애니 = 직접 `PlayAnimationByName` 호출 (★ Phase B-7.4, 중요!)**: 칼리스타 dash 같은 "1타 후 conditional 후속 모션" 을 SkillDef 의 stage2 시스템 (이렐리아 W 패턴) 에 **끼워맞추지 말 것**. stage2 는 "user 입력 1타 → 입력 2타 = 두 개의 별도 시전" 의미라 cooldown/mana/castFrame 반복 적용 → 의미 왜곡. **해결**: recoveryFrame hook 안에서 `slot` 분기 + `PlayAnimationByName("kalista_attack1_dash_0")` 직접 호출. dash 중 `!m_bKalistaPassiveDashActive` 가드로 자동 idle/run 교체 차단.

- **모델 yaw `+XM_PI` 보정 = 180° (★ Winters 전역 컨벤션 재확인)**: `XM_PI ≈ 3.1416` (180°), `XM_PI × 0.5f ≈ 1.5708` (90°). 모델이 **뒷면 = 정면** 컨벤션이라 `atan2(forward.x, forward.z)` 결과에 **+XM_PI** 보정. FX 발사 (mis_spear, W stage2, R 투사체, dash 트레일) 도 동일 부호. 90° (XM_PI/2) 와 180° (XM_PI) 헷갈리지 말 것.

---

## 문서 인덱스

| 문서 | 경로 | 내용 |
|---|---|---|
| **코딩 컨벤션 (최종)** | `.md/architecture/WINTERS_ENGINE_CONVENTIONS.md` | 네이밍/GameInstance Tier 경계/팩토리 패턴 |
| **보안 코딩 컨벤션** | 본 CLAUDE.md §보안/안티치트 | 12대 원칙 + 금지/허용 + /review 체크리스트 |
| 마스터 플랜 | `.md/roadmap/LOL_30DAY_MASTER_PLAN.md` | LoL 30일 모작 Phase 0~8 |
| 개발 로드맵 | `.md/roadmap/ROADMAP.md` | Step 1~6 + 코드 작성 규칙 |
| 엔진 아키텍처 | `.md/architecture/WINTERS_ENGINE_ARCHITECTURE_FINAL.md` | 7-Layer 구조, 기술 스택 |
| 백엔드 플랜 | `.md/plan/backend/00_BACKEND_PLAN_INDEX.md` | Phase 0~8 전체 구현 |
| 엔진 플랜 | `.md/plan/engine/` | Irelia 렌더링, JobSystem, RenderGraph |
| 보안/안티치트 | `.md/plan/security/00_SECURITY_INDEX.md` | Level 0~5 |
| C++/CS 지식베이스 | `Desktop\.markdown\C++&CS\00_CPP_CS_KNOWLEDGE_INDEX.md` | 5파트 ~641KB |
| 에셋 가이드 (챔피언) | `.md/guide/LOL_CHAMPION_EXTRACT_GUIDE.md` | skn→glb→FBX |
| 에셋 가이드 (맵) | `.md/guide/LOL_MAP_EXTRACT_GUIDE.md` | mapgeo→glb→Assimp, Z-fighting 해결 |
| **★ TCP MVP (현 베이스, 즉시 진입)** | `.md/plan/sim/04a_MVP_2CLIENT_TCP_DEMO_v2.md` | 1 server + 2 client Move/HP sync. Layer 1 (~58h) + Layer 2 cast event echo (+20h). A' transport boundary (UDP 마이그 시 28h 분량 그대로 재사용) |
| **★★ 04a v2 D-0 sub-plan** | `.md/plan/sim/04a_v2_D0_SHARED_LINK_AND_SUBSET.md` | Server.vcxproj XML diff + ServerSimSubset 6 파일 (Move/Cooldown/DamageQueue/Death/CommandExecutor + MoveTargetComponent) **h/cpp 전문 박제** |
| **★★ 04a v2 D-1 sub-plan** | `.md/plan/sim/04a_v2_D1_SERVER_TRANSPORT.md` | Server transport 10 작업 (IOCPCore/FrameParser/Session/Manager/Dispatcher/GameRoom/SnapshotBuilder/main + Hello.fbs) **h/cpp 전문 박제** |
| **★★ 04a v2 D-2 sub-plan** | `.md/plan/sim/04a_v2_D2_CLIENT_TRANSPORT.md` | Client transport 5 작업 (ClientNetwork/CommandSerializer/SnapshotApplier + Scene_InGame/InputSystem 통합) **h/cpp 전문 박제** |
| **★★ 04a v2 D-3 sub-plan** | `.md/plan/sim/04a_v2_D3_VERIFICATION.md` | 검증 시나리오 3종 + 디버깅 매트릭스 + 회귀 grep 4종 + 롤백 branch 전략 |
| **★ UDP NetStack 마스터 (production)** | `.md/plan/sim/10_UDP_LOL_NETSTACK_MASTER_v2.md` | LoL 정석 — UDP+Reliability+Delta+AOI+LagComp+Prediction 6 마일스톤 (M1-M6) + Sim-11 Encryption. 04a v2 합격이 M1 prerequisite |
| **★ Ezreal FX v2** | `.md/plan/Champion/08_EZREAL_FX_PRESETS_BAKE_v2.md` | Q/W/E/R/BA FX 안 보이는 문제 — 5 파일 수정 (SkillHookContext + Scene_InGame + FxPresets.h/.cpp + Skills.cpp). vRotation Kalista 패턴 + visual/gameplay speed 일치 |

---

## 스킬
- `/code` — 코드 작성+리뷰 사이클 (Codex 패턴 흡수: 기존 인프라 식별 → 데이터 형태 → DLL 경계 → 검증 결정 포인트 → 최소 수정 → 엣지 케이스). 본문 가이드: `winters-skills/code/SKILL.md`
- `/debug-pipeline` — 렌더/이펙트/스킬 버그 디버깅 (셰이더 우선 Read + 데이터 직접 계측 + 비교 대조군 표). 본문: `winters-skills/debug-pipeline/SKILL.md`
- `/review` — 수정 계획서 검증 + 상세 수정 계획서 생성
- `/testing` — GoogleTest 단위 테스트
- `/todo` — 전체 구현 계획 (LoL 30DAY Phase 0~8 + 엘든링 Phase TBD)
- `/phase-d-next` — 이렐리아 Phase D 완료 후 챔프 파이프라인 진입 (Phase 1 Irelia FBX → 2 Yasuo → 3 Kalista → 4 IChampionController → 5 CMesh submesh → 6 Yone+6챔프 → 7 ChampionCatalog → 99 전수 파싱). 컨텍스트 자동 복구 + 권한 열기 포함
- `/phase-2-yasuo` — **Phase 1 + Phase FX 완료 후 Phase 2 야스오 진입**. caller 단순화 → YasuoFxPresets → Q Conditional 3타 / W Wind Wall / E Sweeping Blade / R Last Breath. IreliaFxPresets 패턴 100% 재사용. 권한 열기 포함

## 계획서 규칙 (★ 행동 강제 — 위반 시 유저 재요청 없이 즉시 교정)
1. **계획서 작성·참조 시 예외 없이 세션 대화에 전체 내용(전문) 붙여넣기.**
   - ExitPlanMode 전에도 plan 파일을 붙여넣고, 기존 `.md/plan/` 계획서를 참조할 때도 Read 한 전문을 본문에 그대로 출력할 것.
   - "파일에 기록했습니다", "이미 있습니다", "authoritative 이므로 생략" 전부 금지. 유저가 "보여달라"를 매번 다시 타이핑하게 만들지 말 것.
   - 스크롤 길이 걱정으로 요약·축약 금지. 길이 문제는 유저가 판단.
2. 생성할 파일마다 h/cpp 코드 전문을 포함할 것
3. .h/.cpp 파일 경로 명시 (예: `Engine/Code/RHI/DX11/CDX11Device.cpp`)
4. 줄 번호 명시 (예: L42-L55)
5. 수정 전/후 코드 cpp 블록 포함
6. 추상적 지시 금지 — 반드시 코드 명시
7. 기존 .md 파일에 관련 내용이 있으면 세션에 불러와서 보여줄 것 (요약 금지, 전문 붙여넣기)

## 대화 규칙
- **"대화 세션에 보여줘"** = 해당 .md 파일의 **전체 내용 (전문)** 을 대화 메시지에 그대로 출력할 것.
  Read 도구 결과를 요약하거나 "위에 출력했습니다" 로 대체 금지. 마크다운 원문 전체 붙여넣기.
- **규칙 상기**: 계획서 Read → 본문 붙여넣기는 **유저가 명시적으로 말하지 않아도** 매번 수행. "참조만 하고 넘어가자"는 모델 내부 판단 금지.

---

## Go 백엔드 진행 상황 (상세: `.md/plan/backend/00_BACKEND_PLAN_INDEX.md`)

```
Services/
├── cmd/                          ← 서비스 엔트리 (package main)
│   ├── auth/main.go              ✅ 동작 확인 (8081)
│   ├── leaderboard/main.go       ✅ 동작 확인 (8082)
│   ├── matchmaking/main.go       ✅ 동작 확인 (8083, 매칭 성사 검증)
│   ├── profile/main.go           ✅ 동작 확인 (8084)
│   ├── payment/main.go           ✅ 동작 확인 (8085)
│   └── shop/main.go              ✅ 동작 확인 (8086)
├── internal/                     ← 비즈니스 로직
│   ├── auth/         (model, service, repository, handler)     ✅
│   ├── leaderboard/  (model, repository, handler, consumer)    ✅
│   ├── matchmaking/  (model, service, handler)                 ✅
│   ├── profile/      (model, repository, handler, consumer)    ✅
│   ├── payment/      (model, service, repository, handler)     ✅
│   └── shop/         (model, service, repository, handler)     ✅
├── pkg/              (config, database, cache, messaging, auth, middleware, errors, response) ✅
├── migrations/       (7개 .up.sql / .down.sql)                 ✅
├── docker-compose.yml (postgres:5433, redis:6379, kafka:9092)  ✅
├── .env / .env.example                                         ✅
└── Makefile                                                    ✅
```

| Phase | 서비스 | 포트 | 상태 | API |
|---|---|---|---|---|
| 0 | 인프라 (Docker+pkg) | — | ✅ | — |
| 1 | Auth | 8081 | ✅ | POST /auth/register, login, refresh, logout |
| 2 | Leaderboard | 8082 | ✅ | GET /leaderboard/top, /rank/{id} |
| 3 | Matchmaking | 8083 | ✅ (매칭 성사 확인) | POST /join, DELETE /leave, GET /status |
| 4 | Profile | 8084 | ✅ | GET /profile/{id}, /profile/{id}/history |
| 5 | Payment | 8085 | ✅ | POST /payment/charge, GET /balance |
| 6 | Shop | 8086 | ✅ | GET /shop/items, POST /purchase, GET /inventory |
| 7 | Kafka E2E | — | ✅ | MatchCompleted, SkinPurchased 이벤트 흐름 |
| 8 | C++ Client SDK | — | 🔄 | WinHTTP 기반 CHttpClient + 서비스별 래퍼 |

### 서비스 실행 방법
```cmd
cd Services
docker compose up -d                    :: 인프라 시작
for %f in (migrations\*.up.sql) do docker exec -i winters-postgres psql -U winters -d winters < %f
go run ./cmd/auth                       :: 터미널 1 (8081)
go run ./cmd/leaderboard                :: 터미널 2 (8082)
go run ./cmd/matchmaking                :: 터미널 3 (8083)
go run ./cmd/profile                    :: 터미널 4 (8084)
go run ./cmd/payment                    :: 터미널 5 (8085)
go run ./cmd/shop                       :: 터미널 6 (8086)
```

---

## 완료된 챔피언 렌더링 (5체)

- 이렐리아 (68 anim, 4 mesh, body+blades 텍스처)
- 야스오 (44 anim, 4 mesh, body 텍스처)
- 사일러스 (80 anim, 6 mesh, body+chain 텍스처)
- 비에고 (83 anim, 6 mesh, body+crown_sword+wraith 텍스처)
- 칼리스타 (body+spear 텍스처)

## Phase 1a 산출물 (2026-04-15~16)

- ✅ `ECS/Components/TransformComponent.h` — 부모/자식 계층 + Dirty Propagation
- ✅ `ECS/Systems/TransformSystem.h/.cpp` — `CTransformSystem`
- ✅ `RHI/IBuffer.h` — Buffer 추상 인터페이스
- ✅ `RHI/DX11/DX11VertexBuffer.h/.cpp` — VB 전담
- ✅ `RHI/DX11/DX11IndexBuffer.h/.cpp` — IB 전담 (16/32bit)
- ✅ `RHI/DX11/DX11StructuredBuffer.h` — 템플릿 + `BoneMatrix` alias
- ✅ `RHI/DX11/SamplerStateCache.h/.cpp` — 6개 프리셋

## Phase B-4 산출물 (2026-04-16)

- ✅ 소환사의 협곡 glb 로딩 (Layer 노드 스킵으로 Z-fighting 해결)
- ✅ 맵 오브젝트 21종 렌더링
- ✅ DX11 ComPtr 마이그레이션 (수업 1일차 컨벤션 통합)

## Phase B-5 진행 중 (2026-04-17~)

- ✅ `Engine/Public/Editor/ImGuiLayer.h/cpp` — DX11 + Win32 백엔드
- ✅ `Engine/Include/IScene.h` — Scene 인터페이스
- ✅ `Engine/Public/Scene/Scene_Manager.h/cpp` — Change_Scene/Update/Render/ImGui 래핑
- ✅ `Client/Public/Scene/Scene_InGame.h` + `Scene_InGame.cpp` — 기존 인게임 렌더링 이관
- ✅ `CEngineApp::Render` 에서 ImGui BeginFrame / EndFrame 체인 연결
- 🔄 Scene_Loading 스캐폴드 (백그라운드 쓰레드 로더, ImGui ProgressBar) — 작업 중
- 🔄 Perf Overlay (FPS, draw call metrics) — 작업 중
- 🔄 `IWintersApp::OnImGui()` 글로벌 훅 연결 — 작업 중
- ⏭️ Scene_InGame 에디터 UI 확장 (엔티티 인스펙터, 파라미터 슬라이더)
