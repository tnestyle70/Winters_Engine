# 11. EldenRing 클라/에디터/추출 — 면접 대비 세션

> 정직성 기준: `.md/이력서/WINTERS_DOMAIN_HONEST_MAP_2026-06-26.md` "### 11." 섹션.
> 성숙도: **prototype**. 이 문서는 "구현된 것"과 "구현 예정"을 명확히 구분해서 쓴다. 과장하면 면접에서 즉사한다.

---

## 0. 한 줄 본질 + 현재 상태

**한 줄 본질**: 원작 Elden Ring 바이너리(MSB/FLVER/TPF/HKX)를 자체 엔진 런타임 포맷(.wmesh/.wmat/.wskel/.wanim)으로 cook하는 **데이터 주도 에셋 파이프라인**을 만들고, 그 MSB placement를 source of truth로 삼아 자동 로드·검수하는 **DX11 쇼케이스 클라이언트**와 **DX12 ImGui 검수 에디터**를 붙인 도메인이다. 즉 "게임을 만든다"가 아니라 **"상용 게임 데이터를 내 엔진으로 재구성하고 그 정합성을 자동 감사한다"**가 본질이다.

**현재 성숙도 (정직하게)**:
- **working**: 5,000줄+(실측 `elden_pipeline.py` 단일 파일 5,259줄, 드라이버/보조 포함 전체 ~11,900줄) Python cook 파이프라인. end-to-end 동작, Limgrave placement 3,862개 자동 복원(missing wmesh 0).
- **working(빌드되는 exe 2개)**: `WintersElden.exe`(DX11 쇼케이스), `WintersEldenRingEditor.exe`(DX12 ImGui 도킹 에디터). 둘 다 실제로 빌드·실행됨.
- **prototype / planned**: 게임플레이 0(입력 구동 캐릭터 상태머신·전투·lock-on·dodge·stamina 코드 0건). 에디터 3D 뷰포트는 "Preview pending" 텍스트. PBR/보스 HFSM/스트리밍 실루프는 미구현.

**한 문장 방어선**: "저는 Elden Ring을 만든 게 아니라, **상용 게임의 바이너리 에셋을 제 엔진 런타임 계약으로 cook하는 툴 파이프라인을 만들고, 그 결과를 자동 로드·감사하는 검수 쇼케이스/에디터를 붙였습니다.** 게임플레이는 다음 단계입니다."

---

## 1. 핵심 개념 (본질)

### 1-1. 왜 "cook 파이프라인"이 존재하는가 — first principles

상용 게임 에셋은 **런타임이 바로 못 읽는 포맷**으로 저장돼 있다. 이유는 두 가지다:
1. **저작 포맷 ≠ 런타임 포맷**. FromSoft의 FLVER(메시), MSB(맵 배치), TPF(텍스처 팩), HKX(Havok 애니/물리)는 그들의 엔진·툴체인에 종속된 컨테이너다. 내 엔진은 이걸 모른다.
2. **런타임은 "최소·고정 레이아웃"을 원한다**. 파싱·검증·가변 분기는 cook 타임에 끝내고, 런타임은 `mmap`처럼 바로 GPU에 올릴 수 있는 고정 stride 바이너리만 읽어야 빠르다.

그래서 **offline cook**이 존재한다. cook 파이프라인의 본질은 *"느리고 복잡한 변환·검증을 빌드 타임에 1회 수행해서, 런타임을 단순·빠르게 만든다"*는 시공간 트레이드오프다. 언리얼의 `.uasset` cook, 소스 엔진의 `.bsp` compile과 같은 계보다.

내 파이프라인의 변환 사슬:
```
원작 bnd/bhd → WitchyBND unpack → FLVER/MSB/TPF/MATBIN/FXR XML
  → Blender(FLVER→FBX, material auto-bind, armature strip)
  → WintersAssetConverter(FBX→.wmesh/.wskel/.wmat, HKX/FBX→.wanim)
  → MSB placement를 map_placement.txt로 정규화
  → 런타임/에디터가 placement를 source of truth로 자동 로드
```

### 1-2. "데이터 주도(data-driven)"의 의미

placement(무엇을·어디에 놓을지)를 **코드가 아니라 데이터(MSB→txt/JSON)**가 결정한다. 코드는 "placement record를 읽어 spawn하는 일반 루프" 하나만 갖는다. 그래서 새 타일·새 에셋이 들어와도 **C++ 재컴파일 없이** 데이터만 갈아끼우면 된다. 이게 `EldenLimgraveShowcaseScene::SpawnPlacements()`가 디렉터리를 순회하며 `map_placement.txt`를 파싱해 generic하게 spawn하는 구조의 이유다.

### 1-3. "source of truth + 감사 카운터" 개념

복원의 정답지는 **원작 MSB placement**다. 따라서 검증은 "내가 그 placement를 몇 개나 닫았는가(closed)"로 정량화된다. 각 record는 3-상태 closure를 갖는다:
- `Closed`: wmesh 존재 + transform 파싱 성공 + renderer init 성공 → spawn 완료.
- `MissingAsset`: cook된 wmesh가 아직 없음(파이프라인 미완 영역).
- `SpawnFailed`: wmesh는 있는데 transform 깨짐 or 렌더러 init 실패.

이 3-상태를 카운트해 `map_closure_audit.json`(schema 버전드)으로 떨군다. **"몇 % 닫혔나"가 곧 복원 진척도**라는 게 핵심 개념이다. 이건 게임 회사 QA 빌드 파이프라인이 "missing reference / bad transform / collision hole을 자동 리포트"하는 것과 같은 사고방식이다.

### 1-4. RHI 추상화 — 왜 DX11 클라 + DX12 에디터가 공존하나

엔진은 `eEngineRHIBackend{ DX11, DX12, Null }`로 백엔드를 런타임 선택한다(`EldenRingEditor/Private/main.cpp:21-35`). 쇼케이스 클라는 DX11(기존 엔진 렌더 경로 재사용), 에디터는 DX12 기본(`config.rhiBackend = DX12; allowRHIFallback = true`)이다. RHI 추상화의 본질은 **상위 게임/툴 코드가 concrete 그래픽 API를 모르게 만들어, 같은 씬 로직을 두 백엔드에서 돌리는 것**이다. 단 정직하게: 에디터 뷰포트는 아직 3D를 안 그리므로 DX12 경로는 "ImGui 도킹 + 패널"을 그리는 데까지만 실증됐다.

---

## 2. 왜 이 선택인가 — 기술 스택 + Trade-off

### 2-1. 변환 도구 선택

| 선택지 | 장점 | 단점 | 내 결정 이유 |
|---|---|---|---|
| **WitchyBND(외부 CLI) 호출** | FromSoft 포맷 unpack을 검증된 도구에 위임, 직접 BND 파서 안 짜도 됨 | 콘솔 핸들 의존(PromptPlus가 detached에서 즉사), MAX_PATH 초과 트리 생성 | 포맷 리버스 엔지니어링은 내 가치가 아님 → 위임. 대신 호출 안정화(CREATE_NO_WINDOW, `\\?\` 확장 경로)는 내가 책임 |
| 직접 BND/FLVER 파서 작성 | 의존성 0 | 수개월 리버스 엔지니어링, 버그 리스크 | 신입 1인 범위 밖. 핵심 가치(런타임 포맷 cook)에 집중 못 함 |
| **Blender headless(FLVER→FBX, material bind)** | 스킨/머티리얼/메시 변환에 성숙한 import/export, `--factory-startup --background`로 재현성 | 무겁고 느림, armature-only export를 Assimp가 거부 | FBX는 내 `WintersAssetConverter`(Assimp 기반)가 읽을 수 있는 공통 교환 포맷 → 중간 허브로 채택 |

### 2-2. cook은 offline Python, 런타임은 C++

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **offline Python cook + 고정 바이너리 런타임 로드** | 복잡한 변환·retry·audit을 빠르게 반복 작성, 런타임은 단순·빠름 | 런타임-cook 2언어 분리, 계약(layout) 동기화 필요 | **채택**. cook은 1회성·실험적이라 Python 반복속도가 압도적. 런타임은 stride 고정 wmesh만 읽음 |
| runtime JSON 파싱으로 통일 | 단일 경로 | 런타임에 무거운 파싱/검증 부담, 매 프레임 비용 | placement 같은 메타는 txt/JSON 허용하되, **메시 데이터는 cook해서 바이너리**로 — hot path 분리 |

근본 트레이드오프: **offline codegen/cook vs runtime parse**. 변환 비용이 큰 메시/스켈/애니는 cook(offline), 가볍고 자주 바뀌는 placement는 텍스트(runtime parse)로 둬서 양쪽 장점을 취했다.

### 2-3. 에디터 undo/redo — command 패턴 vs 스냅샷

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **command 패턴(Do/Undo 1급 객체)** | 메모리 효율(델타만 저장), 히스토리 라벨링 쉬움, 트랜잭션 단위 명확 | 모든 mutation에 대응 command 작성 필요 | **채택**. placement add/delete/transform 3종만 있으면 충분, 각 command가 역연산 보유 |
| 전체 문서 스냅샷 스택 | 구현 단순 | 큰 cell 문서를 매 편집마다 복제 → 메모리 폭발 | placement 수천 개 cell에선 비현실적 |

`EditorTransaction.h`의 `IEditorCommand{ Do/Undo/Name }` + `CAddPlacementCommand/CDeletePlacementCommand/CTransformPlacementCommand`가 이 결정의 산물이다.

### 2-4. 왜 "게임플레이 말고 검수 diorama"인가 (가장 중요한 방어)

신입 1인 프로젝트에서 "Elden Ring 전투까지 복제"는 비현실적이고, 했다고 말하면 거짓이다. 대신 **"상용 데이터를 내 엔진으로 cook하고 자동 감사하는 툴 체인"**은 (a) 실제 게임 회사 Tools/Pipeline 엔지니어의 핵심 업무이고, (b) 1인이 끝까지 완결 가능하며, (c) 정직하게 "여기까지"라고 선을 그을 수 있다. 그래서 의도적으로 게임플레이를 범위에서 잘라냈다. 이건 약점이 아니라 **범위 관리(scoping) 결정**이다.

---

## 3. 실제 구현 (코드 근거)

### 3-1. 쇼케이스 클라이언트 — placement 자동 로드/spawn

핵심 자료구조 (`EldenLimgraveShowcaseScene.h`):
- `EldenRuntimeMapPlacement`(tile/kind/name/model/wmesh/transform/renderer) 벡터 = 로드된 정적/캐릭터 배치.
- `ShowcaseInstance{ unique_ptr<ModelRenderer>, bCycleAnims, iAnimCount, iAnimIndex, fAnimTimer }` 벡터 = 실제 spawn된 렌더러 인스턴스.
- `EldenMapClosure{ Closed, MissingAsset, SpawnFailed }` enum = 3-상태 감사.

데이터 흐름 (`EldenLimgraveShowcaseScene.cpp`):
1. `SpawnPlacements()` (`:607`) — Maps 루트 하위 타일 디렉터리 순회. focus tile만 로드하는 모드(`m_bLoadOnlyFocusTiles`)로 16타일 전체 vs 3타일 선택. 각 `map_placement.txt`를 `|` 구분 파싱(`SplitPipeLine`), 7필드 미만 skip, kind가 `MapPiece/Asset`(정적) 또는 `Enemy/Npc`(캐릭터)인 것만 처리.
2. `TrySpawnMapPlacement()` (`:562`) — wmesh 경로를 `ResolveRepoRelativePath`로 검사. 없으면 `MissingAsset` 리턴 + `AppendMapSpawnFailure("MissingAsset", ...)`로 실패 로그. transform(pos/rot/scale) 3개 `ParseVec3` 실패 시 `BadTransform`→`SpawnFailed`. 통과하면 `SpawnInstance` 호출.
3. `SpawnInstance()` (`:983`) — `ModelRenderer::Initialize(wmesh)` 시도. 실패 시 카운트+로그(상위 24건만)하고 nullptr. 성공 시 `GetAnimationCount()`로 애니 유무 판단, 있으면 `PlayAnimation(0)`.
4. `WriteMapClosureAudit()` (`:1021`) — `winters.elden.map_closure.v1` 스키마 JSON으로 source/closed/open/missingAsset/spawnFailed 카운트 출력. stage별 경로(`limgrave_map_closure_audit.json`, `starting_cave_map_closure_audit.json`).

애니 순환 (`OnUpdate`, `:1240`):
```cpp
instance.fAnimTimer += deltaTime;
if (instance.fAnimTimer >= kAnimCycleSeconds && instance.iAnimCount > 1) {
    instance.fAnimTimer = 0.f;
    instance.iAnimIndex = (instance.iAnimIndex + 1) % instance.iAnimCount;
    instance.pRenderer->PlayAnimation(instance.iAnimIndex);
}
```
**이게 "게임플레이가 아니라 타이머 순환 재생"의 증거다.** 입력·상태머신 없이 6초마다 다음 클립으로 넘긴다. 면접에서 정직하게 짚을 지점.

### 3-2. cook 파이프라인 (Python)

`elden_pipeline.py`는 argparse 서브커맨드 모음(5,259줄). 핵심 함수:
- `run_process()` (`:2522`) — 모든 외부 도구 호출의 단일 게이트. `subprocess.CREATE_NO_WINDOW`로 **WitchyBND가 콘솔 핸들 없이 죽는 문제를 차단**(gotcha 2026-06-11 반영). timeout/OSError를 구조화된 dict로 반환.
- `run_witchy_unpack()` (`:2711`) — `--unpack --passive --location ... [--recursive]` + 산출 파일 확장자별 카운트/바이트 집계(audit).
- `run_flver_to_fbx_batch()` (`:2736`) — Blender를 `--factory-startup --background --python`로 배치 호출, input/summary JSON으로 통신, stderr tail을 record에 보존.
- `convert_fbx_to_winters_binary()` (`:2787`) — `WintersAssetConverter.exe`로 skel→mesh→anim 순차 cook. **다단 fallback**: skel 성공 못 하면 static mesh로 재시도(`:2823`), 그래도 실패면 Blender normalize 후 전체 재시도(`:2851-2924`, `usedNormalizedRetry`). 각 산출물 존재/바이트/`converter info` audit.
- 드라이버: `run_24h_driver.py`는 **연속 5회 전부실패 시 lane abort**(`:165` `consecutive_all_fail >= 5`) — 망가진 toolchain에서 무한 헛돌이 방지. `run_booster_driver.py`는 normal 드라이버 offset 앞 `guard_slices`(기본 40) 간격을 두고 병렬 워커 stride 분할(`:172`)로 충돌 없이 가속.

보조 스크립트:
- `blender_strip_armature.py` — >512본 캐릭터(Margit 등, 런타임 256본 셰이더 한계 초과)를 메시-only bind-pose FBX로 변환해 정적 표시용 boneless wmesh로 cook. **armature-only export를 Assimp가 거부하는 문제(gotcha 2026-06-11)를 메시 포함으로 우회**.

### 3-3. 에디터 — 문서 + 트랜잭션 + Engine 시스템 호출 검증

- `CWorldCellDocument` (`WorldCellDocument.h`) — `winters.world.cell.v1` 스키마. cellId/area/blockXY/variant/cellSize/origin 메타 + `WorldPlacement` 벡터(id/kind/name/wmesh/transform/animated/transformResolved) + `WorldReference` 벡터. `AllocPlacementId()`로 단조 증가 id 발급, Load/Save JSON.
- `CEditorTransaction` (`EditorTransaction.h/cpp`) — undo/redo 두 스택 + history 라벨. `Push()`(`:62`)가 `Do()` 실행 후 스택 적재. 각 command가 selection 복원까지 책임(`SelectPlacementIfAlive`로 dangling 방지).
- 검증 패널 (`EldenRingEditorScene.cpp`) — **실제 Engine 시스템을 호출**:
  - FX graph: `CFxGraph` 로드→`CFxGraphValidator`(topo order)→`CFxGraphCompiler::Compile`로 emitter별 exec plan 컴파일, compiled 카운트(`:333`).
  - Sequencer: `CSequenceAsset::LoadFromJson`→`Validate`로 track/key/duration 집계.
  - World partition: `Engine::CWorldPartitionSystem::Create`→`LoadWorld`→`SetSource`→`Update(0.f)` 3회→`CollectVisibleInstances`+`GetDebugStats`로 cell 상태/전이/누락 에셋 집계(`:388`).
  - Boss hitbox: `WintersPhysics3D::MakeAABB/MakeOBB/MakeSphere`+`Overlap`로 hurtbox vs slash/shockwave 기하 overlap, active/dodge 프레임 윈도우(`:459`).

**정직한 짚기**: 뷰포트는 `DrawViewportPanel()`(`:508`)이 `"Preview pending"` 텍스트만 출력(`:516`). world partition probe는 `Update(0.f)` 즉 dt=0 headless이고, boss probe는 하드코딩된 기하다. 즉 패널은 "Engine 시스템이 정상 동작하는지 검증하는 harness"지 "3D 편집기"가 아니다.

---

## 4. 검증 — 동작을 어떻게 증명했나

1. **빌드되는 2개 exe**: `WintersElden.exe`, `WintersEldenRingEditor.exe`가 CMake로 빌드·실행. "코드만 있고 안 돈다"가 아님.
2. **map_closure_audit.json (스키마 버전드 감사)**: 쇼케이스가 매 로드마다 source/closed/missingAsset/spawnFailed를 JSON으로 출력. Limgrave 기준 placed 3,862 / **missing placed wmesh 0**(`16_LIMGRAVE_..._RECONSTRUCTION.md` 감사 표). "복원됐다"의 판정 근거가 텍스트 인상이 아니라 **카운터**다.
3. **실패 로그(audit trail)**: `limgrave_spawn_failed_assets.txt`에 reason|tile|kind|... 파이프 포맷으로 실패 사유 기록 → 어떤 모델이 왜 안 됐는지 추적.
4. **cook record audit**: 각 변환이 산출물 존재/바이트/`converter info`(magic/version/bone count)를 record로 남김. converter가 "성공 리턴했지만 빈 파일"인 경우를 byte 검사로 잡음.
5. **드라이버 게이트**: 연속 5 all-fail abort(toolchain 회귀 감지), guard-slice 간격(병렬 충돌 방지).
6. **에디터 probe = 시스템 회귀 검증**: FX compile count, sequence valid/invalid, partition transition count, hitbox overlap bool이 패널에 숫자로 떠서 "Engine 시스템이 깨졌는지"를 시각 확인.

---

## 5. 최적화

**실제로 한 것**:
- **cook 타임 vs 런타임 분리**: 메시/스켈/애니의 무거운 변환·검증을 offline cook으로 빼서 런타임은 고정 stride 바이너리만 로드. 런타임 파싱 비용 제거.
- **focus tile 선택 로드**(`m_bLoadOnlyFocusTiles`): 16타일 전체 대신 3 focus tile만 spawn하는 모드로 검수 시 로드 시간/드로우콜 절감. unresolved 7,353건을 다 spawn 시도하지 않음.
- **병렬 cook 드라이버**: `run_booster_driver.py`가 stride 분할 워커로 cook 처리량 가속(I/O·프로세스 bound 작업이라 멀티프로세스가 유효).
- **실패 로그 cap**: spawn 실패 로그를 상위 24건만 기록(`s_iLoggedFailures < 24`)해 대량 실패 시 I/O 폭주 방지.

**정량 수치**: 이 도메인엔 측정된 FPS/ms 수치 없음 → **"측정 예정"**. (프레임 측정 인프라는 도메인 12에 있고, 쇼케이스 씬은 아직 프로파일러 계측 대상이 아님.) 면접에서 숫자 물으면 "쇼케이스는 드로우콜 배칭/스트리밍 최적화 전 단계라 측정 인프라(도메인 12)를 이 씬에 붙이는 게 다음 작업"이라고 답한다.

**계획 중인 최적화**: §6의 정적 메시 batching/chunk·world partition 실루프 스트리밍이 곧 렌더 최적화 항목이다.

---

## 6. 구현 예정 (Planned) — 같은 깊이로

> 사용자는 이걸 실제로 구현한다. "안 했는데 어떻게 할 거냐"에 막힘없이 답하기 위한 설계.

### 6-1. World Partition 실게임 루프 연결 (현재 dt=0 headless probe만)
- **무엇을**: `CWorldPartitionSystem`을 쇼케이스/에디터의 실제 update 루프에 연결, 카메라 위치를 `StreamingSourceComponent`로 매 프레임 공급해 cell load/unload를 동적 구동.
- **왜**: 현재는 `Update(0.f)` 3회로 "전이 로직이 도는가"만 검증. 실제 스트리밍(가까운 cell만 메모리에)을 못 함 → Limgrave 전체를 한 번에 못 띄움.
- **어떻게**: (1) 카메라 Transform→source.position 바인딩, (2) load/unload/visible 반경 budget 설정, (3) cell당 placement를 `WorldCellDocument`로 분할, (4) async 에셋 스트리밍을 `CAssetStreamingSystem`에 위임(이미 존재). 
- **Trade-off**: 동적 로드는 hitching 위험 → 프레임당 load budget 캡 + prefetch 반경. 정적 일괄 로드보다 코드 복잡.
- **검증**: partition debug stats(transition/missing) 카운터를 매 프레임 오버레이로, hitch를 도메인 12 프로파일러로 측정.

### 6-2. 에디터 3D 뷰포트 ("Preview pending" → 실제 렌더)
- **무엇을**: DX12 백엔드로 cell의 placement를 실제 렌더, 마우스 ray-pick으로 placement 선택, 기즈모로 transform 편집→`CTransformPlacementCommand` 발행.
- **왜**: 현재 JSON/텍스트 편집 수준. 시각 검수 불가.
- **어떻게**: (1) `ImGuiLayer` DX12 백엔드 완성(`01_DX12_IMGUI_EDITOR_BOOTSTRAP.md`의 `DX12BackendState`), (2) 뷰포트를 offscreen RT로 렌더해 ImGui image로 표시, (3) screen→world ray + placement AABB intersection으로 pick, (4) 기즈모 드래그를 transform command로 트랜잭션화.
- **Trade-off**: DX12 descriptor heap/PSO 관리 직접 작성 비용. 단 RHI 추상화가 이미 있어 씬 로직 재사용.
- **검증**: pick한 placement id가 selection과 일치, 기즈모 편집 후 undo/redo로 원복(트랜잭션 골든).

### 6-3. PBR multi-slot material resolver (현재 diffuse 휴리스틱)
- **무엇을**: MATBIN shader parameter를 읽어 normal/mask/emissive/roughness 슬롯을 가진 `.wmat`으로 확장.
- **왜**: 현재 `.wmat`은 diffuse 중심 + 텍스처 role 휴리스틱(suffix 추론). 시각 fidelity 한계.
- **어떻게**: `parse_matbin_xml`이 이미 sampler role을 뽑음 → 이 매핑을 정규화해 multi-slot 바인딩으로 확장, 셰이더에 PBR 입력 추가.
- **Trade-off**: MATBIN 파라미터 의미 리버스 필요 → 우선 normal/AO만, 점진 확장.
- **검증**: cook record에 슬롯별 텍스처 존재 카운트, 에디터 material preview 패널에 slot 상태 표시.

### 6-4. MSB Enemy placement → Runtime Character 자동 연결
- **무엇을**: 현재 캐릭터 12종은 수동 showcase placement. MSB의 Enemy record를 runtime character lookup에 직접 연결.
- **어떻게**: MSB Enemy의 model id→cooked character 디렉터리 매핑 테이블, `c6070`(bone mismatch)·`c4300`(skel 실패) 같은 실패 케이스는 missing cook queue로.
- **검증**: actor closure 카운터(수동 placement 의존 0 목표).

### 6-5. Collision/Nav 변환 (현재 raw 수집만)
- **무엇을**: collision hkxbhd/bdt, navmesh nvmhktbnd를 Winters collision/nav 포맷으로 cook, 에디터 overlay로 visual mesh vs 물리 mesh mismatch 표시.
- **현재**: queue 인식 수준, 미변환.
- **검증**: walkable/collision hole 자동 리포트(게임 회사 QA 파이프라인 방식).

### 6-6. 게임플레이 (가장 멀리 있는 것 — 정직하게)
- 입력 구동 캐릭터 상태머신·lock-on·dodge·stamina·보스 HFSM은 **설계도 코드도 없다**. 한다면: 도메인 17(SAT 충돌 라이브러리)을 telegraph/active/recovery 프레임 윈도우와 묶어 보스 1마리 vertical slice부터. 단 면접에선 "이건 로드맵 맨 끝, 현재 범위 밖"이라고 명확히 선 긋는다.

---

## 7. 면접 예상 질문 & 모범 답변

**Q1. (기본) cook 파이프라인이 왜 필요한가요? 런타임에서 바로 원본을 읽으면 안 되나요?**
A. 저작 포맷(FLVER/MSB/HKX)은 FromSoft 툴체인 종속이고 가변·복잡합니다. 런타임은 고정 stride 바이너리를 바로 GPU에 올려야 빠릅니다. 그래서 느리고 복잡한 변환·검증을 cook 타임에 1회 끝내고 런타임을 단순화하는 시공간 트레이드오프입니다. 메시는 cook(.wmesh), 가볍고 자주 바뀌는 placement는 런타임 텍스트 파싱으로 hot path를 분리했습니다.

**Q2. (기본) "data-driven"이라고 했는데 구체적으로 뭐가 데이터고 뭐가 코드인가요?**
A. 무엇을 어디 놓을지(MSB placement)가 `map_placement.txt`/cell JSON 데이터입니다. 코드는 "placement record 읽어 spawn하는 generic 루프" 하나(`SpawnPlacements`→`TrySpawnMapPlacement`)뿐이라, 새 타일이 와도 C++ 재컴파일 없이 데이터만 갈면 됩니다.

**Q3. (설계) undo/redo를 command 패턴으로 한 이유는? 스냅샷이 더 간단하지 않나요?**
A. cell당 placement가 수천 개라 매 편집마다 문서 전체를 복제하면 메모리가 폭발합니다. command는 델타(역연산)만 들고 있어 효율적이고 히스토리 라벨링도 쉽습니다. add/delete/transform 3종만 있으면 충분해서 command 작성 비용도 작습니다. 각 command가 selection 복원까지 책임지게 해서 undo 후 dangling selection을 막았습니다.

**Q4. (설계) "복원됐다"를 어떻게 증명하나요?**
A. 인상이 아니라 카운터입니다. 매 로드마다 source/closed/missingAsset/spawnFailed를 버전드 스키마 JSON으로 떨굽니다. Limgrave 기준 placed 3,862, missing placed wmesh 0입니다. 실패는 reason|tile|kind 포맷 로그로 추적하고, cook은 산출물 byte와 converter info로 "성공 리턴했지만 빈 파일"까지 잡습니다.

**Q5. (adversarial) 이거 Elden Ring을 만든 거 아니죠? 게임플레이가 있나요?**
A. 맞습니다, 게임플레이는 없습니다. 입력 구동 캐릭터 상태머신·전투·lock-on·dodge 코드는 0건입니다. 제가 만든 건 **상용 게임 바이너리를 제 엔진 런타임 포맷으로 cook하는 툴 파이프라인과, 그 결과를 source-of-truth placement 기준으로 자동 로드·감사하는 검수 쇼케이스/에디터**입니다. 캐릭터 애니는 6초 타이머로 클립을 순환 재생하는 diorama지 상태머신이 아닙니다. 이건 게임 회사의 Tools/Pipeline 엔지니어 업무에 해당하고, 1인이 완결 가능한 범위로 의도적으로 잘랐습니다.

**Q6. (adversarial) 에디터가 "DX12 3D 에디터"라던데, 3D로 뭘 편집하나요?**
A. 정직하게, 뷰포트는 아직 "Preview pending" 텍스트입니다. 3D 편집은 안 됩니다. 현재 에디터의 실체는 (1) cell 문서 JSON + command 트랜잭션 undo/redo, (2) **실제 Engine 시스템을 호출해 검증하는 패널들**입니다 — FX graph를 실제 컴파일하고, sequence를 validate하고, world partition을 LoadWorld 후 transition을 돌리고, 물리 hitbox overlap을 계산합니다. 즉 "3D 편집기"가 아니라 "Engine 시스템 검증 harness"입니다. 3D 뷰포트+ray-pick+기즈모는 DX12 ImGui 백엔드 완성 다음 단계로 설계돼 있습니다.

**Q7. (adversarial) world partition 스트리밍 한다고 했는데, 진짜 게임 루프에서 도나요?**
A. 아니요. 현재는 에디터 probe에서 `Update(0.f)`, 즉 dt=0 headless로 "전이 로직과 누락 에셋 집계가 맞는가"만 검증합니다. 실게임 루프(카메라 위치를 source로 매 프레임 공급해 동적 load/unload)는 미연결입니다. 연결 설계는 명확합니다 — 카메라 Transform을 streaming source에 바인딩하고, 프레임당 load budget을 캡해서 hitch를 막고, cell당 placement를 분할하는 겁니다. 이미 `CAssetStreamingSystem`과 partition system이 있어서 배선 작업이 남았습니다.

**Q8. (adversarial) material이 PBR이라고 할 수 있나요?**
A. 아니요. 현재 `.wmat`은 diffuse 중심이고 텍스처 role은 파일명 suffix 휴리스틱으로 추론합니다. 진짜 PBR(normal/mask/emissive/roughness multi-slot)은 아직입니다. 다만 `parse_matbin_xml`이 이미 sampler role을 뽑고 있어서, 그 매핑을 multi-slot 바인딩으로 정규화하고 셰이더 입력을 늘리는 게 다음 작업입니다. normal/AO부터 점진 확장할 계획입니다.

**Q9. (심화) WitchyBND를 무인으로 돌릴 때 어떤 문제가 있었고 어떻게 풀었나요?**
A. WitchyBND가 PromptPlus로 콘솔 커서를 만지는데, detached/hidden 프로세스엔 콘솔 핸들이 없어서 `set_CursorVisible`에서 즉사했습니다. 모든 외부 호출을 거치는 `run_process`에서 `CREATE_NO_WINDOW`로 숨김 콘솔을 할당해 해결했습니다. 추가로 산출 트리가 MAX_PATH(260)를 넘겨서 복사는 `\\?\` 확장 경로, 삭제는 `rd /s /q` 폴백으로 처리하고, 무인 드라이버엔 연속 5회 all-fail abort 가드를 둬서 toolchain이 깨졌을 때 무한 헛돌이를 막았습니다.

**Q10. (심화) >512본 캐릭터는 어떻게 처리했나요?**
A. 런타임 셰이더 본 한계가 256이라 Margit 같은 초과 캐릭터는 스키닝을 못 합니다. `blender_strip_armature.py`로 armature를 제거하고 world transform을 유지한 채 메시-only bind-pose FBX로 export해서, boneless 정적 wmesh로 cook합니다. 이때 주의할 게, Blender의 armature-only export는 Assimp가 통째로 거부하기 때문에 반드시 메시를 포함해야 합니다. 정적 표시용으로만 쓰고, 본 한계 상향이나 LOD 스키닝은 별도 과제입니다.

**Q11. (심화) cook 파이프라인의 신뢰성(reliability)은 어떻게 보장하나요?**
A. 3겹입니다. (1) 변환 단위마다 다단 fallback — skel 실패 시 static mesh, 그래도 실패면 Blender normalize 후 전체 재시도. (2) 산출물 audit — 존재/byte/converter info(magic/version/bone)로 "성공 리턴했지만 빈 파일"을 잡음. (3) 드라이버 가드 — 연속 all-fail abort와 guard-slice 병렬 분할. 다만 자동화 회귀 테스트(유닛/골든)는 아직 없어서 "검증은 audit JSON + 수동 실행 + 드라이버 가드 수준"이라고 정직하게 말합니다.

**Q12. (확장) 게임 회사 들어가면 이 경험이 어떻게 쓰이나요?**
A. 실제 FromSoft/대형 팀의 Tools 엔지니어가 하는 일이 정확히 이겁니다 — 저작/에디터 바이너리를 런타임 포맷으로 cook하는 importer·validator·commandlet 작성, missing reference·bad transform·collision hole 자동 리포트. 저는 그 사고방식(source of truth + 자동 감사 + 무인 드라이버 안정화)을 1인으로 end-to-end 경험했습니다. 게임플레이는 안 했지만, 데이터 파이프라인과 검수 툴링의 본질은 제대로 잡았다고 봅니다.

---

## 8. 30초 엘리베이터 피치

"저는 Elden Ring을 만든 게 아니라, **상용 게임의 바이너리 에셋을 제 자작 엔진의 런타임 포맷으로 cook하는 5천 줄짜리 데이터 파이프라인**을 만들었습니다. WitchyBND·Blender·자체 컨버터를 무인 드라이버로 묶어서 MAX_PATH·콘솔 핸들·연속 실패 같은 현실 문제를 다 잡았고, 그 결과 MSB placement 3,862개를 missing 0으로 자동 복원했습니다. 그걸 source of truth로 자동 로드해서 closure 카운터로 감사하는 DX11 쇼케이스 클라와, FX 컴파일·world partition·hitbox를 실제 Engine 시스템으로 검증하는 DX12 ImGui 에디터를 붙였습니다. 게임플레이는 아직 없습니다 — 이건 게임 회사 Tools/Pipeline 엔지니어가 하는 일이고, 1인이 끝까지 완결할 수 있는 범위로 의도적으로 잡은 겁니다."
