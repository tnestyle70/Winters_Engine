# Phase B-13 마스터 인덱스 — 시야 + 타워 AI + Spatial Hash + 본격 AI 계층 (v2)

**버전**: **v2 (2026-05-04 — Codex 검토 #2 P1×5+P2×5 정정 반영)**
**v1 폐기**: §0 정정 매트릭스 참조. v1 그대로 반영 시 빌드 차단 (uint32_t phase / subdir include / ModelRenderer API mismatch / VisionComponents 미include) + scheduler race + LoL 행동 변경.
**가이드**: [`.md/process/PLAN_AUTHORING_PITFALLS.md`](../../process/PLAN_AUTHORING_PITFALLS.md) (P-1~P-15, 8 GATE)
**작성일**: 2026-05-04
**범위**: Phase B-13 (게임플레이 시스템 4 신규)
**병행 진행**: **JAX/ANNIE/ASHE/YONE 4 챔프 추가** 는 별도 트랙. CHAMPION_WMESH_PIPELINE_GUIDE 패턴 그대로.

---

## §0. v1 → v2 정정 매트릭스 (Codex 검토 #2 반영)

| # | v1 결함 | 본질 (PITFALLS) | v2 정정 |
|---|---|---|---|
| 1 | **Phase 0.5 / 1.5 fractional 박제** — `uint32_t GetPhase()` 캐스트 시 0 (Transform 과 충돌). 같은 phase 직렬 실행 가정 — 실제 [SystemScheduler.cpp:22-42](../../../Engine/Private/ECS/SystemScheduler.cpp:22) 는 같은 phase = JobSystem 병렬 실행. | **P-9** | **정수 phase 재배정**: TransformSystem=0, **CSpatialHashSystem=1**, MinionAISystem=2, NavigationSystem=3, StatusEffectSystem=4, **CVisionSystem=5**, **CTurretAISystem=6**, **CTurretProjectileSystem=6** (mask 충돌 0 — 다른 컴포넌트 read+write), **CBehaviorTreeSystem=7**, **CMCTSSystem=8**, **CYoneSoulSpawnSystem=9**. 모두 단독 phase — race 0. |
| 2 | **공개 헤더 flat include** (`#include "Entity.h"`) 박제 — AGENTS.md L437-441 의 "subdir 보존" 룰과 정반대 인용 | **P-8** | **공개 헤더 = `#include "ECS/Entity.h"` (subdir 보존)**. 루트 헤더 (`WintersAPI.h` / `WintersTypes.h` / `WintersMath.h` / `IScene.h` / `GameContext.h` / `Engine_Defines.h`) 만 flat 허용. |
| 3 | **CSpatialIndex CEngineApp 전역 + `CGameInstance::Get_SpatialIndex()` Tier-2 게터** — 서버 멀티 GameRoom (Sim-10 v2) / 엘든링 멀티 World 깨짐 | **P-10** | **CWorld owned**. `CWorld::Get_SpatialIndex() -> CSpatialIndex*`. 04 sub-plan §4 의 CEngineApp/CGameInstance 게터 박제 전면 폐기. |
| 4 | **VisionComponents.h 의 `eTeam` 사용 + GameplayComponents.h 미include** — TU 가 두 헤더 직접 include 가정 (fragile) | **P-15** | `VisionComponents.h` 가 `#include "ECS/Components/GameplayComponents.h"` 직접 박제. 또는 `WardComponent::ownerTeam` 을 `uint8_t` 로 (eTeam cast). |
| 5 | **FogOfWarRenderer 가 Engine/Public 헤더에서 `<d3d11.h>` + `ID3D11ShaderResourceView` 직접 노출** — RHI 경계 위반, DX12 확장성 차단 | DLL 경계 (CLAUDE.md ComPtr 룰 변형) | `Engine/Public/Renderer/FogOfWarRenderer.h` 에서 `<d3d11.h>` 제거. `IRHIDevice*` + opaque `RHITextureHandle` (이미 RHI/RHITypes.h 박제됨) 사용. DX11-specific 코드는 .cpp 안. SRV 노출은 `void* Get_NativeSRV()` (Tier-2 ImGui 전용). |
| 6 | **Scene render 패턴 `Render(matWorld)` 가정** — 실제 [ModelRenderer](../../../Engine/Public/Renderer/ModelRenderer.h) 는 `UpdateTransform(matWorld)` + `Render()` 분리. main pass + normal pass 둘 다. | **P-2 변형** | 02 §6 박제: `rc.pRenderer->UpdateTransform(tf.GetWorldMatrix()); rc.pRenderer->UpdateCamera(vp); if (mask) rc.pRenderer->RenderWithVisibility(mask) else rc.pRenderer->Render();`. normal pass loop 도 같은 패턴. |
| 7 | **MinionAI 교체 mask 가 Champion/Turret/JungleMob/Inhibitor/Nexus 까지 확장** — 현재는 enemy minion only. 라인전 동작 변경. | **P-14** | 04 §5 mask = `SpatialMask(eSpatialKind::Minion)` only. 행동 보존. 챔프/타워 어그로 확장은 별도 PR (B-13.5 또는 B-14). |
| 8 | **`m_SystemScheduler.GetSystem<CTurretAISystem>()` 호출** — 실제 `CSystemSchedular` 에 없는 API | **P-13** | **ECS event 패턴** — 03 §4 의 `NotifyChampionAttackedAlly` 를 `TowerAggroNotifyComponent` (1-frame request) 부착 + `CTurretAISystem` 매 Execute 시작에 ForEach 로 소비 + 제거. Scene 측 Cache 불필요. |
| 9 | **CSpatialIndex::CELL_SIZE = 8.f Engine 공용 박힘** — LoL 만 유효 | **P-11** | **GridDesc 주입** — `struct SpatialGridDesc { Vec3 worldOrigin; f32_t cellSize; i32_t halfExtentX; i32_t halfExtentZ; };`. `CSpatialIndex::Initialize(const SpatialGridDesc&)`. Scene 또는 GameRoom 이 LoL 용 desc 주입. |
| 10 | **Cell 계산 `static_cast<int32_t>(world / cell)`** — 음수 영역 0-방향 절삭. -0.5/8 = 0 (실제로는 -1 cell). 셀 경계 1 cell 어긋남. | **P-12** | `inline int32_t CellX(f32_t worldX) const { return static_cast<int32_t>(std::floor((worldX - m_desc.worldOrigin.x) / m_desc.cellSize)); }` |

**v2 진입 게이트** (PITFALLS §2 GATE A~H): 위 10 행 모두 정정 후 진입.

---

## 0. 전체 목표 + 의존 그래프

```
                ┌──────────────────────────────┐
                │  04 SPATIAL HASH (M4S)       │  (인프라 — 02/03/05 가 모두 의존)
                │  CSpatialHashSystem          │
                └──────────────────────────────┘
                                 │
            ┌────────────────────┼────────────────────┐
            ▼                    ▼                    ▼
  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
  │  02 VISION FOW  │  │  03 TOWER ATK   │  │  05 AI BT/MCTS  │
  │  CVisionSystem  │  │  CTurretAI      │  │  CBehaviorTree  │
  │  + BushVolume   │  │  System         │  │  + CMCTSPlanner │
  └─────────────────┘  └─────────────────┘  │  + RL ONNX      │
                                             └─────────────────┘

  (별도 트랙) JAX / ANNIE / ASHE / YONE 4 챔프 추가 — 데이터 + FX stub
              본 Phase 와 의존 X. 진행 사항: CLAUDE.md "현재 진행" 참조.
```

**의존 명시**:
- **04 Spatial Hash 가 베이스** — 02/03/05 모두 거리 쿼리 (`QueryRadius(pos, r)`) 를 호출. 04 미완성 시 02/03/05 의 거리 검색은 임시 O(N²) loop (현재 `MinionAISystem::FindClosestEnemy` 같은 패턴) 로 구현 후 04 합류 시 교체. 단 04 부터 박제하는 것이 비용 50% 절감.
- **05 AI 의 RL Stage 8** 는 ONNX Runtime 외부 의존성 추가. 별도 NuGet/Lib 박제 cycle 필요. 본 Phase 에서는 BT(Stage 1~2) + MCTS(Stage 6) 까지만. RL 은 Phase B-14 로 분리.

---

## 1. Sub-Plan 인덱스

| # | 파일 | 범위 | 신규 클래스 | 신규 컴포넌트 | LOC 추정 |
|---|---|---|---|---|---|
| 02 | [02_VISION_FOG_OF_WAR.md](02_VISION_FOG_OF_WAR.md) | 시야 시스템 + Fog of War + Bush | `CVisionSystem`, `CFogOfWarRenderer` | `VisionSourceComponent`, `VisibilityComponent`, `BushVolumeComponent` | ~1500 |
| 03 | [03_TOWER_ATTACK_SYSTEM.md](03_TOWER_ATTACK_SYSTEM.md) | 타워 공격 AI (LoL 타겟 우선순위 규칙) | `CTurretAISystem` | `TurretAIComponent` (TargetableTag 확장) | ~700 |
| 04 | [04_SPATIAL_HASH.md](04_SPATIAL_HASH.md) | 미니언/챔프/구조물 Spatial Hash + 거리 쿼리 인프라 | `CSpatialHashSystem`, `CSpatialIndex` | `SpatialAgentComponent` | ~900 |
| 05 | [05_AI_BT_MCTS_RL.md](05_AI_BT_MCTS_RL.md) | Behavior Tree + MCTS + RL 골격 | `CBehaviorTree`, `CBTNode`, `CMCTSPlanner`, `CRLBridge` | `BotComponent`, `BlackboardComponent` | ~2000 |

**총 LOC 약 5100** (4 sub-plan 합). 본 Phase 1주 ~ 2주 작업. 4 챔프 추가는 별도 트랙 진행 중 (1 챔프 ≈ 30분, 4 × 30 = 2 시간).

---

## 2. ECS Phase 정수 재배정 (v2)

**v1 의 0.5/1.5 fractional phase 박제 폐기**. `ISystem::GetPhase()` 가 `uint32_t` 라 0.5 → 0 으로 절삭, Transform 과 같은 phase = JobSystem 병렬 race ([SystemScheduler.cpp:22-42](../../../Engine/Private/ECS/SystemScheduler.cpp:22)). 정수 단독 phase 로 재배정:

| Phase | 시스템 | 신/기존 | 의존 (Producer phase) | 같은 phase race 검증 |
|---|---|---|---|---|
| 0 | TransformSystem | 기존 | — | 단독 |
| **1** | **CSpatialHashSystem** | 신규 | Transform(0) | 단독 |
| 2 | MinionAISystem | 기존 | Transform(0), SpatialHash(1) | 단독 (FindClosestEnemy = SpatialIndex.QueryClosest read-only) |
| 3 | NavigationSystem | 기존 | MinionAI(2) — `bPathDirty/vTarget` set→consume | 단독 |
| 4 | StatusEffectSystem | 기존 | NavAgent(3) | 단독 |
| **5** | **CVisionSystem** | 신규 | Transform(0), SpatialHash(1), Status(4) | 단독 (100ms 틱 — dt accum) |
| **6** | **CTurretAISystem** | 신규 | Transform(0), SpatialHash(1), Vision(5) | 단독 (★ 2026-05-04 코드 실측 — 마스터 v2 의 "TurretAI+Projectile 같은 6" 박제 폐기. Codex 안전 분리 — 향후 시스템 추가 시 race 회피) |
| **7** | **CTurretProjectileSystem** | 신규 | TurretAI(6) | 단독 (★ 코드 실측) |
| **8** | **CBehaviorTreeSystem** | 신규 | Vision(5), Turret(6/7) | 단독 (★ 코드 실측 — 마스터 v2 의 7 → 8) |
| **9** | **CYoneSoulSpawnSystem** | 신규 | BT(8) | 단독 (client-only) |
| **10** | **CMCTSSystem** | 신규 | BT(8) — Blackboard `macroGoal` set | 단독 (5초 틱) (★ 코드 실측 — 마스터 v2 의 8 → 10) |
| (외부) | Manager.Tick | — | NavAgent(3) → 위치 적용 | (Scheduler 외부) |

**Transform 갱신 후 SpatialHash rebuild 시점**: Transform = phase 0, SpatialHash = phase 1. NavSystem(3) 이 위치 갱신 후 다음 frame 의 Transform(0) → SpatialHash(1) 에 반영. **1 frame 지연** 허용 — 시야/타워 100ms 틱이라 사용자 체감 무관 (PITFALLS sub-plan 04 §11 C-3).

**Same-phase 병렬 안전 매트릭스** (Phase 6 검증):
- TurretAISystem: write `TurretAIComponent.{currentTarget, attackCooldown, stackCount, ...}`, read TransformComponent + SpatialAgentComponent (RO).
- TurretProjectileSystem: write `TurretProjectileComponent` + `HealthComponent.fCurrent` (피격), read TransformComponent (RO).
- 두 시스템이 **다른 컴포넌트 집합** write → JobSystem 병렬 안전. read 만 공유.

---

## 3. 코딩 컨벤션 재확인 (CLAUDE.md §코딩 컨벤션 준수)

본 Phase 의 모든 신규 파일은 다음 규칙 강제:

1. **Engine 측 신규 클래스**: `CXxxSystem` (C 접두사), `Engine/Public/ECS/Systems/` + `Engine/Private/ECS/Systems/`. 헤더 export = `WINTERS_ENGINE`.
2. **POD 컴포넌트**: C 접두사 금지, `Engine/Public/ECS/Components/GameplayComponents.h` 또는 `VisionComponents.h` (신규) 같은 도메인 헤더에 분리.
3. **Include 컨벤션** (★ v2 정정 — AGENTS.md L437-441 "subdirectory 헤더는 반드시 폴더 경로 포함"): 공개 헤더 안의 include 도 **`#include "ECS/Entity.h"` 폴더 경로 보존**. v1 의 "flat include" 박제는 정반대 (P-8). 루트 헤더 (`WintersAPI.h` / `WintersTypes.h` / `WintersMath.h` / `IScene.h` / `GameContext.h` / `Engine_Defines.h`) 만 flat 허용. EngineSDK/inc 는 `xcopy /S` 로 폴더 구조 그대로 복사 — Client TU 가 `"ECS/Entity.h"` 형태로만 해소 가능 (C1083 회피).
4. **`unique_ptr` 멤버 + `WINTERS_ENGINE` export 클래스**: copy ctor/assign 명시 `= delete`. CLAUDE.md L1010 사례 ([CSystemSchedular.h:14~22](Engine/Public/ECS/SystemScheduler.h:14)).
5. **Tier-1 vs Tier-2 경계**: 시야/타워 AI 는 매 frame 거리 쿼리 다중 호출 — Tier-2 (포인터 캐시 직접 호출). `CGameInstance::Get_SpatialIndex()` 인터페이스 게터 1 줄 추가, Client 는 캐시 후 직접 호출.
6. **신규 vcxproj 등록**: `Engine/Include/Engine.vcxproj` + `Engine.vcxproj.filters` 양쪽 필수. AdditionalIncludeDirectories 에 `Engine/Public/AI` 추가.

---

## 4. 빌드 전제 (Phase 진입 전 1회 체크)

CLAUDE.md "★ 다음 세션 즉시 진입 명령" 패턴 준수:

- [ ] devenv.exe 종료 (vc143.pdb lock 회피)
- [ ] `git checkout -b feature/B13-master`
- [ ] Engine 단독 빌드 1회 — EngineSDK/inc 동기화 확인
- [ ] `MSBuild Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:MultiProcessorCompilation=false /maxcpucount:1 /v:minimal` 통과 확인 후 진입.

---

## 5. 진입 순서 권장

```
1. 04 SPATIAL HASH 박제 (인프라)
   ↓ (CSpatialHashSystem.h/cpp + SpatialAgentComponent + Phase 0.5 등록)
2. 02 VISION FOW 박제 (시각 효과 — 가장 임팩트 큰 빠른 시각화)
   ↓ (Vision + Bush + FogOfWarRenderer + 미니맵 시야)
3. 03 TOWER ATTACK SYSTEM 박제 (게임플레이 — LoL 타겟 우선순위)
   ↓ (CTurretAISystem 의 LoL 5 우선순위 규칙)
4. 05 AI BT/MCTS/RL 박제 (지능 계층)
   ↓ (Stage 1 BT → Stage 6 MCTS → Stage 8 RL bridge skeleton)

(병행) JAX/ANNIE/ASHE/YONE 4 챔프 추가 — 별도 트랙. 본 Phase 와 무관.
```

**왜 04 → 02 → 03 → 05 순?**
- 04 가 02/03/05 의 인프라.
- 02 시야는 가장 시각적 임팩트 → 다음 단계 디버깅 (타워 사거리 표시, BT 시야 노드 등) 의 기반.
- 03 타워 AI 는 02 시야 + 04 SpatialHash 모두 사용.
- 05 AI 는 마지막 — BT 의 ConditionNode 가 02 시야 + 04 거리 쿼리 + 03 타워 위협 평가를 모두 사용.

---

## 6. Codex 보정 사전 명시 (Pre-Mortem)

CLAUDE.md "Codex 보정 박제" 패턴 — 박제 전에 잠재적 문제를 먼저 박제:

### C-1 (시야): 부쉬 진입 시 1프레임 깜빡임
**증상**: 부쉬 안에 들어간 챔프가 1프레임 동안 시야가 비어 있는 채 렌더 → 화면 깜빡.
**원인**: VisionSystem 이 dirty 기반 100ms 틱이라 즉시 갱신 X.
**해결**: `BushVolumeComponent` 진입/이탈 이벤트 발생 시 **즉시 dirty 플래그 + 다음 frame 강제 rebuild** (`m_bForceRebuild = true`).

### C-2 (타워): Outer 타워 살아있을 때 Inner 공격 가능 버그
**증상**: 미니언이 Outer 타워 우회해 Inner 까지 진격 → Inner 가 즉시 공격해야 하는데 미공격.
**원인**: TargetableTag 가 모든 타워에 박혀있고 별도 활성/비활성 로직 없음.
**해결**: `CStructure_Manager::UpdateTargetable()` 매 100ms 호출. Outer 살아있는 lane 의 Inner/Inhib/Nexus 타워는 **TargetableTag 제거 (적 측 시각에서)**. LoL 정식 규칙.

### C-3 (Spatial): cell 경계 stale entity
**증상**: 미니언이 cell 경계를 빠른 속도로 넘을 때 1 frame 동안 잘못된 셀에 있어 검색 누락.
**원인**: SpatialHashSystem 이 frame 시작에 빌드 후 frame 동안 갱신 X. NavSystem 이 위치 set 한 후엔 stale.
**해결**: SpatialHash 가 Phase 0.5 (Transform 직후) 에 빌드. NavSystem(2) 후 위치 갱신은 다음 frame 의 Phase 0 에서 반영. 이게 LoL/Dota 통상 패턴 (1 frame 지연 허용). 단 시야/타워는 100ms 틱이라 frame 지연 무관.

### C-4 (BT): Tick 호출 시 노드 트리 deep recursion → 스택 오버플로
**증상**: BT 트리 깊이 50 이상 시 stack overflow.
**원인**: BT 의 자연스러운 재귀 호출 패턴 (`Selector::Tick` → `Sequence::Tick` → ... ).
**해결**: BT 트리 depth 상한 32 강제 (`assert(depth < 32)`). 챔프 BT 는 depth 5~8 이 일반적 (Game AI Pro 권장).

### C-5 (MCTS): Rollout 중 World 변경
**증상**: MCTS Rollout 시 가상 시뮬레이션 도중 다른 시스템이 실제 World 변경 → race.
**원인**: MCTS 가 World snapshot 안 뜨고 직접 ECS 컴포넌트 수정 시도.
**해결**: `WorldSnapshot` 구조체 (위치/HP/쿨다운 deep copy) → MCTSPlanner 는 snapshot 만 mutate. 실제 World 는 read-only.

### C-6 (RL): 모델 로드 실패 시 BT 폴백
**증상**: ONNX 모델 파일 없거나 로드 실패 시 봇이 멈춤.
**원인**: CRLBridge 가 nullptr 반환 시 BotComponent 가 행동 결정 못 함.
**해결**: `BotComponent::useRL = false` 시 BT 만 사용. RL 모델 로드 실패 = false 강제 폴백.

### C-7 (Vision): Fog of War 텍스처 메모리
**증상**: 1024×1024 RGBA fog texture 업데이트 매 frame 시 GPU 대역폭 소모.
**원인**: CPU 가 매 frame fog buffer 생성 → upload.
**해결**: **R8 단일 채널 텍스처** (256×256) — 가시성만 0/255. 메모리 64KB. 매 프레임 X, 100ms 틱.

---

## 7. /code skill 적용 명세 (CLAUDE.md `winters-skills/code/SKILL.md` 강제)

본 Phase 진입 시 매 sub-plan 박제 전에 다음 6단계 강제:

1. **기존 인프라 식별** — `Engine/Public/`, `Engine/Public/Manager/`, `Engine/Public/Renderer/`, `Engine/Public/ECS/` 4 폴더 grep 전수. 시야/Spatial/AI 키워드 (이미 위 보고서에서 "Vision/Fog/FOW 0 hit" 확인) 재검증.
2. **데이터 형태 정의** — POD 컴포넌트 구조체 먼저. 시스템은 컴포넌트 정의 후.
3. **DLL 경계 보수** — 신규 클래스에 `WINTERS_ENGINE` 마크 + 공개 헤더 flat include + ComPtr 완전 수식 (`Microsoft::WRL::ComPtr` — Vision FOW 텍스처).
4. **검증 결정 포인트** — 각 시스템 별 시각적/로그 검증 수단 박제 (시야: ImGui 디버그 오버레이, 타워 AI: 화살표 → 타깃 시각화, BT: ImGui Tree).
5. **최소 수정** — 기존 시스템에 손대는 것 최소화. NavigationSystem, MinionAISystem 등 변경 없음.
6. **엣지 케이스** — 위 C-1~C-7 사전 박제.

---

## 8. 다음 세션 진입 명령

```
"Phase B-13 04 SPATIAL HASH 부터 시작.
.md/plan/B13/04_SPATIAL_HASH.md §1 SpatialAgentComponent 박제
→ §2 CSpatialHashSystem.h/cpp 박제 → §3 vcxproj 등록
→ §4 MinionAISystem 의 FindClosestEnemy O(N²) 를 SpatialIndex.QueryRadius() 로 교체
→ §5 빌드 검증.
합격 시 01 → 02 → 03 → 05 순으로 진입."
```

진입 직전 체크리스트:
- [ ] devenv.exe 종료
- [ ] `git checkout -b feature/B13-04-spatial-hash`
- [ ] Engine 단독 빌드 1회 → SDK 동기화 확인
- [ ] CLAUDE.md "EngineSDK/inc 직접 수정 금지" (L1003) 재상기

---

## 9. Phase B-13 완료 정의 (Definition of Done)

- [ ] **02** — 부쉬 안에 들어간 챔프는 외부에서 안 보임. 부쉬 밖에서는 시야 16m 정상. 미니맵에 fog of war 표시.
- [ ] **03** — 미니언이 타워 사거리 진입 시 타워가 LoL 우선순위 (미니언 → 챔프, 챔프 공격 시 챔프 우선) 로 공격. 1 sec 마다 1 발. Outer 살아있을 때 Inner 무적 (TargetableTag 제거).
- [ ] **04** — `CSpatialIndex::QueryRadius(pos, r)` 가 ECS 외부에서 호출 가능, MinionAI 가 이를 사용해 O(N) → O(log N + K). Profiler 로 변화 측정.
- [ ] **05** — 봇 챔프 1 개에 BT 적용 (Idle / Approach / Attack / Retreat 4 노드), MCTS 는 Rollout 시뮬레이션 1 회 실행 가능 (테스트 코드만), RL Bridge 는 헤더 + 빈 .cpp (Phase B-14 에서 구현).
- [ ] **(별도)** JAX/ANNIE/ASHE/YONE 4 챔프 — 별도 트랙. CLAUDE.md "현재 진행" 추적.

---

## 10. 부록 — 마스터 BUDGET

| Sub-Plan | 박제 시간 | 빌드 통과 | 시각 검증 | 합계 |
|---|---|---|---|---|
| 04 Spatial Hash | 4h | 1h | 30m | 5.5h |
| 02 Vision FoW | 8h | 2h | 1h | 11h |
| 03 Tower Attack | 4h | 1h | 30m | 5.5h |
| 05 AI BT/MCTS/RL | 12h | 2h | 1h | 15h |
| **총** | **28h** | **6h** | **3h** | **37h** |

**1 주 (40h) 안전 범위**. RL 골격까지 포함한 추정. RL 학습/모델 작성은 Phase B-14 별도 사이클. 4 챔프 추가는 별도 트랙 (~4.5h).

---

**END OF MASTER INDEX**
