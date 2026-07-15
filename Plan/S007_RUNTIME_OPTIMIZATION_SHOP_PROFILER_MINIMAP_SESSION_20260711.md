Session - 정상 F5의 중복 렌더와 첫 미니언 스폰 hitch를 제거하고, 454개 아이템 카탈로그·프로파일 저장·비율 기반 미니맵을 인게임 조정 경로에 연결한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Engine/Private/Manager/Profiler/ProfilerOverlay.cpp

`CProfilerOverlay::Draw_ControlBar`에서 compact header와 중복되는 아래 Save 버튼 및 상태 출력을 삭제:

```cpp
	ImGui::SameLine();
	if (ImGui::Button("Save"))
	{
		if (!m_bFreeze)
			Capture_DisplayFrame(true);
		const bool_t bSaved = Save_DisplayFrameToJson("profiler.json");
		m_strSaveStatus = bSaved ? "Saved profiler.json" : "Save failed: profiler.json";
	}

	if (!m_strSaveStatus.empty())
		ImGui::TextDisabled("%s", m_strSaveStatus.c_str());
```

상단 `DrawCompactHeader`의 Save가 유일한 visible ID와 저장 진입점이 된다.

1-2. C:/Users/user/Desktop/Winters/Engine/Private/Renderer/ModelRenderer.cpp

삭제할 코드:

```cpp
    bool_t ShouldLogAnimationName(const string& animName)
    {
        return !animName.empty();
    }
```

`ModelRenderer::PlayAnimationByNameAdvanced`에서 아래 성공 로그를 삭제하고 bounded missing-animation 진단만 유지:

```cpp
    if (ShouldLogAnimationName(pAnim->GetName()))
    {
        OutputDebugStringA((string("[ModelRenderer] Playing (loop=")
            + (bLoop ? "true" : "false")
            + ", reverse=" + (bReverse ? "true" : "false") + "): "
            + pAnim->GetName() + "\n").c_str());
    }
```

1-3. C:/Users/user/Desktop/Winters/Client/Private/Manager/Minion_Manager.cpp

기존 코드:

```cpp
    static constexpr uint32_t kNetworkVisualBindBudgetPerFrame = 3u;
    static constexpr u32_t kNetworkWarmRoleCount = 2u;
    static constexpr u32_t kNetworkWarmPoolSizePerTeamAndRole = 9u;
```

아래로 교체:

```cpp
    static constexpr uint32_t kNetworkVisualBindBudgetPerFrame = 3u;
    static constexpr u32_t kNetworkWarmPoolSizeByRole[] = { 9u, 9u, 3u, 3u, 1u };
```

`CMinion_Manager::Initialize`의 기존 코드:

```cpp
    m_pWorld = pWorld;
    if (m_bInitialized) return S_OK;
    m_fSpawnTimer  = 0.f;
    m_bInitialized = true;
```

아래로 교체:

```cpp
    m_pWorld = pWorld;
    if (m_bInitialized) return S_OK;
    m_fSpawnTimer = 0.f;
    m_bInitialized = true;

    constexpr size_t kExpectedConcurrentMinions = 72u;
    m_vecEntities.reserve(kExpectedConcurrentMinions);
    m_vecSpawnedThisTick.reserve(18u);
    m_mapRenderers.reserve(kExpectedConcurrentMinions);
    m_mapVisualStates.reserve(kExpectedConcurrentMinions);
```

`CMinion_Manager::Ensure_NetworkVisual`에서 base animation owner와 중복되는 아래 코드를 삭제:

```cpp
    const u32_t animCount = pRenderer->GetAnimationCount();
    if (animCount > 0)
        pRenderer->PlayAnimationByName("run", true);
```

`CMinion_Manager::PrewarmNetworkVisualResources`의 기존 코드:

```cpp
            ++loadedCount;
            if (typeIndex >= kNetworkWarmRoleCount)
                continue;

            auto& pool = m_vecNetworkRendererPool[teamIndex][typeIndex];
            while (pool.size() < kNetworkWarmPoolSizePerTeamAndRole)
            {
                WINTERS_PROFILE_SCOPE("MinionVisual::PrewarmRenderer");
                std::unique_ptr<ModelRenderer> pRenderer(new ModelRenderer());
                if (!pRenderer->Initialize(pPath, L"Shaders/Mesh3D.hlsl"))
                {
                    ++failedCount;
                    break;
                }

                pool.push_back(std::move(pRenderer));
                ++rendererCount;
            }
```

아래로 교체:

```cpp
            ++loadedCount;
            auto& pool = m_vecNetworkRendererPool[teamIndex][typeIndex];
            const u32_t warmCapacity = kNetworkWarmPoolSizeByRole[typeIndex];
            while (pool.size() < warmCapacity)
            {
                WINTERS_PROFILE_SCOPE("MinionVisual::PrewarmRenderer");
                std::unique_ptr<ModelRenderer> pRenderer(new ModelRenderer());
                if (!pRenderer->Initialize(pPath, L"Shaders/Mesh3D.hlsl"))
                {
                    ++failedCount;
                    break;
                }

                const ClientData::MinionVisualDefinition* pVisual =
                    ClientData::FindMinionVisualDefinition(typeIndex, teamIndex);
                if (pVisual && pVisual->textureAllMeshes.resourceRelativePath)
                    pRenderer->LoadTextureForAllMeshes(
                        pVisual->textureAllMeshes.resourceRelativePath);

                pool.push_back(std::move(pRenderer));
                ++rendererCount;
            }
```

`CMinion_Manager::ProcessQueueNetworkVisual`의 성공 batch `OutputDebugStringA` 블록은 삭제하고 `MinionVisual::PoolHit`, `MinionVisual::ColdCreate`, `MinionVisual::Created` counter를 유일한 정상 진단으로 유지한다.

1-4. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

`CScene_InGame::OnUpdate`에서 바로 연속 호출되는 첫 번째 아래 코드를 삭제하고, local test transform 반영 뒤 호출과 minion tick 뒤 호출은 유지:

```cpp
    ProjectGameplayActorsToMapSurface();

    m_MapTransform.SetRotation(m_vMapRotation);
```

아래로 교체:

```cpp
    m_MapTransform.SetRotation(m_vMapRotation);
```

1-5. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameRender.cpp

`CScene_InGame::OnRender`의 map, champion, scene-object snapshot 세 곳에서 기존 코드:

```cpp
        if (bRHISceneReady)
```

및

```cpp
    if (bRHISceneReady)
```

아래로 각각 교체:

```cpp
        if (bRHISceneOnly)
```

```cpp
    if (bRHISceneOnly)
```

정상 F5에서는 legacy visible path 한 번만 제출하고, `--rhi-scene-only` parity lab에서만 RHI snapshot을 조립한다. roster, map, minion, champion, UI, FX 시스템은 비활성화하지 않는다.

1-6. C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/ActorHUDAssets.h

기존 코드:

```cpp
    struct UIShopItemAssetDesc
    {
        u16_t iItemId = 0u;
        u16_t iPrice = 0u;
        const char* pDisplayName = nullptr;
        const wchar_t* pIconPath = nullptr;
        const char* const* pStatLines = nullptr;
        u32_t iStatLineCount = 0u;
    };
```

아래로 교체:

```cpp
    struct UIShopItemAssetDesc
    {
        u16_t iItemId = 0u;
        u16_t iPrice = 0u;
        u32_t iOrder = 0u;
        const char* pAssetKey = nullptr;
        const char* pSection = nullptr;
        const char* pDisplayName = nullptr;
        const wchar_t* pIconPath = nullptr;
        const char* pIconSprite = nullptr;
        const char* const* pStatLines = nullptr;
        u32_t iStatLineCount = 0u;
        bool_t bEnabled = true;
        bool_t bPurchasable = false;
    };
```

1-7. C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/LuaUIHost.h

기존 코드:

```cpp
        void SetActorHUDState(const ActorHUDState& State);
        void SetBuyItemCallback(void(*pfn)(void*, u16_t), void* pUser);
```

아래로 교체:

```cpp
        void SetActorHUDState(const ActorHUDState& State);
        void SetShopItems(const UIShopItemAssetDesc* pItems, u32_t iItemCount);
        void SetBuyItemCallback(void(*pfn)(void*, u16_t), void* pUser);
```

`class CLuaUIHost final` private 영역의 기존 코드:

```cpp
        static int LuaGetHudState(lua_State* pState);
        static int LuaText(lua_State* pState);
```

아래로 교체:

```cpp
        static int LuaGetHudState(lua_State* pState);
        static int LuaGetShopItems(lua_State* pState);
        static int LuaText(lua_State* pState);
```

`m_pShopAtlasManifest` 아래에 추가:

```cpp
        struct LuaShopItemView
        {
            u16_t ItemId = 0u;
            u16_t Price = 0u;
            u32_t Order = 0u;
            std::string AssetKey;
            std::string Section;
            std::string DisplayName;
            std::string IconPath;
            std::string IconSprite;
            bool_t Enabled = true;
            bool_t Purchasable = false;
        };

        std::vector<LuaShopItemView> m_ShopItems{};
```

필요 include에 `Manager/UI/ActorHUDAssets.h`와 `<vector>`를 추가한다.

1-8. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/LuaUIHost.cpp

`CLuaUIHost::SetActorHUDState` 바로 아래에 `UIShopItemAssetDesc`를 owned string으로 복사하고 `(Order, AssetKey)`로 stable sort하는 `SetShopItems`를 추가한다.

`CLuaUIHost::RegisterLuaApi`에서 기존 코드:

```cpp
        lua_pushcfunction(pState, &CLuaUIHost::LuaGetHudState);
        lua_setfield(pState, -2, "GetHudState");
```

아래로 교체:

```cpp
        lua_pushcfunction(pState, &CLuaUIHost::LuaGetHudState);
        lua_setfield(pState, -2, "GetHudState");
        lua_pushcfunction(pState, &CLuaUIHost::LuaGetShopItems);
        lua_setfield(pState, -2, "GetShopItems");
```

`CLuaUIHost::LuaGetHudState` 바로 아래에 추가:

```cpp
    int CLuaUIHost::LuaGetShopItems(lua_State* pState)
    {
        CLuaUIHost* pHost = GetHost(pState);
        lua_newtable(pState);
        if (!pHost)
            return 1;

        lua_Integer outputIndex = 1;
        for (const LuaShopItemView& Item : pHost->m_ShopItems)
        {
            if (!Item.Enabled)
                continue;

            lua_newtable(pState);
            lua_pushstring(pState, Item.AssetKey.c_str());
            lua_setfield(pState, -2, "key");
            lua_pushinteger(pState, Item.ItemId);
            lua_setfield(pState, -2, "id");
            lua_pushinteger(pState, Item.Price);
            lua_setfield(pState, -2, "price");
            lua_pushinteger(pState, Item.Order);
            lua_setfield(pState, -2, "order");
            lua_pushstring(pState, Item.Section.c_str());
            lua_setfield(pState, -2, "section");
            lua_pushstring(pState, Item.DisplayName.c_str());
            lua_setfield(pState, -2, "name");
            lua_pushstring(pState, Item.IconPath.c_str());
            lua_setfield(pState, -2, "icon");
            lua_pushstring(pState, Item.IconSprite.c_str());
            lua_setfield(pState, -2, "sprite");
            lua_pushboolean(pState, Item.Purchasable ? 1 : 0);
            lua_setfield(pState, -2, "purchasable");
            lua_rawseti(pState, -2, outputIndex++);
        }
        return 1;
    }
```

1-9. C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h

`InGameShopItemView` 기존 코드를:

```cpp
    struct InGameShopItemView
    {
        u16_t iItemId = 0;
        u16_t iPrice = 0;
        std::string strName;
        std::wstring strIconPath;
        std::vector<std::string> StatLines;
        void* pSRV = nullptr;
    };
```

아래로 교체:

```cpp
    struct InGameShopItemView
    {
        u16_t iItemId = 0;
        u16_t iPrice = 0;
        u32_t iOrder = 0u;
        std::string strAssetKey;
        std::string strSection;
        std::string strName;
        std::wstring strIconPath;
        std::string strIconSprite;
        std::vector<std::string> StatLines;
        bool_t bEnabled = true;
        bool_t bPurchasable = false;
        void* pSRV = nullptr;
    };
```

1-10. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

`CUI_Manager::RegisterInGameShopItems`에서 `Source.iItemId == 0u` 필터를 `itemId 또는 assetKey가 있는 행`으로 교체하고 새 필드를 복사한다. 목록 구축 직후 아래를 추가:

```cpp
    if (m_pLuaUIHost)
        m_pLuaUIHost->SetShopItems(pItems, iItemCount);

    if (!m_bUseLuaUI)
        LoadInGameShopItemTextures();
```

기존 무조건 호출은 삭제:

```cpp
    LoadInGameShopItemTextures();
```

`ClearInGameShopItems`에서 `m_pLuaUIHost->SetShopItems(nullptr, 0u)`도 호출한다. 이로써 Lua 정상 경로는 454개 개별 WIC/SRV를 registration frame에 만들지 않고 item atlas 한 장을 사용한다.

1-11. C:/Users/user/Desktop/Winters/Client/Public/GamePlay/LoLUIContentRegistry.h

기존 코드:

```cpp
namespace Client
{
    void RegisterLoLUIContent(Engine::CGameInstance& GameInstance);
}
```

아래로 교체:

```cpp
namespace Client
{
    void RegisterLoLUIContent(Engine::CGameInstance& GameInstance);
    void DrawLoLShopCatalogEditor(Engine::CGameInstance& GameInstance);
}
```

1-12. C:/Users/user/Desktop/Winters/Client/Private/GamePlay/LoLUIContentRegistry.cpp

`RegisterLoLShopItems`는 15개 고정 배열만 투영하는 구현을 삭제하고 다음 소유권으로 교체한다.

```cpp
        struct ShopCatalogItem
        {
            std::string AssetKey;
            std::string Section = "all";
            std::string DisplayName;
            std::wstring IconPath;
            std::string IconSprite;
            u16_t ItemId = 0u;
            u16_t Price = 0u;
            u16_t AuthoritativePrice = 0u;
            u32_t Order = 0u;
            bool_t Enabled = true;
            bool_t Purchasable = false;
            bool_t Registered = false;
        };
```

`Resource/Texture/UI/Items/**/*.png`를 recursive scan하여 상대 경로를 유일한 `AssetKey`로 만들고, `Data/LoL/ClientPublic/UI/ItemShopCatalog.json`의 `order`, `price`, `section`, `enabled` override를 합친다. 동일 숫자 prefix가 여러 이미지에 있어도 asset key는 중복되지 않으며, `CItemRegistry`에 있는 15개 ID는 ID별 첫 행만 구매 가능하다. 구매 가능 행의 가격은 JSON preview 값 대신 server-authoritative `ItemDef::price`로 강제한다.

`RegisterLoLShopItems`가 생성하는 각 DTO는 아래 필드를 모두 채운다.

```cpp
                Engine::UIShopItemAssetDesc Desc{};
                Desc.iItemId = Item.ItemId;
                Desc.iPrice = Item.Price;
                Desc.iOrder = Item.Order;
                Desc.pAssetKey = Item.AssetKey.c_str();
                Desc.pSection = Item.Section.c_str();
                Desc.pDisplayName = Item.DisplayName.c_str();
                Desc.pIconPath = Item.IconPath.c_str();
                Desc.pIconSprite = Item.IconSprite.c_str();
                Desc.pStatLines = LinePointers.empty() ? nullptr : LinePointers.data();
                Desc.iStatLineCount = static_cast<u32_t>(LinePointers.size());
                Desc.bEnabled = Item.Enabled;
                Desc.bPurchasable = Item.Purchasable;
```

`DrawLoLShopCatalogEditor`는 F8 창에서 검색, order, 비구매 행 preview price, enabled를 편집하고 `Save + Apply`, `Reload`를 제공한다. 각 행은 `ImGui::PushID(Item.AssetKey.c_str())`를 사용한다. 저장은 동일 JSON에 deterministic order로 기록하고, 적용 뒤 `UI_Register_InGameShopItems`와 `UI_Reload_Lua`를 호출한다. 등록 아이템은 authoritative price를 read-only로 표시하여 client/server 차감액 불일치를 만들지 않는다.

1-13. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameImGui.cpp

기존 include 아래에 추가:

```cpp
#include "GamePlay/LoLUIContentRegistry.h"
#include "UI/MinimapPanel.h"
```

기존 코드:

```cpp
        CGameInstance::Get()->UI_OnImGui_Tuner();
```

아래로 교체:

```cpp
        CGameInstance::Get()->UI_OnImGui_Tuner();
        UI::CMinimapPanel::DrawTunerImGui();
        Client::DrawLoLShopCatalogEditor(*CGameInstance::Get());
```

1-14. C:/Users/user/Desktop/Winters/Client/Public/UI/MinimapPanel.h

기존 코드:

```cpp
    class CMinimapPanel
    {
    public:
        static void RenderRuntime(const MinimapFrameState& State);
        static void ShutdownRuntime();
    };
```

아래로 교체:

```cpp
    class CMinimapPanel
    {
    public:
        static void PrewarmChampionPortraits();
        static void DrawTunerImGui();
        static void RenderRuntime(const MinimapFrameState& State);
        static void ShutdownRuntime();
    };
```

1-15. C:/Users/user/Desktop/Winters/Client/Private/UI/MinimapPanel.cpp

anonymous namespace에 아래 runtime layout을 추가:

```cpp
    struct MinimapRuntimeLayout
    {
        f32_t ViewportHeightRatio = 0.35f;
        f32_t RightPadding = 12.f;
        f32_t BottomPadding = 12.f;
        f32_t IconScale = 1.f;
        f32_t ChampionScale = 1.35f;
    };

    MinimapRuntimeLayout s_MinimapLayout{};
```

`ResolveMinimapRect`의 fixed 252px 계산을 아래로 교체:

```cpp
        const f32_t requestedSide = State.fScreenHeight *
            std::clamp(s_MinimapLayout.ViewportHeightRatio, 0.18f, 0.55f);
        fOutSide = (std::max)(96.f,
            (std::min)(requestedSide, State.fScreenHeight - 24.f));
        fOutX = State.fScreenWidth - s_MinimapLayout.RightPadding - fOutSide;
        fOutY = State.fScreenHeight - s_MinimapLayout.BottomPadding - fOutSide;
```

아이콘 반지름은 `fOutSide / 252.f * IconScale`을 적용하고 champion에 `ChampionScale`을 추가한다. minion, ward, jungle icon은 tessellated circle 대신 batched quad 두 장으로 렌더링하고 champion만 원형 portrait mask를 유지한다.

`RenderRuntime`의 단일 icon loop를 non-champion pass와 champion pass로 분리하여 원형 초상화가 minion/structure에 가려지지 않게 한다. `Minimap::ChampionPortraitCount`, `Minimap::PortraitLoadFailure` counter를 추가한다.

`CMinimapPanel::PrewarmChampionPortraits`는 `IRELIA`부터 `LEESIN`까지 roster portrait를 scene initialization 단계에서 생성한다. null texture는 map에 cache하지 않는다.

`CMinimapPanel::DrawTunerImGui` 전체 구현:

```cpp
    void CMinimapPanel::DrawTunerImGui()
    {
        if (!ImGui::Begin("Minimap Layout"))
        {
            ImGui::End();
            return;
        }

        ImGui::PushID("MinimapLayout");
        ImGui::SliderFloat("Viewport Height Ratio", &s_MinimapLayout.ViewportHeightRatio,
            0.18f, 0.55f, "%.2f");
        ImGui::SliderFloat("Right Padding", &s_MinimapLayout.RightPadding,
            0.f, 64.f, "%.0f px");
        ImGui::SliderFloat("Bottom Padding", &s_MinimapLayout.BottomPadding,
            0.f, 64.f, "%.0f px");
        ImGui::SliderFloat("Icon Scale", &s_MinimapLayout.IconScale,
            0.65f, 2.f, "%.2f");
        ImGui::SliderFloat("Champion Scale", &s_MinimapLayout.ChampionScale,
            1.f, 2.f, "%.2f");
        if (ImGui::Button("Reset Minimap Layout"))
            s_MinimapLayout = {};
        ImGui::PopID();
        ImGui::End();
    }
```

1-16. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameLifecycle.cpp

기존 코드:

```cpp
    Client::RegisterLoLUIContent(*CGameInstance::Get());
```

아래로 교체:

```cpp
    Client::RegisterLoLUIContent(*CGameInstance::Get());
    UI::CMinimapPanel::PrewarmChampionPortraits();
```

1-17. C:/Users/user/Desktop/Winters/Data/LoL/ClientPublic/UI/ItemShopCatalog.json

새 파일:

```json
{
  "schemaVersion": 1,
  "columns": 10,
  "items": []
}
```

빈 `items`는 resource scan 기본 순서를 뜻하며, F8 editor의 첫 Save 시 454개 deterministic override가 기록된다.

1-18. C:/Users/user/Desktop/Winters/Client/Bin/Resource/UI/Lua/itemshop_catalog.lua

파일 전체를 아래로 교체:

```lua
WintersItemShopCatalog = {
    columns = 10,
    sections = {
        { section = "all", name = "All Resource Items", y = 174 },
    },
    items = UI.GetShopItems(),
}
```

1-19. C:/Users/user/Desktop/Winters/Client/Bin/Resource/UI/Lua/ui_boot.lua

`Shop`의 `selectedId`를 `selectedKey`로 교체한다. item draw에서 `item.sprite`가 있으면 `UI.DrawSprite`, 없을 때만 `UI.DrawImage`를 사용한다. 선택 highlight는 `item.key`를 사용하고, 구매 요청만 server item ID를 사용한다. `x/y` authored 좌표는 사용하지 않고 `(order, columns)`로 계산한다.

1-20. C:/Users/user/Desktop/Winters/Tools/build_item_shop_atlas.py

새 파일:

```python
from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image


CELL_SIZE = 64
COLUMNS = 16
ATLAS_WIDTH = CELL_SIZE * COLUMNS
ATLAS_HEIGHT = 2048


def ParseArguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build the Winters item icon atlas and merge sprite metadata.")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    return parser.parse_args()


def CollectIcons(items_root: Path) -> list[Path]:
    return sorted(
        (path for path in items_root.rglob("*.png") if path.is_file()),
        key=lambda path: path.relative_to(items_root).as_posix().lower())


def BuildAtlas(root: Path) -> tuple[int, int]:
    resource_root = root / "Client" / "Bin" / "Resource"
    items_root = resource_root / "Texture" / "UI" / "Items"
    manifest_path = resource_root / "UI" / "itemshop_atlas_manifest.json"
    atlas_path = resource_root / "Texture" / "UI" / "item_icons_atlas.png"
    icons = CollectIcons(items_root)
    if len(icons) > COLUMNS * (ATLAS_HEIGHT // CELL_SIZE):
        raise RuntimeError(f"item atlas capacity exceeded: {len(icons)}")

    atlas = Image.new("RGBA", (ATLAS_WIDTH, ATLAS_HEIGHT), (0, 0, 0, 0))
    with manifest_path.open("r", encoding="utf-8") as source:
        manifest = json.load(source)

    textures = manifest.setdefault("textures", {})
    textures["items"] = {
        "path": "Resource/Texture/UI/item_icons_atlas.png",
        "width": ATLAS_WIDTH,
        "height": ATLAS_HEIGHT,
    }
    sprites = manifest.setdefault("sprites", {})
    for key in [key for key in sprites if key.startswith("item:")]:
        del sprites[key]

    for index, icon_path in enumerate(icons):
        relative = icon_path.relative_to(items_root).as_posix()
        x = (index % COLUMNS) * CELL_SIZE
        y = (index // COLUMNS) * CELL_SIZE
        with Image.open(icon_path) as source:
            icon = source.convert("RGBA")
            if icon.size != (CELL_SIZE, CELL_SIZE):
                icon = icon.resize((CELL_SIZE, CELL_SIZE), Image.Resampling.LANCZOS)
            atlas.alpha_composite(icon, (x, y))
        sprites[f"item:{relative}"] = {
            "texture": "items",
            "x": x,
            "y": y,
            "w": CELL_SIZE,
            "h": CELL_SIZE,
        }

    atlas.save(atlas_path, optimize=True)
    with manifest_path.open("w", encoding="utf-8", newline="\n") as output:
        json.dump(manifest, output, indent=4, ensure_ascii=False)
        output.write("\n")
    return len(icons), len({path.relative_to(items_root).as_posix() for path in icons})


def Main() -> int:
    args = ParseArguments()
    icon_count, unique_count = BuildAtlas(args.root.resolve())
    print(f"item atlas: icons={icon_count} uniqueAssetKeys={unique_count}")
    return 0 if icon_count == unique_count else 1


if __name__ == "__main__":
    raise SystemExit(Main())
```

1-21. C:/Users/user/Desktop/Winters/Engine/Include/GameInstance.h

기존 코드:

```cpp
    bool_t Profiler_SaveJson(const char* pPath);
```

아래로 교체:

```cpp
    bool_t Profiler_SaveJson(const char* pPath, bool_t bForceCapture = true);
```

1-22. C:/Users/user/Desktop/Winters/Engine/Private/GameInstance.cpp

기존 코드:

```cpp
bool_t CGameInstance::Profiler_SaveJson(const char* pPath)
{
	return m_pProfilerOverlay && m_pProfilerOverlay->CaptureToJson(pPath, true);
}
```

아래로 교체:

```cpp
bool_t CGameInstance::Profiler_SaveJson(const char* pPath, bool_t bForceCapture)
{
	return m_pProfilerOverlay && m_pProfilerOverlay->CaptureToJson(pPath, bForceCapture);
}
```

1-23. C:/Users/user/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp

기존 코드:

```cpp
            CGameInstance::Get()->Profiler_SaveJson(capturePath);
            CGameInstance::Get()->Profiler_SaveJson("profiler.json");
```

아래로 교체:

```cpp
            CGameInstance::Get()->Profiler_SaveJson(capturePath, true);
            CGameInstance::Get()->Profiler_SaveJson("profiler.json", false);
```

동일 F4 frame은 첫 호출에서 한 번만 capture하여 stable sample/EMA를 갱신하고, 두 번째 파일은 같은 cached display frame을 기록한다.

2. 검증

현재 기준 캡처:
- `C:/Users/user/Desktop/Winters/profiler.json`: JSON parse 성공, Frame 13.1878ms, Update 3.7601ms, Render 9.1741ms, GPU 8.704ms, Map::RHISceneSnapshot 4.8899ms/1080 meshes, Mesh::DrawCalls 462, limiter 60 FPS.
- 현재 수치는 300FPS의 3.33ms budget을 충족하지 않으며 limiter가 켜져 있어 최종 판정 자료가 아니다.

검증 명령:
- `python Tools/build_item_shop_atlas.py`
- item source 454, catalog 454, unique asset key 454, atlas sprite 454, missing path 0, duplicate asset key 0 검사.
- server item def 15, primary purchasable mapping 15, authoritative price mismatch 0 검사.
- `git diff --check`
- `msbuild Engine/Include/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
- `msbuild Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
- `msbuild Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`

후속 동기화:
- Engine public header 변경 후 Engine build의 SDK 배포 단계로 `EngineSDK/inc`를 동기화한다.

수동 확인:
- F8에서 minimap ratio/padding/icon/champion scale을 바꾸면 렌더와 클릭 영역이 함께 바뀌는지 확인.
- 시야에 들어온 적 champion portrait가 표시되고 minion/structure보다 항상 위에 그려지는지 확인.
- F8 Item Shop Catalog에서 order와 비등록 preview price 저장 후 Lua shop 재로드 시 즉시 반영되는지 확인.
- 등록 아이템 가격이 read-only authoritative 값이며 실제 서버 차감액과 같은지 확인.
- Profiler Details 상태에서 Save visible ID conflict가 재발하지 않고 `profiler.json`이 저장되는지 확인.
- 첫 웨이브 직전/직후 F4 capture에서 `MinionVisual::ColdCreate=0`, PoolHit=36, animation success log=0인지 확인.
- 정상 F5 capture에서 `Map::RHISceneSnapshot=0`, 맵·roster·minion·champion·UI·FX가 그대로 보이는지 확인.
- F11 또는 `--uncapped --no-vsync`로 limiter를 해제한 뒤 Frame/GPU worst frame을 다시 저장한다. Frame과 GPU가 모두 3.33ms 이하일 때만 300FPS 달성으로 판정한다.
