# 08 인게임 상점 및 아이템 스탯 계획서

Current sequence

01/08: 미니언 지형 높이 및 맵 콜라이더 투영
-> 02/08: 우클릭 공격 추격
-> 03/08: B 귀환
-> 04/08: 미니맵
-> 05/08: 전장의 안개
-> 06/08: HUD 아틀라스/초상화/스킬 아이콘
-> 07/08: 스킬 레벨업 및 데미지 +5
-> 08/08 현재: P 인게임 상점 및 아이템 스탯 구매

Smoke 정책: 자동 smoke 검증은 폐지한다. 상점 동작은 유저가 인게임에서 직접 확인한다.

Bot AI is a GameCommand producer and must not directly mutate gameplay truth.
즉, Bot AI는 `GameCommand` 생산자이며 이동/공격/데미지 같은 gameplay truth를 직접 바꾸면 안 된다.

Goal

`P` 키를 누르면 인게임 상점이 열린다. 플레이어는 골드를 사용해 아이템을 구매할 수 있고, 구매한 아이템의 stat modifier만큼 서버 권위 플레이어 스탯이 증가한다.

Non-goals

- 백엔드 계정/상점 서비스와 연동하지 않는다.
- 판매/되돌리기/추천 아이템 페이지는 첫 pass에서 구현하지 않는다.
- 우물에서만 상점 사용 가능 제약은 recall/fountain 상태가 준비된 뒤 별도 적용한다.
- 최종 item catalog를 JSON/Lua로 저장하지는 않지만, 나중에 hardcoded catalog source만 교체 가능하게 구조를 잡는다.

Why this order

상점은 HUD/gold display와 command routing에 의존한다. recall과 level-up 이후에 들어가야 플레이어가 귀환 -> 상점 열기 -> 아이템 구매 -> 전투 stat 증가를 자연스럽게 확인할 수 있다.

Current-code evidence

- [UI_Manager.h](C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h:82)는 이미 `SetInGameShopOpen`, `ToggleInGameShop`, `GetInGameShopOpen`을 제공한다.
- [UI_Manager.h](C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h:137)는 `InGameShopItemView`를 정의한다.
- [UI_Manager.cpp](C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp:304)는 `LoadInGameShopAssets` stub을 가지고 있다.
- [UI_Manager.cpp](C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp:309)는 빈 `DrawInGameShop`을 가지고 있다.
- [UI_Manager.cpp](C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp:325)는 server-authority 메모와 함께 `TryBuyInGameItem`이 stub이다.
- [CommandSerializer.cpp](C:/Users/user/Desktop/Winters/Client/Private/Network/Client/CommandSerializer.cpp:123)는 이미 `BuyItem` command를 보낸다.
- [CommandExecutor.cpp](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp:1307)는 빈 `HandleBuyItem`을 가지고 있다.
- [ItemDef.h](C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ItemDef.h:5)는 이미 `ItemStatModifier`를 정의한다.
- [StatComponent.h](C:/Users/user/Desktop/Winters/Shared/GameSim/Components/StatComponent.h:16)는 AD/AP/HP/방어력/MR/공속 등 스탯 필드를 가지고 있다.

Files touched

- 추가: [GoldComponent.h](C:/Users/user/Desktop/Winters/Shared/GameSim/Components/GoldComponent.h)
- 추가: [InventoryComponent.h](C:/Users/user/Desktop/Winters/Shared/GameSim/Components/InventoryComponent.h)
- 추가: [ItemCatalog.h](C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ItemCatalog.h)
- 추가: [ItemCatalog.cpp](C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ItemCatalog.cpp)
- 수정: [CommandExecutor.cpp](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp)
- 수정: [UI_Manager.h](C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h)
- 수정: [UI_Manager.cpp](C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp)
- 수정: [Scene_InGame.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp) 또는 input bridge
- 필요 시 수정: [CommandSerializer.cpp](C:/Users/user/Desktop/Winters/Client/Private/Network/Client/CommandSerializer.cpp)
- gold/inventory를 client에 바로 보여주려면 snapshot/event replication 파일도 수정한다.

Insertion/replacement anchors

- `CScene_InGame::OnImGui` 또는 input bridge keyboard block: `P`가 `CGameInstance::Get()->UI_ToggleInGameShop()` 또는 기존 manager method를 호출하게 한다.
- `CUI_Manager::LoadInGameShopAssets`: 작은 hardcoded starter item list와 icon path/price를 채운다.
- `CUI_Manager::DrawInGameShop`: shop panel과 item button을 그린다.
- `CUI_Manager::TryBuyInGameItem`: stat을 직접 mutate하지 않고 `m_pfnBuyItem` callback으로 command를 보낸다.
- `CDefaultCommandExecutor::HandleBuyItem`: gold, inventory space, item catalog, issuer alive를 검증한 뒤 stat을 적용한다.

Full new-file contents

```cpp
// Shared/GameSim/Components/GoldComponent.h
#pragma once

#include "WintersTypes.h"

struct GoldComponent
{
    u32_t current = 500;
};

static_assert(std::is_trivially_copyable_v<GoldComponent>);
```

```cpp
// Shared/GameSim/Components/InventoryComponent.h
#pragma once

#include "WintersTypes.h"

struct InventoryComponent
{
    static constexpr u8_t kSlotCount = 6;
    u16_t itemIds[kSlotCount] = {};
};

static_assert(std::is_trivially_copyable_v<InventoryComponent>);
```

Implementation outline

1. `P`가 shop UI를 open/close한다.
2. UI item click이 `BuyItem(itemId)` command를 보낸다.
3. 서버가 item 존재 여부, player gold, inventory 빈 슬롯, issuer alive를 검증한다.
4. 서버가 gold를 차감하고 inventory에 item id를 저장한 뒤 `ItemStatModifier`를 `StatComponent`에 적용한다.
5. 클라이언트 HUD gold와 inventory display는 replicated state/event로 갱신된다.
6. 초기 catalog는 `ItemCatalog.cpp`에 hardcoded로 두고, 추후 JSON/Lua는 catalog source만 교체한다.

Verification commands and expected results

- 자동 smoke 없음.
- 구현 후 위생 확인: `git diff --check`.
- 유저 수동 검증: `P`를 눌러 상점을 열고 starter AD item을 구매하면 gold가 줄고, inventory slot이 채워지고, 플레이어 damage/stat이 증가하는지 확인한다.
- 예상 debug log를 추가한다면: `[Shop] buy accept entity=... item=... gold=...`, 또는 `[Shop] buy reject reason=not-enough-gold`.

Rollback scope

`GoldComponent`, `InventoryComponent`, `ItemCatalog`를 제거하고 shop stub으로 되돌린다. `SendBuyItem`은 사용되지 않는 serializer method로 남겨도 된다.

Next slice

08/08 이후에는 item sell/undo, 우물에서만 상점 열기, JSON/Lua 기반 item/champion stat table 중 하나를 다음 slice로 선택한다.
