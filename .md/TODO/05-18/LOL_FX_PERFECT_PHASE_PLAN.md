# LoL FX Perfect Phase Plan

## Goal

LoL FX를 서버 권위 결과와 분리된 client visual layer에서 강화한다. gameplay truth는 `Shared/GameSim`과 server cue 흐름에 남기고, 이 계획은 visual playback, material tuning, lifecycle, editor tooling만 다룬다.

## Phase 0 - Baseline And Guardrails

- 기존 `FxBillboardComponent`, `FxMeshComponent`, `FxBeamComponent`, `FxRibbonComponent`의 lifetime/fade/material 필드를 유지한다.
- FX spawn은 server cue 또는 `VisualHookRegistry` visual path에서만 발생하게 유지한다.
- champion skill gameplay hook에 damage/range/cooldown 같은 결과 로직을 추가하지 않는다.
- `FxLegacyManifest`의 hook 단위를 기준으로 우선순위를 잡는다.

Success:
- FX 코드 변경 후 client build 통과.
- server-authoritative flow에 gameplay mutation 추가 0.
- LoL visual preset 변경이 `.wfx/.wmi` 전환으로 이어질 수 있는 필드만 사용.

## Phase 1 - Master Material Core Knobs

- Sprite/Billboard도 Mesh와 같은 `elapsed`, `normalized age`, `magic surface`, `rim`, `dissolve`, `UV pan` 표현을 사용한다.
- `FxMaterialDesc`에 `LOLMagicSurface` style mode를 명시한다.
- preset helper로 반복되는 LoL magic surface 세팅을 한 곳으로 모은다.
- Ezreal BA/Q를 첫 검증 slice로 적용한다.

Success:
- `FxSprite.hlsl`와 DX12 embedded sprite shader가 같은 style mode를 지원한다.
- Ezreal BA/Q billboard가 age 기반 dissolve/emission/rim을 사용한다.
- 기존 style mode 0 billboard는 시각 결과 유지.

## Phase 2 - Lifecycle State

- 단순 `elapsed >= lifetime -> DestroyEntity` 흐름 위에 visual lifecycle adapter를 추가한다.
- Spawn, Active, Completing, Complete 개념을 component field 또는 system-local state로 넣는다.
- Completing에서는 새 spawn을 멈추고 잔여 visual만 자연 소멸시킨다.
- loop FX와 one-shot FX를 분리한다.

Success:
- short burst, projectile trail, attached aura가 같은 lifecycle vocabulary를 사용한다.
- hard pop destroy가 감소한다.
- lifetime/fade 파라미터가 ImGui debug에서 확인 가능하다.

## Phase 3 - Data-Driven Tuning

- `.wfx` emitter field에 core knob를 round-trip한다.
- `.wmi` 또는 현재 material desc bridge로 UV pan, dissolve, emission, color over life를 편집 가능하게 한다.
- hot reload는 asset reload -> spawned instance reset 순서로 제한한다.

Success:
- C++ 재빌드 없이 1개 champion skill FX의 색/소멸/스크롤 조절 가능.
- legacy hardcoded preset과 dumped asset 결과가 의미상 일치한다.

## Phase 4 - Champion Visual Pass

- Priority order: Ezreal, Irelia, Yasuo, Yone, Zed, Riven, Garen, Annie, Ashe, Kalista, Jax.
- 각 챔프는 BA, Q, W, E, R, passive/mark 중 최소 5 hook을 phase/timeline으로 분리한다.
- 각 skill은 windup, cast, travel, impact, fadeout 중 필요한 단계만 가진다.

Success:
- 각 champion마다 1개 대표 skill이 old flat FX와 비교해 3 layer 이상으로 구성된다.
- impact/hit feedback이 projectile/trail과 분리된다.
- readability를 해치는 overdraw는 budget panel에서 확인한다.

## Phase 5 - Combat Readability

- team color, enemy danger, ally support, neutral ambient FX 기준을 만든다.
- Fog of War와 hitbox clarity를 깨는 alpha/scale을 제한한다.
- AoE telegraph와 pure flourish를 분리한다.

Success:
- 스킬 판정 범위와 장식 이펙트가 화면에서 구분된다.
- 한타 50+ FX에서도 주요 projectile/CC/ultimate가 읽힌다.

## Phase 6 - Effect Tuner MVP

- ImGui panel: Asset, Instance, Emitter, Material, Curve, Budget.
- 우선 Graph editor가 아니라 Stack/Parameter/Curve/Viewport를 만든다.
- Graph panel은 custom material/compiler 단계까지 보류한다.

Success:
- live preview에서 core knob 4개를 200ms 이내 반영한다.
- selected instance lifetime, normalized age, material style, active layer 수를 표시한다.

## Phase 7 - Runtime Budget And Render Quality

- LoL domain budget: 4096 particles, 16 emitters, 5ms visual budget를 InitDesc로 주입한다.
- Additive는 sort 생략, AlphaBlend는 필요 시만 sort한다.
- Beam/Ribbon/Mesh/Sprite별 render snapshot을 분리한다.

Success:
- 한타 stress scene에서 FX frame cost를 측정한다.
- worst-case에서도 gameplay update와 render가 분리된다.

## Phase 8 - Elden Bridge

- LoL에서 검증한 lifecycle, material knob, tuner를 Elden domain InitDesc로 확장한다.
- Elden에서는 decal/light/volumetric/smoke/sword trail/shockwave를 우선한다.
- 노드 그래프와 GPU compute는 Elden boss telegraph부터 본격 적용한다.

Success:
- LoL 시스템을 버리지 않고 Elden boss FX로 확장한다.
- EffectTool의 최종 형태가 LoL 실전 slice와 연결된다.
