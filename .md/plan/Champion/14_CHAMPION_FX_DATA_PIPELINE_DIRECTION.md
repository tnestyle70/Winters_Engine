Session - LeeSin 반영 전에 champion FX를 데이터/툴/에디터 친화 파이프라인으로 고정한다.

1. 반영해야 하는 코드

1-1. 방향 결정

이 문서는 LeeSin FX를 바로 C++ 하드코딩으로 늘리기 전에, Winters의 champion FX 제작 방식을 어디까지 데이터화할지 고정하는 기준이다.

결론:
- 지금 잡고 간다. LeeSin 이후 champion 수가 늘어난 뒤에는 C++ preset, projectile 예외, resource 경로, timing 값이 퍼져서 수정 비용이 커진다.
- Riot과 동일한 내부 구현을 목표로 하지 않는다. 목표는 같은 운영 원칙이다: server authoritative gameplay, cue-driven client visual, designer-editable effect data.
- 완성형 node editor를 지금 만들지 않는다. 먼저 source-controlled `.wfx`/JSON 데이터, validator, runtime loader, hot reload 준비선까지 만든다.
- LeeSin은 첫 "데이터 기반 champion FX" 샘플로 삼는다. Irelia 구조는 버리지 않고, `FxPresets`를 얇은 adapter로 줄이는 방향으로 성숙시킨다.

1-2. 현재 구조에서 유지할 것

현재 champion 구조의 큰 분리는 유지한다.

```text
Client Input
-> GameCommand
-> Server GameSim
-> Snapshot/Event/EffectTrigger
-> Client EventApplier
-> VisualHookRegistry
-> Champion Visual Adapter
-> FxAsset/FxSystem spawn
```

유지 이유:
- server gameplay와 client visual의 권한 경계가 이미 맞다.
- Irelia의 `Registration + Skills + FxPresets` 분리는 developer ownership이 분명하다.
- `Engine/Public/FX/FxAsset.h`, `Client/Public/GameObject/FX/LegacyFxAdapter.h`, `CFxSystem::SpawnFromAsset`가 이미 점진 migration의 발판을 갖고 있다.

1-3. 현재 구조에서 고칠 것

다음 문제는 LeeSin 반영 전에 방향을 고정한다.

```text
문제:
- FxPresets.cpp에 texture path, size, lifetime, color, blend, offset이 박힌다.
- EventApplier.cpp에 champion projectile switch가 계속 늘어난다.
- Registration과 ChampionRuntimeDefaults에 skill timing/range가 중복된다.
- Client/Bin/Resource와 Client/Bin/Data는 git ignore 대상이라 협업 재현성이 약하다.
- designer/TA가 FX 수치를 조정하려면 C++ 수정과 rebuild가 필요하다.
```

해결 방향:
- gameplay truth는 계속 Shared/GameSim C++에 둔다.
- visual composition은 `.wfx`/JSON 데이터로 뺀다.
- C++ champion visual code는 cue id와 spawn context만 넘기는 thin adapter가 된다.
- source-controlled authoring data는 `Data/LoL/FX/...` 같은 tracked 경로에 둔다.
- cooked/runtime copy는 `Client/Bin/...`에 둘 수 있지만 source of truth로 보지 않는다.

1-4. 목표 ownership

```text
Server/GameSim owner:
- skill validation
- hit validation
- damage
- dash/movement truth
- slow/stun/mark/status
- cooldown/mana/rank
- effect cue emission timing

Client champion visual owner:
- cue id mapping
- local presentation fallback only when explicitly offline/smoke
- server cue context -> visual context conversion
- animation/FX/sound playback request

FX data owner:
- texture/model path
- render type
- layer/emitter list
- size/color/lifetime/fade
- attach mode/offset
- blend/depth/material parameters
- atlas/uv scroll
- designer-visible labels/tags

Tool/editor owner:
- data validation
- preview
- hot reload
- eventually graph/timeline editing
```

1-5. 데이터 경로 규칙

권장 source-controlled 경로:

```text
Data/LoL/FX/Champions/LeeSin/q_cast.wfx
Data/LoL/FX/Champions/LeeSin/q_hit.wfx
Data/LoL/FX/Champions/LeeSin/q_mark.wfx
Data/LoL/FX/Champions/LeeSin/q2_dash.wfx
Data/LoL/FX/Champions/LeeSin/w_shield.wfx
Data/LoL/FX/Champions/LeeSin/w2_iron_will.wfx
Data/LoL/FX/Champions/LeeSin/e_impact.wfx
Data/LoL/FX/Champions/LeeSin/e2_slow.wfx
Data/LoL/FX/Champions/LeeSin/r_kick.wfx
```

주의:
- `Client/Bin/Resource`는 `.gitignore`의 `**/Bin/Resource/`에 걸린다.
- `Client/Bin/Data`도 `.gitignore`에 걸린다.
- 그러므로 authoring data는 `Bin` 아래에 두지 않는다.
- runtime copy/cook 결과만 `Bin`으로 보낸다.

1-6. MVP `.wfx` 형태

처음부터 Niagara급 graph를 요구하지 않는다. MVP는 "여러 layer를 가진 effect asset"이면 충분하다.

예시:

```json
{
  "schema": "Winters.FxCue.v1",
  "name": "LeeSin.Q.Cast",
  "category": "Champion/LeeSin",
  "gameplayDeterministic": false,
  "emitters": [
    {
      "name": "cast_shockwave",
      "renderType": "Billboard",
      "texture": "Client/Bin/Resource/Texture/Character/LeeSin/particles/leesin_base_q_cast_shockwave.png",
      "attach": "Source",
      "offset": [0.0, 1.0, 0.9],
      "size": [2.0, 1.0],
      "lifetime": 0.28,
      "fadeIn": 0.02,
      "fadeOut": 0.18,
      "blend": "Additive",
      "billboard": true,
      "color": [0.65, 0.90, 1.45, 0.78]
    }
  ]
}
```

MVP 필수 필드:

```text
schema
name
category
emitters[]
emitters[].name
emitters[].renderType
emitters[].texture or model
emitters[].attach
emitters[].offset
emitters[].size or scale
emitters[].lifetime
emitters[].fadeIn/fadeOut
emitters[].blend
emitters[].billboard
emitters[].color
```

나중에 추가할 필드:

```text
startDelay
atlas
uvScroll
alphaClip
erodeThreshold
depthMode
sound
cameraShake
visibility/fog policy
team color override
quality tier
lod
```

1-7. C++ adapter 목표

Champion C++은 다음 정도만 남긴다.

```text
LeeSin::Visual::OnCastFrame_Q_Visual(ctx)
-> Resolve source/target/position/forward/stage
-> CFxCuePlayer::Play("LeeSin.Q.Cast", context)
```

남기지 않을 것:

```text
const wchar_t* kPathQCastShockwave = ...
fx.fWidth = ...
fx.fHeight = ...
fx.vColor = ...
fx.blendMode = ...
```

단, transition 기간에는 `FxPresets`가 존재할 수 있다.

transition rule:
- 새 champion은 가능하면 data cue 우선.
- 기존 Irelia/Yasuo/Annie/Yone preset은 `LegacyFxAdapter`로 asset화 후 점진 이관.
- hardcoded preset은 "temporary adapter"로 표시하고 새 수치 추가를 피한다.

1-8. Projectile visual 분리

`Client/Private/Network/Client/EventApplier.cpp`에 champion별 projectile texture switch를 계속 추가하지 않는다.

목표:

```text
ProjectileKind -> FxCueTag
LeeSinQ -> LeeSin.Q.Projectile
MysticShot -> Ezreal.Q.Projectile
AsheVolley -> Ashe.W.Projectile
```

EventApplier의 책임:
- projectile spawn/hit cue를 받는다.
- projectile kind와 cue context를 registry에 넘긴다.
- generic fallback만 가진다.

Champion/module registry의 책임:
- projectile kind별 visual asset/cue를 등록한다.

1-9. Skill timing/range 중복 축소

단기:
- `ChampionRuntimeDefaults`와 `Registration`의 값 차이를 검증 로그로 잡는다.
- LeeSin을 반영할 때 range/cooldown/lock/stage window는 한 표에서 먼저 정한다.

중기:
- gameplay-affecting value는 server/shared data가 source of truth다.
- client `SkillDef`는 visual/input 편의를 위해 cache하되, server와 다르면 validation 경고를 낸다.

금지:
- client registration만 수정해서 사거리/쿨다운이 바뀐 것처럼 보이게 만들지 않는다.
- FX 크기를 gameplay radius처럼 쓰지 않는다. FX radius는 시각화이고, gameplay radius는 GameSim truth다.

1-10. LeeSin 적용 순서

LeeSin은 아래 순서로 간다.

```text
1. Data/LoL/FX/Champions/LeeSin 에 MVP .wfx 작성
2. loader/registry가 없으면 최소 loader stub 또는 LegacyFxAdapter path로 임시 흡수
3. LeeSin visual hook은 data cue tag만 호출
4. server LeeSinGameSim은 Q mark/Q2/W/E/R gameplay truth만 담당
5. projectile visual은 EventApplier switch 확장 대신 registry 경계부터 만든다
6. F5에서 server cue 1회 -> client FX 1회 재생 확인
```

LeeSin cue tag 권장:

```text
LeeSin.BA.Hit
LeeSin.Q.Cast
LeeSin.Q.Projectile
LeeSin.Q.Hit
LeeSin.Q.Mark
LeeSin.Q2.Dash
LeeSin.W.Shield
LeeSin.W2.IronWill
LeeSin.E.Impact
LeeSin.E2.Slow
LeeSin.R.Kick
```

1-11. Designer/TA 친화 기준

기획자/디자이너가 C++ 없이 바꿀 수 있어야 하는 것:

```text
texture/model 선택
layer on/off
size
color/tint
lifetime
fade
blend
start delay
attach offset
ground vs billboard
atlas fps
```

개발자가 유지해야 하는 것:

```text
server cue emission
target/source context
gameplay radius/damage/status
renderer feature support
asset validation
crash-safe fallback
performance budget
```

1-12. Build/memory 협업 기준

Build:
- FX 수치 조정은 C++ rebuild 없이 가능해야 한다.
- `.wfx` 변경은 hot reload 또는 reload button으로 확인한다.
- C++ rebuild는 새 renderer feature, 새 gameplay cue, 새 schema field가 필요할 때만 한다.

Memory:
- runtime spawn마다 path string을 새로 만들지 않는다.
- asset registry가 string/path/material data를 소유하고, spawn instance는 handle/context 중심으로 간다.
- transition 기간의 raw path는 허용하되 final direction은 `FxAssetHandle`이다.
- champion마다 같은 texture를 중복 load하지 않는다.

협업:
- engineer는 cue schema/loader/renderer를 소유한다.
- gameplay engineer는 GameSim truth와 cue emission을 소유한다.
- technical artist/designer는 `.wfx`와 preview tuning을 소유한다.
- review는 C++ review와 FX data review를 분리한다.

1-13. Tool/editor 단계

지금 당장 하지 않을 것:

```text
full node graph editor
GPU compute particle graph
timeline sequencer
complex expression VM
boss-scale Elden magic editor
```

지금 해야 할 것:

```text
Stage A: .wfx JSON v1 schema
Stage B: LoadFxAssetFromFile 구현/보강
Stage C: FxAssetRegistry LoadDirectory/ReloadFromFile 검증
Stage D: ChampionFxCueRegistry 또는 FxCuePlayer 도입
Stage E: LeeSin Q/W/E/R sample data 작성
Stage F: missing texture/schema error validation
Stage G: simple ImGui reload/preview panel
```

Elden 전 목표:
- LoL champion FX가 C++ hardcoded preset 없이 한 champion 이상 돌아간다.
- LeeSin 또는 Irelia 하나가 data-driven champion FX reference가 된다.
- EffectTool 대형 계획은 이 MVP 위에 얹는다.

1-14. Riot/AAA 비교 기준

Riot과 동일한 구조를 추측해서 따라 하지 않는다.

따라 할 원칙:
- gameplay 판정과 visual은 분리한다.
- visual은 cue/event와 data asset으로 움직인다.
- designer/TA가 수치와 layer를 빠르게 조정한다.
- programmer는 runtime contract와 performance budget을 지킨다.
- data validation이 build/runtime 사고를 막는다.

Winters식 적용:
- 서버 권위는 `Shared/GameSim`을 유지한다.
- client visual은 `VisualHookRegistry`와 `EffectTrigger` single-source를 유지한다.
- FX data는 `FxAsset`/`.wfx`/`LegacyFxAdapter` 방향으로 이관한다.
- full editor는 MVP data가 충분히 쌓인 뒤 만든다.

1-15. 반영 우선순위

```text
P0: 방향 고정
- 이 문서를 기준으로 LeeSin FX plan을 다시 쓴다.
- 새 champion hardcoded FxPresets 추가를 최소화한다.

P1: Data MVP
- .wfx JSON v1 loader/validator
- FxCue tag -> FxAssetHandle lookup
- LeeSin Q/W/E/R 최소 data cue

P2: Client integration
- VisualHookRegistry에서 cue tag 호출
- Projectile visual registry
- EventApplier switch 증가 중단

P3: Tooling
- reload all
- missing asset/error panel
- simple preview spawn

P4: Migration
- Irelia hardcoded layers를 data로 옮긴다.
- Yasuo/Annie/Yone legacy preset도 asset화한다.
```

2. 검증

문서 검증:
- 이 문서의 source of truth 경로가 `Bin` 아래가 아닌지 확인한다.
- LeeSin 계획을 다시 쓸 때 `FxPresets.cpp`에 새 texture/size/color 하드코딩이 늘지 않는지 확인한다.
- `EventApplier.cpp`에 LeeSin projectile 전용 switch가 추가되는 대신 registry 경계가 생기는지 확인한다.

구현 검증 명령:

```text
git diff --check
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64
```

수동 검증:
- `.wfx` 파일 하나 수정 후 C++ rebuild 없이 reload로 visual 값이 바뀌는지 확인한다.
- server `EffectTrigger` 1회가 client FX 1회로만 재생되는지 확인한다.
- missing texture path가 crash가 아니라 validation error로 잡히는지 확인한다.
- `Client/Bin/Resource`에만 있는 raw resource 때문에 다른 작업자가 재현 실패하지 않는지 확인한다.

확인 필요:
- `Data/LoL/FX` 루트 디렉터리를 새로 tracked source data 경로로 확정할지 팀 결정이 필요하다.
- `.wfx` JSON parser를 existing `LoadFxAssetFromFile`에 붙일지, champion-specific cue loader를 먼저 둘지 결정이 필요하다.
- runtime cooked copy를 `Client/Bin/Data`로 할 경우, ignore 정책과 packaging script가 필요하다.
