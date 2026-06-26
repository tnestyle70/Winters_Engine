# Raw Asset Path Code Audit Result - 2026-06-24

## 기준 문서

- `AGENTS.md`: runtime resource는 `Client/Bin/Resource`에서 해석하되 config별 Resource 복사는 금지한다.
- `.md/architecture/WINTERS_CODEBASE_COMPASS.md`: JSON은 authoring/cook input이고 runtime frame code는 검증된 immutable pack을 읽는다.
- `.md/plan/collab-pipeline/00_INDEX_DATA_DRIVEN_ENTITY_PIPELINE.md`: 현재 data path는 있으나 assembly가 hardcoded인 gap이 남아 있다.
- `.md/plan/engine/asset-pipeline/2026-05-21_SESSION_01_ASSET_IDENTITY_MANIFEST.md`: Asset identity, virtual path, cooked path, dependency manifest 방향을 기준으로 삼는다.
- `.md/plan/refactor/S1_FOLDER_CONTRACT_CODE_CONCEPT.md`: designer-changeable value와 presentation source는 Data 쪽으로 빼고, tool output을 source로 취급하지 않는다.

## 조사 범위

대상:
- `Client`, `Engine`, `Server`, `Shared`
- `*.cpp`, `*.h`, `*.hpp`, `*.inl`

제외:
- `Client/Bin/**`
- `EngineSDK/**`
- `Engine/External/**`
- `Engine/ThirdPartyLib/**`
- `**/Generated/**`

검색 기준:

```powershell
rg -n --fixed-strings "Client/Bin/Resource" Client Engine Server Shared `
  -g "*.cpp" -g "*.h" -g "*.hpp" -g "*.inl" `
  --glob "!Client/Bin/**" `
  --glob "!EngineSDK/**" `
  --glob "!Engine/External/**" `
  --glob "!Engine/ThirdPartyLib/**" `
  --glob "!**/Generated/**"
```

## 전수 조사 baseline

현재 non-generated code 기준 raw resource path 후보는 53개 파일, 328회다.

상위 분포:

| Top | File count |
| --- | ---: |
| Client | 51 |
| Engine | 2 |

상위 파일:

| Occurrences | File |
| ---: | --- |
| 34 | `Client/Private/Scene/LobbyRosterHelpers.cpp` |
| 33 | `Client/Private/GameObject/ChampionTable.cpp` |
| 27 | `Client/Private/GameObject/Champion/Irelia/IreliaFxPresets.cpp` |
| 22 | `Client/Private/Scene/Scene_InGameLifecycle.cpp` |
| 16 | `Client/Private/GameObject/Champion/Ashe/Ashe_FxPresets.cpp` |
| 14 | `Client/Private/GameObject/Champion/Jax/Jax_FxPresets.cpp` |
| 13 | `Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp` |
| 13 | `Client/Private/Manager/Jungle_Manager.cpp` |
| 10 | `Client/Private/GameObject/Champion/Yone/Yone_Registration.cpp` |
| 10 | `Client/Private/Manager/Minion_Manager.cpp` |
| 8 | `Client/Private/GameObject/Champion/Annie/Annie_FxPresets.cpp` |
| 8 | `Client/Private/GameObject/Champion/Fiora/Fiora_FxPresets.cpp` |
| 7 | `Client/Private/ECS/Systems/YoneSoulSpawnSystem.cpp` |
| 7 | `Client/Private/GameObject/Champion/Kalista/KalistaFxPresets.cpp` |
| 7 | `Client/Private/UI/EffectTuner.cpp` |
| 6 | `Client/Private/GameObject/Champion/Garen/GarenFxPresets.cpp` |
| 6 | `Client/Private/GameObject/Champion/Irelia/Irelia_Registration.cpp` |
| 6 | `Client/Private/GameObject/Champion/Sylas/SylasRegistration.cpp` |
| 6 | `Client/Private/Manager/Structure_Manager.cpp` |

## 본질 분류

`Client/Bin/Resource` 자체는 런타임 기준 루트다. 문제는 제품 asset 선택 정보가 C++ 코드 곳곳에 흩어져 있다는 점이다.

유지 가능한 예외:
- resource root resolver
- Engine generic fallback asset
- validation/tool script
- generated output

이관 대상:
- Structure, jungle, minion, ambient prop, map visual assembly
- Champion model, texture, loading, portrait, roster image
- Champion FX preset asset path
- Projectile visual asset path
- LoL product UI image path

주의 대상:
- `Engine/Private/Manager/UI/UI_Manager.cpp`, `Engine/Private/Manager/UI/LuaUIHost.cpp`는 Engine generic fallback과 LoL product asset을 분리해서 판단해야 한다.
- Shader path는 designer product content path와 다르다. RHI/Engine shader registry 또는 renderer bootstrap으로 따로 분류한다.

## 리팩터링 방향

기존에 이미 있는 축을 확장한다.

- Source data: `Data/LoL/ClientPublic/Visual`
- Generator: `Tools/LoLData/Build-LoLDefinitionPack.py`
- Runtime generated pack: `Client/Private/Data/Generated/LoLVisualDefinitions.generated.cpp`
- Runtime query surface: `Client/Private/Data/LoLVisualDefinitionPack.h`

새 registry를 병렬로 만들지 않는다. asset path도 DOD table로 다룬다.

핵심 row 예시:
- `StructureVisualDefinition`: kind, team, mesh, shader, submesh visibility state
- `ChampionVisualDefinition`: champion key, model asset, texture slots, portrait/loading image, skill timing
- `FxCueVisualDefinition`: cue key/id, mesh/texture/wfx, spawn policy, playback policy
- `ObjectVisualDefinition`: minion/jungle/ambient kind, mesh/material/texture, scale/yaw

런타임 frame code는 JSON을 읽지 않는다. scene enter, spawn, preload 단계에서 generated pack 또는 cooked manifest를 통해 handle/path를 resolve하고, frame update는 resolved handle만 사용한다.

## 첫 번째 slice

1. `Structure_Manager.cpp`부터 진행한다.
2. 이유는 raw path가 6회로 작고, Blue Nexus texture bug가 이미 submesh/material semantic 문제였기 때문이다.
3. `ObjectVisualDefs.json`에 Nexus/Inhibitor/Turret Blue/Red visual row를 만든다.
4. `LoLVisualDefinitionPack`에 `StructureVisualDefinition` 조회를 추가한다.
5. `Structure_Manager.cpp`는 `FindStructureVisualDefinition(kind, team)`만 호출한다.
6. Nexus alive/destroyed submesh index는 C++ 상수가 아니라 structure visual row의 visibility state가 소유한다.

## 반복 루프 규칙

각 loop는 한 owner만 줄인다.

추천 순서:
1. Structure
2. Jungle
3. Minion
4. Ambient/Map visual
5. Champion model/table/roster
6. Champion FX presets
7. Projectile visual
8. UI/Lua product images
9. Engine generic fallback allowlist 정리

각 loop의 종료 기준:
- raw path occurrence count가 감소한다.
- normal F5 flow에서 해당 owner가 빠지지 않는다.
- `Build-LoLDefinitionPack.py --check`가 stale을 잡는다.
- `Verify-LoLDataDrivenPipeline.ps1`에 raw path audit가 연결된다.
- Client Debug build와 `git diff --check`가 통과한다.

## 회귀 방지

필수 gate:

```powershell
powershell -ExecutionPolicy Bypass -File Tools/LoLData/FindRawAssetPaths.ps1 -FailOnCandidate
```

gate가 완전히 도입되기 전에는 baseline mode로 출력만 보고, owner별 migration이 끝난 뒤 `-FailOnCandidate`를 CI/검증 파이프라인에 연결한다.

금지:
- 새 raw `Client/Bin/Resource/...` literal을 normal F5 runtime code에 추가한다.
- Client migration 편의를 위해 Shared/GameSim, Server, Engine public API에 LoL visual path를 올린다.
- visual migration 중 ServerPrivate gameplay value를 ClientPublic에 섞는다.
- 문제를 숨기기 위해 roster, map, minion, champion, snapshot, UI, FX normal flow를 끈다.

## 현재 상태 판정

문서 기준 반영률은 asset-path/data-driven visual cutover만 보면 아직 낮다. data-driven gameplay/visual pack의 뼈대는 존재하지만, 제품 asset selection은 다수의 C++ raw path에 남아 있다.

현재 판정:
- DefinitionKey 기반 gameplay/visual pack 축: 존재
- Champion visual timing data: 일부 반영
- Structure visual asset data: 미반영
- Champion asset path data: 미반영
- FX asset path data: 미반영
- raw path regression gate: 미반영

따라서 다음 구현 세션의 목표는 “새 시스템 설계”가 아니라 “기존 LoLDefinitionPack 축에 asset path table을 붙이고 owner별 raw path를 제거하는 반복”이다.
