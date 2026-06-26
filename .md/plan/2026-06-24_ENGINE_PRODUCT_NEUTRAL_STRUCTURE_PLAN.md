Session - Engine은 제품별 게임 데이터를 모르고 LoL/Elden Client와 Editor가 같은 공용 runtime/editor 계약을 쓰도록 구조를 재정렬한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Engine/Include/GameContext.h

기존 코드:

```cpp
enum class eChampion : uint8_t
{
	NONE = 0,
	IRELIA = 1,
	YASUO = 2,
	KALISTA = 3,
	SYLAS = 4,
	VIEGO = 5,
	ANNIE = 6,
	ASHE = 7,
	FIORA = 8,
	GAREN = 9,
	RIVEN = 10,
	ZED = 11,
	EZREAL = 12,
	YONE = 13,
	JAX = 14,
	MASTERYI = 15,
	KINDRED = 16,
	LEESIN = 17,
	END = 255
};
```

삭제할 범위:
`enum class eChampion : uint8_t` 줄부터 파일 끝까지 Engine public SDK에서 제거한다.
Engine은 LoL champion roster, bot lane, network roster, selected champion을 소유하지 않는다.

아래로 교체:

```text
CONFIRM_NEEDED - GameContext의 최종 소유 위치를 먼저 확정한다.
- Server와 Client가 같은 LoL roster/session schema를 읽어야 하면 Engine 밖의 Shared/LoL session 또는 Shared/GameSim definition 계층으로 옮긴다.
- Client local selection만 필요하면 Client/Public/GameMode/LOL 계층으로 옮긴다.
- 어느 경우에도 Engine/Include에는 대체 GameContext를 두지 않는다.
```

1-2. C:/Users/tnest/Desktop/Winters/Engine/Include/GameInstance.h

기존 코드:

```cpp
#include "GameContext.h"
```

삭제할 코드:

```cpp
#include "GameContext.h"
```

기존 코드:

```cpp
public: // Game context
    GameContext& Get_GameContext() { return m_GameContext; }
```

삭제할 코드:

```cpp
public: // Game context
    GameContext& Get_GameContext() { return m_GameContext; }
```

기존 코드:

```cpp
    void UI_Set_PlayerChampion(eChampion champ);
```

삭제할 코드:

```cpp
    void UI_Set_PlayerChampion(eChampion champ);
```

기존 코드:

```cpp
    void UI_Push_KillFeedBanner(eChampion eSourceChampion, eChampion eTargetChampion,
        u8_t iObjectKind, bool_t bSourceAlly, const char* pMessage);
    void UI_RecordGameContextChampionKill(u8_t iSourceTeam, u8_t iTargetTeam,
        bool_t bLocalSource, bool_t bLocalTarget);
    void UI_RecordGameContextMinionKill();
    void UI_SetGameContextServerTimeMs(u64_t iServerTimeMs);
```

삭제할 코드:

```cpp
    void UI_Push_KillFeedBanner(eChampion eSourceChampion, eChampion eTargetChampion,
        u8_t iObjectKind, bool_t bSourceAlly, const char* pMessage);
    void UI_RecordGameContextChampionKill(u8_t iSourceTeam, u8_t iTargetTeam,
        bool_t bLocalSource, bool_t bLocalTarget);
    void UI_RecordGameContextMinionKill();
    void UI_SetGameContextServerTimeMs(u64_t iServerTimeMs);
```

기존 코드:

```cpp
    DX11Shader* Get_MeshShader();
    DX11Pipeline* Get_MeshPipeline();
    CBlendStateCache* Get_BlendStateCache();
    DX11Shader* Get_FxSpriteShader();
    DX11Pipeline* Get_FxSpritePipeline();
    DX11Shader* Get_FxMeshShader();
    DX11Pipeline* Get_FxMeshPipeline();
    DX11Shader* Get_UIPlaneShader();
    DX11Pipeline* Get_UIPlanePipeline();
    DX11Shader* Get_ContactShadowShader();
    DX11Pipeline* Get_ContactShadowPipeline();
```

아래로 교체:

```text
CONFIRM_NEEDED - LoL/Elden Client와 Editor가 공통으로 쓰는 public surface는 IRHIDevice, RenderWorldSnapshot, resource handle, UI draw primitive 중심으로 축소한다.
DX11Shader/DX11Pipeline/CBlendStateCache getter는 Engine 내부 backend shim 또는 좁은 legacy adapter로 가둔다.
```

기존 코드:

```cpp
    GameContext m_GameContext{};
```

삭제할 코드:

```cpp
    GameContext m_GameContext{};
```

1-3. C:/Users/tnest/Desktop/Winters/Engine/Private/GameInstance.cpp

삭제할 범위:
`void CGameInstance::UI_Set_PlayerChampion(eChampion champ)` 함수 전체를 삭제한다.

삭제할 범위:
`void CGameInstance::UI_Push_KillFeedBanner(eChampion eSourceChampion,` 함수 전체를 삭제한다.

삭제할 범위:
`void CGameInstance::UI_RecordGameContextChampionKill(u8_t iSourceTeam, u8_t iTargetTeam,` 함수 전체를 삭제한다.

삭제할 범위:
`void CGameInstance::UI_RecordGameContextMinionKill()` 함수 전체를 삭제한다.

삭제할 범위:
`void CGameInstance::UI_SetGameContextServerTimeMs(u64_t iServerTimeMs)` 함수 전체를 삭제한다.

아래로 교체:

```text
CONFIRM_NEEDED - 삭제된 UI/GameContext 호출자는 LoL Client presenter가 보유한 LoL view state 갱신으로 옮긴다.
Engine GameInstance는 Timer, Scene, Sound, Profiler, JobSystem, generic resource/RHI gateway만 유지한다.
```

1-4. C:/Users/tnest/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h

기존 코드:

```cpp
#include "GameContext.h"
#include "World.h"
#include "Entity.h"
#include "Manager/UI/ChampionHUDState.h"
```

삭제할 코드:

```cpp
#include "GameContext.h"
#include "World.h"
#include "Entity.h"
#include "Manager/UI/ChampionHUDState.h"
```

삭제할 범위:
`void    Set_PlayerChampion(eChampion champ);` 줄부터
`void SetGameContextServerTimeMs(u64_t iServerTimeMs);` 줄까지의 LoL HUD, killfeed, game context public API를 제거한다.

삭제할 범위:
`struct KillFeedBanner` 정의 전체를 삭제한다.

삭제할 범위:
`struct GameContextHUDState` 정의 전체를 삭제한다.

삭제할 범위:
`struct StatusPanelMatchScore` 줄부터
`struct StatusPanelSpellIconCache` 정의 끝까지 삭제한다.

삭제할 범위:
`void    DrawHealthBars` 줄부터
`void ApplyChampionHUDSkillIconOverrides(const ChampionHUDState& State);` 줄까지의 world-query 기반 LoL HUD drawing helper 선언을 Engine에서 제거한다.

아래로 교체:

```text
CONFIRM_NEEDED - CUI_Manager는 product-neutral UI runtime service로 축소한다.
남길 수 있는 범위는 Font_Manager, UIAtlasManifest, UIRenderer, RawImagePass, LuaUIHost 같은 렌더/리소스 primitive다.
Champion HUD, item shop, status panel, killfeed, minion/turret/champion health bar, map ping wheel은 Client/Private/UI 또는 Client/Private/GameModule/LOL presenter로 이동한다.
Engine UI는 CWorld, ChampionComponent, MinionComponent, TurretComponent를 직접 조회하지 않고 Client가 만든 view state 또는 draw list만 받는다.
```

1-5. C:/Users/tnest/Desktop/Winters/Engine/Public/Manager/UI/ChampionHUDState.h

기존 코드:

```cpp
#include "Entity.h"
#include "GameContext.h"
#include "WintersTypes.h"
```

아래로 교체:

```text
CONFIRM_NEEDED - ChampionHUDState는 Engine/Public/Manager/UI 소유가 아니다.
LoL HUD 상태로 유지하려면 Client/Public/UI 또는 Client/Public/GameModule/LOL 아래로 이동한다.
공용 HUD layout primitive만 필요하면 eChampion, SkillRanks, InventoryItemIds, LoL stat 이름을 제거한 별도 Engine UI view model로 새로 정의한다.
```

1-6. C:/Users/tnest/Desktop/Winters/Engine/Private/Manager/UI/ChampionHUDPanel.h

기존 코드:

```cpp
#include "Manager/UI/ChampionHUDState.h"
```

아래로 교체:

```text
CONFIRM_NEEDED - 이 패널은 LoL Champion HUD 구현이면 Client/Private/UI로 이동한다.
Engine에 남길 경우 이름과 입력 타입을 product-neutral layout renderer로 바꾸고 ChampionHUDState 의존을 제거한다.
```

1-7. C:/Users/tnest/Desktop/Winters/Engine/Public/ECS/Components/GameplayComponents.h

기존 코드:

```cpp
#include "GameContext.h"   // eChampion
```

삭제할 코드:

```cpp
#include "GameContext.h"   // eChampion
```

삭제할 범위:
`enum class eTeam : uint8_t` 줄부터 파일 끝까지 Engine 소유에서 제거한다.
여기에는 `ChampionComponent`, `MinionComponent`, `TurretComponent`, `YasuoStateComponent`, `RivenStateComponent`, `SkillStateComponent`, `StatusEffectComponent`, `CommandQueueComponent`가 포함된다.

아래로 교체:

```text
CONFIRM_NEEDED - Engine ECS에는 Entity, ComponentStore, Transform, Render, SpatialIndex 같은 product-neutral primitive만 남긴다.
LoL authoritative gameplay component는 Shared/GameSim/Components로 옮긴다.
Yasuo/Riven 같은 champion-specific state는 Shared/GameSim/Components/*SimComponent.h 소유로 분산한다.
Client visual-only component는 Client/Public 또는 Client/Private GameModule/LOL 아래로 옮긴다.
Shared/GameSim이 Engine ECS를 include하는 현재 흐름은 끊어야 하므로, GameSim이 계속 ECS primitive를 필요로 하면 Shared/SimCore 같은 Engine 밖 deterministic primitive 소유 위치를 먼저 확정한다.
```

1-8. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/ChampionComponent.h

기존 코드:

```cpp
#pragma once

#include "ECS/Components/GameplayComponents.h"
```

아래로 교체:

```text
CONFIRM_NEEDED - Engine forwarding include를 제거하고 Shared/GameSim 소유 ChampionComponent 본문을 둔다.
이 파일은 Engine, Renderer, UI, ImGui, DX type을 include하지 않는다.
```

1-9. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/SkillStateComponent.h

기존 코드:

```cpp
#pragma once

#include "ECS/Components/GameplayComponents.h"
```

아래로 교체:

```text
CONFIRM_NEEDED - Engine forwarding include를 제거하고 Shared/GameSim 소유 SkillStateComponent 본문을 둔다.
Champion skill slot 수, rank, cooldown은 Shared/GameSim definition 또는 component가 소유하고 Engine은 알지 않는다.
```

1-10. C:/Users/tnest/Desktop/Winters/Engine/Public/ECS/Components/SpatialAgentComponent.h

기존 코드:

```cpp
enum class eSpatialKind : u32_t
{
    None = 0,
    Champion = 1u << 0,
    Minion = 1u << 1,
    Turret = 1u << 2,
    JungleMob = 1u << 3,
    Inhibitor = 1u << 4,
    Nexus = 1u << 5,
    Ward = 1u << 6,
    Projectile = 1u << 7,
    Bush = 1u << 8,
    All = 0xFFFFFFFFu
};
```

아래로 교체:

```text
CONFIRM_NEEDED - Engine spatial component는 product-neutral layer/mask 값만 가진다.
LoL의 Champion/Minion/Turret/JungleMob/Inhibitor/Nexus/Ward/Projectile 의미는 Shared/GameSim 또는 Client/LoL mapping enum으로 옮긴다.
Engine SpatialIndex는 u32_t mask 비교만 수행하고 각 bit의 게임 의미를 해석하지 않는다.
```

1-11. C:/Users/tnest/Desktop/Winters/Engine/Public/ECS/Systems/MinionAISystem.h

삭제할 범위:
파일 전체를 Engine 소유에서 제거한다.

아래로 교체:

```text
CONFIRM_NEEDED - Minion AI는 server-authoritative gameplay이므로 Shared/GameSim/Systems 또는 Server/Game 계층으로 이동한다.
Client local-only smoke가 필요하면 Client/Private/ECS/Systems 아래에 명시적인 smoke path로 분리한다.
```

1-12. C:/Users/tnest/Desktop/Winters/Engine/Public/ECS/Systems/TurretAISystem.h

삭제할 범위:
파일 전체를 Engine 소유에서 제거한다.

아래로 교체:

```text
CONFIRM_NEEDED - Turret AI와 turret projectile authority는 Shared/GameSim/Systems 또는 Server/Game 계층으로 이동한다.
Engine에는 projectile renderer나 generic spatial/query primitive만 남긴다.
```

1-13. C:/Users/tnest/Desktop/Winters/Engine/Public/AI/BTNodes_Champion.h

삭제할 범위:
파일 전체를 Engine 소유에서 제거한다.

아래로 교체:

```text
CONFIRM_NEEDED - Engine에는 BehaviorTree, Blackboard, generic selector/sequence/action/condition primitive만 남긴다.
Cond_EnemyChampInSight, Cond_EnemyMinionInRange, Cond_SkillReady, Act_CastSkill, Act_FarmMinion 같은 LoL node factory는 Shared/GameSim/Systems/ChampionAI 또는 Server/Game AI policy로 이동한다.
```

1-14. C:/Users/tnest/Desktop/Winters/Engine/Public/AI/MCTSPlanner.h

기존 코드:

```cpp
enum class eMCTSAction : u8_t
{
    None = 0,
    AttackNearest,
    CastQ,
    CastW,
    CastE,
    CastR,
    MoveAway,
    MoveTowardEnemy,
    Retreat,
    Hold,
    END
};
```

아래로 교체:

```text
CONFIRM_NEEDED - CastQ/CastW/CastE/CastR 같은 LoL action space는 Engine AI core가 아니다.
MCTS core를 Engine에 남기려면 action/state/evaluator를 interface로 주입받는 product-neutral planner로 바꾸고, 현재 champion action enum과 WorldSnapshot encoder는 Shared/GameSim 또는 Server/Game AI로 이동한다.
```

1-15. C:/Users/tnest/Desktop/Winters/Engine/Public/AI/RLBridge.h

기존 코드:

```cpp
static constexpr u32_t STATE_DIM = 24;
static constexpr u32_t ACTION_DIM = static_cast<u32_t>(eMCTSAction::END);

void EncodeState(CWorld& world, EntityID self, std::vector<f32_t>& out) const;
```

아래로 교체:

```text
CONFIRM_NEEDED - Engine에 ML inference runtime을 남기더라도 state/action schema는 제품이 제공한다.
현재 EncodeState는 ChampionComponent, HealthComponent, mana, cooldown을 읽으므로 Engine 밖 LoL AI adapter로 이동한다.
```

1-16. C:/Users/tnest/Desktop/Winters/Engine/Public/FX/FxMaterialDesc.h

기존 코드:

```cpp
enum class eFxMaterialStyleMode : u32_t
{
	LegacyTint = 0,
	LOLBrushRim = 1,
	ToonCell = 2,
	Gradient = 3,
	LOLMagicSurface = 4,
	EldenSmoke = 16,
	EldenTrail = 17,
};
```

아래로 교체:

```text
CONFIRM_NEEDED - Engine enum에는 LOL/Elden 제품명을 넣지 않는다.
Engine에는 LegacyTint, BrushRim, ToonCell, Gradient, MagicSurface, Smoke, Trail처럼 material algorithm 이름만 남기거나, u32_t StyleId를 데이터 카탈로그가 해석하게 한다.
LoL/Elden preset 이름은 Client/Elden asset catalog 또는 WFX authoring data가 소유한다.
```

1-17. C:/Users/tnest/Desktop/Winters/Engine/Include/Engine.vcxproj

아래로 교체:

```text
확인 필요:
- Engine에서 제거하거나 이동한 GameContext, ChampionHUD, GameplayComponents, Minion/Turret systems, Champion BT/MCTS/RL files가 Engine.vcxproj membership에서 제거되는지 확인한다.
- 이동 대상 프로젝트의 .vcxproj/.filters 등록은 실제 파일 이동 패치에서 확인한다.
- 계획서 단계에서는 XML 항목을 직접 나열하지 않는다.
```

2. 검증

미검증:
- 계획서 작성만 수행했고 코드 이동, 빌드, 런타임 확인은 미수행.

검증 명령:

```powershell
git diff --check
rg -n "#include \"GameContext.h\"|eChampion|ChampionComponent|MinionComponent|TurretComponent|YasuoStateComponent|RivenStateComponent|BTNodes_Champion|MinionAISystem|TurretAISystem|LOL|LoL|Elden|Shared/GameSim" Engine/Include Engine/Public Engine/Private -g "*.h" -g "*.cpp"
rg -n "ID3D11|DX11Shader|DX11Pipeline|CBlendStateCache" Engine/Include Engine/Public Client/Public Shared -g "*.h" -g "*.cpp"
msbuild Winters.sln /t:Engine /p:Configuration=Debug /p:Platform=x64
msbuild Winters.sln /t:Client /p:Configuration=Debug /p:Platform=x64
msbuild Winters.sln /t:Server /p:Configuration=Debug /p:Platform=x64
msbuild Winters.sln /t:EldenRingClient /p:Configuration=Debug /p:Platform=x64
msbuild Winters.sln /t:EldenRingEditor /p:Configuration=Debug /p:Platform=x64
```

확인 필요:
- `Shared/GameSim`이 `Engine`, `Renderer`, `UI`, `ImGui`, `DX11` include 없이 빌드되는지 확인한다.
- LoL normal F5 flow에서 roster, map, minion, champion, snapshot, UI, FX를 숨기지 않고 smoke를 통과하는지 확인한다.
- EldenRingClient와 EldenRingEditor가 `WintersEngine.dll`의 같은 RHI/resource/editor primitive를 사용하고 LoL GameContext 또는 champion enum을 include하지 않는지 확인한다.
- Engine public header 변경 후 `UpdateLib.bat` 또는 빌드 전 SDK sync가 필요한지 확인한다.

후속 동기화:
- Engine public header에서 제품 타입을 제거한 뒤 `EngineSDK/inc` 동기화 여부를 확인한다.
- 이 계획이 팀 구조 결정으로 확정되면 `.md/architecture/WINTERS_CODEBASE_COMPASS.md`에는 세부 파일 목록이 아니라 확정된 소유권 규칙만 반영한다.
