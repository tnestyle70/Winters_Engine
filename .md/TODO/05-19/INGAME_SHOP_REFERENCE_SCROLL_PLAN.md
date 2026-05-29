Session - 인게임 상점을 상점1/상점2.png 레퍼런스와 64x64 ItemIcon 기준으로 재배치하고, 아이템 리스트 스크롤을 추가한다.

1. 반영해야 되는 코드

공통 기준:
- `Client/Bin/Resource/Texture/UI/상점1.png`는 `1165x736`이고 현재 `DrawInGameShop` 기준 좌표계와 일치한다.
- `Client/Bin/Resource/Texture/UI/상점2.png`는 `1132x729`이며 스크롤이 내려간 상태의 검증 레퍼런스로 사용한다.
- `Client/Bin/Resource/Texture/UI/Items/{itemId}_*.png` 아이콘들은 확인된 대상 기준 `64x64`다.
- 이번 코드는 UI 배치/스크롤/아이콘 표시를 목표로 한다. `Shared/GameSim/Definitions/ItemDef.h`에 없는 아이템은 서버 구매/스탯 적용까지 확정하지 않는다.
- CONFIRM_NEEDED: 상점1/상점2에 보이는 전체 아이템의 정확한 `itemId/name/price/stat` 표. 현재 repo의 authoritative `CItemRegistry`에는 14개만 등록되어 있으므로, 전체 카탈로그를 GameSim 구매 가능 아이템으로 확장하려면 별도 표가 필요하다.

1-1. C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h

기존 코드:

```cpp
    struct InGameShopItemView
    {
        u16_t iItemId = 0;
        u16_t iPrice = 0;
        const char* pName = nullptr;
        std::wstring strIconPath;
        void* pSRV = nullptr;
    };
```

아래로 교체:

```cpp
    struct InGameShopItemView
    {
        u16_t iItemId = 0;
        u16_t iPrice = 0;
        u8_t iSection = 0;
        bool_t bPurchasable = false;
        const char* pName = nullptr;
        std::wstring strIconPath;
        void* pSRV = nullptr;
    };
```

기존 코드:

```cpp
    std::vector<InGameShopItemView> m_InGameShopItems;
    std::array<u16_t, 6> m_InGameInventorySlots{};
    void* m_pSRV_InGameShopReference = nullptr;
    CUIAtlasManifest m_InGameShopAtlasManifest;
    bool_t m_bInGameShopOpen = false;
    f32_t m_fInGameShopReferenceAlpha = 0.14f;
    u16_t m_iSelectedInGameShopItemId = 0;
```

아래로 교체:

```cpp
    std::vector<InGameShopItemView> m_InGameShopItems;
    std::array<u16_t, 6> m_InGameInventorySlots{};
    void* m_pSRV_InGameShopReference = nullptr;
    CUIAtlasManifest m_InGameShopAtlasManifest;
    bool_t m_bInGameShopOpen = false;
    f32_t m_fInGameShopReferenceAlpha = 0.14f;
    f32_t m_fInGameShopScrollY = 0.f;
    bool_t m_bInGameShopScrollbarDragging = false;
    u16_t m_iSelectedInGameShopItemId = 0;
```

1-2. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

`namespace` 내부에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    constexpr f32_t kChampionHUDRefW = 861.f;
    constexpr f32_t kChampionHUDRefH = 167.f;
```

아래에 추가:

```cpp
    enum eInGameShopSection : u8_t
    {
        SHOP_SECTION_STARTER = 0,
        SHOP_SECTION_BASIC = 1,
        SHOP_SECTION_EPIC = 2,
        SHOP_SECTION_LEGENDARY = 3,
    };

    struct InGameShopCatalogEntry
    {
        u16_t iItemId = 0;
        u16_t iPrice = 0;
        u8_t iSection = SHOP_SECTION_BASIC;
        const char* pName = nullptr;
    };

    const char* UI_GetInGameShopSectionName(u8_t iSection)
    {
        switch (iSection)
        {
        case SHOP_SECTION_STARTER:
            return "Starter";
        case SHOP_SECTION_BASIC:
            return "Basic";
        case SHOP_SECTION_EPIC:
            return "Epic";
        case SHOP_SECTION_LEGENDARY:
            return "Legendary";
        default:
            return "";
        }
    }
```

`LoadInGameShopAssets` 안에서 아래 기존 코드 블록을 교체:

기존 코드:

```cpp
    static constexpr u16_t kShopItemIds[] =
    {
        1055, 1056, 1036, 1042, 1028, 1029, 1033,
        1001, 1052, 1037, 1043, 1053, 1058, 1038,
    };

    for (const u16_t itemId : kShopItemIds)
    {
        const ItemDef* pItem = CItemRegistry::Instance().Find(itemId);
        if (!pItem)
            continue;

        InGameShopItemView Item{};
        Item.iItemId = pItem->itemId;
        Item.iPrice = pItem->price;
        Item.pName = pItem->displayName;
        wchar_t IconPath[MAX_PATH] = {};
        if (UI_TryFindItemIconPath(Item.iItemId, IconPath, MAX_PATH))
        {
            Item.strIconPath = IconPath;
            if (FAILED(Load_TextureSRV(Item.strIconPath.c_str(), &Item.pSRV)))
                Item.pSRV = nullptr;
        }
        m_InGameShopItems.push_back(Item);
    }
```

아래로 교체:

```cpp
    static constexpr InGameShopCatalogEntry kShopCatalog[] =
    {
        { 1056, 400, SHOP_SECTION_STARTER, "Doran's Ring" },
        { 1055, 450, SHOP_SECTION_STARTER, "Doran's Blade" },
        { 1036, 350, SHOP_SECTION_BASIC, "Long Sword" },
        { 1042, 250, SHOP_SECTION_BASIC, "Dagger" },
        { 1028, 400, SHOP_SECTION_BASIC, "Ruby Crystal" },
        { 1029, 300, SHOP_SECTION_BASIC, "Cloth Armor" },
        { 1033, 400, SHOP_SECTION_BASIC, "Null-Magic Mantle" },
        { 1001, 300, SHOP_SECTION_BASIC, "Boots" },
        { 1052, 400, SHOP_SECTION_BASIC, "Amplifying Tome" },
        { 1037, 875, SHOP_SECTION_BASIC, "Pickaxe" },
        { 1043, 700, SHOP_SECTION_BASIC, "Recurve Bow" },
        { 1053, 900, SHOP_SECTION_BASIC, "Vampiric Scepter" },
        { 1058, 1200, SHOP_SECTION_BASIC, "Needlessly Large Rod" },
        { 1038, 1300, SHOP_SECTION_BASIC, "B. F. Sword" },
    };

    for (const InGameShopCatalogEntry& Entry : kShopCatalog)
    {
        const ItemDef* pItem = CItemRegistry::Instance().Find(Entry.iItemId);

        InGameShopItemView Item{};
        Item.iItemId = Entry.iItemId;
        Item.iPrice = pItem ? pItem->price : Entry.iPrice;
        Item.iSection = Entry.iSection;
        Item.bPurchasable = pItem != nullptr;
        Item.pName = (pItem && pItem->displayName) ? pItem->displayName : Entry.pName;
        wchar_t IconPath[MAX_PATH] = {};
        if (UI_TryFindItemIconPath(Item.iItemId, IconPath, MAX_PATH))
        {
            Item.strIconPath = IconPath;
            if (FAILED(Load_TextureSRV(Item.strIconPath.c_str(), &Item.pSRV)))
                Item.pSRV = nullptr;
        }
        m_InGameShopItems.push_back(Item);
    }
```

`LoadInGameShopAssets` 끝에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    m_strInGameShopStatus = "Left click selects, right click buys";
```

아래에 추가:

```cpp
    m_fInGameShopScrollY = 0.f;
    m_bInGameShopScrollbarDragging = false;
```

1-3. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

`SetInGameShopOpen`에서 아래 기존 코드 교체:

기존 코드:

```cpp
void CUI_Manager::SetInGameShopOpen(bool_t b)
{
    m_bInGameShopOpen = b;
    if (m_pLuaUIHost)
        m_pLuaUIHost->SetBoolean("InGameShopOpen", b);
}
```

아래로 교체:

```cpp
void CUI_Manager::SetInGameShopOpen(bool_t b)
{
    if (b && !m_bInGameShopOpen)
    {
        m_fInGameShopScrollY = 0.f;
        m_bInGameShopScrollbarDragging = false;
    }

    m_bInGameShopOpen = b;
    if (m_pLuaUIHost)
        m_pLuaUIHost->SetBoolean("InGameShopOpen", b);
}
```

1-4. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

`DrawInGameShop` 안에서 아래 기존 코드 교체:

기존 코드:

```cpp
    constexpr f32_t kIconSize = 40.f;
    constexpr f32_t kIconStepX = 56.f;
    constexpr f32_t kRowStepY = 114.f;
    constexpr f32_t kStartX = 172.f;
    constexpr f32_t kStartY = 200.f;
```

아래로 교체:

```cpp
    constexpr u32_t kShopColumns = 7u;
    constexpr f32_t kIconSize = 40.f;
    constexpr f32_t kIconStepX = 56.f;
    constexpr f32_t kRowStepY = 114.f;
    constexpr f32_t kStartX = 172.f;
    constexpr f32_t kStartY = 200.f;
    constexpr f32_t kListClipX0 = 154.f;
    constexpr f32_t kListClipY0 = 166.f;
    constexpr f32_t kListClipX1 = 724.f;
    constexpr f32_t kListClipY1 = 686.f;
    constexpr f32_t kScrollSpeed = 58.f;
    constexpr f32_t kScrollBarX = 720.f;
    constexpr f32_t kScrollBarY0 = 165.f;
    constexpr f32_t kScrollBarY1 = 669.f;
```

`DrawInGameShop` 안에서 아래 기존 코드 삭제:

삭제할 코드:

```cpp
    UI_DrawOutlinedText(pDraw, pTitleFont, TitleFontSize, ToShop(170.f, 174.f), NormalColor, "Starter");
    UI_DrawOutlinedText(pDraw, pTitleFont, TitleFontSize, ToShop(170.f, 288.f), NormalColor, "Basic");
```

`DrawInGameShop` 안에서 아래 기존 아이템 그리기 루프 전체를 교체:

기존 코드:

```cpp
    for (u32_t Index = 0; Index < m_InGameShopItems.size(); ++Index)
    {
        const u32_t Row = Index / 7u;
        const u32_t Col = Index % 7u;
        if (Row >= 2u)
            break;

        const f32_t X = kStartX + static_cast<f32_t>(Col) * kIconStepX;
        const f32_t Y = kStartY + static_cast<f32_t>(Row) * kRowStepY;
        const ImVec2 CellMin = ToShop(X, Y);
        const ImVec2 CellMax = ToShop(X + kIconSize, Y + kIconSize);
        const bool_t bHovered = PointInRect(IO.MousePos, CellMin, CellMax);

        const InGameShopItemView& Item = m_InGameShopItems[Index];
        if (bHovered && IO.MouseClicked[0])
        {
            m_iSelectedInGameShopItemId = Item.iItemId;
            m_strInGameShopStatus = Item.pName ? Item.pName : "Item selected";
        }
        if (bHovered && IO.MouseClicked[1])
        {
            m_iSelectedInGameShopItemId = Item.iItemId;
            TryBuyInGameItem(Item.iItemId);
        }

        DrawSprite("slot.frame", X - 1.f, Y - 1.f, kIconSize + 2.f, kIconSize + 2.f, 0.86f);
        if (Item.pSRV)
        {
            pDraw->AddImage(
                reinterpret_cast<ImTextureID>(Item.pSRV),
                CellMin,
                CellMax,
                ImVec2(0.f, 0.f),
                ImVec2(1.f, 1.f),
                IM_COL32(255, 255, 255, 255));
        }
        else
        {
            pDraw->AddRectFilled(CellMin, CellMax, IM_COL32(9, 29, 35, 236));
            char FallbackText[16]{};
            std::snprintf(FallbackText, sizeof(FallbackText), "%u", static_cast<u32_t>(Item.iItemId));
            UI_DrawOutlinedText(
                pDraw,
                pFont,
                SmallFontSize,
                ImVec2(CellMin.x + 2.f * Scale, CellMin.y + 10.f * Scale),
                bHovered ? HoverColor : NormalColor,
                FallbackText);
        }

        pDraw->AddRect(
            CellMin,
            CellMax,
            (bHovered || m_iSelectedInGameShopItemId == Item.iItemId)
                ? IM_COL32(244, 211, 126, 255)
                : IM_COL32(92, 85, 80, 210),
            0.f,
            0,
            (bHovered || m_iSelectedInGameShopItemId == Item.iItemId) ? 2.f : 1.f);

        char PriceText[16]{};
        std::snprintf(PriceText, sizeof(PriceText), "%u", static_cast<u32_t>(Item.iPrice));
        const ImVec2 PriceSize = pFont->CalcTextSizeA(SmallFontSize, FLT_MAX, 0.f, PriceText);
        ImVec2 PricePos = ToShop(X + kIconSize * 0.5f, Y + 44.f);
        PricePos.x -= PriceSize.x * 0.5f;
        UI_DrawOutlinedText(
            pDraw,
            pFont,
            SmallFontSize,
            PricePos,
            bHovered ? HoverColor : NormalColor,
            PriceText);
    }
```

아래로 교체:

```cpp
    struct ShopItemLayout
    {
        u32_t iIndex = 0;
        u32_t iVisualRow = 0;
        u32_t iCol = 0;
    };
    struct ShopSectionLayout
    {
        u8_t iSection = 0;
        u32_t iVisualRow = 0;
    };

    std::vector<ShopItemLayout> ItemLayouts;
    std::vector<ShopSectionLayout> SectionLayouts;
    ItemLayouts.reserve(m_InGameShopItems.size());

    u32_t VisualRow = 0;
    u32_t ItemsInSection = 0;
    u8_t LastSection = 0xffu;
    auto FinishSection = [&]()
    {
        if (ItemsInSection == 0)
            return;

        VisualRow += (ItemsInSection + kShopColumns - 1u) / kShopColumns;
        ItemsInSection = 0;
    };

    for (u32_t Index = 0; Index < m_InGameShopItems.size(); ++Index)
    {
        const InGameShopItemView& Item = m_InGameShopItems[Index];
        if (Item.iSection != LastSection)
        {
            if (LastSection != 0xffu)
                FinishSection();
            SectionLayouts.push_back(ShopSectionLayout{ Item.iSection, VisualRow });
            LastSection = Item.iSection;
        }

        ItemLayouts.push_back(ShopItemLayout{
            Index,
            VisualRow + ItemsInSection / kShopColumns,
            ItemsInSection % kShopColumns });
        ++ItemsInSection;
    }
    FinishSection();

    const ImVec2 ListMin = ToShop(kListClipX0, kListClipY0);
    const ImVec2 ListMax = ToShop(kListClipX1, kListClipY1);
    const bool_t bListHovered = PointInRect(IO.MousePos, ListMin, ListMax);
    const f32_t ViewHeight = kListClipY1 - kListClipY0;
    const f32_t ContentBottom = (VisualRow > 0)
        ? kStartY + static_cast<f32_t>(VisualRow - 1u) * kRowStepY + 70.f
        : kListClipY1;
    const f32_t ContentHeight = std::max(0.f, ContentBottom - kListClipY0);
    const f32_t MaxScrollY = std::max(0.f, ContentHeight - ViewHeight);

    if (bListHovered && IO.MouseWheel != 0.f && MaxScrollY > 0.f)
        m_fInGameShopScrollY = std::clamp(m_fInGameShopScrollY - IO.MouseWheel * kScrollSpeed, 0.f, MaxScrollY);
    else
        m_fInGameShopScrollY = std::clamp(m_fInGameShopScrollY, 0.f, MaxScrollY);

    if (MaxScrollY > 0.f)
    {
        const ImVec2 TrackMin = ToShop(kScrollBarX, kScrollBarY0);
        const ImVec2 TrackMax = ToShop(kScrollBarX + 5.f, kScrollBarY1);
        const f32_t TrackHeight = TrackMax.y - TrackMin.y;
        const f32_t ThumbHeight = std::max(36.f * Scale, TrackHeight * (ViewHeight / (ViewHeight + MaxScrollY)));
        const f32_t ThumbTravel = std::max(1.f, TrackHeight - ThumbHeight);
        const f32_t ThumbRatio = MaxScrollY > 0.f ? (m_fInGameShopScrollY / MaxScrollY) : 0.f;
        ImVec2 ThumbMin(TrackMin.x, TrackMin.y + ThumbTravel * ThumbRatio);
        ImVec2 ThumbMax(TrackMax.x, ThumbMin.y + ThumbHeight);
        const bool_t bThumbHovered = PointInRect(IO.MousePos, ThumbMin, ThumbMax);

        if (bThumbHovered && IO.MouseClicked[0])
            m_bInGameShopScrollbarDragging = true;
        if (!IO.MouseDown[0])
            m_bInGameShopScrollbarDragging = false;
        if (m_bInGameShopScrollbarDragging)
        {
            const f32_t LocalY = std::clamp(IO.MousePos.y - TrackMin.y - ThumbHeight * 0.5f, 0.f, ThumbTravel);
            m_fInGameShopScrollY = (LocalY / ThumbTravel) * MaxScrollY;
            ThumbMin.y = TrackMin.y + ThumbTravel * (m_fInGameShopScrollY / MaxScrollY);
            ThumbMax.y = ThumbMin.y + ThumbHeight;
        }

        pDraw->AddRectFilled(TrackMin, TrackMax, IM_COL32(111, 82, 38, 170), 2.f * Scale);
        pDraw->AddRectFilled(
            ThumbMin,
            ThumbMax,
            (bThumbHovered || m_bInGameShopScrollbarDragging)
                ? IM_COL32(223, 169, 70, 255)
                : IM_COL32(185, 132, 53, 235),
            2.f * Scale);
    }

    pDraw->PushClipRect(ListMin, ListMax, true);

    for (const ShopSectionLayout& Section : SectionLayouts)
    {
        const f32_t LabelY = kStartY + static_cast<f32_t>(Section.iVisualRow) * kRowStepY - 26.f - m_fInGameShopScrollY;
        if (LabelY + 24.f < kListClipY0 || LabelY > kListClipY1)
            continue;

        UI_DrawOutlinedText(
            pDraw,
            pTitleFont,
            TitleFontSize,
            ToShop(170.f, LabelY),
            NormalColor,
            UI_GetInGameShopSectionName(Section.iSection));
    }

    for (const ShopItemLayout& Layout : ItemLayouts)
    {
        const InGameShopItemView& Item = m_InGameShopItems[Layout.iIndex];
        const f32_t X = kStartX + static_cast<f32_t>(Layout.iCol) * kIconStepX;
        const f32_t Y = kStartY + static_cast<f32_t>(Layout.iVisualRow) * kRowStepY - m_fInGameShopScrollY;
        if (Y + kIconSize + 28.f < kListClipY0 || Y > kListClipY1)
            continue;

        const ImVec2 CellMin = ToShop(X, Y);
        const ImVec2 CellMax = ToShop(X + kIconSize, Y + kIconSize);
        const bool_t bHovered = bListHovered && PointInRect(IO.MousePos, CellMin, CellMax);

        if (bHovered && IO.MouseClicked[0])
        {
            m_iSelectedInGameShopItemId = Item.iItemId;
            m_strInGameShopStatus = Item.pName ? Item.pName : "Item selected";
        }
        if (bHovered && IO.MouseClicked[1])
        {
            m_iSelectedInGameShopItemId = Item.iItemId;
            if (Item.bPurchasable)
                TryBuyInGameItem(Item.iItemId);
            else
                m_strInGameShopStatus = "Item stats not registered";
        }

        DrawSprite("slot.frame", X - 1.f, Y - 1.f, kIconSize + 2.f, kIconSize + 2.f, 0.86f);
        if (Item.pSRV)
        {
            pDraw->AddImage(
                reinterpret_cast<ImTextureID>(Item.pSRV),
                CellMin,
                CellMax,
                ImVec2(0.f, 0.f),
                ImVec2(1.f, 1.f),
                Item.bPurchasable ? IM_COL32(255, 255, 255, 255) : IM_COL32(170, 170, 170, 220));
        }
        else
        {
            pDraw->AddRectFilled(CellMin, CellMax, IM_COL32(9, 29, 35, 236));
            char FallbackText[16]{};
            std::snprintf(FallbackText, sizeof(FallbackText), "%u", static_cast<u32_t>(Item.iItemId));
            UI_DrawOutlinedText(
                pDraw,
                pFont,
                SmallFontSize,
                ImVec2(CellMin.x + 2.f * Scale, CellMin.y + 10.f * Scale),
                bHovered ? HoverColor : NormalColor,
                FallbackText);
        }

        pDraw->AddRect(
            CellMin,
            CellMax,
            (bHovered || m_iSelectedInGameShopItemId == Item.iItemId)
                ? IM_COL32(244, 211, 126, 255)
                : IM_COL32(92, 85, 80, 210),
            0.f,
            0,
            (bHovered || m_iSelectedInGameShopItemId == Item.iItemId) ? 2.f : 1.f);

        char PriceText[16]{};
        std::snprintf(PriceText, sizeof(PriceText), "%u", static_cast<u32_t>(Item.iPrice));
        const ImVec2 PriceSize = pFont->CalcTextSizeA(SmallFontSize, FLT_MAX, 0.f, PriceText);
        ImVec2 PricePos = ToShop(X + kIconSize * 0.5f, Y + 44.f);
        PricePos.x -= PriceSize.x * 0.5f;
        UI_DrawOutlinedText(
            pDraw,
            pFont,
            SmallFontSize,
            PricePos,
            bHovered ? HoverColor : NormalColor,
            PriceText);
    }

    pDraw->PopClipRect();
```

1-5. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

`TryBuyInGameItem`에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    if (iItemId == 0 || !m_pfnBuyItem)
    {
        OutputDebugStringA("[UI_Manager] BuyItem command callback missing\n");
        return false;
    }
```

아래에 추가:

```cpp
    const InGameShopItemView* pItemView = FindInGameShopItem(iItemId);
    if (pItemView && !pItemView->bPurchasable)
    {
        m_strInGameShopStatus = "Item stats not registered";
        return false;
    }
```

2. 검증

미검증
- 계획서만 작성됨. 코드 변경, 빌드, 런타임 확인은 아직 하지 않음.

검증 명령:
- `git diff --check`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64`

수동 확인:
- F5 인게임에서 `P`로 상점을 열었을 때 `상점1.png` 기준으로 패널 위치, 탭, 검색창, 카테고리 아이콘, 아이템 셀 시작 좌표가 맞는지 확인.
- 마우스 휠을 아이템 리스트 영역 위에서 굴렸을 때 아이템/섹션 라벨만 스크롤되고, 우측 상세 패널/인벤토리/구매 버튼은 고정되는지 확인.
- 스크롤을 내린 화면이 `상점2.png` 레퍼런스처럼 기본/서사급/전설급 영역이 위로 올라오는 구조인지 확인.
- 리스트 viewport 밖 아이콘/가격/hover outline이 `PushClipRect`에 의해 잘리는지 확인.
- 등록된 14개 `CItemRegistry` 아이템은 우클릭/BUY로 기존 `BuyItem` 서버 명령을 보내고, 미등록 visual-only 아이템은 구매되지 않고 `Item stats not registered` 상태가 뜨는지 확인.
- 구매 후 snapshot으로 골드/인벤토리가 갱신될 때 기존 HUD 인벤토리 아이콘 표시가 깨지지 않는지 확인.

확인 필요:
- `상점1.png`, `상점2.png`에 보이는 전체 아이템을 실제 구매 가능 아이템으로 만들려면 `Shared/GameSim/Definitions/ItemDef.h`에 들어갈 정확한 `itemId/name/price/stat` 표를 먼저 확정해야 한다.
- 전체 아이템이 GameSim에 추가되는 경우 `StatSystem`, `CommandExecutor::HandleBuyItem`, snapshot 적용 경로가 기존 서버 권위 흐름을 유지하는지 별도 검증해야 한다.

