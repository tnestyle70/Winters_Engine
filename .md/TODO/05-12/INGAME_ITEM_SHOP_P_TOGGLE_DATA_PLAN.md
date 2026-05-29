# In-Game Item Shop P Toggle + Data Plan

작성일: 2026-05-12

목표: `P` 키로 인게임 아이템 상점을 열고 닫으며, `Client/Bin/Resource/Texture/UI` 아래의 itemshop atlas와 `UI/Items/*.png` 아이콘을 사용해 실제 구매 UI를 구성한다. 구매는 장식 UI에서 끝내지 않고 `GameCommand::BuyItem` -> 서버 검증 -> 골드/인벤토리/스탯 반영 -> HUD 아이템 슬롯 갱신까지 이어지는 구조로 잡는다.

## 결론

이번 상점은 메인메뉴의 영속 상점이 아니라 인게임 골드 아이템 상점이다.

- 기존 `CShopClient` / Go `shop` service: RP, 스킨, 계정 인벤토리 같은 메타 상점.
- 이번 `P` 상점: 챔피언이 게임 안에서 골드로 아이템을 구매하는 전투 상점.
- 이미 `Shared/GameSim`에는 `BuyItem = 5`, `UseItem = 6`, `GameCommand.itemId`가 있으므로 이 통로를 살린다.
- `CommandExecutor::HandleBuyItem`은 현재 빈 함수라서, UI 이후 바로 서버 권위 구현 지점으로 사용한다.

## 현재 확인된 진입점

### UI/HUD

- `Engine/Private/Manager/UI/UI_Manager.cpp:32`
  - 현재 HUD atlas 경로 상수들이 모여 있다.
  - 여기에 `itemshop_texture_atlas*.png`, `itemshopatlas.png`, `itemshopsupportatlas.png`, `HUD/itemshophud-default.png`를 추가 로드한다.
- `Engine/Private/Manager/UI/UI_Manager.cpp:110`
  - `CUI_Manager::Render_Overlay`에서 HP bar와 챔피언 HUD를 그리고 있다.
  - 여기서 shop input toggle과 `Draw_ItemShopWindow`를 호출한다.
- `Engine/Public/Manager/UI/UI_Manager.h:75`
  - private draw 함수들이 모여 있다.
  - `Handle_ShopInput`, `Draw_ItemShopWindow`, `Draw_ItemGrid`, `Draw_ItemDetailPane`를 추가할 위치다.
- `Engine/Private/Manager/UI/UI_Manager.cpp:650`
  - ImGui 튜너에 현재 HUD atlas 상태가 출력된다.
  - `Show Item Shop`, `Item Shop Atlas loaded`, `Item Icon Count`를 추가한다.

### 입력

- `Engine/Public/Core/CInput.h:37`
  - `bool IsKeyPressed(uint8 vKey) const` 사용 가능.
- 기존 사용 예:
  - `Engine/Private/Framework/CEngineApp.cpp:217`: `VK_F3`
  - `Client/Private/Scene/InGameCombatInputBridge.cpp:405`: `Q/W/E/R`
  - `Client/Private/Scene/Scene_InGame.cpp:1453`: `M`
- 1차 구현은 `CUI_Manager::Render_Overlay` 또는 별도 `CUI_Manager::Handle_ShopInput`에서 `CInput::Get().IsKeyPressed('P')`로 토글한다.

### GameSim 구매 명령

- `Shared/GameSim/Systems/ICommandExecutor.h:28`
  - `eCommandKind::BuyItem = 5`
- `Shared/GameSim/Systems/ICommandExecutor.h:44`
  - `GameCommandWire::itemId`
- `Shared/GameSim/Systems/ICommandExecutor.h:57`
  - `GameCommand::itemId`
- `Shared/GameSim/Systems/CommandExecutor.cpp:1279`
  - `CDefaultCommandExecutor::HandleBuyItem`는 현재 비어 있다.
- `Shared/Schemas/Command.fbs:9`
  - FlatBuffers command schema에도 `BuyItem`과 `itemId`가 이미 있다.

### 기존 아이템 정의

- `Shared/GameSim/Definitions/ItemDef.h`
  - 현재 필드: `itemId`, `price`, `stats`, `displayName`
  - 현재 stat: `flatAd`, `flatAp`, `flatArmor`, `flatMr`, `percentMoveSpeed`
  - 확장 필요: icon path, tags, tier, recipe, sell price, usable/active 정보.

### 현재 에셋

상점 프레임/atlas:

- `Client/Bin/Resource/Texture/UI/itemshop_texture_atlas.png`
- `Client/Bin/Resource/Texture/UI/itemshop_texture_atlas_2.png`
- `Client/Bin/Resource/Texture/UI/itemshop_texture_atlas_3.png`
- `Client/Bin/Resource/Texture/UI/itemshop_texture_atlas4.png`
- `Client/Bin/Resource/Texture/UI/itemshopatlas.png`
- `Client/Bin/Resource/Texture/UI/itemshopsupportatlas.png`
- `Client/Bin/Resource/Texture/UI/hovershop.png`
- `Client/Bin/Resource/Texture/UI/HUD/itemshophud-default.png`

아이콘:

- `Client/Bin/Resource/Texture/UI/Items/*.png`
- 파일명은 대부분 `1001_class_t1_bootsofspeed.png`처럼 item id prefix를 갖고 있다.
- 1차 manifest는 파일명 앞 4자리 숫자를 파싱해 `itemId -> iconPath`로 자동 생성할 수 있다.

## 구현 순서

### SHOP-0. 범위 고정

먼저 메타 상점과 인게임 상점을 분리한다.

- 메타 상점: `CShopClient`, `Services/internal/shop`, 스킨/RP/계정 인벤토리.
- 인게임 상점: `P` UI, 골드, 추천 아이템, 아이템 슬롯, `BuyItem` command.

이번 작업은 인게임 상점만 한다. 기존 Go Shop Service는 건드리지 않는다.

### SHOP-1. P 토글 + 상점 shell

목표: 게임 화면 중앙/좌측에 상점 창이 뜨고 `P`를 다시 누르면 닫힌다.

추가 상태:

```cpp
bool_t m_bShowItemShop = false;
void* m_pSRV_ItemShopAtlas0 = nullptr;
void* m_pSRV_ItemShopAtlas1 = nullptr;
void* m_pSRV_ItemShopAtlas2 = nullptr;
void* m_pSRV_ItemShopAtlas3 = nullptr;
void* m_pSRV_ItemShopHud = nullptr;
```

추가 함수:

```cpp
void Handle_ShopInput();
void Draw_ItemShopWindow(ImDrawList* pDraw);
void Draw_ItemShopFrame(ImDrawList* pDraw, const ImVec2& pos, const ImVec2& size);
```

렌더 흐름:

```cpp
void CUI_Manager::Render_Overlay(const Mat4& matVP)
{
    if (!m_pWorld) return;

    Handle_ShopInput();

    ImDrawList* pDraw = ImGui::GetBackgroundDrawList();
    // HP bars...

    ImDrawList* pFG = ImGui::GetForegroundDrawList();
    if (m_bShowChampionHUD) Draw_ChampionHUD(pFG);
    if (m_bShowItemShop) Draw_ItemShopWindow(pFG);
}
```

입력 흐름:

```cpp
void CUI_Manager::Handle_ShopInput()
{
    if (CInput::Get().IsKeyPressed('P'))
        m_bShowItemShop = !m_bShowItemShop;

    if (m_bShowItemShop && CInput::Get().IsKeyPressed(VK_ESCAPE))
        m_bShowItemShop = false;
}
```

주의: 상점이 열린 상태에서는 전투 입력을 막을지 결정해야 한다. 1차는 입력 차단 없이 UI만 띄우고, 2차에서 마우스가 상점 rect 안에 있을 때만 월드 우클릭/스킬 입력을 소비한다.

### SHOP-2. item icon manifest 자동 생성

목표: `UI/Items` 폴더의 실제 png를 그대로 상점에 연결한다.

생성 파일 후보:

- `Client/Bin/Data/UI/item_icon_manifest.json`

형식:

```json
{
  "1001": "Resource/Texture/UI/Items/1001_class_t1_bootsofspeed.png",
  "1036": "Resource/Texture/UI/Items/1036_class_t1_longsword.png",
  "1055": "Resource/Texture/UI/Items/1055_marksman_t1_doransblade.png"
}
```

규칙:

- 파일명 앞 4자리 숫자를 `itemId`로 사용한다.
- 중복 item id가 있으면 우선순위는 `class/base/role` 일반 파일 > 실험/테스트 파일.
- manifest 생성은 처음에는 수동 JSON으로 두고, 이후 `Tools`에 generator를 추가한다.

### SHOP-3. 실제 item data source 도입

목표: UI 텍스트와 GameSim 구매 검증이 같은 데이터를 보게 한다.

신규/확장 파일 후보:

- `Shared/GameSim/Definitions/ItemDef.h`
- `Shared/GameSim/Definitions/ItemCatalog.h`
- `Shared/GameSim/Definitions/ItemCatalog.cpp`
- `Client/Bin/Data/GameSim/items.json`
- `Server/Bin/Data/GameSim/items.json`

확장 구조:

```cpp
enum class eItemTag : u32_t
{
    None        = 0,
    Starter     = 1 << 0,
    Fighter     = 1 << 1,
    Marksman    = 1 << 2,
    Mage        = 1 << 3,
    Tank        = 1 << 4,
    Assassin    = 1 << 5,
    Support     = 1 << 6,
    Boots       = 1 << 7,
    Consumable  = 1 << 8,
};

struct ItemRecipeDef
{
    u16_t from[4]{};
    u8_t fromCount = 0;
    u16_t combineCost = 0;
};

struct ItemDef
{
    u16_t itemId = 0;
    u16_t price = 0;
    u16_t sellPrice = 0;
    u32_t tagMask = 0;
    ItemStatModifier stats{};
    ItemRecipeDef recipe{};
    const char* displayName = nullptr;
    const char* iconPath = nullptr;
};
```

초기 JSON 예:

```json
{
  "items": [
    {
      "itemId": 1036,
      "displayName": "Long Sword",
      "price": 350,
      "sellPrice": 245,
      "iconPath": "Resource/Texture/UI/Items/1036_class_t1_longsword.png",
      "tags": ["Fighter", "Marksman"],
      "stats": { "flatAd": 10.0 }
    },
    {
      "itemId": 1001,
      "displayName": "Boots of Speed",
      "price": 300,
      "sellPrice": 210,
      "iconPath": "Resource/Texture/UI/Items/1001_class_t1_bootsofspeed.png",
      "tags": ["Boots"],
      "stats": { "percentMoveSpeed": 0.25 }
    }
  ]
}
```

수치 원칙:

- 1차는 로컬 JSON을 source of truth로 둔다.
- 최신 LoL 라이브 수치와 정확히 맞출 필요가 생기면 별도 데이터 수집 단계에서 Riot/Data Dragon 또는 CommunityDragon 기준으로 갱신한다.
- 엔진/서버는 외부 API를 런타임 의존하지 않는다. 빌드 전/개발 중 데이터만 JSON으로 동결한다.

### SHOP-4. 챔피언 데이터와 추천 아이템 연결

목표: 선택한 챔피언에 따라 추천 탭이 달라진다.

신규 파일 후보:

- `Shared/GameSim/Definitions/ChampionShopProfile.h`
- `Client/Bin/Data/GameSim/champion_shop_profiles.json`

구조:

```cpp
struct ChampionShopProfile
{
    eChampion champion = eChampion::END;
    u16_t starterItems[8]{};
    u8_t starterCount = 0;
    u16_t coreItems[12]{};
    u8_t coreCount = 0;
    u16_t boots[4]{};
    u8_t bootsCount = 0;
};
```

JSON 예:

```json
{
  "Irelia": {
    "starter": [1055, 2003],
    "core": [3153, 3078, 3053],
    "boots": [3006, 3047, 3111]
  },
  "Yasuo": {
    "starter": [1055, 2003],
    "core": [3046, 3031, 3153],
    "boots": [3006]
  }
}
```

UI binding:

- local player는 `ChampionComponent + LocalPlayerTag`로 찾는다.
- `ChampionComponent.id`로 `ChampionShopProfile`을 조회한다.
- 추천 탭은 `starter/core/boots` 순서로 섹션 렌더링한다.
- 챔피언이 없거나 profile이 없으면 All 탭으로 fallback한다.

### SHOP-5. 상점 UI 레이아웃

초기 창:

- 위치: 화면 중앙보다 약간 왼쪽, `960x650` 기준.
- 상단: 검색창 + 탭.
- 좌측: 카테고리.
- 중앙: 아이템 그리드.
- 우측: 선택 아이템 상세.
- 하단: 현재 골드, 구매 버튼, 되돌리기/판매 예정 영역.

탭:

- Recommended
- All
- Starter
- Attack
- Magic
- Defense
- Boots
- Consumable

아이템 카드:

- icon: `UI/Items/*.png`
- name: `ItemDef.displayName`
- price: `ItemDef.price`
- disabled: 골드 부족, 구매 불가, 재료 부족 등.

상세 패널:

- 큰 아이콘
- 이름
- 가격
- stat summary
- recipe tree
- Buy button

### SHOP-6. 구매 command 연결

목표: UI 버튼 클릭이 바로 `GameCommand::BuyItem`로 이어진다.

1차 클라이언트 API 후보:

```cpp
void Request_BuyItem(u16_t itemId);
```

wire:

```cpp
GameCommandWire wire{};
wire.kind = eCommandKind::BuyItem;
wire.itemId = itemId;
```

서버 검증:

- issuer entity가 살아 있는가.
- shop/fountain 구매 가능 범위 안인가.
- `ItemCatalog`에 존재하는 itemId인가.
- 현재 gold가 충분한가.
- inventory slot이 있는가.
- recipe item을 보유하면 combine cost만 차감한다.
- 구매 성공 시 inventory와 gold 변경.
- `StatSystem`에 dirty flag를 주고 재계산한다.

### SHOP-7. 인벤토리/골드/스탯 컴포넌트

신규 후보:

```cpp
struct GoldComponent
{
    u32_t gold = 500;
};

struct InventorySlot
{
    u16_t itemId = 0;
    u8_t stacks = 0;
};

struct InventoryComponent
{
    static constexpr u32_t kItemSlotCount = 6;
    static constexpr u32_t kTrinketSlot = 6;
    InventorySlot slots[7]{};
};
```

스탯 반영:

- `BaseStats + LevelGrowth + ItemStats + BuffStats` 순서.
- 아이템 구매/판매/사용 시 `StatComponent`를 재계산한다.
- 기존 `ItemStatModifier`의 `flatAd`, `flatAp`, `flatArmor`, `flatMr`, `percentMoveSpeed`부터 연결한다.
- 이후 attack speed, crit, health, mana, ability haste, lifesteal 등 확장한다.

### SHOP-8. HUD 아이템 슬롯 연동

목표: 구매한 아이템이 하단 HUD 슬롯에 즉시 보인다.

- `Draw_ChampionHUD`의 현재 placeholder item slot을 `InventoryComponent` 기반으로 바꾼다.
- `InventoryComponent.slots[i].itemId` -> `ItemCatalog.iconPath` -> SRV cache -> `AddImage`.
- 빈 슬롯은 기존 ability/item slot atlas crop으로 유지.
- active item은 `UseItem` command로 연결한다.

### SHOP-9. snapshot/replication

서버 권위 목표:

- 서버가 inventory/gold를 authoritative state로 가진다.
- 클라이언트는 구매 버튼을 눌러 command만 보낸다.
- 서버 snapshot/event가 돌아오면 UI/HUD를 확정 반영한다.

추가 replication 후보:

- `gold`
- `inventory[7]`
- `itemCooldowns`
- `lastShopErrorCode`

실패 event:

- NotEnoughGold
- InventoryFull
- OutOfShopRange
- InvalidItem
- DeadOrInvalidIssuer

## 1차 코드 변경 파일 예상

### 바로 구현 slice

- `Engine/Public/Manager/UI/UI_Manager.h`
  - shop open 상태, atlas SRV, selected item, filter state 추가.
- `Engine/Private/Manager/UI/UI_Manager.cpp`
  - atlas load/release.
  - `P` toggle.
  - atlas 기반 shop frame.
  - item grid dummy 10개 또는 manifest 기반 아이콘 렌더.
- `Client/Bin/Data/UI/item_icon_manifest.json`
  - `UI/Items` 실제 파일 기반 manifest.
- `.md/TODO/05-12/INGAME_ITEM_SHOP_P_TOGGLE_DATA_PLAN.md`
  - 본 계획서.

### 다음 구현 slice

- `Shared/GameSim/Definitions/ItemDef.h`
- `Shared/GameSim/Definitions/ItemCatalog.h/.cpp`
- `Shared/GameSim/Components/InventoryComponent.h`
- `Shared/GameSim/Components/GoldComponent.h`
- `Shared/GameSim/Systems/CommandExecutor.cpp`
  - `HandleBuyItem` 실제 구현.
- `Shared/GameSim/Systems/StatSystem.cpp`
  - item stat aggregation.
- `Client/Private/Network/Client/CommandSerializer.cpp`
  - `BuyItem` command send helper 정리.
- snapshot schema / applier
  - inventory/gold 동기화.

## UI atlas 사용 방침

1차는 정확한 pixel-perfect crop보다 "실제 atlas를 써서 조립된 상점 창이 뜨는 것"을 우선한다.

- `itemshophud-default.png`가 완성형 배경이면 먼저 통짜 배경으로 사용한다.
- 버튼/탭/검색창/hover는 `itemshop_texture_atlas*.png`에서 필요한 조각을 점진 crop한다.
- 아이템 아이콘은 atlas가 아니라 `UI/Items/*.png` 원본 개별 파일을 쓴다. 아이템은 개수가 많고 id 기반 lookup이 필요해서 개별 png가 유지보수에 더 좋다.
- `itemshopatlas.png`, `itemshopsupportatlas.png`는 frame, divider, scrollbar, button state 용도로 사용한다.

## 구현 중 주의점

- 경로 문자열은 반드시 `/` 또는 wide string raw-safe 방식 사용.
  - `"C:\Users\..."` 금지.
  - `"Resource/Texture/UI/Items/1036_class_t1_longsword.png"` 사용.
- `CUI_Manager`는 Engine 내부 클래스다. Client는 `CGameInstance::UI_*` forwarding만 사용한다.
- 상점이 열린 동안 마우스가 shop rect 위에 있으면 월드 클릭을 막아야 한다. 이건 2차에서 combat input bridge와 협의한다.
- 현재 full Client 빌드는 `Scene_InGame.cpp` brace/private access 쪽 unrelated 에러가 있으므로, 상점 1차는 Engine 빌드와 해당 파일 컴파일 단위 확인부터 한다.

## 검증 순서

1. Engine 빌드.
2. `P` 입력으로 상점 열림/닫힘 확인.
3. `Esc`로 닫힘 확인.
4. atlas load 실패 시 debug output 경로 확인.
5. `UI/Items` 아이콘 10개 이상 표시 확인.
6. local champion이 Irelia면 recommended profile이 Irelia로 선택되는지 확인.
7. dummy buy click이 `BuyItem itemId` debug log까지 도달하는지 확인.
8. 서버 `HandleBuyItem` 구현 후 골드 감소, inventory slot 추가 확인.
9. HUD item slot에 구매 아이콘이 뜨는지 확인.
10. 골드 부족/인벤토리 full/상점 범위 밖 실패 케이스 확인.

## 바로 다음 작업

1. `CUI_Manager`에 `P` 토글 상태와 `Draw_ItemShopWindow` shell을 추가한다.
2. itemshop atlas와 `itemshophud-default.png`를 로드한다.
3. `UI/Items` 파일명 prefix 기반으로 1차 manifest를 만든다.
4. 상점 창에 실제 아이템 아이콘 grid를 띄운다.
5. 선택 아이템 detail pane과 Buy 버튼까지 붙인다.
6. 그 다음 `BuyItem` command와 `HandleBuyItem` 서버 검증으로 넘어간다.
