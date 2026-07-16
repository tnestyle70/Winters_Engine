Session - Resource Scanned Item Shop Lua Designer

## 1. 반영해야 하는 코드

### 본질 원인과 경계

`Items` 폴더에는 수백 PNG가 있지만 Client catalog는 34개를 constexpr로 고정했다. 폴더 자산과 서버 `ItemDef`는 다른 권위다. 모든 PNG는 배치 preview에 노출하되 구매와 가격은 서버 registry에 등록된 ID만 허용한다.

### 기존 파일: `Client/Private/GamePlay/LoLUIContentRegistry.cpp`

`Resource/Texture/UI/Items`와 repo 실행용 fallback을 스캔한다. 파일명 선두 숫자를 item ID로 해석하고 `_t1_/_t2_/_t3_/_t4_`를 기본 section으로 분류한다. 기존 34개 순서는 seed로 보존하며 asset filename으로 중복 제거한다.

```cpp
const std::filesystem::path candidates[] =
{
    std::filesystem::path(L"Resource/Texture/UI/Items"),
    std::filesystem::path(L"Client/Bin/Resource/Texture/UI/Items"),
};

Desc.iPrice = pItem ? pItem->price : 0u;
Desc.bPurchasable = pItem != nullptr && Entry.iItemId != 0u;
```

### 기존 파일: `Client/Private/UI/ChampionTuner.cpp`

Shop Layout에 검색, PNG 폴더 재스캔, Lua reload, server/resource 구분, price 표시, 등록 item의 Balance sheet 이동을 추가한다. 기존 Item Balance의 Price override가 서버 practice command를 계속 사용한다.

### 기존 파일: `Client/Bin/Resource/UI/Lua/itemshop_catalog.lua`

파일명별 아래 table을 UI 기획자가 직접 편집한다.

```lua
WintersItemShopLayoutOverrides = {
    -- ["1055_marksman_t1_doransblade.png"] = {
    --     order = 1, x = 172, y = 201,
    --     section = "starter", hidden = false,
    -- },
}
```

스크립트는 `UI.GetShopItems()` 결과에 override를 적용하고 order/key로 정렬한 뒤 x/y가 없는 항목만 10열 자동 배치한다. 즉 폴더 추가 → Rescan → Lua 위치 편집 → Reload Lua 순서로 코드 재빌드 없이 반복한다.

## 2. 검증

```powershell
(Get-ChildItem Client/Bin/Resource/Texture/UI/Items -Filter *.png -File).Count
msbuild Client/Include/Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64
```

튜너 gate: PNG 개수와 catalog 개수가 일치하고 검색/재스캔이 동작한다. Lua gate: 한 파일에 `order/x/y/section` override를 넣고 reload하면 즉시 위치가 바뀐다. 구매 gate: resource-only 항목은 보이지만 우클릭 구매가 거절되고 등록 ItemDef만 서버 가격 검증을 통과한다.
