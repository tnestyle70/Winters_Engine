# FX Editor 상세 설계 (Niagara급 .wfx)

> 작성: 2026-06-23. 대상: Codex / EldenRing 팀.
> 선행 문서: `17_UE5_GRADE_EDITOR_SUITE_MASTER.md`(전체 매핑·게이트), `12_..._BIG_PICTURE.md`(Phase A~J·G0~G9),
> `06_FX_GRAPH_SEQUENCER_EDITOR.md`(FX 섹션), `plan/EldenRingEditor/05_NIAGARA_WFX_FX_EDITOR.md`,
> `10_ASSET_PIPELINE_TOOLING.md`(FXR 파서), `plan/EffectTool/01~28`(기존 FX 런타임 설계 자산).
> 근거 코드: `Engine/Public/FX/FxAsset.h`, `Engine/Public/FX/FxMaterialDesc.h`,
> `Engine/Public/Renderer/FxShaderConstants.h`, `Client/Private/GameObject/FX/{WfxDocument,FxSystem,FxCuePlayer}.cpp`,
> `Client/Public/GameObject/FX/FxBillboardComponent.h`, `Shaders/{FxSprite,FxMesh}.hlsl`,
> `Tools/EldenAssetPipeline/elden_pipeline.py`.

---

## 0. 한 줄 목표 + 시스템 경계

**목표**: UE5 Niagara를 reference로만 삼아, Winters `.wfx` 그래프 문서를 **검증→CPU execution plan으로 컴파일**하고,
이를 기존 `FxAsset/FxEmitterDesc` 런타임으로 bake해 `FxSprite.hlsl`/`FxMesh.hlsl`로 렌더링하는 **노드 그래프 FX 에디터**를 만든다.

**이 문서가 다루는 경계**:
- 포함: `.wfx` 그래프 스키마(graph metadata + emitter desc 동시 저장), `CFxGraph` 검증/컴파일,
  CPU 실행 계획(`CFxExecPlan`), `EmitterInstance`/`ParticlePool`, `FxGraphEditor` ImGui 패널,
  EldenRing FXR(`.fxr`) → `.wfx` 변환 경로 설계.
- 제외(다른 문서 소유): Sequencer(`.wseq`), World Partition(`.wcell`), Boss/Hitbox, GPU compute 시뮬(Phase 후순위로만 언급),
  서버 권위 판정 자체(GameSim) — 여기서는 **FX cue 수신/재생 contract**만 다룬다.

**presentation/truth 분리(절대 원칙)**: FX 그래프는 전부 presentation이다. 판정·데미지·페이즈 전이는 Server GameSim이 소유한다.
visual-only FX는 클라 로컬 시드, gameplay-affecting FX는 **서버 event id + seed**를 받아 결정적으로 재생한다.

---

## 1. UE5 Niagara 실제 아키텍처 (깊이)

### 1.1 객체 계층: System → Emitter → Module Stack

Niagara의 1급 에셋은 `UNiagaraSystem`이다. System은 N개의 `UNiagaraEmitter`를 묶고, 각 Emitter는 **Module Stack**을 가진다.
스택은 고정된 실행 페이즈로 나뉜다:

```text
System Spawn      (시스템 1회 초기화)
System Update     (매 프레임 시스템 단위 — 스폰 burst, age, loop)
Emitter Spawn     (emitter 1회)
Emitter Update    (매 프레임 emitter 단위 — spawn rate 계산)
Particle Spawn    (새 파티클 1회 — InitPosition/Velocity/Color)
Particle Update   (매 프레임 살아있는 파티클 — Gravity/Drag/Curl/SizeOverLife)
Render            (Sprite/Mesh/Ribbon/Light Renderer)
```

각 스택 칸에 들어가는 **Module**은 작은 함수형 노드다. Niagara의 핵심 통찰: 아티스트가 보는 것은 스택 UI지만,
내부적으로 모든 모듈은 **하나의 노드 그래프(HLSL-like AST)** 로 결합되어 **단일 시뮬레이션 함수로 컴파일**된다.
스택은 "그래프를 선형으로 보여주는 뷰"일 뿐, 진실은 컴파일된 함수다.

### 1.2 Parameter Namespace

Niagara는 변수에 namespace 접두사를 강제한다:

| Namespace | 수명/스코프 | 예 |
|---|---|---|
| `User.` | 외부(게임플레이 코드)가 set | `User.Color`, `User.SpawnCount` |
| `System.` | 시스템 전역 | `System.Age`, `System.LoopCount` |
| `Emitter.` | emitter 단위 | `Emitter.SpawnRate`, `Emitter.Age` |
| `Particle.` | 파티클 per-instance | `Particle.Position`, `Particle.Velocity`, `Particle.Lifetime` |
| `Engine.` | 엔진 제공 read-only | `Engine.DeltaTime`, `Engine.Owner.Position` |

이 namespace가 **데이터 흐름의 타입 안전성과 스코프**를 결정한다. `Particle.` 변수는 ParticlePool의 컬럼(SoA),
`System./Emitter.`는 스칼라 상태, `User.`는 외부 입력 바인딩이다.

### 1.3 Data Interface

`UNiagaraDataInterface`는 시뮬이 **외부 데이터에 접근**하는 통로다 (static mesh 표면 샘플, skeletal mesh socket,
collision query, curve, texture, audio spectrum). 시뮬 그래프에서 함수처럼 호출되지만 구현은 CPU VM/GPU 양쪽에 따로 있다.
DI 덕분에 "메시 표면에서 스폰", "충돌하면 bounce" 같은 게 그래프 안에서 표현된다.

### 1.4 CPU VM vs GPU HLSL — 두 백엔드 한 그래프

Niagara 그래프는 두 가지로 컴파일된다:
- **CPU**: 그래프 → 자체 바이트코드 → `FNiagaraVectorVM`(SIMD 인터프리터). 적은 파티클·CPU 접근 DI에 유리.
- **GPU**: 그래프 → 생성된 HLSL compute shader. 수십만 파티클·**Sim Stage**(멀티패스, 이웃 그리드/유체) 가능.

핵심: **소스(노드 그래프)는 하나, 백엔드는 둘**. 이 분리가 Niagara를 강력하게 만들지만 컴파일러 복잡도의 원천이다.

### 1.5 Renderer + Scratch Pad

- **Renderer**: 시뮬 결과(파티클 속성)를 화면에 그리는 단계. SpriteRenderer/MeshRenderer/RibbonRenderer/LightRenderer/Component.
  각 Renderer는 어떤 `Particle.` 속성을 어디(위치/색/크기/정렬)에 바인딩할지 정의한다.
- **Scratch Pad**: 에디터 안에서 임시 module을 즉석으로 만드는 그래프 편집기(블루프린트 유사). 재사용 전 빠른 실험용.

### 1.6 왜 이렇게 설계했나 (철학)

1. **데이터 지향**: 모든 게 attribute(SoA 컬럼). 모듈은 attribute를 읽고 쓰는 순수 함수 → 컴파일·SIMD·GPU 이식 용이.
2. **그래프=소스, 컴파일된 함수=진실**: UI(스택)와 실행(컴파일된 함수)을 분리. 에디터가 자유로워도 런타임은 빠르다.
3. **하위호환을 위한 desc 분리**: 그래프 없이도 cooked emitter desc로 돌 수 있게 한다 → 빌드/패키지에서 그래프 컴파일러 불필요.

**Winters가 가져갈 통찰 4개**: (a) 그래프→컴파일된 실행 계획, (b) Particle 속성을 SoA로,
(c) 그래프 metadata와 런타임 desc를 **함께 저장**, (d) namespace로 스코프/타입 안전성. **버릴 것**: GPU 백엔드 초기 도입, DI 풀세트, Sim Stage.

---

## 2. Winters 현재 구조 (실측 근거)

### 2.1 재사용 가능한 실제 코드

| 자산 | 파일:줄 | 역할 | 그래프 에디터에서의 위치 |
|---|---|---|---|
| `FxEmitterDesc` | `Engine/Public/FX/FxAsset.h:80-156` | **런타임 emitter desc(컴파일 타깃)** | 그래프 bake 결과가 여기로 내려간다 |
| `FxNodeDesc` | `FxAsset.h:72-78` | `strType/inputs/outputs/bytecodeBlob` — **이미 노드 슬롯 존재** | 그래프 노드 직렬화 컨테이너로 채택 |
| `FxAsset` | `FxAsset.h:201-207` | `emitters[] + initialUserParams(CFxParameterMap)` | `.wfx` 문서의 런타임 절반 |
| `FxMaterialDesc` | `Engine/Public/FX/FxMaterialDesc.h:18-41` | tint/uv/style/magic 파라미터 | Render 노드 material 핀 |
| `CBFxParams` + `MakeFxParamsFromMaterial` | `Engine/Public/Renderer/FxShaderConstants.h:33-84` | **셰이더 ABI(고정)** | 변경 금지. bake 산출이 이 ABI로 흘러야 함 |
| `FxSprite.hlsl` / `FxMesh.hlsl` | `Shaders/` | Billboard/Mesh 렌더 | Render 단계 백엔드 |
| `CFxSystem` | `Client/Public/GameObject/FX/FxSystem.h:27-70` | ECS spawn/update/render, `CFxAssetRegistry` 소유 | bake된 asset의 런타임 소비자 |
| `FxBillboardComponent` | `Client/Public/.../FxBillboardComponent.h:14-132` | POD 파티클/이펙트 인스턴스 + lifecycle | 1차 EmitterInstance 대용(이미 동작) |
| `CWfxDocument` | `Client/Private/.../WfxDocument.cpp:371-425` | `.wfx` JSON load/save | **graph block 확장 지점** |
| `LoadFxAssetFromFile` | `Engine/Private/FX/FxAsset.cpp:1-90+` | JSON → FxAsset 파서(키-스캔 방식) | graph block 무시하고도 로드(하위호환) |
| `CFxParameterMap` | `Engine/Public/FX/ParameterMap.h` | User param 저장 | `User.` namespace 백엔드 |
| FXR 파서 | `Tools/EldenAssetPipeline/elden_pipeline.py:643-712` | `.fxr` XML → action/container + `integerCandidates` | FXR→.wfx 변환 입력 |

### 2.2 핵심 사실(설계 제약)

1. **`FxEmitterDesc`는 이미 매우 풍부하다** — billboard/ribbon/trail/atlas/material/anchor/lifecycle 필드를 전부 가진다.
   따라서 그래프는 **새 런타임을 만들 필요가 없다**. 그래프의 출력 = `FxEmitterDesc` 채우기.
2. **`.wfx` 저장 포맷은 emitter 배열 JSON**이며(`WfxDocument.cpp:357-367`, `schema:"WintersWfx", version:1`),
   파서는 키-스캔(`ExtractString/ExtractNumber`)이라 **모르는 키를 무시**한다 → graph block을 추가해도 기존 로더가 안 깨진다.
3. **`FxNodeDesc nodes` 컨테이너가 `FxEmitterDesc`에 이미 있다**(`FxAsset.h:86`). 현재 비어 있음 → 여기에 그래프 노드를 넣을 수 있다.
4. **셰이더 ABI(`CBFxParams`)는 고정**이다. bake 결과는 반드시 이 ABI를 통과해야 하며 ABI 변경은 금지.
5. **`FxCuePlayer`(서버 cue 경로)는 이번 변경 대상이 아니다**(`05_NIAGARA_WFX_FX_EDITOR.md:88` 결정). bake 산출이 `FxAsset`로 내려온 뒤에만 cue가 소비.

### 2.3 미구현(신규 작성 대상)

| 미구현 | 위치(제안) | 비고 |
|---|---|---|
| `CFxGraph`(노드/엣지/검증) | `Engine/Public/FX/Graph/FxGraph.h` | 신규 |
| `CFxExecPlan`(컴파일된 CPU 실행 계획) | `Engine/Public/FX/Exec/FxExecPlan.h` | 신규 — Niagara "컴파일된 함수" 대응 |
| `CFxEmitterInstance`/`CFxParticlePool`(SoA) | `Engine/Public/FX/Exec/` | 신규. 초기엔 1-particle billboard로 `FxBillboardComponent` 재사용 가능 |
| `BakeGraphToEmitters()` | `Client/Private/.../WfxGraphDocument.cpp` | 그래프→`FxEmitterDesc` |
| `FxGraphEditor` 패널 | `Client/Private/UI/FxGraphEditorPanel.cpp` | ImGui 노드 그래프 |
| FXR→.wfx 변환기 | `Tools/EldenAssetPipeline/fxr_to_wfx.py` | integerCandidate→mesh/tex id 매핑 |

> 참고: `.md/plan/EffectTool/01_ARCHITECTURE.md`에 이미 풀세트 Niagara 클론 디렉토리 설계가 있으나, 이는 **과설계 위험**(전체 GPU/Expression/DI 포함)이다.
> 본 문서는 17/12문서 게이트(G6: graph→emitter bake)에 맞춰 **최소 CPU 실행 계획부터** 잡는다.

---

## 3. Winters 설계

### 3.1 포맷 스키마: `.wfx` (JSON 초기 → `.wfx` binary 승격)

**불변식**: 기존 `emitters[]` 블록은 그대로 둔다(런타임 desc = 항상 저장됨). 그 옆에 **optional `graph` 블록**을 추가한다.
graph 없는 `.wfx`도 로드되고, graph 있는 `.wfx`는 bake 결과를 emitters[]에 함께 저장한다.

```jsonc
{
  "schema": "WintersWfx",
  "version": 2,                    // v1: emitters only. v2: + optional graph
  "name": "Boss_Stomp_Dust",
  "emitters": [ /* 기존 FxEmitterDesc 직렬화 — 항상 bake 결과 포함 */ ],
  "graph": {                        // ← 신규 optional. 없으면 v1처럼 동작
    "userParams": [
      { "name": "User.Tint", "type": "Vec4", "value": [1,0.6,0.2,1] },
      { "name": "User.SpawnCount", "type": "Int", "value": 24 }
    ],
    "emitterGraphs": [
      {
        "name": "DustRing",
        "renderType": "Billboard",   // bake 시 FxEmitterDesc.renderType
        "nodes": [
          { "id": 1, "type": "SpawnBurst",   "x": 40,  "y": 60,
            "params": { "count": "User.SpawnCount", "time": 0.0 } },
          { "id": 2, "type": "InitPosition", "x": 240, "y": 40,
            "params": { "shape": "Ring", "radius": 2.0, "jitter": 0.15 } },
          { "id": 3, "type": "InitVelocity", "x": 240, "y": 160,
            "params": { "mode": "Radial", "speed": 3.5 } },
          { "id": 4, "type": "InitLifetime", "x": 240, "y": 280,
            "params": { "min": 0.8, "max": 1.4 } },
          { "id": 5, "type": "Age",          "x": 440, "y": 60,  "params": {} },
          { "id": 6, "type": "Gravity",      "x": 440, "y": 160, "params": { "g": -2.0 } },
          { "id": 7, "type": "SizeOverLife", "x": 440, "y": 280,
            "params": { "curve": [[0,0.2],[0.3,1.0],[1.0,0.0]] } },
          { "id": 8, "type": "ColorOverLife","x": 640, "y": 60,
            "params": { "curve": [[0,[1,1,1,1]],[1,[1,0.5,0.2,0]]] } },
          { "id": 9, "type": "BillboardRenderer", "x": 840, "y": 120,
            "params": { "texture": "EldenRing/FX/dust_a.png",
                        "blend": "Additive", "styleMode": 16 } }
        ],
        "edges": [
          { "from": 1, "to": 2 }, { "from": 2, "to": 3 }, { "from": 3, "to": 4 },
          { "from": 4, "to": 5 }, { "from": 5, "to": 6 }, { "from": 6, "to": 7 },
          { "from": 7, "to": 8 }, { "from": 8, "to": 9 }
        ]
      }
    ]
  },
  "fx": {                            // gameplay 결정성 메타
    "authority": "ServerCue",        // "LocalVisual" | "ServerCue"
    "seedPolicy": "FromEvent"        // visual-only=Local, gameplay=FromEvent
  },
  "preview": { "duration": 3.0, "loop": true }
}
```

**필드 정의(graph 블록)**:
- `userParams[]`: `{name(namespace 접두), type(Float/Int/Vec3/Vec4/Color), value}`. `User.` 스코프만 외부 set 허용.
- `node`: `{id(u32), type(string), x/y(에디터 좌표), params(노드별 키맵)}`. `params` 값이 `"User.X"` 문자열이면 파라미터 바인딩.
- `edge`: `{from(nodeId), to(nodeId)}`. 단순 실행 순서 DAG. (핀 단위 연결은 P2 — 초기엔 노드=스테이지 묶음.)
- `renderType`: bake 시 `FxEmitterDesc.renderType`(`eFxRenderType`)로 직결.
- `fx.authority/seedPolicy`: presentation/truth 경계 메타. 런타임 spawn 시 seed 소스를 결정.

**바이너리 승격(`.wfx` v3)**: 안정화 후 JSON→packed binary. graph 블록은 **에디터 전용**이라 cooked/release 빌드에선 strip하고
emitters[]만 남길 수 있다(Niagara의 desc 분리와 동일 철학).

### 3.2 노드 스테이지 분류 (초기 세트)

| Stage | 노드 type | bake 대상 필드(FxEmitterDesc) |
|---|---|---|
| Spawn | `SpawnBurst`(count,time), `SpawnRate`(rate) | `spawnRate`, `maxParticles` |
| Init | `InitPosition`(shape/radius/jitter), `InitVelocity`(mode/speed), `InitLifetime`(min/max), `InitColor` | `vVelocity`, `fLifetime`, `vColor`, `fStartRadius/fEndRadius` |
| Update | `Age`, `Gravity`(g), `Drag`(k), `SizeOverLife`(curve), `ColorOverLife`(curve) | (실행 계획 + `fGrowDuration`/`fFadeIn`/`fFadeOut` 근사) |
| Render | `BillboardRenderer`, `MeshRenderer`(model), `RibbonRenderer` | `renderType`, `material`, `blendMode`, `strTexturePath`, `strModelPath` |

### 3.3 런타임 클래스 계층 (C++ 시그니처)

```text
.wfx(JSON) ── load ──► CWfxGraphDocument (graph + emitters 동시 보유)
                          │ BakeGraphToEmitters()  → FxEmitterDesc[] (항상 갱신)
                          │ CompileGraph()         → CFxExecPlan (CPU 실행 계획)
                          ▼
        CFxGraph ──validate──► CFxGraphValidator ──compile──► CFxExecPlan
                                                                  │ Instantiate
                                                                  ▼
                                       CFxEmitterInstance(plan, CFxParticlePool SoA)
                                                                  │ Tick(dt) → particles
                                                                  ▼
                          기존 CFxSystem::Render / FxSprite.hlsl / FxMesh.hlsl
```

핵심: **두 경로 모두 항상 성립** —
- **Bake 경로**(필수, G6): 그래프 → `FxEmitterDesc[]` → 기존 `CFxSystem`/`FxBillboardComponent`가 즉시 소비(이미 동작).
- **Plan 경로**(에디터 프리뷰/심화): 그래프 → `CFxExecPlan` → SoA 파티클 시뮬. 초기엔 프리뷰 뷰포트 전용,
  안정화 후 런타임 emitter로 승격. bake가 실패해도 기존 emitter desc를 유지한다(`05_..._FX_EDITOR.md:80`).

### 3.4 에디터 패널 (ImGui) — `FxGraphEditor`

DX12 ImGui 셸(`EldenRingEditor`, 또는 우선 Client `WfxEffectToolPanel` 강화) 위에 도킹된 패널.

| 구역 | 기능 |
|---|---|
| **Toolbar** | New/Open/Save `.wfx`, Bake, Compile, Play/Stop/Restart, Loop toggle |
| **Node Canvas** | 노드 추가(우클릭 메뉴)/삭제/이동/엣지 연결, 핀 타입 검증(색상), DAG 사이클 경고 |
| **Stage Lanes** | Spawn/Init/Update/Render 레인 배경(Niagara 스택 가독성) |
| **Param Inspector** | 선택 노드 `params` 편집(슬라이더/컬러/커브 에디터), `User.` 바인딩 드롭다운 |
| **Preview Viewport** | RT에 그린 실시간 프리뷰(plan 경로 시뮬) + 카메라 orbit + 그리드 |
| **User Params** | `userParams[]` add/remove, 런타임 set 시뮬(슬라이더로 외부 입력 흉내) |
| **Validation Log** | 검증 결과(누락 attr, 사이클, 미연결 Render, 타입 불일치) |
| **Texture Picker** | ER UI/ER Textures preset 스캔(`05_..._FX_EDITOR.md:31-38` 기반) + `*_a/_n/_m/_r/_em` 역할 매칭 |

**완료 기준(패널 단위)**: "그래프 편집 → Save `.wfx` → 재실행 후 로드 → Bake → 프리뷰에서 burst billboard가 보임".

---

## 4. 데이터 흐름 (presentation/truth 경계)

```text
[FxGraphEditor]
   │ 편집(노드/엣지/param)
   ▼
.wfx (graph + emitters[] + fx.authority/seedPolicy)        ← 디스크
   │
   ▼
CAssetStreamingSystem (handle/state, async, 핫리로드)        ← 모든 런타임 로드는 여기 경유(우회 금지)
   │  (현 단계: CFxAssetRegistry::LoadFromFile 동기 baseline, 인터페이스만 async-ready)
   ▼
CWfxGraphDocument → BakeGraphToEmitters() → FxAsset(emitters[])
   │
   ├─► [presentation] CFxSystem::SpawnFromAsset → FxBillboardComponent / EmitterInstance
   │        - visual-only FX: seed = 클라 로컬 RNG (fx.seedPolicy=Local)
   │        - 에디터 프리뷰: plan 경로 시뮬(독립)
   │
   └─► [truth 경계] gameplay-affecting FX:
            Server GameSim ── event{id, seed} ──► Client FxCuePlayer
                 (FxCuePlayer는 변경 없음; bake된 FxAsset을 seed로 1회 재생)
                 판정/데미지/페이즈 전이 = Server. Client = anim/telegraph FX/UI/camera shake.
```

**불변식**:
1. 에디터가 만든 `.wfx`는 `CAssetStreamingSystem`을 거쳐 로드(에디터 전용 우회 경로 금지).
2. 그래프 metadata와 emitter desc는 **함께 저장**(graph 없는 `.wfx`도 로드).
3. gameplay FX의 결정성은 **서버 event id + seed**가 소유. 그래프는 "어떻게 보일지"만 정의.

---

## 5. 구현 순서 (S0~S6) + 완료기준 + 게이트

전제: **G2(추출 static mesh `.wmesh`/`.wmat` 표시)와 RHI-02(texture+light)** 가 선행이어야 프리뷰가 의미 있다.
미충족이면 FX 패널 확장을 멈추고 선행부터(17문서 4절 게이트 우선 원칙).

| 단계 | 내용 | 완료기준 | 게이트 |
|---|---|---|---|
| **S0** | `.wfx` v2 스키마 + `CWfxGraphDocument`(graph block load/save, 기존 v1 로드 유지) | graph 없는 기존 `.wfx` 정상 로드 + graph block 라운드트립 | (선행) G5 핸들/상태 |
| **S1** | `CFxGraph` + `CFxGraphValidator`(DAG/타입/미연결 Render 검출) | invalid 그래프가 Validation Log에 사유 표시 | — |
| **S2** | `BakeGraphToEmitters()` — burst billboard 그래프 → `FxEmitterDesc` | bake 산출을 기존 `CFxSystem`이 spawn하여 화면에 1회 재생 | **G6 WFX bake** |
| **S3** | `CFxExecPlan` + `CFxParticlePool`(SoA) + `CFxEmitterInstance::Tick` (CPU) | 프리뷰 뷰포트에서 N파티클 burst가 age/gravity/sizeOverLife로 시뮬 | G6(심화) |
| **S4** | `FxGraphEditor` 패널 — 캔버스/inspector/preview play-stop-restart | 편집→Save→재실행 로드→프리뷰 왕복 | G6 완료 |
| **S5** | 서버 cue 연동 검증 — bake된 FxAsset을 event{id,seed}로 1회 재생(`FxCuePlayer` 무수정) | gameplay FX가 서버 cue로 1회만 재생(중복/클라발동 없음) | **G9 서버 권위** |
| **S6** | FXR→.wfx 변환기(`fxr_to_wfx.py`) — `integerCandidate`→mesh/tex id 매핑 후보 emit | `.fxr` 1개 → `.wfx`(emitters[]) 산출 + 에디터 로드 | (Phase B/D 에셋) |

**게이트 막힘 대응**:
- G6(bake) 막히면 → gameplay FX cue 연결(S5) 중단. CPU plan(S3) 확장 중단.
- G9(서버 권위) 막히면 → client-only gameplay FX 결과 생성 금지. visual-only로만 진행.

---

## 6. 코드 스켈레톤 (Winters 타입 사용)

> 타입: `f32_t/u32_t/u8_t/bool_t`, `Vec3/Vec4`, `wstring_t`. ABI(`CBFxParams`/`FxEmitterDesc`) 변경 금지.

### 6.1 `Engine/Public/FX/Graph/FxGraph.h`

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "FX/FxAsset.h"          // eFxRenderType, FxEmitterDesc
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

enum class eFxNodeStage : u8_t { Spawn, Init, Update, Render };

enum class eFxNodeType : u8_t
{
    SpawnBurst, SpawnRate,
    InitPosition, InitVelocity, InitLifetime, InitColor,
    Age, Gravity, Drag, SizeOverLife, ColorOverLife,
    BillboardRenderer, MeshRenderer, RibbonRenderer,
};

// 노드 파라미터 값: 리터럴 또는 User.* 바인딩 이름
using FxParamValue = std::variant<f32_t, i32_t, Vec3, Vec4, std::string /*binding*/>;

struct FxCurveKey { f32_t t = 0.f; Vec4 v = { 0,0,0,0 }; };

struct FxGraphNode
{
    u32_t                 id = 0;
    eFxNodeType           type = eFxNodeType::SpawnBurst;
    f32_t                 x = 0.f, y = 0.f;                 // editor canvas
    std::unordered_map<std::string, FxParamValue> params;
    std::vector<FxCurveKey> curve;                          // SizeOverLife/ColorOverLife
};

struct FxGraphEdge { u32_t from = 0; u32_t to = 0; };

enum class eFxParamScope : u8_t { User, System, Emitter, Particle, Engine };
struct FxGraphParam { std::string name; eFxParamScope scope = eFxParamScope::User; FxParamValue value; };

struct FxEmitterGraph
{
    std::string             strName;
    eFxRenderType           renderType = eFxRenderType::Billboard;
    std::vector<FxGraphNode> nodes;
    std::vector<FxGraphEdge> edges;
};

struct CFxGraph
{
    std::vector<FxGraphParam>  userParams;
    std::vector<FxEmitterGraph> emitterGraphs;

    const FxGraphNode* FindNode(const FxEmitterGraph& e, u32_t id) const;
};
```

### 6.2 `Engine/Public/FX/Graph/FxGraphValidator.h`

```cpp
#pragma once
#include "FX/Graph/FxGraph.h"
#include <string>
#include <vector>

struct FxValidationIssue { u32_t nodeId = 0; std::string message; bool_t bError = true; };

struct FxValidationResult
{
    bool_t bValid = false;
    std::vector<FxValidationIssue> issues;
    std::vector<u32_t> topoOrder;   // emitter별 DAG 위상 정렬(컴파일 입력)
};

class CFxGraphValidator
{
public:
    // DAG 사이클, 미연결 Render 노드, 타입 불일치, 누락 attribute 검출
    WINTERS_ENGINE static FxValidationResult Validate(const FxEmitterGraph& emitter);
};
```

### 6.3 `Engine/Public/FX/Exec/FxExecPlan.h` (컴파일된 CPU 실행 계획)

```cpp
#pragma once
#include "WintersAPI.h"
#include "FX/Graph/FxGraph.h"
#include <functional>
#include <vector>

class CFxParticlePool;          // SoA
struct FxExecContext { f32_t dt = 0.f; f32_t emitterAge = 0.f; u64_t seed = 0; };

// 컴파일된 스테이지 스텝: 노드 → 캡처된 실행 함수(Niagara "컴파일된 함수" 대응)
struct FxExecStep
{
    eFxNodeStage stage = eFxNodeStage::Update;
    std::function<void(CFxParticlePool&, const FxExecContext&)> fn;
};

struct CFxExecPlan
{
    std::vector<FxExecStep> spawnSteps;   // burst/rate
    std::vector<FxExecStep> initSteps;    // per-new-particle
    std::vector<FxExecStep> updateSteps;  // per-alive-particle
    eFxRenderType           renderType = eFxRenderType::Billboard;
    FxMaterialDesc          renderMaterial{};
    wstring_t               strTexturePath;
    u32_t                   maxParticles = 256;
};

class CFxGraphCompiler
{
public:
    // 검증 통과 그래프 → CPU 실행 계획. 실패 시 plan.spawnSteps 비움(호출측이 bake desc 유지).
    WINTERS_ENGINE static bool_t Compile(const FxEmitterGraph& graph,
                                         const std::vector<u32_t>& topoOrder,
                                         CFxExecPlan& outPlan,
                                         std::string& outError);
};
```

### 6.4 `Engine/Public/FX/Exec/FxParticlePool.h` (SoA)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include <vector>

// Structure-of-Arrays — Particle.* 속성 컬럼
class CFxParticlePool
{
public:
    WINTERS_ENGINE explicit CFxParticlePool(u32_t capacity);

    WINTERS_ENGINE u32_t Allocate(u32_t count);      // 새 파티클 인덱스 범위 시작
    WINTERS_ENGINE void   KillExpired();             // age >= lifetime 스왑-제거
    u32_t AliveCount() const { return m_alive; }

    std::vector<Vec3>  position;
    std::vector<Vec3>  velocity;
    std::vector<Vec4>  color;
    std::vector<f32_t> size;
    std::vector<f32_t> age;
    std::vector<f32_t> lifetime;
private:
    u32_t m_capacity = 0;
    u32_t m_alive = 0;
};
```

### 6.5 `Client/Public/UI/WfxGraphDocument.h` (graph + emitters 동시 보유)

```cpp
#pragma once
#include "FX/FxAsset.h"
#include "FX/Graph/FxGraph.h"
#include <string>

namespace WfxTool
{
    // 기존 CWfxDocument(emitters만)를 확장 — graph block은 optional.
    class CWfxGraphDocument
    {
    public:
        bool_t LoadFromFile(const wstring_t& strPath);   // v1(graph 없음)도 로드
        bool_t SaveToFile(const wstring_t& strPath) const; // emitters[] + graph block

        // 그래프 → FxEmitterDesc[]. 실패 시 false, 기존 emitters 유지(파괴하지 않음).
        bool_t BakeGraphToEmitters(std::string& outError);

        bool_t HasGraph() const { return !m_graph.emitterGraphs.empty(); }
        const FxAsset& Asset() const { return m_asset; }
        CFxGraph&       Graph() { return m_graph; }

    private:
        FxAsset   m_asset{};      // 항상 유효(bake 결과 / 기존 desc)
        CFxGraph  m_graph{};      // optional
        wstring_t m_strPath;
        std::string m_strLastError;
        bool_t    m_bLoaded = false;
    };
}
```

### 6.6 FXR→.wfx 변환기 시그니처 (`Tools/EldenAssetPipeline/fxr_to_wfx.py`)

```python
# elden_pipeline.py 의 action_record(...)['integerCandidates'] 를 입력으로 받아
# mesh/texture id 후보를 추론해 .wfx(emitters[]) 로 emit 한다.
def fxr_to_wfx(fxr_parse_manifest: dict, asset_index: dict) -> dict:
    """
    fxr_parse_manifest: parse-fxr 산출(JSON) — actions[].integerCandidates 포함.
    asset_index: {flver_id -> .wmesh path, tpf_stem -> .png path}.
    반환: WintersWfx v2 dict (emitters[] + fx.authority="LocalVisual" 기본,
          매핑 불확실 id 는 emitter.note="candidate" 로 표시 — 자동 배치 금지).
    """
    # 1) 각 action 의 integerCandidate 를 asset_index 와 대조해 mesh/tex id 매핑 후보 점수화
    # 2) Billboard/MeshParticle renderType 추정(텍스처만이면 Billboard, flver id 매칭이면 MeshParticle)
    # 3) 확정 매칭만 emitter 로, 나머지는 candidate note 로 남겨 에디터에서 수동 확정
    ...
```

---

## 7. 검증 · 리스크

### 7.1 빌드 타겟별 MSBuild

```powershell
$MSB = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
# Engine(FX Graph/Exec public header 추가):
& $MSB Winters.sln /t:Engine /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
# Client(WfxGraphDocument/FxGraphEditor):
& $MSB Winters.sln /t:Client /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
# Editor(패널 이식 후):
& $MSB Winters.sln /t:EldenRingEditor /m /p:Configuration=Debug-DX12 /p:Platform=x64 /v:minimal
# LoL 영향(기존 .wfx/FxSystem 회귀):
& $MSB Winters.sln /t:Client /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
git diff --check
```
- Engine public header(`FX/Graph/*`, `FX/Exec/*`) 추가 시 **`UpdateLib.bat`(SDK sync)** 필요 — `EngineSDK/inc/FX/`에 반영 확인.
- `/utf-8` 누락 시 한글 주석 CP949 오판(gotchas 2026-06-04/UTF-8 메모) — vcxproj 확인.

### 7.2 게이트 막힐 때 대응

| 막힘 | 대응 |
|---|---|
| G2 미충족(static mesh 프리뷰 안 됨) | FX 패널 확장 중단, RHI-02~04 선행 |
| G6 bake 실패 | gameplay cue(S5) 중단. bake 실패해도 **기존 emitters 유지**(파괴 금지) |
| G9 서버 권위 미연결 | client-only gameplay FX 금지. visual-only(`fx.authority=LocalVisual`)만 |
| 셰이더 ABI 흔들림 | `CBFxParams` 변경 금지. bake는 `MakeFxParamsFromMaterial` 경유 |

### 7.3 과설계 방지

- `.md/plan/EffectTool/01_ARCHITECTURE.md`의 풀세트(GPU/Expression VM/DI/Sim Stage)는 **초기 도입 금지**.
  S0~S4는 **CPU plan + bake**만. GPU compute/Expression VM은 G6 통과 + 프로파일 근거 후 별도 세션.
- `FxEmitterDesc`가 이미 풍부하므로 **새 런타임 컴포넌트 신설을 최소화**. 1차 EmitterInstance는 `FxBillboardComponent` 재사용.
- 핀 단위 그래프(타입별 핀 연결)는 P2. 초기엔 노드=스테이지 묶음 + 단순 DAG 엣지로 충분.
- DI(Data Interface)는 "메시 표면 스폰"이 실제로 필요할 때만. 초기엔 shape 프리셋(Ring/Box/Sphere)으로 대체.

---

## 8. Codex 요구사항 프롬프트 (복붙용)

```text
SYSTEM=FX_NIAGARA  (FX Editor — Niagara급 .wfx 그래프 + CPU 실행 계획 + bake)

너는 Winters 엔진에 UE5 Niagara를 reference로만 삼아 .wfx 노드 그래프 FX 에디터를 구축하는
시니어 엔진/툴 엔지니어다. 코드 복사 없이 Winters .w* contract + Winters runtime 으로 구현한다.

[절대 원칙 — 위반 시 작업 무효]
1. UE5 Niagara 는 개념/UX/책임분리 관찰용 reference. 코드 복사/모듈 링크/object model 이식 금지.
2. "에디터 화면 먼저 크게" 금지. runtime contract(.wfx 포맷 + bake + CPU plan)를 작게 증명 →
   에디터가 그 contract 를 편집. 모든 패널 완료기준 = "편집→Save .wfx→재실행 로드→프리뷰" 왕복.
3. 에디터가 만든 .wfx 는 CAssetStreamingSystem(현재는 CFxAssetRegistry::LoadFromFile baseline)을 거쳐 로드.
   에디터 전용 우회 경로 금지. gameplay FX 판정/데미지/페이즈는 Server GameSim(presentation/truth 분리).
   visual-only=클라 로컬 seed, gameplay-affecting=서버 event id+seed.
4. Engine→Client 의존 역전 금지. Client/Public·Shared 에 DX11/DX12 concrete type 노출 금지.
5. 셰이더 ABI(CBFxParams, FxEmitterDesc, MakeFxParamsFromMaterial) 변경 금지. bake 는 이 ABI 통과.

[환경]
- 저장소: C:/Users/tnest/Desktop/Winters
- 셰이더: Shaders/FxSprite.hlsl, FxMesh.hlsl   (CBFxParams b2 ABI 고정)
- 런타임: Engine/Public/FX/FxAsset.h(FxEmitterDesc/FxNodeDesc/FxAsset),
          Engine/Public/FX/FxMaterialDesc.h, Engine/Public/Renderer/FxShaderConstants.h,
          Client/.../GameObject/FX/{FxSystem,FxBillboardComponent,FxCuePlayer}
- 문서 모델: Client/Private/GameObject/FX/WfxDocument.cpp  (.wfx JSON load/save — graph block 확장 지점)
- 미구현(신규): CFxGraph, CFxGraphValidator, CFxGraphCompiler, CFxExecPlan, CFxParticlePool,
  CFxEmitterInstance, CWfxGraphDocument, FxGraphEditor 패널, Tools/.../fxr_to_wfx.py

[먼저 읽을 문서 — 순서대로]
1. .md/EldenRing/18_FX_EDITOR_NIAGARA_WFX_DETAILED_DESIGN.md  ← 이 설계(스키마/계층/게이트/스켈레톤)
2. .md/EldenRing/17_UE5_GRADE_EDITOR_SUITE_MASTER.md (FX 섹션 2.2 / 게이트)
3. .md/EldenRing/12_UE5_REFERENCE_DX12_RHI_EDITOR_BIG_PICTURE.md (Phase F / G6 / G9)
4. .md/EldenRing/06_FX_GRAPH_SEQUENCER_EDITOR.md, plan/EldenRingEditor/05_NIAGARA_WFX_FX_EDITOR.md
5. .md/EldenRing/10_ASSET_PIPELINE_TOOLING.md (FXR), Tools/EldenAssetPipeline/elden_pipeline.py(643~ FXR 파서)
6. CLAUDE.md / .claude/gotchas.md
   (참고만: plan/EffectTool/01~28 — 풀세트 Niagara 클론. 과설계라 초기 도입 금지, 개념만 참조.)

[작업 범위 — FX_NIAGARA]
- .wfx v2 스키마(emitters[] + optional graph 블록). graph 없는 v1 .wfx 도 로드.
- CFxGraph(노드/엣지/userParams) + CFxGraphValidator(DAG/타입/미연결 Render).
- BakeGraphToEmitters(): 그래프 → FxEmitterDesc[]. 실패해도 기존 emitters 유지.
- CFxExecPlan + CFxParticlePool(SoA) + CFxEmitterInstance::Tick(dt) — CPU 시뮬(프리뷰).
- FxGraphEditor 패널: 캔버스/inspector/preview play-stop-restart/texture picker.
- 노드 초기 세트: SpawnBurst/SpawnRate, InitPosition/Velocity/Lifetime/Color,
  Age/Gravity/Drag/SizeOverLife/ColorOverLife, Billboard/Mesh/RibbonRenderer.
- FXR→.wfx: elden_pipeline.py 의 integerCandidate → mesh/tex id 매핑 후보 emit(불확실은 candidate note).

[작업 루프 — 게이트 통과까지]
1. 선행 게이트 확인: G2(static mesh 프리뷰)·RHI-02(texture+light). 미충족이면 FX 패널 확장 멈추고 보고.
2. runtime contract 먼저: .wfx v2 load/save(graph block) + BakeGraphToEmitters → 기존 CFxSystem 으로 1회 재생.
3. CPU plan(CFxExecPlan/ParticlePool) → 프리뷰 뷰포트 시뮬. bake 실패 시 emitter desc 유지.
4. 에디터 패널: 편집→Save→재실행 로드→프리뷰 왕복 완료기준 충족.
5. 서버 cue 검증: bake된 FxAsset 을 event{id,seed}로 1회 재생(FxCuePlayer 무수정). 중복/클라발동 없음 확인.
6. 막히면 사유 분류 보고(서버 권위/의존 역전/ABI/게이트 미통과). 나머지는 계속.

[완료 기준]
- graph 없는 기존 .wfx 정상 로드 + graph block 라운드트립.
- burst billboard 그래프 → bake → 기존 CFxSystem 으로 화면 1회 재생(G6).
- CFxExecPlan 으로 프리뷰 뷰포트에서 N파티클 age/gravity/sizeOverLife 시뮬.
- FxGraphEditor 편집→Save→재실행 로드→프리뷰 왕복.
- gameplay FX 가 서버 event{id,seed}로 1회만 재생(G9, 클라발동/중복 없음).
- .fxr 1개 → .wfx(emitters[]) 산출 + 에디터 로드(candidate 매핑은 수동 확정 표시).

[빌드 검증]
$MSB = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe'
& $MSB Winters.sln /t:Engine /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
& $MSB Winters.sln /t:Client /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
& $MSB Winters.sln /t:EldenRingEditor /m /p:Configuration=Debug-DX12 /p:Platform=x64 /v:minimal
git diff --check
# Engine public header(FX/Graph, FX/Exec) 추가 시 UpdateLib.bat 로 EngineSDK/inc 동기화 확인.
# vcxproj /utf-8 확인(한글 주석 CP949 오판 방지).

[금지 사항]
- CBFxParams / FxEmitterDesc / MakeFxParamsFromMaterial ABI 변경.
- 초기 단계 GPU compute 시뮬 / Expression VM / Data Interface 풀세트 도입(과설계).
- FxCuePlayer 서버 cue 재생 경로 수정(이번 범위 아님).
- graph 없는 기존 .wfx 로드 깨기 / bake 실패 시 emitter desc 파괴.
- 정상 LoL F5 FX(FxSystem/기존 .wfx) 회귀 — Client 빌드 + visual smoke 유지.
- Client/Public·Shared 에 DX11/DX12 concrete type 노출, Engine→Client 의존 역전.

[시작]
지금: (1) 위 문서와 근거 코드를 읽고, (2) G2/RHI-02 선행 충족 여부와 현재 .wfx/FxSystem 상태를 집계 보고,
(3) S0(.wfx v2 + CWfxGraphDocument) 부터 시작하라. 막히면 사유 분류 보고하고 나머지는 계속 진행하라.
```
