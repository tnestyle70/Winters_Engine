Session - Engine 구조 Refactoring을 제품 중립 100% 달성까지 세션 루프와 검증 게이트로 반복 진행한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Engine

아래 기준을 모든 세션의 완료 조건으로 적용:

```text
Engine 100% 달성 기준:
- Engine public/private/include 코드에는 LoL/Elden 제품별 gameplay data, champion roster, minion/turret/jungle/ward/nexus/inhibitor 규칙, item/shop/killfeed/status panel, champion skill/action schema가 남지 않는다.
- Engine은 Client 제품 코드, Server 권위 코드, Shared/GameSim gameplay truth를 include하지 않는다.
- Engine public surface는 `WintersRun`, `EngineConfig`, `IWintersApp`, `IScene`, product-neutral `CGameInstance`, `IRHIDevice`, RHI handle/descriptor, resource/runtime primitive, UI draw primitive, editor primitive만 노출한다.
- Engine 내부 DX11/DX12 backend는 허용하되 `Client/Public`, `Shared`, `Engine/Include`에 `ID3D11*`, `DX11Shader`, `DX11Pipeline`, `CBlendStateCache` 노출을 늘리지 않는다.
- Engine ECS에는 `Entity`, `EntityHandle`, `CWorld`, `CComponentStore`, scheduler, transform/render/nav/spatial primitive만 남긴다.
- Engine AI에는 `BehaviorTree`, `Blackboard`, generic planner/inference shell만 남기고 LoL action/state/node factory는 Engine 밖으로 이동한다.
- Engine UI에는 font, atlas, raw image, layout renderer, Lua host 같은 primitive만 남기고 제품 HUD/presenter는 Client 또는 Editor 제품 계층이 소유한다.
```

세션 루프마다 아래 순서를 반복:

```text
1. Audit:
   - 이번 세션 대상 파일군과 현재 위반 항목을 `rg`로 확인한다.
2. Slice:
   - 한 세션은 하나의 소유권 축만 바꾼다.
   - 동시에 gameplay truth 이동, UI presenter 이동, RHI API 변경을 섞지 않는다.
3. Patch:
   - 기존 호출자를 먼저 새 소유 위치로 이행한 뒤 Engine membership을 제거한다.
   - normal F5 LoL runtime을 숨기는 임시 단축은 금지한다.
4. Build:
   - Engine 단독 빌드와 영향 프로젝트 빌드를 모두 실행한다.
5. Regression:
   - LoL Client, Server, EldenRingClient, EldenRingEditor 중 영향 대상 smoke를 실행한다.
6. Gate:
   - 검색 게이트가 이전보다 줄었는지 확인한다.
   - 새 Engine public/product 의존이 생겼으면 세션 실패로 되돌려 수정한다.
7. Record:
   - 다음 세션이 시작할 수 있도록 남은 위반 수와 예외 사유를 계획 문서 또는 세션 결과 문서에 남긴다.
```

1-2. C:/Users/tnest/Desktop/Winters/Engine/Include/GameContext.h

Session S01에서 아래 소유권을 먼저 정리:

```text
목표:
- `GameContext.h`를 Engine SDK에서 제거한다.
- `eChampion`, roster slot, selected champion, bot lane/session field의 소유 위치를 Engine 밖으로 확정한다.

이동 기준:
- 서버와 클라이언트가 동일하게 읽는 roster/session schema이면 Shared contract 또는 Shared/GameSim definition 계층으로 이동한다.
- Client local selection/presentation 전용이면 Client/GameMode/LOL 또는 Client/GameModule/LOL 계층으로 이동한다.
- Engine에는 대체 `GameContext`를 만들지 않는다.

완료 게이트:
- `rg -n 'GameContext|eChampion' Engine/Include Engine/Public Engine/Private -g '*.h' -g '*.cpp'` 결과에서 Engine product context 사용이 0이 된다.
- Shared/Server/Client 호출자가 Engine include path가 아닌 새 소유 위치를 include한다.
```

1-3. C:/Users/tnest/Desktop/Winters/Engine/Public/ECS/Components/GameplayComponents.h

Session S02에서 아래 소유권을 분리:

```text
목표:
- `ChampionComponent`, `MinionComponent`, `TurretComponent`, `StructureComponent`, `JungleComponent`, `SkillStateComponent`, `StatusEffectComponent`, champion-specific state를 Engine 밖으로 이동한다.
- Shared/GameSim forwarding header가 Engine `GameplayComponents.h`를 include하는 구조를 제거한다.

이동 기준:
- 서버 권위 gameplay truth는 Shared/GameSim/Components 또는 Shared/GameSim/Systems가 소유한다.
- Client visual-only state는 Client/Public 또는 Client/Private의 LoL presentation 계층이 소유한다.
- Engine에는 Transform, Render, NavAgent, SpatialIndex 같은 product-neutral component만 남긴다.

완료 게이트:
- `rg -n 'GameplayComponents.h|ChampionComponent|MinionComponent|TurretComponent|YasuoStateComponent|RivenStateComponent' Engine/Include Engine/Public Engine/Private -g '*.h' -g '*.cpp'` 결과에서 product gameplay component가 0이 된다.
- `rg -n 'ECS/Components/GameplayComponents.h' Shared Server Client -g '*.h' -g '*.cpp'` 결과가 0이 된다.
```

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim

Session S03에서 Shared/GameSim의 Engine 의존을 끊는다:

```text
목표:
- Shared/GameSim은 Engine, Renderer, RHI, UI, ImGui, DX type을 include하지 않는다.
- Shared/GameSim이 필요한 deterministic primitive는 Shared 쪽 소유로 둔다.

분기 기준:
- `EntityHandle`, compact entity identity, deterministic component iteration이 GameSim에 필요하면 Shared/SimCore 또는 Shared/GameSim/Core로 이전한다.
- Engine runtime ECS가 같은 타입을 계속 써야 하면 Engine이 Shared의 deterministic primitive를 소비하는 방향으로만 의존을 만든다.
- Shared가 Engine의 `WintersTypes.h`에 의존하는 경우 basic type alias 소유 위치를 확인하고 Shared-safe header로 분리한다.

완료 게이트:
- `rg -n '#include \"(Engine|ECS|Renderer|RHI|Manager|GameContext|Winters)' Shared/GameSim -g '*.h' -g '*.cpp'` 결과가 0이 된다.
- `rg -n 'ID3D11|DX11|ImGui|IRHIDevice|CWorld' Shared/GameSim -g '*.h' -g '*.cpp'` 결과가 0이 된다.
```

1-5. C:/Users/tnest/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h

Session S04에서 UI 소유권을 presenter 방식으로 전환:

```text
목표:
- Engine UI는 draw primitive와 layout/resource primitive만 제공한다.
- LoL HUD, in-game shop, status panel, killfeed, map ping, champion/minion/turret health bar는 Client LoL presenter가 소유한다.

이동 기준:
- Engine `CUI_Manager`가 `CWorld`, `ChampionComponent`, `MinionComponent`, `TurretComponent`, `eChampion`을 직접 조회하면 실패다.
- Client presenter가 snapshot/event/GameSim view data를 읽고 Engine UI primitive로 draw call 또는 view state를 공급한다.
- EldenRingEditor는 같은 Engine UI primitive를 사용하되 LoL HUD 타입을 include하지 않는다.

완료 게이트:
- `rg -n 'eChampion|ChampionHUD|KillFeed|InGameShop|Minion|Turret|ChampionComponent|CWorld' Engine/Public/Manager/UI Engine/Private/Manager/UI -g '*.h' -g '*.cpp'` 결과가 0이 된다.
- LoL Client normal F5에서 HUD/shop/killfeed/healthbar가 Engine product type 없이 표시된다.
```

1-6. C:/Users/tnest/Desktop/Winters/Engine/Public/Renderer

Session S05에서 렌더 public surface를 `RenderWorldSnapshot` 중심으로 축소:

```text
목표:
- LoL과 Elden은 renderer class hierarchy를 복제하지 않고 각자 `RenderWorldSnapshot` 또는 backend-neutral render item을 만든다.
- Engine public API에 LoL/Elden product type 또는 DX11 concrete helper getter가 노출되지 않는다.

이동 기준:
- `Get_MeshShader`, `Get_MeshPipeline`, `Get_FxSpriteShader`, `Get_FxSpritePipeline`, `Get_FxMeshShader`, `Get_FxMeshPipeline`, `Get_UIPlaneShader`, `Get_UIPlanePipeline`, `Get_ContactShadowShader`, `Get_ContactShadowPipeline` 같은 concrete getter는 legacy adapter로 격리하거나 제거한다.
- Client code는 가능하면 `IRHIDevice`, resource handle, renderer abstraction, snapshot builder만 사용한다.

완료 게이트:
- `rg -n 'DX11Shader|DX11Pipeline|CBlendStateCache|ID3D11' Engine/Include Engine/Public Client/Public Shared -g '*.h' -g '*.cpp'` 결과가 허용된 backend-private 범위 밖에서 0이 된다.
- EldenRingClient와 LoL Client가 같은 Engine renderer/RHI primitive 위에서 빌드된다.
```

1-7. C:/Users/tnest/Desktop/Winters/Engine/Public/AI

Session S06에서 AI core와 LoL policy를 분리:

```text
목표:
- Engine에는 generic `BehaviorTree`, `Blackboard`, scheduler/inference primitive만 남긴다.
- `BTNodes_Champion`, `CastQ/W/E/R`, champion/minion/turret query, LoL state encoder는 Shared/GameSim 또는 Server/Game AI policy가 소유한다.

이동 기준:
- Engine AI node는 product component 이름을 template 또는 callback 밖에서 직접 알지 않는다.
- LoL bot decision은 command를 생산하고 truth component를 직접 임의 변경하지 않는다.

완료 게이트:
- `rg -n 'Champion|Minion|Turret|SkillReady|CastQ|CastW|CastE|CastR|Mana|Cooldown' Engine/Public/AI Engine/Private/AI -g '*.h' -g '*.cpp'` 결과에서 LoL policy가 0이 된다.
- Server authoritative AI smoke에서 bot command generation이 유지된다.
```

1-8. C:/Users/tnest/Desktop/Winters/Engine/Public/FX/FxMaterialDesc.h

Session S07에서 FX material style 이름을 product-neutral로 변경:

```text
목표:
- Engine material style enum과 shader constant에는 LoL/Elden 제품명이 없다.
- 제품별 preset 이름은 Client/Elden catalog 또는 WFX authoring data가 소유한다.

이동 기준:
- `LOLBrushRim`, `LOLMagicSurface`, `EldenSmoke`, `EldenTrail`은 algorithm 이름 또는 data-driven `StyleId`로 바꾼다.
- 기존 visual 결과가 바뀌지 않도록 old preset id to new style mapping을 Client/Elden data layer에 둔다.

완료 게이트:
- `rg -n 'LOL|LoL|Elden' Engine/Public/FX Engine/Private/FX Engine/Public/Renderer Engine/Private/Renderer -g '*.h' -g '*.cpp'` 결과가 0이 된다.
- 기존 LoL FX와 Elden showcase FX가 같은 visual preset으로 표시된다.
```

1-9. C:/Users/tnest/Desktop/Winters/Engine/Include/Engine.vcxproj

Session S08에서 Engine build membership을 정리:

```text
목표:
- 이동된 product-owned file이 Engine project와 CMake source group에 남지 않는다.
- Visual Studio filter가 소유권 착시를 만들지 않는다.

확인 대상:
- `GameContext`
- `GameplayComponents`
- `ChampionHUD`
- `MinionAISystem`
- `TurretAISystem`
- `TurretProjectileSystem`
- `BTNodes_Champion`
- `MCTSPlanner`의 LoL action schema
- `RLBridge`의 LoL state encoder

완료 게이트:
- `rg -n 'GameContext|GameplayComponents|ChampionHUD|BTNodes_Champion|MinionAISystem|TurretAISystem|TurretProjectileSystem|LOL|LoL|Elden' Engine/Include/Engine.vcxproj Engine/Include/Engine.vcxproj.filters cmake/WintersEngine.cmake` 결과에서 Engine-owned 예외 외 product membership이 0이 된다.
- CMake generated project와 hand-maintained `.vcxproj.filters`의 도메인명이 같은 방향을 가리킨다.
```

1-10. C:/Users/tnest/Desktop/Winters/Client

Session S09에서 LoL Client presentation ownership을 검증:

```text
목표:
- Client는 Engine primitive와 Shared schema/component를 소비하되 authoritative gameplay truth를 만들지 않는다.
- LoL 전용 scene/input/camera/presenter/UI state는 Client LoL 계층에 남긴다.

완료 게이트:
- `rg -n 'ID3D11|DX11Shader|DX11Pipeline|CBlendStateCache' Client/Public -g '*.h' -g '*.cpp'` 결과가 0이 된다.
- `rg -n 'ChampionHUD|KillFeed|InGameShop|StatusPanel|HealthBar' Engine/Include Engine/Public Engine/Private -g '*.h' -g '*.cpp'` 결과가 0이면서 Client LoL 계층에서 기능이 유지된다.
- normal F5 LoL flow에서 roster, map, minion, champion, snapshot, UI, FX가 숨겨지지 않는다.
```

1-11. C:/Users/tnest/Desktop/Winters/EldenRingClient

Session S10에서 Elden Client 독립성을 검증:

```text
목표:
- EldenRingClient는 LoL Client code 또는 LoL GameContext/champion enum을 include하지 않는다.
- Elden renderer를 복제하지 않고 Engine RHI/resource/render snapshot 또는 equivalent primitive를 사용한다.

완료 게이트:
- `rg -n 'GameContext|eChampion|ChampionComponent|MinionComponent|TurretComponent|Client/Public/GameObject/Champion|Client/Private/GameObject/Champion' EldenRingClient EldenRingEditor -g '*.h' -g '*.cpp'` 결과가 0이 된다.
- EldenRingClient Debug 빌드와 launch smoke가 통과한다.
```

1-12. C:/Users/tnest/Desktop/Winters/EldenRingEditor

Session S11에서 Editor 공용 계약을 검증:

```text
목표:
- Editor는 Engine editor primitive, resource/asset contract, RHI/UI primitive를 사용한다.
- Editor 전용 기능은 normal runtime을 숨기거나 우회하지 않는다.

완료 게이트:
- EldenRingEditor가 LoL HUD/GameContext/GameSim truth를 include하지 않는다.
- Editor에서 만든 asset/runtime data가 Engine resource/path/validation contract를 탄다.
- EldenRingEditor Debug 빌드와 launch smoke가 통과한다.
```

1-13. C:/Users/tnest/Desktop/Winters/.md/architecture/WINTERS_CODEBASE_COMPASS.md

Session S12에서 확정된 구조 규칙만 Compass에 반영:

```text
목표:
- 세션 루프에서 확정된 소유권 규칙만 Compass에 남긴다.
- 세부 파일 목록, 임시 카운트, 일회성 진행 상황은 Compass에 넣지 않는다.

완료 게이트:
- Compass에는 Engine/Shared/Server/Client/Editor 의존 방향과 재발 방지 규칙만 남는다.
- 반복 실수는 `.claude/gotchas.md`에 짧게 기록한다.
```

1-14. C:/Users/tnest/Desktop/Winters

최종 수렴 루프는 아래 조건을 모두 만족할 때까지 S01부터 S12 중 실패한 세션으로 되돌아간다:

```text
최종 수렴 조건:
- Engine product-token audit 통과.
- Shared/GameSim Engine include audit 통과.
- Client/Public DX11 concrete audit 통과.
- Engine build membership audit 통과.
- Engine, Client, Server, EldenRingClient, EldenRingEditor Debug x64 빌드 통과.
- LoL normal F5 smoke 통과.
- Client-server gameplay smoke 통과.
- EldenRingClient launch smoke 통과.
- EldenRingEditor launch smoke 통과.
- `UpdateLib.bat` 또는 빌드 전 SDK sync 이후 EngineSDK/inc에 stale product public header가 남지 않는다.
```

2. 검증

검증 명령:

```powershell
git diff --check

rg -n 'GameContext|eChampion|ChampionComponent|MinionComponent|TurretComponent|YasuoStateComponent|RivenStateComponent|BTNodes_Champion|MinionAISystem|TurretAISystem|TurretProjectileSystem|KillFeed|InGameShop|LOL|LoL|Elden' Engine/Include Engine/Public Engine/Private -g '*.h' -g '*.cpp'

rg -n '#include "(Engine|ECS|Renderer|RHI|Manager|GameContext|Winters)' Shared/GameSim -g '*.h' -g '*.cpp'

rg -n 'ID3D11|DX11Shader|DX11Pipeline|CBlendStateCache' Engine/Include Engine/Public Client/Public Shared -g '*.h' -g '*.cpp'

rg -n 'GameContext|GameplayComponents|ChampionHUD|BTNodes_Champion|MCTSPlanner|RLBridge|MinionAISystem|TurretAISystem|TurretProjectileSystem|LOL|LoL|Elden' Engine/Include/Engine.vcxproj Engine/Include/Engine.vcxproj.filters cmake/WintersEngine.cmake

msbuild Winters.sln /t:Engine /p:Configuration=Debug /p:Platform=x64
msbuild Winters.sln /t:Client /p:Configuration=Debug /p:Platform=x64
msbuild Winters.sln /t:Server /p:Configuration=Debug /p:Platform=x64
msbuild Winters.sln /t:EldenRingClient /p:Configuration=Debug /p:Platform=x64
msbuild Winters.sln /t:EldenRingEditor /p:Configuration=Debug /p:Platform=x64
```

수동 확인:

```text
- LoL normal F5 runtime에서 roster, map, minion, champion, snapshot, UI, FX가 모두 살아 있는지 확인한다.
- Client-server smoke에서 command -> Server GameSim -> snapshot/event -> Client visual 흐름이 유지되는지 확인한다.
- EldenRingClient가 LoL Client 코드 없이 Engine RHI/resource/render primitive로 뜨는지 확인한다.
- EldenRingEditor가 LoL HUD/GameContext 없이 Engine editor/UI/RHI primitive로 뜨는지 확인한다.
- EngineSDK/inc 동기화 후 stale `GameContext.h`, LoL HUD, DX11 concrete public header 노출이 남지 않는지 확인한다.
```

미검증:
- 이 문서는 세션 루프 설계이며 코드 이동, 빌드, 런타임 smoke는 아직 수행하지 않았다.
