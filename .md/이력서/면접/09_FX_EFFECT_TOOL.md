# 09. FX / Effect Tool (Niagara 등가) — 면접 대비 세션

> 그라운드 트루스: `.md/이력서/WINTERS_DOMAIN_HONEST_MAP_2026-06-26.md` §9 (상태: **working**).
> 정직선: **production은 WFX 큐 런타임 · 서버 cue 경로 · ImGui 에디터**, **노드그래프 / VM / GPU compute는 PoC · 로드맵**.
> 이 문서의 모든 코드 근거는 실제 파일:라인을 인용한다. 과장은 코드리뷰에서 즉사하므로, "여기까지 구현 / 여기부터 계획"의 경계를 본문 안에서 명시한다.

---

## 0. 한 줄 본질 + 현재 상태

**한 줄 본질**: FX 도메인은 "스킬이 발동했을 때 화면에 무엇이 보이는가"를 **C++ 재컴파일 없이 JSON 데이터로 정의**하고, 그 데이터를 **서버가 보낸 권위 이벤트(projectile spawn/hit/attached)에 cue 이름으로 매핑**해 클라이언트 ECS에 멀티-에미터 비주얼로 스폰하는 시스템이다. 본질적으로 "게임플레이 truth(서버)"와 "비주얼 표현(클라 데이터)"의 경계를 긋는 일이다.

**현재 성숙도(정직하게 혼재)**:

| 경로 | 상태 | 근거 |
|---|---|---|
| WFX(JSON) 에셋 → ECS 멀티에미터 스폰 런타임 | **production** | `Data/LoL/FX`에 **112개 `.wfx`, 13챔피언, 452 emitter 블록** 실재. `FxCuePlayer::PlayAll`이 매치마다 구동 |
| 서버 권위 이벤트 → cue 1회 재생 | **working** | `EventApplier.cpp:629/722/737/969`에서 replicated event → `CFxCuePlayer::Play` |
| Billboard/Beam/Ribbon/Mesh/Decal/ShockwaveRing 렌더 | **working** | `eFxRenderType` 6종, `FxSystem`/`FxBeamSystem`/`FxMeshSystem` 분기 |
| ImGui WFX load/save 에디터 패널 | **working** | `WfxEffectToolPanel.cpp`, `WfxDocument.cpp` round-trip |
| 노드그래프 + SoA 파티클풀 + 위상정렬 컴파일러 | **prototype** | `FxGraph`/`FxExecPlan`/`ParticlePool` 구현됐으나 **호출부는 EldenRing 에디터 1곳뿐**, LoL 런타임 미통합 |
| Expression VM / GPU compute | **planned-only** | `FxNodeDesc::bytecodeBlob` 필드만 존재, .cpp 0줄 |

핵심 메시지: **"production 경로(WFX 큐 런타임)는 매치마다 실제로 돈다. 노드그래프/SoA풀은 컴파일·동작하지만 Elden 에디터 격리 PoC다. 이 경계를 내가 직접 긋고 있다."**

---

## 1. 핵심 개념 (본질)

### 1.1 파티클 시스템의 1차 원리 — 왜 "데이터 흐름"인가

파티클 시스템은 Reeves(1983, "Particle Systems")가 정립한, **수명을 가진 다수의 단순 입자**로 퍼지(fuzzy) 객체(불, 연기, 마법진)를 근사하는 기법이다. 입자 하나의 생애는 `Spawn → Init → Update(매 틱) → Render → Kill`의 고정 파이프라인이다.

1세대(UE3 Cascade, Unity Shuriken)는 이 파이프라인을 **엔진에 하드코딩**하고 아티스트는 정해진 모듈만 끼웠다. Niagara / Unity VFX Graph / FromSoft FXR이 바꾼 핵심은 **"데이터 흐름 자체를 아티스트가 정의"**한다는 점이다:

- **노드 = 순수 함수** (input → output)
- **엣지 = 데이터 의존성** (누가 누구의 출력을 소비)
- **그래프 = DAG** → **위상 정렬(topological sort)**로 실행 순서 결정
- **컴파일 단계** 존재 → 그래프를 바이트코드/HLSL로 변환

즉 이펙트가 **작은 프로그램**이 된다. 엔진을 수정하지 않고 새 시뮬레이션을 합성할 수 있다. 이게 "Niagara 등가"가 목표하는 본질이다.

> **내 경계**: 위 "그래프 = 작은 프로그램" 모델은 내 코드에 **PoC 수준으로 실재**한다(`FxGraph` + `FxGraphValidator`의 Kahn 위상정렬 + `FxExecPlan` 컴파일러). 하지만 **LoL production은 이 모델을 안 쓴다.** Production은 한 단계 낮은 **"플랫 에셋(WFX)" 모델** — 그래프 대신 emitter 파라미터 테이블을 직접 채운다.

### 1.2 SoA(Structure-of-Arrays)와 cache locality

입자 N개를 `struct Particle { Vec3 pos; Vec3 vel; ... }`의 배열(AoS)로 두면, `Update`에서 position만 만질 때도 velocity/color가 같은 캐시라인에 끌려와 캐시 효율이 떨어진다. SoA는 `pos[]`, `vel[]`, `color[]`를 **컬럼별 연속 배열**로 둬서, 한 패스가 만지는 컬럼만 스트리밍한다 → SIMD/캐시 친화. Niagara의 `FNiagaraDataSet`이 Float/Int 버퍼를 따로 할당하는 이유다.

> **내 경계**: `CParticlePool`(`Engine/Public/FX/ParticlePool.h:58-66`)이 정확히 SoA다 — `m_vecPos`, `m_vecVelocity`, `m_vecColor`, `m_vecLifetime`, `m_vecAge`, `m_vecSize`, `m_vecUV` + custom 컬럼. **swap-back kill**(`KillSwapBack`)로 죽은 입자를 끝과 swap해 dense 유지. **그러나 이 풀도 PoC 경로다.** Production WFX 런타임은 입자 단위 SoA를 안 돌리고, **emitter = ECS 엔티티 1개**(`FxBillboardComponent` 등)로 표현해 셰이더가 atlas/UV scroll로 "파티클처럼" 보이게 한다.

### 1.3 서버 권위 게임에서 FX의 결정성 문제 (MOBA 특화)

일반 VFX 엔진과 MOBA의 결정적 차이: **"판정에 영향을 주는 FX"와 "순수 시각 FX"를 분리**해야 한다. Niagara는 결정성이 약해 경쟁 슈터들이 고생했다. LoL류에서 스킬 히트박스는 **서버가 결정론적으로 판정**하고, 클라 FX는 그 결과를 받아 그리기만 한다. 즉 FX는 **"비결정적이어도 무방한 시각 표현"**으로 격리되고, AoE 링의 반지름 같은 "보이는 판정 범위"만 서버 truth와 일치시키면 된다.

> **내 구현이 이 원칙을 지키는 방식**: 클라가 FX를 임의로 못 만든다. 서버가 `ReplicatedEvent`(projectile spawn/hit/attached)를 보내면 `EventApplier`가 **visual catalog**를 통해 cue 이름으로 변환하고 `FxCuePlayer::Play`를 1회 호출한다. FX 스폰 타이밍의 source-of-truth가 서버라서, 10인 한타에서도 모든 클라가 같은 이벤트로 같은 cue를 본다.

### 1.4 빌보드 / 빔 / 리본의 기하학

- **Billboard**: 카메라를 향하는 quad. 카메라의 (Right, Up) 축으로 quad를 회전시켜 항상 정면을 보게 한다.
- **Beam**: 시작점-끝점 두 anchor를 잇는 stretched quad(레이저/사슬).
- **Ribbon/Trail**: 시간축으로 샘플된 점들의 연속 띠(검 궤적). history 샘플링으로 꼬리를 만든다.

이 세 가지가 LoL 스킬 비주얼의 90%를 커버한다(투사체=billboard, 빔스킬=beam, 검궤적=ribbon, AoE=GroundDecal/ShockwaveRing).

---

## 2. 왜 이 선택인가 — 기술 스택 선택 이유 + Trade-off

### 2.1 핵심 결정: 노드그래프 먼저가 아니라 "플랫 WFX 에셋" 먼저

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **(택함) WFX = 플랫 JSON 에셋** (emitter 파라미터 직렬화) | git diff/merge 가능, 디자이너가 텍스트로 편집, 재컴파일 0, 1주 내 동작 | 표현력 한계(노드 합성 불가), 절차적 시뮬(curl noise 등) 못 함 | **신입 1인 범위에서 "보이는 결과물"을 먼저 확보**. 노드그래프는 표현력은 높지만 컴파일러+VM+에디터가 다 필요해 수개월. LoL Q/W/E/R 이펙트는 플랫 파라미터로 충분 |
| 노드그래프 + 컴파일러 first | Niagara 등가 표현력, 절차적 합성 | 컴파일러+VM+에디터 풀스택 필요, 검증 비용 큼 | **PoC로만 구현**해 DAG/위상정렬을 "이해했다"는 근거를 남기고, production은 보류 |
| C++ 하드코딩 FxPresets | 가장 빠름 | 챔프 늘면 texture/size/lifetime이 코드에 퍼져 수정=rebuild | **마이그레이션 대상**. `14_CHAMPION_FX_DATA_PIPELINE_DIRECTION.md`가 명시: FxPresets를 thin adapter로 줄이고 수치를 .wfx로 뺀다 |

근본 트레이드오프: **표현력(노드그래프) vs 출하 속도(플랫 에셋)**. 1인 프로젝트에서 "동작하는 13챔피언 112이펙트"가 "동작 안 하는 풀스택 Niagara 클론"보다 면접 가치가 높다. 그리고 노드그래프는 *버린 게 아니라* PoC로 박제해 두고 로드맵(`17_NIAGARA_FULL_REWRITE_MASTER.md`)으로 분리했다.

### 2.2 결정: emitter = ECS 엔티티 (입자 단위 SoA 풀이 아님, production 경로)

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **(택함) emitter 1개 = ECS 컴포넌트 1개** (`FxBillboardComponent`) | 기존 ECS Update/Render 루프 재사용, anchor 추적/lifetime/atlas가 컴포넌트 필드, 구현 단순 | 진짜 만 단위 파티클 불가, "파티클"이 셰이더 트릭(atlas/UV scroll) | LoL FX는 대부분 소수의 quad. 셰이더 atlas로 충분. ECS와 자연스럽게 통합 |
| 입자 단위 SoA 풀을 LoL 런타임에 통합 | 진짜 파티클, 대량 입자 | 별도 tick/render 경로, 셰이더 인스턴싱 필요, 통합 비용 | **PoC(`CParticlePool`)로만**. 엘든링 마법진/대량 궤적에서 필요해지면 통합 |

### 2.3 결정: 서버 권위 cue 매핑 (클라가 FX 못 만듦)

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **(택함) 서버 이벤트 → cue 이름 → 클라 1회 재생** | 멀티플레이 일관성, 클라 임의 FX 차단, 판정/시각 분리 | 서버가 cue 타이밍 소유(약간의 결합) | MOBA 권위 모델과 정합. `EventApplier`가 cue dispatch |
| 클라가 입력 시점에 즉시 FX | 저지연 | 다른 클라와 불일치, 핵 시각화 가능 | 권위 위반이라 배제 |

### 2.4 결정: 수기 string 파서 (JSON 라이브러리 미사용)

| 선택지 | 장점 | 단점 | 내 결정 |
|---|---|---|---|
| **(택함) 수기 `find`/`substr` 추출기** | 외부 의존 0, 빌드 단순 | 견고성 약함(중첩/순서 민감), 엣지케이스 취약 | MVP 속도 우선. **이게 약점인 걸 안다** — `17_...MASTER §0.1-5`가 "structured JSON reader/writer로 교체"를 EFX-1로 명시 |

이건 방어가 아니라 **자인하는 약점**이다(§7 Q8 참조).

---

## 3. 실제 구현 (코드 근거)

### 3.1 데이터 모델 — `FxAsset` / `FxEmitterDesc` (production)

`Engine/Public/FX/FxAsset.h`:

- `FxAsset`(L201-207): `strName` + `std::vector<FxEmitterDesc> emitters` + `CFxParameterMap initialUserParams`. **이게 WFX 한 파일의 in-memory 형태**.
- `FxEmitterDesc`(L80-156): emitter 하나의 전 파라미터. `renderType`(6종 enum L19-27), texture/model 경로, anchor(`FxAnchorDesc`), lifecycle(`FxLifecycleDesc`), velocity/scale/rotation, atlas(cols/rows/frameCount/fps/loop), UV scroll, blend/depth, 그리고 stylized 머티리얼 파라미터(`vStyleColorA/B`, `vRimColor`, `vMagicScrollA/Shape/Core`).
- `CFxAssetRegistry`(L218-256): **generation handle 기반 슬롯 테이블**. `FxAssetHandle = RHIHandle<FxAssetTag>`(index+generation). `RegisterOrReplaceByName`이 hot-replace 시 generation을 올려(L619-624) 옛 핸들을 무효화 → use-after-reload 방지. RHI/ECS와 동일한 핸들 규약을 FX에도 일관 적용.

### 3.2 WFX 로더 — `LoadFxAssetFromFile` / `ParseWfxJson` (production, 약점 인지)

`Engine/Private/FX/FxAsset.cpp`:

- `ParseWfxJson`(L388-563): emitter 배열을 **중괄호 depth 카운팅**(`ExtractEmitterBlocks` L278-315)으로 잘라 블록별로 파싱. string 인식 상태머신으로 따옴표 안의 `{}`는 무시(`ExtractObjectBlock` L219-276) — 즉 anchor/lifecycle 중첩 객체는 제대로 처리한다.
- 추출기 `ExtractString/Number/UInt/Bool/Vec2/Vec3/Vec4`(L52-217): `"key"` 찾고 `:` 다음 값을 `,}\r\n`까지 잘라 `stof`. **snake_case와 camelCase 둘 다 시도**(L459-491)해 옛 에셋 호환.
- `LoadDirectory`(L692-713): `recursive_directory_iterator`로 `.wfx` 전부 로드 → 부팅 시 `Data/LoL/FX` 일괄 등록(`FxCuePlayer.cpp:180-192 PreloadDefaultCueDirectoriesOnce`).
- **약점(자인)**: 정규 JSON 파서가 아니라 first-match `find`. 키가 두 번 나오거나 배열 안 객체에 같은 키가 있으면 오인 가능. cue 누락 시 C++ 하드코드 fallback billboard 잔존(`EventApplier.cpp:973 SpawnBillboard`).

### 3.3 cue 재생 — `FxCuePlayer::PlayAll` (production 핵심 호출 경로)

`Client/Private/GameObject/FX/FxCuePlayer.cpp`:

- `FindCue`(L391-403): registry에서 cue 이름으로 핸들 조회, 없으면 디렉토리 lazy preload 후 재조회.
- `PlayAll`(L410-474): asset의 **emitter 배열을 순회**하며 `renderType`에 따라 분기 스폰:
  - Billboard/GroundDecal/ShockwaveRing → `CFxSystem::Spawn`(L432) via `BuildCueBillboard`
  - Beam → `CFxBeamSystem::Spawn`(L438) via `BuildCueBeam`
  - Ribbon → `CFxBeamSystem::Spawn`(L444) via `BuildCueRibbon`
  - MeshParticle → `CFxMeshSystem::Spawn`(L452) via `BuildCueMesh`
  - 미지원 타입 → `LogSkippedCueEmitter`(L459)로 카운트(누락 추적)
- `FxCueContext`로 호출자 override 주입: `vWorldPos`, `vForward`, `attachTo`(엔티티 부착), `bOverrideEndWorldPos`(빔 끝점=타겟), size/lifetime/velocity override. `ResolveCueAnchorWorldPos`(L147-163)/`ResolveCueEndWorldPos`(L165-178)가 forward 기준 right-vector로 로컬 오프셋을 월드로 변환.

### 3.4 서버 권위 연결 — `EventApplier` (working)

`Client/Private/Network/Client/EventApplier.cpp`:
- L629: projectile spawn event → `visual.pszSpawnCue` → `PlayAll`
- L722: hit event → `visual.pszHitCue` → `Play`
- L737: attached event → `visual.pszAttachedCue` → `Play`(엔티티 부착)
- L969: 명명 챔피언 cue 직접 재생, 실패 시 L973 `SpawnBillboard` fallback

즉 **클라는 "무엇을 그릴지" 결정권이 없다.** 서버 이벤트가 cue를 고르고, 클라는 visual catalog로 cue→.wfx asset을 찾아 그리기만 한다.

### 3.5 빌보드 Update/Render — `CFxSystem` (production)

`Client/Private/GameObject/FX/FxSystem.cpp`:
- `Update`(L218-268): `ForEach<FxBillboardComponent>`로 startDelay 차감 → elapsed 누적 → **anchor 재해석**(`FxAnchor::TryResolveWorldPosition`, 부착 추적) 실패 시 velocity 적분 → lifetime 만료 시 삭제 수집.
- `Render`(L270-459): 카메라 거리/방향 컬링(`IsFxRenderRelevant` L35-61, near/far + forward dot), fade-in/out 알파, **GroundDecal grow / ShockwaveRing 반지름 보간**(L327-342), **atlas frame → UV rect**(L344-365), UV scroll, tint*fade 합성 → billboard quad 월드행렬(L385-406, 카메라 Right/Up 축) → `m_pPlane->Render` (DX11) 또는 `m_pRHISprite->Draw` (DX12). `WINTERS_PROFILE_COUNT("Fx::Drawn"/"Fx::CullSkipped")` 계측(L457-458).

### 3.6 노드그래프 컴파일러 — `FxGraph`/`FxGraphValidator`/`FxExecPlan` (**PoC, Elden 에디터 전용**)

이건 **정직성 지도가 "prototype"으로 명시한 경로**다. 구현은 진짜다:

- `FxGraph.h`: `eFxNodeType`(14종 L21-37: SpawnBurst/SpawnRate/Init*/Age/Gravity/Drag/*Renderer), `eFxNodeStage`(Spawn/Init/Update/Render), `FxGraphNode`(id+type+params variant map+curve), `FxGraphEdge`. `FxStageFromNodeType`(L134-153)이 노드→스테이지 매핑.
- `FxGraphValidator.cpp:50-151`: **Kahn 알고리즘 위상정렬**. indegree 계산 → ready 큐 → 순서 누적. 검증: node_id 중복/0, **stage 역행 엣지**(L99-103 fromStage>toStage), render 노드 개수(정확히 1), **사이클 탐지**(L138 topoOrder.size != nodeCount). 에러 시 topoOrder clear.
- `FxExecPlan.cpp:332-397 Compile`: 검증 통과 후 위상 순서대로 노드를 **`std::function` step**으로 컴파일(`AppendCompiledNodeStep` L156-284). SpawnBurst→`pool.Allocate`, Gravity→velocity 적분, Drag→감쇠 등. render 노드는 `ApplyRenderNodeDefaults`로 plan의 renderType/material 설정. render 노드 없으면 `missing_render_node` 거부.
- `CParticlePool`(SoA, §1.2)이 이 step들의 대상.

**결정적 사실**: `FxGraphCompiler::Compile`/`CFxGraph::LoadFromJson`의 **유일한 호출처는 `EldenRingEditor/Private/EldenRingEditorScene.cpp`**(grep 확인). **LoL 런타임(`FxCuePlayer`/`FxSystem`)은 이 컴파일러를 호출하지 않는다.** 그래서 "노드그래프로 LoL 이펙트를 만든다"는 표현은 **금지**다.

### 3.7 ImGui 에디터 — `WfxEffectToolPanel` / `WfxDocument` (working)

`Client/Private/GameObject/FX/WfxDocument.cpp`: `LoadFromFile`/`SaveToFile`(L371-405)가 WFX **round-trip**. `SaveWfxAsset`(L342-368)이 emitter 전 필드를 JSON으로 직렬화(precision 4). `WfxEffectToolPanel.cpp`가 ImGui로 로드/세이브/카탈로그 UI 제공.

---

## 4. 검증 — 동작을 어떻게 증명했나

정직성 지도에 명시된 대로 **자동 골든/스모크 테스트는 FX 도메인에 없다.** 검증은 다음으로 한정한다(정직하게):

1. **데이터 무결성 (정량 가능)**: `Data/LoL/FX`에 **112개 `.wfx`, 13챔피언 디렉토리, 452 emitter 블록**이 실재. `LoadDirectory`가 부팅 시 전부 로드되고 `GetAssetCount`로 등록 수 확인. cue 누락은 `LogMissingCue`(`FxCuePlayer.cpp:102`), emitter 스킵은 `LogSkippedCueEmitter`(L108)로 추적 → "전부 로드됐는가"를 카운터로 판정.
2. **프로파일러 계측**: `Fx::Update`/`Fx::Render` 스코프(`FxSystem.cpp:220/272`) + `Fx::Drawn`/`Fx::CullSkipped` 카운터(L457-458). 매 프레임 ImGui 오버레이/JSON 캡처로 "이펙트가 몇 개 그려지고 몇 개 컬링되는가"를 본다. → "FX가 실제로 발화하는가"의 직접 증거.
3. **WFX round-trip**: `WfxDocument` load→save→load로 직렬화 안정성 확인(에디터에서 수동).
4. **그래프 검증 게이트(PoC)**: `FxGraphValidator`가 사이클/stage 역행/render 노드 수를 **컴파일 전에 거부**. 잘못된 그래프는 exec plan을 못 만든다. (단 이건 Elden 에디터 경로.)
5. **인게임 시각 확인**: F5 인게임에서 13챔피언 스킬을 눌러 cue가 뜨는지 수동 확인.

판정 기준: "112 에셋이 0 missing으로 로드 + `Fx::Drawn`이 스킬 발동 시 증가 + 미스큐 로그 없음". **"골든 테스트로 픽셀을 비교했다"는 주장은 하지 않는다.**

---

## 5. 최적화

### 5.1 실제로 한 것

- **거리/방향 컬링**(`FxSystem.cpp:35-61`): near(8m)면 무조건 그리고, far(70m) 초과면 버리고, 그 사이는 카메라 forward와의 dot이 -0.25 미만(뒤쪽)이면 컬링. → 화면 밖/뒤쪽 FX 드로우 제거. `Fx::CullSkipped` 카운터로 효과 관측.
- **텍스처 캐시**(L461-497): `m_TextureCache`(DX11) / `m_RHITextureCache`(DX12)로 경로별 1회 로드. 매 프레임 재로드 방지.
- **SoA swap-back kill**(PoC, `ParticlePool`): dense 배열 유지로 죽은 입자 순회 비용 제거.
- **generation handle**: asset hot-replace 시 옛 핸들 무효화로 dangling 방지(성능보다 안전성).

### 5.2 정량 수치

정직성 지도 원칙상 **FX 단독 정량 수치(예: "X% 빨라짐")는 측정 기록이 없다 → "측정 예정"**. 다만 엔진 전역 측정(`12. 성능 최적화` 도메인)에서 "작은 씬 CPU 바운드, 드로우콜 ~94"가 확정돼 있고, FX 드로우는 그 일부다. FX별 GPU 타임은 `Fx::Render` 스코프 + GPU 타임스탬프로 **캡처 가능하나 아직 분석 안 함**.

### 5.3 계획 중인 최적화

- **인스턴싱**: 같은 텍스처/blend의 billboard를 1 draw로 batch(현재 emitter당 개별 draw). `17_...MASTER §6`의 multi-renderer가 vertex factory 인스턴싱 전제.
- **SoA + GPU compute**: 대량 입자(엘든링 마법진)는 CPU 불가 → DX12 compute + indirect draw(EFX-7, Track 2 DX12 안정 후).
- **`.wfxbin` 쿠킹**: JSON 파싱 제거, zero-parse 로드(EFX-9).

---

## 6. 구현 예정 (Planned) — 동일 깊이

> 이 절은 "아직 안 한 부분"이지만 **실제로 구현할 것**이므로, 구현된 부분과 같은 깊이로 설명한다. 면접에서 "그건 어떻게 할 건가?"에 막힘없이 답하기 위함.

### 6.1 노드그래프를 LoL 런타임에 통합 (PoC → production)

- **무엇**: 현재 Elden 에디터에만 연결된 `FxGraph`→`FxExecPlan`→`CParticlePool` 경로를 LoL FX 런타임에 통합. 일부 절차적 이펙트(curl noise 트레일, age-over-life 색상)를 그래프로 저작.
- **왜**: 플랫 WFX는 "정적 파라미터"만. 입자별 시간축 시뮬(난기류, 중력 낙하)을 표현 못 함. 절차적 표현이 필요한 이펙트(이렐리아 R 검 8자루 트레일)에서 한계.
- **어떻게**: (1) `FxExecPlan`을 ECS 컴포넌트(`FxSystemInstanceHandle`)와 연결, World-owned storage가 `CParticlePool` 소유(`17_...MASTER` P-10 ownership). (2) Fx Spawn/Tick/Render 3 phase 분리, 같은 phase는 read+write 0 시스템만 묶어 ECS 스케줄러와 충돌 회피(P-9). (3) WFX와 공존: emitter가 "flat" 또는 "graph" 모드를 가리키게.
- **Trade-off 예상**: 입자 단위 tick은 CPU 비용↑. 그래서 "절차적 필요" emitter만 그래프, 나머지는 플랫 유지(하이브리드). 결정성 분리: 시각 FX RNG는 비결정적이어도 되지만, 판정에 닿는 건 `CXoroshiro128` 결정 RNG(`DeterministicRandom.h`) 사용.
- **검증**: `FxGraphValidator` 게이트는 이미 있음. 추가로 SoA 풀에 입자 수 카운터 + golden(같은 seed → 같은 입자 위치 해시).

### 6.2 Expression VM (바이트코드 스택 머신)

- **무엇**: `FxNodeDesc::bytecodeBlob`(현재 빈 필드, `FxAsset.h:77`)을 채울 **opcode 기반 스택 VM**. Niagara VectorVM 등가(Add/Sub/Mul/Normalize/External/Output).
- **왜**: "Age 비율 기반 색상 커브", "차지 비율 기반 크기" 같은 **수식 노드**를 데이터로. 매 입자마다 C++ 분기 대신 바이트코드 평가.
- **어떻게**: (1) 그래프의 수식 노드를 후위 표기 바이트코드로 컴파일(이미 위상정렬 있음). (2) SoA 컬럼 위에서 SIMD 친화 평가 루프. (3) 파서: 간단한 토큰화 → AST → 바이트코드.
- **Trade-off**: VM 인터프리트 오버헤드 vs 유연성. 핫패스는 inline expansion(Niagara `FNiagaraHlslTranslator` 모델)로 펼쳐 분기 제거.
- **검증**: 알려진 입력→출력 단위테스트(예: `Add(2,3)=5`), 그래프 컴파일→VM 평가 골든.

### 6.3 GPU compute 백엔드

- **무엇**: 그래프를 **HLSL compute shader로 코드 생성** + indirect draw. 만 단위 입자.
- **왜**: 엘든비스트 별똥별(7000/s × 2s = 14000 동시)은 CPU 불가.
- **어떻게**: VM 바이트코드/그래프를 HLSL로 트랜슬레이트 → DX12 compute로 SoA 버퍼 갱신 → `DrawIndexedInstancedIndirect`. **DX12 RHI(Track 2)가 scene-parity로 안정된 뒤** 진입(정직성 지도: DX12는 현재 parity 검증 단계).
- **Trade-off**: GPU 경로는 결정성/디버깅 난이도↑. 그래서 판정 FX는 CPU, 시각 대량 FX만 GPU.
- **검증**: CPU VM 결과 vs GPU 결과 대조(같은 seed), GPU 타임스탬프로 입자 수 대비 비용.

### 6.4 structured JSON 파서로 교체 + `.wfxbin` 쿠킹 + hot reload

- **무엇**: 수기 string 추출기(§3.2 약점)를 제대로 된 JSON reader/writer로(EFX-1). 배포용 `.wfxbin`(EFX-9). async hot reload(EFX-8).
- **왜**: 견고성(중첩/순서 무관) + 디자이너 저장 안정성 + zero-parse 로드 + 편집 즉시 반영.
- **어떻게**: round-trip 테스트(load→save→load 동일성)로 교체 안전성 확보. `.wfxbin`은 16B 헤더 + zero-copy 레이아웃(`10. 에셋 파이프라인`의 .wmesh 규약 재사용).
- **검증**: round-trip 골든, 112 에셋 전부 재파싱 일치.

### 6.5 챔피언 FxPresets → thin adapter 마이그레이션

- **무엇**: `14_CHAMPION_FX_DATA_PIPELINE_DIRECTION.md` 방향대로 C++ `FxPresets.cpp`(texture/size/lifetime 하드코딩)를 cue id+context만 넘기는 thin adapter로.
- **왜**: 챔프 늘면 수치가 코드에 퍼져 수정=rebuild. 디자이너/TA가 못 만짐.
- **어떻게**: gameplay truth는 Shared/GameSim C++ 유지, visual composition만 .wfx로. `EventApplier`의 챔프별 switch 제거.
- **검증**: 마이그레이션 전후 인게임 비주얼 동일 + FxPresets 라인 수 감소(rg).

---

## 7. 면접 예상 질문 & 모범 답변 (12개)

**Q1. (기본) 이 FX 시스템이 근본적으로 하는 일이 뭔가요?**
A. 스킬 비주얼을 C++ 재컴파일 없이 JSON(WFX) 데이터로 정의하고, 서버가 보낸 권위 이벤트에 cue 이름으로 매핑해 클라 ECS에 멀티-에미터로 스폰합니다. 핵심은 "게임플레이 truth(서버)"와 "비주얼(클라 데이터)"의 경계를 긋는 겁니다. 현재 13챔피언 112개 .wfx가 production으로 돕니다.

**Q2. (기본) 빌보드는 어떻게 카메라를 향하게 하나요?**
A. quad의 로컬 축을 카메라의 Right/Up 벡터로 교체합니다. `FxSystem.cpp:385-397`에서 월드행렬의 row0=Right*width, row1=-Forward, row2=Up*height로 만들어 quad가 항상 카메라 평면에 평행하게 합니다. non-billboard는 yaw 회전 quad로 GroundDecal에 씁니다.

**Q3. (설계) WFX 같은 플랫 에셋과 Niagara식 노드그래프 중 왜 플랫을 먼저 골랐나요?**
A. 표현력 vs 출하 속도의 트레이드오프입니다. 노드그래프는 컴파일러+VM+에디터 풀스택이라 수개월이고, LoL Q/W/E/R 이펙트는 emitter 파라미터 테이블로 90% 커버됩니다. 1인 범위에선 "동작하는 112 이펙트"가 우선이라 플랫을 먼저 출하했습니다. 단 노드그래프를 버린 게 아니라, DAG/위상정렬을 PoC로 박제해 두고(`FxGraphValidator`의 Kahn 정렬) 로드맵으로 분리했습니다.

**Q4. (설계) 왜 입자 단위 SoA 풀이 아니라 emitter=ECS 엔티티로 했나요?**
A. LoL FX는 대부분 소수의 quad라 셰이더 atlas/UV scroll로 "파티클처럼" 보이게 하면 충분하고, 기존 ECS Update/Render 루프와 anchor 추적/lifetime을 그대로 재사용할 수 있어서입니다. 진짜 대량 입자(엘든링 마법진)는 SoA 풀이 필요한데, 그건 `CParticlePool`로 구현해 뒀지만 아직 LoL 런타임엔 통합 안 했습니다.

**Q5. (설계) 서버 권위 게임에서 FX 결정성은 어떻게 다루나요?**
A. "판정 FX"와 "시각 FX"를 분리합니다. 히트박스는 서버가 결정론적으로 판정하고, 클라 FX는 그 결과(ReplicatedEvent)를 받아 cue로 그리기만 합니다. `EventApplier`가 이벤트→cue를 dispatch하므로 10인 한타에서도 모두 같은 이벤트로 같은 cue를 봅니다. 시각 FX는 비결정적이어도 무방하고, AoE 링 반지름 같은 "보이는 판정 범위"만 서버 truth와 맞춥니다.

**Q6. (심화) 노드그래프 컴파일러의 위상정렬은 어떻게 구현했나요?**
A. `FxGraphValidator.cpp:50-151`에서 Kahn 알고리즘입니다. 각 노드 indegree를 세고, indegree 0을 ready 큐에 넣고, 꺼내면서 인접 노드 indegree를 감소시켜 0이 되면 큐에 넣습니다. topoOrder 크기가 노드 수와 다르면 사이클로 판정합니다. 추가로 stage 역행 엣지(Update→Init 같은)와 render 노드가 정확히 1개인지 검증하고, 실패하면 exec plan을 아예 못 만들게 거부합니다.

**Q7. (adversarial) "Niagara급 노드 그래프 이펙트 시스템"이라는데, 실제로 LoL 게임에서 노드그래프로 만든 이펙트가 있나요?**
A. **없습니다, 정직하게.** 노드그래프 컴파일러(`FxGraph`/`FxExecPlan`)와 SoA 풀은 구현돼서 컴파일·동작하지만, **호출처는 EldenRing 에디터 한 곳뿐이고 LoL 런타임은 안 씁니다.** LoL production은 한 단계 낮은 플랫 WFX 에셋 경로입니다. 그래서 제 표현은 "production은 WFX 큐 런타임, 노드그래프/VM/GPU는 PoC·로드맵"으로 경계를 긋습니다. 노드그래프의 가치는 "DAG 컴파일러를 이해하고 위상정렬·검증까지 구현했다"는 데 있지, "그걸로 게임을 돌린다"가 아닙니다.

**Q8. (adversarial) WFX 파서를 봤더니 그냥 `find`/`substr`던데, 깨진 JSON 주면 어떻게 되나요?**
A. 맞습니다, 정규 JSON 파서가 아니라 수기 first-match 추출기입니다. 키가 두 번 나오거나 배열 안 객체에 같은 키가 있으면 오인할 수 있는 약점이 있습니다. 다만 emitter 블록 자르기와 anchor/lifecycle 중첩은 중괄호 depth + string 상태머신으로 처리해서(`ExtractEmitterBlocks`, `ExtractObjectBlock`) 단순 케이스는 견딥니다. 이 약점을 알고 있어서 마스터플랜 EFX-1에 "structured JSON reader/writer로 교체 + round-trip 검증"을 명시해 뒀습니다. MVP에선 외부 의존 0과 속도를 우선했고, 견고성은 다음 단계입니다.

**Q9. (adversarial) Expression VM, GPU compute 자랑하던데 그거 구현됐나요?**
A. **아니요. VM/GPU compute는 문서와 `bytecodeBlob` 빈 필드만 있고 .cpp는 0줄입니다.** 이건 planned-only로 분류합니다. 다만 "어떻게 할지"는 설계가 잡혀 있습니다 — VM은 그래프 수식 노드를 후위 바이트코드로 컴파일해 SoA 위에서 평가하고, GPU는 그걸 HLSL로 트랜슬레이트해 DX12 compute+indirect draw로 돌립니다. GPU는 DX12 RHI가 scene-parity로 안정된 뒤 진입합니다. 지금 "했다"고 말하면 코드리뷰에서 즉사하니까, 저는 "여기까지 했고 여기부터 계획, 이유는 출하 우선순위"로 선을 긋습니다.

**Q10. (adversarial) FX가 제대로 동작하는 걸 어떻게 증명하나요? 테스트 있나요?**
A. FX 도메인엔 자동 골든/픽셀 테스트가 없습니다, 정직하게. 대신 (1) 112 에셋이 0 missing으로 로드되는지 카운터(`GetAssetCount`, `LogMissingCue`), (2) `Fx::Drawn`/`Fx::CullSkipped` 프로파일러 카운터로 스킬 발동 시 실제 드로우가 증가하는지, (3) WFX round-trip 안정성, (4) 그래프 검증 게이트(PoC)로 잘못된 그래프 거부, (5) 인게임 수동 시각 확인으로 판정합니다. "픽셀을 비교했다"는 주장은 안 합니다.

**Q11. (확장) 만 단위 입자(엘든링 마법진)는 어떻게 처리할 건가요?**
A. CPU로는 불가능합니다. SoA 파티클풀(`CParticlePool`, 이미 구현)을 DX12 compute에 올려, 입자 갱신을 compute shader로 하고 `DrawIndexedInstancedIndirect`로 그립니다. 그래프를 HLSL로 코드 생성하는 단계가 필요하고, 이게 EFX-7입니다. 다만 MOBA엔 오버스펙이라 LoL은 Stage 1~5(플랫+빌보드)로 충분하고, GPU compute는 엘든링+포트폴리오 용입니다.

**Q12. (확장) 챔프가 50명이 되면 FX 관리가 어떻게 되나요?**
A. 지금도 일부는 C++ FxPresets에 수치가 박혀 있어서, 챔프가 늘면 texture/size/lifetime이 코드에 퍼져 수정=rebuild가 됩니다. 그래서 `14_CHAMPION_FX_DATA_PIPELINE_DIRECTION.md`에서 방향을 고정했습니다: gameplay truth는 Shared/GameSim C++에 두고, visual composition만 source-controlled .wfx로 빼서 FxPresets를 cue id+context만 넘기는 thin adapter로 줄입니다. 디자이너/TA가 C++ 없이 수치를 조정할 수 있게요. 이게 LeeSin을 첫 데이터 기반 샘플로 진행 중입니다.

---

## 8. 30초 엘리베이터 피치

"제 FX 시스템의 본질은 '스킬이 어떻게 보일지'를 C++ 재컴파일 없이 JSON 데이터로 정의하고, 서버가 보낸 권위 이벤트에 cue 이름으로 매핑해 클라이언트 ECS에 멀티-에미터로 그리는 겁니다. 게임플레이 truth는 서버, 비주얼은 클라 데이터로 경계를 그어서, 10인 한타에서도 모두 같은 이벤트로 같은 이펙트를 봅니다. 지금 13챔피언 112개 .wfx가 빌보드·빔·리본·메시·데칼로 production에서 돌고요. Niagara식 노드그래프 컴파일러랑 SoA 파티클풀도 위상정렬·검증까지 구현해 뒀는데, 이건 솔직히 아직 엘든링 에디터에만 붙은 PoC고 LoL 런타임은 플랫 에셋 경로입니다. 저는 '동작하는 112 이펙트'를 먼저 출하하고, 노드그래프·VM·GPU compute는 어디까지가 구현이고 어디부터가 로드맵인지 코드로 선을 긋는 걸 보여드리고 싶습니다."
