# 계획서 작성 사고 과정 함정 (Plan Authoring Pitfalls)

**작성일**: 2026-05-04
**계기**: `13_YONE_MESH_SEPARATION_PHASE_B16_v1.md` Codex 검토 결과 P1×2 + P2×2 결함 — 모두 코드베이스 실측 누락에서 비롯됨.
**적용 범위**: `.md/plan/**` 모든 신규/개정 계획서 박제 시. CLAUDE.md "★★★ C++ 코드 작성 전 반드시 읽을 것" 리스트에 본 문서 추가.
**관계 문서**: `winters-skills/code/SKILL.md` (코드 작성 6단계 사이클) 의 §1 "기존 인프라 식별" 단계의 **하위 강제 가이드**.

---

## §0. 왜 이 문서가 필요한가 — 누적 사례

### 사례 1 — 13_YONE_v1 (2026-05-04, P1×2 + P2×2)

13_YONE_v1 은 1344 lines, §0 Agent Contract Evidence 표 + §1 Preflight Evidence Table 까지 갖춘 "정밀 박제" 양식이었다. 그런데 실제로는 다음 4 결함이 검증 단계에서 잡힘:

| 결함 | 우선순위 | 라인 | 본질 |
|---|---|---|---|
| Yone submesh 이름 = Icosphere 1 + Mesh_0 13 반복 → 이름 기반 매핑 깨짐 | P1 | 607-626 | **P-1 데이터 미검증** |
| ModelRenderer `RenderWithVisibility` 의사코드가 PIMPL 내부 추측 (`pContext`/`UpdateBoneCBuffer`/`BindShader` — 실제 Impl 에 없음) | P1 | 254-301 | **P-2 헤더만 보고 .cpp 미검증** |
| Main pass 만 mask 적용 → normal/depth/SSAO 패스에 hidden submesh 잔존 | P2 | 304-352 | **P-3 render 호출 경로 grep 누락** |
| `extern Winters_SpawnYoneSoul` Scene 직접 의존 — 네트워크/엘든링 액터 시스템과 충돌 | P2 | 1033-1054 | **P-4 장기 아키텍처 영향 미평가** |

### 사례 2 — Phase B-13 마스터 + 4 sub-plan (2026-05-04, P1×5 + P2×5)

B-13 박제 (마스터 + 02/03/04/05 sub-plan, 약 5100 LOC) 는 코드 작성 직전에 검토 — 코드 반영 시 빌드 깨짐 + scheduler race + 행동 변경 가능성.

| # | 결함 | 우선 | 파일 | 본질 |
|---|---|---|---|---|
| 1 | Phase 0.5 / 1.5 fractional 박제 (uint32_t 캐스트 무효 + 같은 phase 직렬 가정) | P1 | 00 §2 / 04 §3 | **P-9 Scheduler 동시성 모델 가정** |
| 2 | 공개 헤더 include 룰 정반대 (flat 박제 vs AGENTS.md "subdir 보존") | P1 | 00 §3 (3) | **P-8 인용 의미 반전** |
| 3 | SpatialIndex 를 CEngineApp 전역 + Tier-2 게터 — 서버 멀티 GameRoom / 엘든링 멀티 World 깨짐 | P1 | 04 §4 | **P-10 Owner scope 미결정** |
| 4 | VisionComponents.h 의 `WardComponent::ownerTeam = eTeam` 사용하는데 GameplayComponents.h 미include | P1 | 02 §1 | **P-15 헤더 외부 의존 미include** |
| 5 | FogOfWarRenderer 가 Engine/Public 헤더에서 `<d3d11.h>` + `ID3D11ShaderResourceView` 직접 노출 — RHI 경계 위반 | P1 | 02 §5 | **DLL 경계 + DX12 확장성** (CLAUDE.md ComPtr 룰 변형) |
| 6 | Scene render 패턴 가정 (`Render(matWorld)`) 이 실제 ModelRenderer (`UpdateTransform` + `Render`) 와 다름 | P2 | 02 §6 | **P-2 PIMPL 추측 변형 (호출 시그니처 추측)** |
| 7 | MinionAI 교체안 mask 가 Champion/Turret/JungleMob/Inhibitor/Nexus 까지 확장 — 라인전 행동 변경 | P2 | 04 §5 | **P-14 행동 정책 무의식적 변경** |
| 8 | `m_SystemScheduler.GetSystem<CTurretAISystem>()` 호출 — 실제 `CSystemSchedular` 에 없는 API | P2 | 03 §4 | **P-13 미존재 API 호출** |
| 9 | `CSpatialIndex::CELL_SIZE = 8.f` Engine 공용 박힘 — LoL 만 유효, 엘든링 부적합 | P2 | 04 §2 | **P-11 도메인 상수 Engine 박제** |
| 10 | Cell 계산 `static_cast<int32_t>(world / cell)` — 음수 영역 0-방향 절삭으로 셀 경계 1 cell 어긋남 | P2 | 04 §2 | **P-12 음수 좌표 truncation** |

§1 Preflight 표에 "submesh 후보 (FBX 실측 필요)" 라고 **TODO 명시**까지 했지만 본문 박제는 그 TODO 를 안 풀고 진입. 형식은 갖췄는데 핵심 데이터 결정을 박제 시점에 미룸 — 결과적으로 v1 그대로 코드 반영하면 빌드 실패 또는 동작 깨짐.

**교훈**: `Preflight 표 / Agent Contract` 같은 "박제 형식" 자체가 결함을 막아주지 못한다. **결정해야 할 사실을 박제 전에 실제로 결정** 해야 한다. B-13 사례에서 보듯, 코드 실측 (Scheduler 본체 / AGENTS.md include 룰 / ModelRenderer Impl) 을 안 읽고 박제하면 *형식* 은 완벽해도 *기술적 사실* 이 어긋난다. **인용은 줄 번호 + 직접 인용 블록**, **API 호출은 grep 검증**, **자료 구조 변경은 행동 보존 검증** 이 강제.

---

## §1. 7가지 함정 (Pitfalls)

### P-1. 실제 데이터 형태 미검증 (Data Shape Unverified)

**증상**: ".wmesh / .wanim / .json / DB row 등 실제 binary/text 데이터를 안 읽고 추측한 스키마/필드/이름으로 박제."

**13_YONE_v1 사례**:
- §4.6 의 `YoneMesh::Body=0, Katana_R=1, ...` 인덱스 매핑 — 실제 yone.wmesh 는 `Icosphere(1) + Mesh_0(13) = 14 submesh`. 이름이 전부 "Mesh_0" 으로 중복돼 `FindSubmeshIndex(string)` 자체가 작동 불가.
- §1 Preflight 에 "submesh 후보 (FBX 실측 필요)" TODO 명시. 박제 진입 시 안 풀음.

**회피**:
- 박제 전에 **실제 파일을 도구로 dump**. wmesh 면 `WintersAssetConverter info <file>` 류 로 메시 카운트 + 이름 + material_hash 출력.
- 출력 결과를 §1 Preflight 표의 **"실측" 행에 박제**. "필요" 또는 "추정" 같은 단어가 표에 남으면 박제 진입 금지.
- 데이터가 깨끗하지 않으면 (이름 중복 / 필드 부재) 이름 매핑 대신 **manifest 파일 + index/hash 페어** 로 우회.

**체크리스트** (박제 전 강제):
- [ ] 본 계획서가 의존하는 모든 binary/text 파일을 Read/dump 했는가?
- [ ] 그 파일의 **모든 필드 + 실측값** 이 §1 Preflight 표에 있는가?
- [ ] "추정", "필요", "TBD" 같은 단어가 본문 데이터 결정 영역에 남아있지 않은가?

---

### P-2. PIMPL/private 멤버 추측 (Hidden Impl Speculation)

**증상**: "헤더 (`.h`) 만 보고 본체 (`.cpp`) PIMPL 내부 멤버 / 헬퍼 함수를 추측한 의사코드."

**13_YONE_v1 사례**:
- §3.5 의 `RenderWithVisibility` 안에서 `m_pImpl->pModel`, `m_pImpl->pContext`, `UpdateBoneCBuffer()`, `BindShader()` 4 토큰 사용.
- 실제 `ModelRenderer::Impl` 의 멤버는 `pSharedModel` (shared_ptr), `pInstanceAnimator`, `cbBones` 등 — `pModel`/`pContext` 미존재.
- 실제 `Render()` 본체는 `BindShader()` 같은 helper 없이 `pSharedShader->Bind(pContext)` 형태로 직접 호출.
- `pContext` 는 `IRHIDevice` 를 매번 캐시 + `GetNativeDX11Context(pDevice)` 로 추출 — 멤버 X.

**회피**:
- PIMPL 클래스를 만지는 계획서는 반드시 `.cpp` 의 `struct Impl { ... };` 정의 라인을 **Read 후 본문에 인용**.
- "의사코드" 라는 단어 자체를 박제에서 금지 — 박제는 컴파일 가능한 코드여야 함.
- helper 함수 (`UpdateBoneCBuffer` 등) 를 호출하기 전 `Grep` 으로 해당 함수 존재 검증. 없으면 신규 박제 또는 패턴 변경.

**체크리스트**:
- [ ] PIMPL 클래스에 박제하는 메서드가 사용하는 모든 멤버를 `.cpp Impl 구조체`에서 확인했는가?
- [ ] 호출하는 모든 helper 함수가 `Grep` 으로 실재함을 확인했는가?
- [ ] 박제 코드를 그대로 컴파일러에 넣어도 unresolved symbol 이 0 인가?

---

### P-3. Render 호출 경로 단일 가정 (Single Render Path Assumption)

**증상**: "현재 코드에 main pass / normal pass / depth pass / shadow pass 가 따로 있는데, 한 곳만 변경하고 나머지를 잊음."

**13_YONE_v1 사례**:
- Scene_InGame 의 챔프 렌더 loop 1 곳에만 `RenderWithVisibility(mask)` 박제.
- 실제 `ModelRenderer::RenderNormalPass(meshShader, meshPipeline, skinnedShader, skinnedPipeline)` 가 별도 존재 (PBR G-Buffer pre-pass).
- 결과: hidden submesh 가 G-Buffer normal pass 에 찍혀 SSAO/depth 가 valid → main pass 에서만 사라짐 = 시각적 hole + ghost normal.

**회피**:
- 신규 mask/visibility/filter 류 추가 시 **모든 render 호출 경로 grep**:
  ```bash
  grep -rn "pRenderer->Render\|pRenderer->RenderNormal\|pRenderer->RenderShadow" Client/ Engine/
  ```
- 경로 N 개면 N 개 모두에 변형 박제. 1 경로만 박제는 P-3 결함.
- "main pass" / "normal pass" / "shadow pass" / "depth-only pass" / "transparent pass" 5 후보를 표로 박제.

**체크리스트**:
- [ ] 변경 대상 함수의 **모든 호출자** 를 grep 했는가?
- [ ] 각 호출자가 같은 의미론적 변경을 받아야 하는가? 받아야 한다면 박제됐는가?
- [ ] G-Buffer / Shadow / SSAO / Depth pre-pass 가 코드에 존재한다면 그곳도 박제됐는가?

---

### P-4. ECS vs Scene 책임 경계 모호 (ECS / Scene Coupling)

**증상**: "`extern Winters_XxxFn(...)` 또는 `static_cast<CScene_InGame*>` 같은 RTTI/전역 함수로 Scene 직접 호출 — 향후 네트워크 / 멀티 Scene / 엘든링 액터 시스템 진입 시 불가능."

**13_YONE_v1 사례**:
- §5.3 `Yone_Skills.cpp` 안에서 `extern EntityID Winters_SpawnYoneSoul(EntityID);` + Scene 끝의 `static_cast<CScene_InGame*>(CGameInstance::Get()->Get_CurrentScene())`.
- Yone E (Soul Unbound) 가 InGame Scene 의 구체 타입에 묶임. Phase Sim 04a v2 (TCP MVP) → Sim-10 v2 (UDP) 진입 시 서버 권위 시뮬레이션 측에는 InGame Scene 자체가 없음.

**회피 패턴 — 본 프로젝트 컨벤션**:
- 데이터 변경 의도는 **ECS 컴포넌트 / 이벤트** 로 표현:
  ```cpp
  struct YoneSoulRequestComponent {   // 1 frame 만 살아있는 request
      EntityID bodyEntity;
      bool_t   bSpawnRequested = true;
  };
  ```
- 별도 시스템 (`CYoneSoulSpawnSystem` 또는 client-only `Scene_InGame::ConsumeYoneSoulRequests()` step) 이 매 frame 소비 + Spawn + Component 제거.
- 이렇게 하면:
  - 서버 측 ServerSimSubset 은 같은 컴포넌트 read 후 다른 시스템 (`CServerSoulReplicator`) 에서 처리.
  - 멀티 Scene 시 각 Scene 이 own 시스템 등록.
  - 엘든링 액터/AI 도 같은 패턴 ("RequestComponent" → "Spawn System") 재사용.

**체크리스트**:
- [ ] 박제하는 코드가 `Scene_InGame::*`, `CGameApp::*`, `extern` 함수를 호출하는가?
- [ ] 그 호출이 **데이터 의도** (entity spawn / FX 발생 / 사운드 재생) 라면 ECS 컴포넌트/이벤트로 변환 가능한가?
- [ ] 변환 후에도 클라/서버 양쪽이 같은 컴포넌트 재사용 가능한가?

---

### P-5. 유령 의존 / 인용 파일 부재 (Phantom References)

**증상**: "계획서가 `XXX.md v1 적용` 같이 다른 문서를 인용하지만 실제로 그 파일이 없거나 다른 위치에 있음."

**13_YONE_v1 사례**:
- §0 헤더 `Framework: BUILD_INTEGRITY_FRAMEWORK.md v1 적용` 명시. 실제 `.md/architecture/` 에 그 파일 부재 (Glob `BUILD_INTEGRITY*` 0 hit).
- 다음 박제자가 framework 를 따른다고 가정해도 framework 자체를 못 찾음 → 비어있는 promise.

**회피**:
- 인용하는 모든 .md / .h / .cpp 경로를 **Read** 또는 **Glob** 으로 실존 검증.
- 박제 시 인용 파일의 **L## 줄 번호 + 핵심 인용** 박제 (CLAUDE.md "계획서 규칙 §3 줄 번호 명시").
- "v1 적용", "참조", "준수" 같은 부사어구만 있고 실제 항목 박제가 없으면 P-5.

**체크리스트**:
- [ ] 본 계획서가 인용하는 모든 .md / .h / .cpp 가 실재하는가?
- [ ] 각 인용에 **줄 번호 + 직접 인용 블록**이 있는가?
- [ ] "참조하라" "준수하라" 만 있고 인용 본문이 없는 항목이 0 인가?

---

### P-6. TODO 박제 진입 (Premature Commit on TODO)

**증상**: "Preflight 표에 'TBD' / '확정 필요' / '실측 필요' 가 남은 채 본문 코드 박제 진입."

**13_YONE_v1 사례**:
- §1 Preflight: "submesh 후보 (FBX 실측 필요) — 첫 변환 후 Output 로그 검증"
- §4.6 본문: 그 검증 없이 `Body=0, Katana_R=1, ...` 가정값 박제.

**회피**:
- §1 Preflight 표가 "TBD" 가 0 일 때만 §3 본문 진입 — **Gate 강제**.
- TBD 항목이 변환/실행 결과 로그 의존이면, 박제는 "변환 절차" 까지만 박제 + 다음 cycle 에서 결과 받아 분리 박제 (v1.5).
- 한 계획서 안에 "TBD → 본문 박제" 같이 두 단계가 섞이면 분리.

**체크리스트**:
- [ ] §1 Preflight 표의 모든 행이 **확정값** 인가? "필요"/"추정"/"TBD" 가 0 인가?
- [ ] 확정 못 한 항목이 있다면 박제 범위를 그 직전까지로 한정했는가?

---

### P-8. 인용 의미 반전 (Citation Polarity Flip) ★ 2026-05-04 추가

**증상**: "다른 문서 L## 줄 번호 인용 + 직접 인용 블록 없이 부사어 (`...의 룰` / `...준수`) 만 박제. 실제로는 인용 의미와 박제 의미가 **정반대**."

**B-13 마스터 사례**:
- `00_INDEX_MASTER.md` §3 (3) 항목: "Include 컨벤션 (CLAUDE.md L795 'EngineSDK/inc 는 flat 구조'): 공개 헤더 안의 include 는 `#include "Entity.h"` (flat)."
- 실제 AGENTS.md L437: "서브디렉토리 헤더는 **반드시 폴더 경로 포함**: `"ECS/Entity.h"`. **금지**: `"Entity.h"` (경로 생략) — Client 빌드 실패."
- 인용 위치는 맞는데 의미가 정반대. 박제 그대로 반영 시 Client TU C1083 다발.

**회피**:
- `L##` 인용 시 반드시 **그 줄 직접 인용 블록** 박제. P-5 (유령 의존) 와 동일하지만, P-5 는 파일 부재, P-8 은 파일 실재 + 의미 반전.
- 부사어 (`...의 룰`, `...준수`) 만 박제 시 검토자가 의미 일치 검증 불가 → 박제 거절.

**체크리스트**:
- [ ] L## 인용 옆에 그 줄 직접 인용 블록 있는가?
- [ ] 인용 의미 ↔ 박제 결론이 **같은 방향** 인가? (반대 방향이면 P-8)

---

### P-9. ECS Scheduler 동시성 모델 가정 (Concurrency Model Assumption) ★ 2026-05-04 추가

**증상**: "같은 phase 내 시스템이 **직렬** 실행된다고 가정하고 Producer→Consumer 의존을 같은 phase 에 배치. 또는 `0.5` / `1.5` 같은 fractional phase 박제 — `uint32_t` 캐스트로 의미 무효."

**B-13 마스터 사례**:
- `00_INDEX_MASTER.md` §2 phase 배정: SpatialHash=0.5 / TurretAI=1.5. 실제 `ISystem::GetPhase()` 가 `uint32_t` 라 0.5 → 0 (Transform 과 충돌 phase). 같은 phase 시스템 2 개 → JobSystem 병렬 실행 ([SystemScheduler.cpp:22-42](Engine/Private/ECS/SystemScheduler.cpp:22)).
- 04 sub-plan §3 코멘트: "정수만 쓰므로 스케줄러는 0.5 = 0 으로 floor. 따라서 같은 phase 내 다른 시스템과 race 안 나도록 phase=0 의 마지막 RegisterSystem 으로." — 등록 순서로 race 회피 가정. 실제는 같은 phase=0 시스템 2 개면 **JobSystem Submit 으로 모두 병렬 실행**, 등록 순서 무관.

**회피**:
- ECS Scheduler 가정 박제 전 [`SystemScheduler.cpp::Execute`](Engine/Private/ECS/SystemScheduler.cpp:18) Read 후 본문 인용.
- Producer→Consumer 데이터 의존이 있는 두 시스템은 **반드시 다른 정수 phase**. 같은 phase 는 데이터 race 0 인 시스템만 (예: TransformSystem 만 단독, 그 다음 phase 에 SpatialHash 단독).
- Fractional phase 박제 금지 — 정수만.

**체크리스트**:
- [ ] 박제하는 phase 가 정수인가?
- [ ] 같은 phase 안에 박제하는 시스템들이 같은 컴포넌트를 동시에 read+write 하지 않는가?
- [ ] Producer→Consumer 의존이 있는 두 시스템이 다른 phase 에 있는가?

---

### P-10. Owner Scope 미결정 (Singleton vs World vs Scene) ★ 2026-05-04 추가

**증상**: "인프라 인덱스 (Spatial / Vision / NavGrid) 를 `CGameInstance::Get_X()` Tier-2 게터로 노출. 향후 서버 멀티 GameRoom / 멀티 Scene / 엘든링 멀티 World 진입 시 단일 인스턴스 한계 노출."

**B-13 04 sub-plan 사례**:
- §4 `CEngineApp::m_SpatialIndex` 단일 인스턴스 + `CGameInstance::Get_SpatialIndex()` Tier-2 게터.
- §5 의 `CWorld::SetSpatialIndex` / `world.GetSpatialIndex()` 도 박제 — 두 패턴 공존하며 어느 쪽이 권위인지 모호.
- 서버 GameRoom (Sim-10 v2) 1 process 안에 5 GameRoom × 자체 World × 자체 SpatialIndex 가 필요하면 Engine 전역은 자연스럽게 깨짐.

**회피 — Owner Scope 결정 매트릭스**:

| Index 종류 | 권장 Owner | 이유 |
|---|---|---|
| Spatial / Vision / NavGrid | **CWorld** | World 마다 entity 다름. World ↔ index 1:1 |
| RHI Device / Shader Cache | CEngineApp 전역 | 프로세스당 1 GPU |
| Sound / Resource Cache | CGameInstance Tier-1 | 프로세스 공용 |
| Input | CGameInstance Tier-1 | 한 프로세스 한 활성 윈도우 |
| Scene 한정 시스템 | Scene::m_X 멤버 | Scene 수명 = 시스템 수명 |

`CWorld* CGameInstance::Get_World()` 같은 게터로 World 하나만 노출 → World 가 인덱스 보유. CGameInstance 전역 인덱스 게터는 **금지**.

**체크리스트**:
- [ ] 박제하는 인덱스/매니저의 owner scope 가 위 표 매트릭스와 일치하는가?
- [ ] 서버 멀티 GameRoom 또는 멀티 Scene 시나리오에서 인스턴스 N 개 필요해지면 박제 구조가 깨지지 않는가?

---

### P-11. 도메인 상수 Engine 공용 박제 (Hard-coded Domain Constants) ★ 2026-05-04 추가

**증상**: "한 게임 (LoL) 의 맵 크기/cell 사이즈/엔티티 수 같은 도메인 상수를 Engine 공용 클래스의 `static constexpr` 로 박제. 엘든링 / 다른 게임 진입 시 재사용 불가."

**B-13 04 sub-plan 사례**:
- `CSpatialIndex::CELL_SIZE = 8.f`, `GRID_HALF_EXTENT = 32` — LoL 280×280m 맵 가정.
- 엘든링 오픈월드 (~수 km) / 던전 (~수 백m) 진입 시 같은 클래스 재사용 불가.
- "Engine 공용" 이름 (`CSpatialIndex`) 인데 실제는 LoL 전용 — 명명/책임 mismatch.

**회피**:
- 도메인 상수는 **`InitDesc` / `GridDesc` 구조체 주입**:
  ```cpp
  struct SpatialGridDesc {
      Vec3   worldOrigin;
      f32_t  cellSize;
      i32_t  halfExtentX;
      i32_t  halfExtentZ;
  };
  void CSpatialIndex::Initialize(const SpatialGridDesc& desc);
  ```
- 또는 LoL 전용임을 명시: `class CLoLSpatialIndex { static constexpr ... };`. Engine 공용 base + LoL 파생.

**체크리스트**:
- [ ] 박제하는 클래스가 `static constexpr` 로 박힌 도메인 값 (게임 맵 크기 등) 이 있는가?
- [ ] 그 값을 다른 게임 (엘든링) 또는 다른 맵에서 재사용해야 하면 InitDesc 주입 또는 게임-specific 클래스 분리가 가능한가?

---

### P-12. 음수 좌표 정수 truncation (Negative Coordinate Truncation) ★ 2026-05-04 추가

**증상**: "Spatial cell 계산 / 그리드 인덱싱 시 `static_cast<int32_t>(world_x / cell_size)` 사용. C++ 정수 변환은 **0-방향 절삭** 이라 음수 영역에서 셀 경계가 1 cell 만큼 어긋남."

**B-13 04 sub-plan 사례**:
- §2 `CellX(f32_t worldX) const { return static_cast<int32_t>(worldX / CELL_SIZE); }`.
- `worldX = -0.5f, cellSize = 8.f` → division -0.0625 → cast 0 (정확히는 cell -1 이어야 함).
- 맵 원점 중심 좌표계 (LoL 맵 -140 ~ +140) 의 negative quadrant 에서 **셀 경계 1 cell 어긋남** → MinionAI 의 적 검색 시 같은 cell 못 찾고 stale entity.

**회피**:
- 음수 가능 좌표는 **`std::floor(worldX / cellSize)`** 사용:
  ```cpp
  inline int32_t CellX(f32_t worldX) const {
      return static_cast<int32_t>(std::floor(worldX / CELL_SIZE));
  }
  ```
- 또는 좌표를 양수 영역으로 시프트한 후 정수 절삭. `(worldX + halfWorld) / cellSize` 패턴은 OK 단 halfWorld > |minWorld| 보장.

**체크리스트**:
- [ ] 음수 좌표가 들어올 수 있는 cell 계산이 `static_cast<int>` 사용하는가?
- [ ] 그렇다면 `std::floor` 또는 양수 시프트로 교체했는가?
- [ ] 단위 테스트: cell(-0.5) == -1 / cell(-7.9) == -1 / cell(-8.0) == -1 / cell(-8.1) == -2 검증.

---

### P-13. 미존재 API 호출 (Phantom API Call) ★ 2026-05-04 추가

**증상**: "박제 본문에서 `m_Scheduler.GetSystem<T>()` 같이 헤더에 없는 API 를 호출. grep/Read 검증 안 함."

**B-13 03 sub-plan 사례**:
- §4 `pTurretAI = m_SystemScheduler.GetSystem<CTurretAISystem>()` 캐시. 실제 `CSystemSchedular` ([SystemScheduler.h:11-35](Engine/Public/ECS/SystemScheduler.h:11)) 에 그 메서드 없음. 박제만 보고 따라쓰면 unresolved symbol.

**회피**:
- 호출하는 모든 API 를 `Grep` 으로 헤더에서 실재 검증.
- 없으면 (a) 신규 박제 명시, (b) 우회 패턴 (raw 포인터 캐시 / ECS event) 박제.
- 우회 패턴: 시스템 등록 직후 raw 포인터를 Scene 멤버에 캐시 — `auto pTurretAI = CTurretAISystem::Create(...).get(); m_pTurretAI = pTurretAI; m_Scheduler.RegisterSystem(unique_ptr<...>(pTurretAI));`. 또는 **NotifyChampionAttackedAlly 같은 외부 알림은 ECS event 컴포넌트 (`TowerAggroNotifyComponent` 1-frame request)** 로 변환.

**체크리스트**:
- [ ] 박제 본문에서 호출하는 모든 API 가 그 클래스 헤더에 있는가?
- [ ] 미존재 시 신규 박제 명시 또는 우회 패턴 (ECS event) 박제했는가?

---

### P-14. 행동 정책 무의식적 변경 (Behavior Drift via Optimization) ★ 2026-05-04 추가

**증상**: "성능 최적화 박제 시 자료 구조/검색 마스크를 슬며시 확장 → 게임 행동 (타겟팅 정책 / 어그로 룰) 이 변경됨."

**B-13 04 sub-plan 사례**:
- §5 MinionAISystem::FindClosestEnemy 교체:
  - **현재 행동** (실측): 적 미니언 only 타겟팅 (`MinionStateComponent` 가진 엔티티만).
  - **계획서 mask**: `Minion | Champion | Turret | JungleMob | Inhibitor | Nexus` 6 종 — 미니언이 챔프/타워/정글몹까지 첫 어그로 후보로 검색.
- 결과: 라인전 동작 (미니언이 챔프 무시 + 적 미니언만 평타) 자동 변경. 사용자가 의도하지 않은 게임 디자인 변경.

**회피**:
- 최적화는 **행동 보존 (behavior-preserving)** 원칙. 자료구조/알고리즘만 바꾸고 입력/출력 도메인은 동일하게.
- 04 (인프라) 단계: mask = 기존 검색 도메인과 동일 (`SpatialMask(eSpatialKind::Minion)` only).
- 행동 확장 (챔프 어그로 / 정글몹 어그로 / 타워 어그로) 은 **별도 phase / 별도 PR** 로 분리.

**체크리스트**:
- [ ] 최적화 박제 시 새 자료구조의 입력/출력 도메인이 기존 알고리즘과 정확히 동일한가?
- [ ] 행동 확장 (mask 추가 / 조건 완화) 이 들어가면 별도 박제로 분리했는가?
- [ ] 박제 후 기존 동작 회귀 테스트 (`Profiler::TargetChanged == 0` 등) 가 검증 매트릭스에 있는가?

---

### P-15. 헤더 외부 의존 미include (Missing External Dependency Include) ★ 2026-05-04 추가

**증상**: "신규 컴포넌트 헤더가 다른 헤더의 enum/struct 사용. 그 의존 헤더 미include — TU 가 직접 include 해야 함을 가정."

**B-13 02 sub-plan 사례**:
- `Engine/Public/ECS/Components/VisionComponents.h` 의 `WardComponent::ownerTeam` 이 `eTeam` 사용. 그러나 `eTeam` 정의는 `GameplayComponents.h` L22-28. VisionComponents.h 가 GameplayComponents.h 미include.
- 모든 사용 TU 가 두 헤더 같이 include 해야 컴파일 — fragile, **공개 헤더가 자기 의존을 자체 해소** 가 컨벤션.

**회피**:
- 헤더는 자기가 사용하는 모든 타입의 정의 헤더를 직접 include.
- 무거운 의존이 우려되면 `enum class eTeam : uint8_t;` forward declare 가능 (단 멤버 변수에 사용 시 fully-defined 필요).
- 또는 컴포넌트 헤더에서 의존하는 enum 만 별도 작은 헤더 (`Engine/Public/ECS/Components/TeamEnum.h`) 로 분리.

**체크리스트**:
- [ ] 신규 헤더가 사용하는 모든 외부 타입 (enum/struct/class) 의 정의 헤더를 직접 include 했는가?
- [ ] forward declare 만 했다면 그 타입이 멤버 사용 (값) 이 아닌 포인터/참조에서만 사용되는가?

---

### P-16. 산술 검증 누락 (Arithmetic Verification Skipped) ★ 2026-05-04 추가

**증상**: "비트 폭 / 메모리 한도 / 인덱스 범위 등 자료형 결정 박제 시 **자기 박제 사실의 산술 검증** 안 함. 16M ≫ 100K 같은 명백한 비교를 '부족' 으로 박제."

**ECS v2 rev 1 사례**:
- "32-bit gen + 24-bit index = 16M index 한도 → 엘든링 100K 시 부족" 박제 — `2^24 = 16,777,216` ≫ `100,000`. **충분**. 산술 검증 안 함.
- 오히려 24-bit gen 이 서버 장기 churn 시 위험 (60FPS × 144 entity/min × 60min = 518K spawn/h. 16M / 518K = 32시간 wrap. **단일 세션은 안전하지만 서버 5h+ 운영 시 wrap-around 가능**).

**회피**:
- 비트 폭 결정 시 **2^N 값을 명시 박제**. `2^24 = 16,777,216` 같이 숫자로.
- 사용 사례 상한과 직접 산술 비교 — `엘든링 100K < 16M = 충분` 또는 `세션 5h × 1000 spawn/sec = 18M > 16M = 부족`.
- 박제 진입 전 LLM 또는 검토자에게 "산술 검증" 명시 요청 (자기 검증 회피).

**체크리스트**:
- [ ] 모든 비트 폭 / 메모리 한도 박제 옆에 `2^N` 숫자 명시?
- [ ] 사용 사례 상한 (엔티티 수 / 세션 길이 / 메시 수 등) 과 명시 산술 비교?
- [ ] "부족" / "충분" 결론에 숫자 인용?

---

### P-17. Typedef 일괄 변경 = ABI/컴파일 폭발 (Mass Typedef Substitution) ★ 2026-05-04 추가

**증상**: "기존 타입 (`using EntityID = uint32_t;`) 을 새 struct 타입 (`using EntityID = EntityHandle;`) 으로 일괄 typedef 변경 박제. 기존 코드의 vector index / `%u` 로그 / `unordered_map<EntityID, ...>` key / sparse array index 등 광범위 사용 처리 가 폭발."

**ECS v2 rev 1 사례**:
- "Phase 2 — `using EntityID = EntityHandle;` typedef 통합" 박제. 현재 `EntityID` 가 vector index / `OutputDebugString("entity %u", id)` / `std::unordered_map<EntityID, X>` / sparse 배열 index 로 사용. struct handle 로 일괄 변경 = 컴파일 에러 수천 건 + STL 컨테이너 호환성 깨짐 + ABI 변경.

**회피 — Legacy Adapter 패턴**:
- 신규 API (CECSWorld) 만 EntityHandle 받음. 기존 EntityID = uint32_t 는 그대로 유지.
- 변환 헬퍼 (`ToHandle(legacyId, gen)`, `ToLegacyId(handle)`) 박제 — 두 표현 공존.
- 마이그는 phase 별 점진 — 핫패스 → 콜드패스 → 통합 (3+ cycle).
- **일괄 typedef 변경 절대 박제 X**. 변경하려면 별도 검토 cycle + 호출 매트릭스 N 천 건 박제 후.

**체크리스트**:
- [ ] 박제하는 타입 변경이 기존 코드의 광범위 사용처 (vector index / 로그 / map key / sparse index) 와 호환되는가?
- [ ] 호환 안 되면 legacy adapter 박제 후 phase 별 점진 마이그?
- [ ] "일괄 typedef" 단어가 박제 본문에 등장하면 P-17 의심.

---

### P-18. RHI/Engine 인프라 미인지 (Existing Infrastructure Unaware) ★ 2026-05-04 추가

**증상**: "신규 시스템 박제 시 **이미 존재하는 인프라** 의 패턴/타입을 미인지하고 별도 박제. 코드베이스 grep / Read 누락."

**ECS v2 rev 1 / RG v2 rev 1 사례**:
- ECS v2 rev 1: `EntityHandle 64-bit struct` 별도 박제 — 실제 [RHIHandles.h:6-47](../../../Engine/Public/RHI/RHIHandles.h:6) 에 `RHIHandle<TTag>` 가 이미 박제됨 (index 32 + generation 32). **그 패턴 재사용** 이 정답 — `using EntityHandle = RHIHandle<EntityTag>;`.
- RG v2 rev 1: `IRHIDevice::TextureHandle` / `BufferHandle` 박제 — 실제 [RHIHandles.h:58-65](../../../Engine/Public/RHI/RHIHandles.h:58) 의 `RHITextureHandle` / `RHIBufferHandle` 등 8 종 typed handle 이 이미 박제됨.
- GPU Driven v2 rev 1: `pDevice->DrawIndexedIndirect(...)` 박제 — 실제 [IRHICommandList.h:24-26](../../../Engine/Public/RHI/IRHICommandList.h:24) 에 `Draw / DrawIndexed / Dispatch` 가 이미 박제됨. `DrawIndexedIndirect` 신규는 `IRHIDevice` 가 아닌 **`IRHICommandList` 확장**.

**회피**:
- 박제 진입 전 **`Engine/Public/RHI/`, `Engine/Public/Core/`, `Engine/Public/Resource/`, `Engine/Public/Renderer/` 4 폴더 grep + Glob 전수 스캔** (CLAUDE.md "★ 새 기능 계획 전 의무" 와 일관).
- 검색 키워드: `Handle`, `Manager`, `Cache`, `Pool`, `System`, `Buffer`, `Pipeline` 등 신규 시스템과 유사 도메인 어휘.
- 발견된 인프라는 **재사용 또는 확장** — 별도 박제 금지.

**체크리스트**:
- [ ] 박제 진입 전 4 폴더 (RHI / Core / Resource / Renderer / ECS) 전수 grep?
- [ ] 신규 박제하는 타입/패턴의 유사 인프라가 코드베이스에 이미 있는가?
- [ ] 있다면 재사용 또는 확장 — 별도 박제 X?

---

### P-19. Render/Simulation 결합 (Render-Sim Coupling) ★ 2026-05-04 추가

**증상**: "Render Pass 가 ECS World/Component Query 를 직접 호출. 향후 멀티스레드 / 서버 분리 / 결정적 시뮬 / 서버 권위 검증 진입 시 깨짐."

**RG v2 rev 1 사례**:
- `IRgPass::Execute(ctx)` 안에서 `ctx.GetWorld()->Query().With<...>().ForEach(...)` 직접 호출 박제.
- 문제:
  - Render thread 가 ECS Query 호출 = sim phase 와 race
  - 서버 권위 시뮬에서는 "Render 는 클라 전용" 이라 ECS 의 일부 컴포넌트 (FxBillboard 등) 만 client-only — render pass 가 server ECS query 호출 시 Component 부재로 crash
  - 결정성 검증 (서버↔클라 sim 비교) 시 render path 가 sim 부수효과 발생 시 결정성 깨짐

**회피 — RenderWorldSnapshot 패턴**:
- ECS Simulation tick 후 `CRenderExtractionSystem` 이 ECS Query → POD `RenderWorldSnapshot` 으로 추출.
- Render Graph 는 snapshot 만 read (POD struct, Query 호출 0).
- 흐름: `ECS Sim → CRenderExtractionSystem → RenderWorldSnapshot → CRenderGraph (Pass read snapshot)`.
- 장점: render thread 가 ECS Query 호출 0 (race 0), 서버 측은 extraction system 미등록으로 자동 제외, 결정성 검증 가능.

**체크리스트**:
- [ ] Render Pass / GPU 코드가 ECS World/Query 직접 호출하는가?
- [ ] 호출하면 RenderWorldSnapshot 패턴으로 추출 후 read-only 로 분리?
- [ ] 서버 빌드에서 자동 제외 가능한 구조?

---

### P-7. 확장 사례 데이터 모델 검증 (Future-Use Data Model)

**증상**: "현재 사례 (Yone 13 submesh) 만 맞는 자료형 (`u32_t mask`) 박제. 본 계획이 의식적으로 언급하는 미래 사례 (엘든링 보스 50+ submesh, 장비 슬롯) 에서는 작거나 부적합."

**13_YONE_v1 사례**:
- §3.1 `MeshGroupVisibilityComponent { u32_t visibilityMask; }` — 32 submesh 한도. Yone 14 OK.
- §6 Layer 3 가 명시적으로 "엘든링 Malenia 날개 / Godrick dragon arm / 보스 변신 stage 의 submesh hide" 거론. 보스 모델은 50~100 submesh.
- 32 bit 로는 그 시나리오 불가능. 본 계획서가 자기 모순.

**회피**:
- 자료형 결정 시 **본 계획서가 자체 거론하는 모든 향후 사례** 의 상한을 검사.
- bitmask 폭은 사용 사례 max + 50% 여유. 엘든링 거론 시 64 bit 또는 `std::array<u64_t, N>`.
- 또는 sparse representation (`std::vector<u32_t> hiddenIndices`) 으로 한도 제거.

**체크리스트**:
- [ ] 본 계획서가 거론하는 향후 사례 (Layer 3 / 후속 Phase) 의 데이터 상한을 검사했는가?
- [ ] 자료형 (mask 폭 / array size / id 길이) 가 그 상한 + 여유를 만족하는가?

---

## §2. 박제 진입 강제 게이트 (Author Gate) — v2 (2026-05-04 확장)

신규 또는 개정 계획서 (`v#`) 박제 시 다음 8 단계를 **순서대로** 통과:

1. **GATE A — 사실 수집 (Read/Grep/Glob/Bash dump)** [P-1, P-2 회피]
   - 인용하는 모든 .h / .cpp / .md 를 Read. 인용 시 줄 번호 + 직접 인용 블록 박제.
   - **PIMPL 클래스** 신규 메서드 박제 시 `.cpp` 의 `struct Impl { ... };` 정의 라인 인용 의무.
   - 의존 binary/text 데이터를 dump (`WintersAssetConverter info` 등).
   - 결과를 §1 Preflight 표에 **실측값** 으로 박제.

2. **GATE B — TODO 0 확인** [P-6 회피]
   - §1 표의 "필요"/"추정"/"TBD" 가 0 인가?
   - 0 이 아니면 GATE A 로 회귀.

3. **GATE C — 호출 경로 grep** [P-3 회피]
   - 변경 대상 함수/멤버의 **모든 호출자** 를 grep.
   - 같은 변경을 받아야 할 N 경로 (main pass / normal pass / shadow pass / depth pre-pass) 모두 박제 본문에 포함.
   - 호출하는 모든 API 가 헤더에 실재함을 grep 검증 [P-13].

4. **GATE D — ECS 책임 경계** [P-4 회피]
   - 박제하는 코드가 Scene/CGameApp 직접 호출 시 ECS 컴포넌트/이벤트로 변환 시도.
   - 변환 후 서버/멀티-Scene/엘든링 재사용성 확인.

5. **GATE E — 향후 사례 자료형 검사** [P-7 회피]
   - 본 계획서가 거론하는 후속 phase / Layer 3 / 다른 게임 (엘든링) 의 데이터 상한 검사.
   - 자료형 (mask 폭 / array 크기) 이 그 상한을 만족.

6. **GATE F — Scheduler / 동시성 모델** ★ 신규 [P-9 회피]
   - 박제하는 phase 가 정수인가? Fractional (0.5/1.5) 박제 금지.
   - 같은 phase 시스템 2개 이상이면 동시 read+write 컴포넌트 0 인가? (ECS Scheduler 가 같은 phase = JobSystem 병렬 실행).
   - Producer→Consumer 의존 시스템이 다른 phase 에 있는가?

7. **GATE G — Owner Scope 매트릭스 일치** ★ 신규 [P-10, P-11 회피]
   - 박제하는 인덱스/매니저의 owner scope 가 PITFALLS §P-10 매트릭스와 일치하는가? (Spatial/Vision/NavGrid → CWorld owned, RHI → Engine 전역, Sound/Input → Tier-1).
   - 도메인 상수 (LoL 맵 크기 등) 가 Engine 공용 클래스에 박혀있지 않은가? (`InitDesc` 주입 또는 게임-specific 클래스 분리).

8. **GATE H — 인용 의미 일치 + 행동 보존** ★ 신규 [P-8, P-14, P-15 회피]
   - 인용한 다른 문서/코드의 의미 ↔ 박제 결론이 **같은 방향** 인가? (정반대면 P-8).
   - 최적화 박제 시 입력/출력 도메인이 기존과 정확히 동일한가? (mask/조건 슬며시 확장 = P-14).
   - 신규 헤더가 사용하는 모든 외부 타입의 정의 헤더를 직접 include 했는가? (P-15).

8 게이트 모두 통과해야 박제 본문 (§3+) 진입.

---

## §3. 검토 (Codex / 사용자) 결과 박제 양식

검토 결과 P# 보정이 들어오면 본 문서의 §1 케이스에 **추가** 박제하고, 해당 계획서를 v## → v(##+1) 로 올려 박제. v1 은 폐기 (`status: deprecated, replaced-by v2`) 명시.

검토 결과 박제 양식:

```markdown
### Codex 검토 N차 (YYYY-MM-DD)

| # | 우선순위 | 라인 | 결함 | 본질 (P#) | 해결 (vN+1) |
|---|---|---|---|---|---|
| 1 | P1 | 607-626 | Yone submesh 이름 매핑 깨짐 | P-1 (데이터 미검증) | manifest + material_hash |
| 2 | P1 | 254-301 | PIMPL 추측 의사코드 | P-2 | .cpp 본체 인용 후 정정 |
| 3 | P2 | 304-352 | Main pass 만 mask | P-3 | RenderNormalPassWithVisibility 동시 박제 |
| 4 | P2 | 1033-1054 | Scene 전역 브리지 | P-4 | YoneSoulRequestComponent + System |
```

---

## §4. CLAUDE.md 통합 위치

CLAUDE.md "★★★ C++ 코드 작성 전 반드시 읽을 것" 리스트 (현재 5 항목) 의 **2번째** 로 본 문서 추가. `winters-skills/code/SKILL.md` 보다 먼저 읽기 — 본 문서가 박제 게이트 조건 박제 (메타) 이고, code SKILL 은 작성 사이클 박제 (실제 작업).

---

**END OF DOCUMENT**
