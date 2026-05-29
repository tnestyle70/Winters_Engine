# FX Tool MVP WFX SoA Renderer Preview

Session - `.wfx` JSON round-trip, runtime SoA, renderer 3종, ImGui preview로 FX Tool MVP를 완성한다.

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Engine/Public/FX/FxAsset.h

목표:
- legacy preset field와 graph 기반 `.wfx` asset이 같은 runtime asset으로 들어온다.

반영:
- `FxAsset`, `FxEmitterDesc`, `FxNodeDesc`, `CFxParameterMap`을 `.wfx` JSON schema와 1:1로 맞춘다.
- emitter render type은 MVP에서 `Billboard`, `Ribbon`, `Beam` 3종을 우선 통과시킨다.
- mesh particle, decal, shockwave ring은 schema에는 남기되 MVP 검증 필수에서 제외한다.

### 1-2. C:/Users/user/Desktop/Winters/Engine/Private/FX/FxAsset.cpp

목표:
- `.wfx` load/save round-trip이 deterministic하다.

반영:
- JSON load -> `FxAsset` -> JSON save -> reload 결과가 field 단위로 같아야 한다.
- unknown field는 MVP에서 fail-fast로 처리한다.
- asset name 기반 `RegisterOrReplaceByName`이 hot reload preview에 사용할 수 있게 한다.

### 1-3. C:/Users/user/Desktop/Winters/Engine/Public/FX/ParticlePool.h

목표:
- runtime particle storage는 SoA와 swap-back kill로 유지한다.

반영:
- particle id, age, lifetime, position, velocity, color, size를 MVP attribute로 고정한다.
- free list보다 dense array iteration을 우선한다.
- renderer upload용 stable view를 제공한다.

### 1-4. C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxSystem.cpp

목표:
- billboard renderer path가 `.wfx` asset에서 직접 spawn된다.

반영:
- emitter lifetime, spawn rate, burst, fade in/out, atlas animation을 `.wfx` 값에서 읽는다.
- fog-of-war visibility와 local team visibility filter를 적용한다.
- server cue로 spawn된 FX와 local preview FX를 owner/source로 구분한다.

### 1-5. C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxRibbonSystem.cpp

새 파일:
- ribbon renderer MVP를 만든다.

반영:
- trail point buffer, width, color over life, fade out을 지원한다.
- Yasuo/Yone류 slash trail 또는 projectile trail에 붙여 검증한다.

확인 필요:
- 현재 ribbon 관련 파일명이 이미 있으면 기존 파일을 확장하고 새 파일을 만들지 않는다.

### 1-6. C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxBeamSystem.cpp

목표:
- beam renderer는 start/end offset과 attach entity를 지원한다.

반영:
- caster/target entity transform을 읽어 start/end를 계산한다.
- target이 사라지면 beam lifetime을 fade out으로 종료한다.
- wind wall blockable flag는 LoL path에서 후속 검증한다.

### 1-7. C:/Users/user/Desktop/Winters/Client/Private/UI/EffectToolPanel.cpp

새 파일:
- ImGui 기반 EffectTool MVP preview panel을 만든다.

반영:
- asset open/save, emitter list, parameter edit, play/stop/restart, viewport spawn position을 제공한다.
- graph editor는 MVP에서 full node editor가 아니라 stack/list editor로 시작한다.
- preview scene은 game server authority path와 분리된 tool-only path로 둔다.

### 1-8. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

목표:
- server cue에서 `.wfx` asset을 spawn할 수 있다.

반영:
- cue tag -> fx asset name registry를 둔다.
- actionSeq/cue id 기반 de-dup으로 중복 재생을 막는다.
- cue payload가 없는 legacy skill은 임시 bridge로 처리하되 authoritative result를 만들지 않는다.

## 2. 검증

검증 명령:
- `git diff --check`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`

에셋 검증:
- sample `.wfx` 3개 생성: billboard burst, ribbon slash, beam tether.
- load -> save -> reload round-trip field equality 확인.
- invalid JSON, unknown render type, missing texture path fail-fast 확인.

런타임 검증:
- ImGui preview에서 3종 renderer가 재생된다.
- LoL skill cue 1개를 `.wfx`로 교체해 server event -> client FX까지 확인한다.
- 한타 smoke에서 particle pool이 leak 없이 재사용된다.
