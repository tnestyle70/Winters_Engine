Session - DX11 HUD 미니맵을 Client view data와 Engine UI render로 분리해 기본 HUD에 고정한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Engine/Public/Manager/UI/MinimapTypes.h

새 파일:

```cpp
#pragma once

#include "GameContext.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <vector>

namespace Engine
{
    enum class eMinimapIconKind : u8_t
    {
        Structure,
        Jungle,
        Minion,
        Champion
    };

    enum class eMinimapStructureKind : u8_t
    {
        None,
        Turret,
        Inhibitor,
        Nexus
    };

    enum class eMinimapJungleKind : u8_t
    {
        Camp,
        Baron,
        Dragon,
        Blue,
        Red
    };

    struct MinimapIconView
    {
        eMinimapIconKind Kind = eMinimapIconKind::Champion;
        eMinimapStructureKind StructureKind = eMinimapStructureKind::None;
        eMinimapJungleKind JungleKind = eMinimapJungleKind::Camp;
        eChampion Champion = eChampion::NONE;
        u8_t Team = 0;
        Vec3 WorldPos{};
        bool_t bAlly = false;
        bool_t bDead = false;
    };

    struct MinimapFrameState
    {
        bool_t bValid = false;
        u8_t LocalTeam = 0;
        f32_t WorldSize = 1.f;
        void* pFogOverlaySRV = nullptr;
        std::vector<MinimapIconView> Icons;

        void Reset()
        {
            bValid = false;
            LocalTeam = 0;
            WorldSize = 1.f;
            pFogOverlaySRV = nullptr;
            Icons.clear();
        }
    };
}
```

1-2. C:/Users/tnest/Desktop/Winters/Engine/Public/Manager/UI/MinimapHudPanel.h

새 파일:

```cpp
#pragma once

#include "Manager/UI/MinimapTypes.h"
#include "WintersTypes.h"

struct ImDrawList;
struct ImVec2;

namespace Engine
{
    class CUIAtlasManifest;

    class CMinimapHudPanel final
    {
    public:
        bool_t Initialize(CUIAtlasManifest* pAtlasManifest);
        void Shutdown();

        bool_t LoadLayout(const wchar_t* pPath);
        void Draw(ImDrawList* pDraw, const MinimapFrameState& State,
            f32_t fScreenW, f32_t fScreenH);
        void DrawTuner();

    private:
        struct Layout
        {
            bool_t bShow = true;
            f32_t fSize = 236.f;
            f32_t fOffsetX = 16.f;
            f32_t fOffsetY = 12.f;
            f32_t fChampionSize = 22.f;
            f32_t fStructureSize = 14.f;
            f32_t fJungleSize = 12.f;
            f32_t fMinionSize = 4.f;
            f32_t fFogAlpha = 1.f;
        };

        void DrawIconsByKind(ImDrawList* pDraw, const MinimapFrameState& State,
            const ImVec2& Root, f32_t fSide, eMinimapIconKind Kind) const;
        void DrawChampionIcon(ImDrawList* pDraw, const ImVec2& Center,
            const MinimapIconView& Icon) const;
        void DrawStructureIcon(ImDrawList* pDraw, const ImVec2& Center,
            const MinimapIconView& Icon) const;
        void DrawJungleIcon(ImDrawList* pDraw, const ImVec2& Center,
            const MinimapIconView& Icon) const;
        void DrawMinionDot(ImDrawList* pDraw, const ImVec2& Center,
            const MinimapIconView& Icon) const;

        bool_t DrawSprite(ImDrawList* pDraw, const char* pSpriteID,
            const ImVec2& Min, const ImVec2& Max, u32_t Tint) const;
        bool_t WorldToMinimap(const Vec3& WorldPos, const ImVec2& Root,
            f32_t fSide, f32_t fWorldSize, ImVec2& Out) const;

        const char* ResolveChampionSprite(eChampion Champion) const;
        const char* ResolveStructureSprite(eMinimapStructureKind Kind, u8_t Team) const;
        const char* ResolveJungleSprite(eMinimapJungleKind Kind) const;

        CUIAtlasManifest* m_pAtlasManifest = nullptr;
        Layout m_Layout{};
    };
}
```

1-3. C:/Users/tnest/Desktop/Winters/Engine/Private/Manager/UI/MinimapHudPanel.cpp

새 파일:

```cpp
#include "Manager/UI/MinimapHudPanel.h"

#include "Manager/UI/UIAtlasManifest.h"
#include "WintersPaths.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Engine
{
    namespace
    {
        bool ReadTextFileW(const wchar_t* pPath, std::string& outText)
        {
            outText.clear();
            if (!pPath)
                return false;

            wchar_t resolvedPath[MAX_PATH] = {};
            const wchar_t* pReadPath = pPath;
            if (WintersResolveContentPath(pPath, resolvedPath, MAX_PATH))
                pReadPath = resolvedPath;

            FILE* pFile = nullptr;
            if (_wfopen_s(&pFile, pReadPath, L"rb") != 0 || !pFile)
                return false;

            fseek(pFile, 0, SEEK_END);
            const long fileSize = ftell(pFile);
            fseek(pFile, 0, SEEK_SET);
            if (fileSize < 0)
            {
                fclose(pFile);
                return false;
            }

            outText.resize(static_cast<size_t>(fileSize));
            if (fileSize > 0)
                fread(outText.data(), 1, static_cast<size_t>(fileSize), pFile);
            fclose(pFile);
            return true;
        }

        const char* FindJsonValue(const std::string& Text, const char* pKey)
        {
            if (!pKey)
                return nullptr;

            std::string needle = "\"";
            needle += pKey;
            needle += "\"";
            const size_t keyPos = Text.find(needle);
            if (keyPos == std::string::npos)
                return nullptr;

            const size_t colonPos = Text.find(':', keyPos + needle.size());
            if (colonPos == std::string::npos)
                return nullptr;

            const char* p = Text.c_str() + colonPos + 1;
            while (*p && std::isspace(static_cast<unsigned char>(*p)))
                ++p;
            return p;
        }

        bool ExtractFloat(const std::string& Text, const char* pKey, f32_t& Out)
        {
            const char* p = FindJsonValue(Text, pKey);
            if (!p)
                return false;

            char* pEnd = nullptr;
            const float value = std::strtof(p, &pEnd);
            if (pEnd == p)
                return false;

            Out = value;
            return true;
        }

        bool ExtractBool(const std::string& Text, const char* pKey, bool_t& Out)
        {
            const char* p = FindJsonValue(Text, pKey);
            if (!p)
                return false;

            if (std::strncmp(p, "true", 4) == 0)
            {
                Out = true;
                return true;
            }

            if (std::strncmp(p, "false", 5) == 0)
            {
                Out = false;
                return true;
            }

            return false;
        }

        u32_t ColorForTeam(u8_t Team, u8_t Alpha = 255)
        {
            if (Team == 0)
                return IM_COL32(70, 155, 255, Alpha);
            if (Team == 1)
                return IM_COL32(255, 70, 70, Alpha);
            return IM_COL32(230, 202, 115, Alpha);
        }

        ImVec2 HalfSizeMin(const ImVec2& Center, f32_t Size)
        {
            return ImVec2(Center.x - Size * 0.5f, Center.y - Size * 0.5f);
        }

        ImVec2 HalfSizeMax(const ImVec2& Center, f32_t Size)
        {
            return ImVec2(Center.x + Size * 0.5f, Center.y + Size * 0.5f);
        }
    }

    bool_t CMinimapHudPanel::Initialize(CUIAtlasManifest* pAtlasManifest)
    {
        m_pAtlasManifest = pAtlasManifest;
        return m_pAtlasManifest != nullptr;
    }

    void CMinimapHudPanel::Shutdown()
    {
        m_pAtlasManifest = nullptr;
        m_Layout = Layout{};
    }

    bool_t CMinimapHudPanel::LoadLayout(const wchar_t* pPath)
    {
        std::string text;
        if (!ReadTextFileW(pPath, text))
            return false;

        Layout next{};
        ExtractBool(text, "show", next.bShow);
        ExtractFloat(text, "size", next.fSize);
        ExtractFloat(text, "offsetX", next.fOffsetX);
        ExtractFloat(text, "offsetY", next.fOffsetY);
        ExtractFloat(text, "championSize", next.fChampionSize);
        ExtractFloat(text, "structureSize", next.fStructureSize);
        ExtractFloat(text, "jungleSize", next.fJungleSize);
        ExtractFloat(text, "minionSize", next.fMinionSize);
        ExtractFloat(text, "fogAlpha", next.fFogAlpha);

        m_Layout = next;
        return true;
    }

    void CMinimapHudPanel::Draw(ImDrawList* pDraw, const MinimapFrameState& State,
        f32_t fScreenW, f32_t fScreenH)
    {
        if (!m_Layout.bShow || !State.bValid || !pDraw ||
            fScreenW <= 0.f || fScreenH <= 0.f)
        {
            return;
        }

        const f32_t side = std::clamp(m_Layout.fSize, 96.f, std::min(fScreenW, fScreenH));
        const ImVec2 root(fScreenW - side - m_Layout.fOffsetX,
            fScreenH - side - m_Layout.fOffsetY);
        const ImVec2 max(root.x + side, root.y + side);

        pDraw->AddRectFilled(root, max, IM_COL32(4, 8, 10, 235));
        DrawSprite(pDraw, "minimap.base", root, max, IM_COL32(255, 255, 255, 255));

        if (State.pFogOverlaySRV)
        {
            const i32_t alpha =
                static_cast<i32_t>(std::clamp(m_Layout.fFogAlpha, 0.f, 1.f) * 255.f);
            pDraw->AddImage(
                reinterpret_cast<ImTextureID>(State.pFogOverlaySRV),
                root,
                max,
                ImVec2(0.f, 0.f),
                ImVec2(1.f, 1.f),
                IM_COL32(255, 255, 255, alpha));
        }

        DrawIconsByKind(pDraw, State, root, side, eMinimapIconKind::Structure);
        DrawIconsByKind(pDraw, State, root, side, eMinimapIconKind::Jungle);
        DrawIconsByKind(pDraw, State, root, side, eMinimapIconKind::Minion);
        DrawIconsByKind(pDraw, State, root, side, eMinimapIconKind::Champion);

        pDraw->AddRect(root, max, IM_COL32(0, 0, 0, 255), 0.f, 0, 2.f);
    }

    void CMinimapHudPanel::DrawTuner()
    {
        if (ImGui::TreeNode("Minimap"))
        {
            bool bShow = m_Layout.bShow != 0;
            if (ImGui::Checkbox("Show", &bShow))
                m_Layout.bShow = bShow;
            ImGui::SliderFloat("Size", &m_Layout.fSize, 160.f, 360.f, "%.0f");
            ImGui::SliderFloat("Offset X", &m_Layout.fOffsetX, 0.f, 180.f, "%.0f");
            ImGui::SliderFloat("Offset Y", &m_Layout.fOffsetY, 0.f, 180.f, "%.0f");
            ImGui::SliderFloat("Champion Icon", &m_Layout.fChampionSize, 12.f, 34.f, "%.0f");
            ImGui::SliderFloat("Structure Icon", &m_Layout.fStructureSize, 8.f, 28.f, "%.0f");
            ImGui::SliderFloat("Jungle Icon", &m_Layout.fJungleSize, 8.f, 28.f, "%.0f");
            ImGui::SliderFloat("Minion Dot", &m_Layout.fMinionSize, 2.f, 8.f, "%.1f");
            ImGui::SliderFloat("Fog Alpha", &m_Layout.fFogAlpha, 0.f, 1.f, "%.2f");
            ImGui::TreePop();
        }
    }

    void CMinimapHudPanel::DrawIconsByKind(ImDrawList* pDraw, const MinimapFrameState& State,
        const ImVec2& Root, f32_t fSide, eMinimapIconKind Kind) const
    {
        for (const MinimapIconView& icon : State.Icons)
        {
            if (icon.Kind != Kind)
                continue;

            ImVec2 center{};
            if (!WorldToMinimap(icon.WorldPos, Root, fSide, State.WorldSize, center))
                continue;

            switch (icon.Kind)
            {
            case eMinimapIconKind::Structure:
                DrawStructureIcon(pDraw, center, icon);
                break;
            case eMinimapIconKind::Jungle:
                DrawJungleIcon(pDraw, center, icon);
                break;
            case eMinimapIconKind::Minion:
                DrawMinionDot(pDraw, center, icon);
                break;
            case eMinimapIconKind::Champion:
                DrawChampionIcon(pDraw, center, icon);
                break;
            default:
                break;
            }
        }
    }

    void CMinimapHudPanel::DrawChampionIcon(ImDrawList* pDraw, const ImVec2& Center,
        const MinimapIconView& Icon) const
    {
        const f32_t size = m_Layout.fChampionSize;
        const ImVec2 min = HalfSizeMin(Center, size);
        const ImVec2 max = HalfSizeMax(Center, size);
        const u32_t tint = Icon.bDead
            ? IM_COL32(120, 120, 120, 220)
            : IM_COL32(255, 255, 255, 255);

        if (!DrawSprite(pDraw, ResolveChampionSprite(Icon.Champion), min, max, tint))
        {
            pDraw->AddCircleFilled(Center, size * 0.5f, ColorForTeam(Icon.Team, 240), 24);
            pDraw->AddCircle(Center, size * 0.5f, IM_COL32(0, 0, 0, 230), 24, 1.5f);
        }

        if (Icon.bDead)
            DrawSprite(pDraw, "minimap.champion.dead", min, max, IM_COL32(255, 255, 255, 220));
    }

    void CMinimapHudPanel::DrawStructureIcon(ImDrawList* pDraw, const ImVec2& Center,
        const MinimapIconView& Icon) const
    {
        const f32_t size = m_Layout.fStructureSize;
        const ImVec2 min = HalfSizeMin(Center, size);
        const ImVec2 max = HalfSizeMax(Center, size);

        if (!DrawSprite(pDraw, ResolveStructureSprite(Icon.StructureKind, Icon.Team),
            min, max, IM_COL32(255, 255, 255, 255)))
        {
            pDraw->AddRectFilled(min, max, ColorForTeam(Icon.Team, 235));
            pDraw->AddRect(min, max, IM_COL32(0, 0, 0, 230), 0.f, 0, 1.5f);
        }
    }

    void CMinimapHudPanel::DrawJungleIcon(ImDrawList* pDraw, const ImVec2& Center,
        const MinimapIconView& Icon) const
    {
        const f32_t size = m_Layout.fJungleSize;
        const ImVec2 min = HalfSizeMin(Center, size);
        const ImVec2 max = HalfSizeMax(Center, size);

        if (!DrawSprite(pDraw, ResolveJungleSprite(Icon.JungleKind),
            min, max, IM_COL32(255, 255, 255, 245)))
        {
            pDraw->AddCircleFilled(Center, size * 0.5f, ColorForTeam(2, 230), 16);
            pDraw->AddCircle(Center, size * 0.5f, IM_COL32(0, 0, 0, 210), 16, 1.f);
        }
    }

    void CMinimapHudPanel::DrawMinionDot(ImDrawList* pDraw, const ImVec2& Center,
        const MinimapIconView& Icon) const
    {
        const f32_t radius = m_Layout.fMinionSize * 0.5f;
        pDraw->AddCircleFilled(Center, radius, ColorForTeam(Icon.Team, 230), 8);
        pDraw->AddCircle(Center, radius, IM_COL32(0, 0, 0, 180), 8, 1.f);
    }

    bool_t CMinimapHudPanel::DrawSprite(ImDrawList* pDraw, const char* pSpriteID,
        const ImVec2& Min, const ImVec2& Max, u32_t Tint) const
    {
        if (!m_pAtlasManifest || !pSpriteID || !pDraw)
            return false;

        const UISpriteDef* pSprite = m_pAtlasManifest->FindSprite(pSpriteID);
        if (!pSprite)
            return false;

        const UIAtlasTextureDef* pTexture = m_pAtlasManifest->FindTexture(pSprite->strTextureID);
        if (!pTexture || !pTexture->pSRV)
            return false;

        const Vec4 uv = m_pAtlasManifest->ResolveUVRect(*pSprite);
        pDraw->AddImage(
            reinterpret_cast<ImTextureID>(pTexture->pSRV),
            Min,
            Max,
            ImVec2(uv.x, uv.y),
            ImVec2(uv.z, uv.w),
            Tint);
        return true;
    }

    bool_t CMinimapHudPanel::WorldToMinimap(const Vec3& WorldPos, const ImVec2& Root,
        f32_t fSide, f32_t fWorldSize, ImVec2& Out) const
    {
        if (fWorldSize <= 0.f)
            return false;

        const f32_t half = fWorldSize * 0.5f;
        const f32_t u = (WorldPos.x + half) / fWorldSize;
        const f32_t v = (WorldPos.z + half) / fWorldSize;
        if (u < 0.f || u > 1.f || v < 0.f || v > 1.f)
            return false;

        Out = ImVec2(Root.x + u * fSide, Root.y + v * fSide);
        return true;
    }

    const char* CMinimapHudPanel::ResolveChampionSprite(eChampion Champion) const
    {
        switch (Champion)
        {
        case eChampion::IRELIA: return "minimap.champion.irelia";
        case eChampion::YASUO: return "minimap.champion.yasuo";
        case eChampion::KALISTA: return "minimap.champion.kalista";
        case eChampion::SYLAS: return "minimap.champion.sylas";
        case eChampion::VIEGO: return "minimap.champion.viego";
        case eChampion::ANNIE: return "minimap.champion.annie";
        case eChampion::ASHE: return "minimap.champion.ashe";
        case eChampion::FIORA: return "minimap.champion.fiora";
        case eChampion::GAREN: return "minimap.champion.garen";
        case eChampion::RIVEN: return "minimap.champion.riven";
        case eChampion::ZED: return "minimap.champion.zed";
        case eChampion::EZREAL: return "minimap.champion.ezreal";
        case eChampion::YONE: return "minimap.champion.yone";
        case eChampion::JAX: return "minimap.champion.jax";
        case eChampion::MASTERYI: return "minimap.champion.masteryi";
        case eChampion::KINDRED: return "minimap.champion.kindred";
        case eChampion::LEESIN: return "minimap.champion.leesin";
        default: return nullptr;
        }
    }

    const char* CMinimapHudPanel::ResolveStructureSprite(eMinimapStructureKind Kind, u8_t Team) const
    {
        const bool_t bBlue = Team == 0;
        const bool_t bRed = Team == 1;

        switch (Kind)
        {
        case eMinimapStructureKind::Turret:
            return bBlue ? "minimap.structure.tower.blue" :
                (bRed ? "minimap.structure.tower.red" : "minimap.structure.tower.neutral");
        case eMinimapStructureKind::Inhibitor:
            return bBlue ? "minimap.structure.inhibitor.blue" :
                (bRed ? "minimap.structure.inhibitor.red" : "minimap.structure.inhibitor.neutral");
        case eMinimapStructureKind::Nexus:
            return bBlue ? "minimap.structure.nexus.blue" :
                (bRed ? "minimap.structure.nexus.red" : "minimap.structure.nexus.neutral");
        default:
            return nullptr;
        }
    }

    const char* CMinimapHudPanel::ResolveJungleSprite(eMinimapJungleKind Kind) const
    {
        switch (Kind)
        {
        case eMinimapJungleKind::Baron: return "minimap.jungle.baron";
        case eMinimapJungleKind::Dragon: return "minimap.jungle.dragon";
        case eMinimapJungleKind::Blue: return "minimap.jungle.blue";
        case eMinimapJungleKind::Red: return "minimap.jungle.red";
        case eMinimapJungleKind::Camp:
        default:
            return "minimap.jungle.camp";
        }
    }
}
```

1-4. C:/Users/tnest/Desktop/Winters/Client/Public/UI/InGameMinimapBridge.h

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"

class CWorld;

namespace Engine
{
    struct MinimapFrameState;
}

namespace UI
{
    void BuildInGameMinimapFrame(
        CWorld& World,
        u8_t LocalTeam,
        bool_t bRevealAllForPlayback,
        void* pFogOverlaySRV,
        Engine::MinimapFrameState& OutState);
}
```

1-5. C:/Users/tnest/Desktop/Winters/Client/Private/UI/InGameMinimapBridge.cpp

새 파일:

```cpp
#include "UI/InGameMinimapBridge.h"

#include "ECS/World.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "ECS/Systems/VisionSystem.h"
#include "Manager/UI/MinimapTypes.h"
#include "Shared/GameSim/Components/ReplicatedStateComponent.h"
#include "Shared/GameSim/Definitions/SnapshotStateFlags.h"

namespace UI
{
    namespace
    {
        bool_t IsDead(CWorld& World, EntityID Entity)
        {
            if (World.HasComponent<HealthComponent>(Entity))
            {
                const HealthComponent& health = World.GetComponent<HealthComponent>(Entity);
                if (health.bIsDead || health.fCurrent <= 0.f)
                    return true;
            }

            if (World.HasComponent<ReplicatedStateComponent>(Entity))
            {
                const ReplicatedStateComponent& state =
                    World.GetComponent<ReplicatedStateComponent>(Entity);
                return (state.stateFlags & kSnapshotStateDeadFlag) != 0u;
            }

            return false;
        }

        bool_t IsInvisible(CWorld& World, EntityID Entity)
        {
            if (!World.HasComponent<ReplicatedStateComponent>(Entity))
                return false;

            const ReplicatedStateComponent& state =
                World.GetComponent<ReplicatedStateComponent>(Entity);
            return (state.stateFlags & kSnapshotStateInvisibleFlag) != 0u;
        }

        bool_t IsVisibleForMinimap(
            CWorld& World,
            EntityID Entity,
            u8_t Team,
            u8_t LocalTeam,
            bool_t bRevealAllForPlayback)
        {
            if (Entity == NULL_ENTITY)
                return false;

            if (IsInvisible(World, Entity))
                return false;

            if (Team == LocalTeam)
                return true;

            if (bRevealAllForPlayback)
                return true;

            if (LocalTeam >= 8)
                return false;

            if (!World.HasComponent<VisibilityComponent>(Entity))
                return false;

            const VisibilityComponent& visibility = World.GetComponent<VisibilityComponent>(Entity);
            return (visibility.teamVisibilityMask & static_cast<u8_t>(1u << LocalTeam)) != 0;
        }

        Engine::eMinimapStructureKind ResolveStructureKind(CWorld& World, EntityID Entity)
        {
            if (World.HasComponent<NexusTag>(Entity))
                return Engine::eMinimapStructureKind::Nexus;
            if (World.HasComponent<InhibitorTag>(Entity))
                return Engine::eMinimapStructureKind::Inhibitor;
            if (World.HasComponent<TurretComponent>(Entity))
                return Engine::eMinimapStructureKind::Turret;
            return Engine::eMinimapStructureKind::Turret;
        }

        Engine::eMinimapJungleKind ResolveJungleKind(const JungleComponent& Jungle)
        {
            switch (Jungle.subKind)
            {
            case 0: return Engine::eMinimapJungleKind::Baron;
            case 1: return Engine::eMinimapJungleKind::Dragon;
            case 2: return Engine::eMinimapJungleKind::Blue;
            case 3: return Engine::eMinimapJungleKind::Red;
            default: return Engine::eMinimapJungleKind::Camp;
            }
        }
    }

    void BuildInGameMinimapFrame(
        CWorld& World,
        u8_t LocalTeam,
        bool_t bRevealAllForPlayback,
        void* pFogOverlaySRV,
        Engine::MinimapFrameState& OutState)
    {
        OutState.Reset();
        OutState.bValid = true;
        OutState.LocalTeam = LocalTeam;
        OutState.WorldSize = Engine::CVisionSystem::FOW_TEX_WORLD_SIZE;
        OutState.pFogOverlaySRV = pFogOverlaySRV;
        OutState.Icons.reserve(128u);

        World.ForEach<StructureComponent, TransformComponent>(
            [&](EntityID Entity, StructureComponent& Structure, TransformComponent& Transform)
            {
                const u8_t team = static_cast<u8_t>(Structure.team);
                if (Structure.hp <= 0.f || IsDead(World, Entity))
                    return;
                if (!IsVisibleForMinimap(World, Entity, team, LocalTeam, bRevealAllForPlayback))
                    return;

                Engine::MinimapIconView icon{};
                icon.Kind = Engine::eMinimapIconKind::Structure;
                icon.StructureKind = ResolveStructureKind(World, Entity);
                icon.Team = team;
                icon.WorldPos = Transform.GetPosition();
                icon.bAlly = team == LocalTeam;
                icon.bDead = false;
                OutState.Icons.push_back(icon);
            });

        World.ForEach<JungleComponent, TransformComponent>(
            [&](EntityID Entity, JungleComponent& Jungle, TransformComponent& Transform)
            {
                const u8_t team = static_cast<u8_t>(eTeam::Neutral);
                if (Jungle.hp <= 0.f || IsDead(World, Entity))
                    return;
                if (!IsVisibleForMinimap(World, Entity, team, LocalTeam, bRevealAllForPlayback))
                    return;

                Engine::MinimapIconView icon{};
                icon.Kind = Engine::eMinimapIconKind::Jungle;
                icon.JungleKind = ResolveJungleKind(Jungle);
                icon.Team = team;
                icon.WorldPos = Transform.GetPosition();
                icon.bAlly = false;
                icon.bDead = false;
                OutState.Icons.push_back(icon);
            });

        World.ForEach<MinionStateComponent, TransformComponent>(
            [&](EntityID Entity, MinionStateComponent& MinionState, TransformComponent& Transform)
            {
                const u8_t team = static_cast<u8_t>(MinionState.team);
                const bool_t bDead =
                    MinionState.current == MinionStateComponent::Dead ||
                    IsDead(World, Entity) ||
                    (World.HasComponent<MinionComponent>(Entity) &&
                        World.GetComponent<MinionComponent>(Entity).hp <= 0.f);
                if (bDead)
                    return;
                if (!IsVisibleForMinimap(World, Entity, team, LocalTeam, bRevealAllForPlayback))
                    return;

                Engine::MinimapIconView icon{};
                icon.Kind = Engine::eMinimapIconKind::Minion;
                icon.Team = team;
                icon.WorldPos = Transform.GetPosition();
                icon.bAlly = team == LocalTeam;
                icon.bDead = false;
                OutState.Icons.push_back(icon);
            });

        World.ForEach<ChampionComponent, TransformComponent>(
            [&](EntityID Entity, ChampionComponent& Champion, TransformComponent& Transform)
            {
                const u8_t team = static_cast<u8_t>(Champion.team);
                const bool_t bDead = Champion.hp <= 0.f || IsDead(World, Entity);
                if (!IsVisibleForMinimap(World, Entity, team, LocalTeam, bRevealAllForPlayback))
                    return;

                Engine::MinimapIconView icon{};
                icon.Kind = Engine::eMinimapIconKind::Champion;
                icon.Champion = Champion.id;
                icon.Team = team;
                icon.WorldPos = Transform.GetPosition();
                icon.bAlly = team == LocalTeam;
                icon.bDead = bDead;
                OutState.Icons.push_back(icon);
            });
    }
}
```

1-6. C:/Users/tnest/Desktop/Winters/Client/Bin/Resource/UI/minimap_atlas_manifest.json

새 파일:

```json
{
  "textures": {
    "base": { "path": "Resource/Texture/UI/InGameUI/minimap_base_clean.png", "width": 1254, "height": 1254 },
    "tower.neutral": { "path": "Resource/Texture/UI/InGameUI/minimap_tower_neutral_32.png", "width": 32, "height": 32 },
    "tower.blue": { "path": "Resource/Texture/UI/InGameUI/minimap_tower_blue_32.png", "width": 32, "height": 32 },
    "tower.red": { "path": "Resource/Texture/UI/InGameUI/minimap_tower_red_32.png", "width": 32, "height": 32 },
    "inhibitor.neutral": { "path": "Resource/Texture/UI/InGameUI/minimap_inhibitor_neutral_32.png", "width": 32, "height": 32 },
    "inhibitor.blue": { "path": "Resource/Texture/UI/InGameUI/minimap_inhibitor_blue_32.png", "width": 32, "height": 32 },
    "inhibitor.red": { "path": "Resource/Texture/UI/InGameUI/minimap_inhibitor_red_32.png", "width": 32, "height": 32 },
    "nexus.neutral": { "path": "Resource/Texture/UI/ux/minimap/icons/icon_ui_nexus_minimap_v2.png", "width": 32, "height": 32 },
    "nexus.blue": { "path": "Resource/Texture/UI/ux/minimap/icons/icon_ui_nexus_minimap_v2.png", "width": 32, "height": 32 },
    "nexus.red": { "path": "Resource/Texture/UI/ux/minimap/icons/icon_ui_nexus_minimap_v2.png", "width": 32, "height": 32 },
    "jungle.camp": { "path": "Resource/Texture/UI/ux/minimap/icons/camp.png", "width": 32, "height": 32 },
    "jungle.baron": { "path": "Resource/Texture/UI/ux/minimap/icons/baron.png", "width": 64, "height": 64 },
    "jungle.dragon": { "path": "Resource/Texture/UI/ux/minimap/icons/dragon.png", "width": 64, "height": 64 },
    "jungle.blue": { "path": "Resource/Texture/UI/ux/minimap/icons/blue.png", "width": 64, "height": 64 },
    "jungle.red": { "path": "Resource/Texture/UI/ux/minimap/icons/red.png", "width": 64, "height": 64 },
    "champion.dead": { "path": "Resource/Texture/UI/ux/minimap/icons/champion_dead.png", "width": 32, "height": 32 },
    "champion.annie": { "path": "Resource/Texture/UI/InGameUI/ChampionCircle32/champion_annie_circle_32.png", "width": 32, "height": 32 },
    "champion.ashe": { "path": "Resource/Texture/UI/InGameUI/ChampionCircle32/champion_ashe_circle_32.png", "width": 32, "height": 32 },
    "champion.ezreal": { "path": "Resource/Texture/UI/InGameUI/ChampionCircle32/champion_ezreal_circle_32.png", "width": 32, "height": 32 },
    "champion.fiora": { "path": "Resource/Texture/UI/InGameUI/ChampionCircle32/champion_fiora_circle_32.png", "width": 32, "height": 32 },
    "champion.garen": { "path": "Resource/Texture/UI/InGameUI/ChampionCircle32/champion_garen_circle_32.png", "width": 32, "height": 32 },
    "champion.irelia": { "path": "Resource/Texture/UI/InGameUI/ChampionCircle32/champion_irelia_circle_32.png", "width": 32, "height": 32 },
    "champion.jax": { "path": "Resource/Texture/UI/InGameUI/ChampionCircle32/champion_jax_circle_32.png", "width": 32, "height": 32 },
    "champion.kalista": { "path": "Resource/Texture/UI/InGameUI/ChampionCircle32/champion_kalista_circle_32.png", "width": 32, "height": 32 },
    "champion.kindred": { "path": "Resource/Texture/UI/InGameUI/ChampionCircle32/champion_kindred_circle_32.png", "width": 32, "height": 32 },
    "champion.leesin": { "path": "Resource/Texture/UI/InGameUI/ChampionCircle32/champion_leesin_circle_32.png", "width": 32, "height": 32 },
    "champion.masteryi": { "path": "Resource/Texture/UI/InGameUI/ChampionCircle32/champion_masteryi_circle_32.png", "width": 32, "height": 32 },
    "champion.riven": { "path": "Resource/Texture/UI/InGameUI/ChampionCircle32/champion_riven_circle_32.png", "width": 32, "height": 32 },
    "champion.sylas": { "path": "Resource/Texture/UI/InGameUI/ChampionCircle32/champion_sylas_circle_32.png", "width": 32, "height": 32 },
    "champion.viego": { "path": "Resource/Texture/UI/InGameUI/ChampionCircle32/champion_viego_circle_32.png", "width": 32, "height": 32 },
    "champion.yasuo": { "path": "Resource/Texture/UI/InGameUI/ChampionCircle32/champion_yasuo_circle_32.png", "width": 32, "height": 32 },
    "champion.yone": { "path": "Resource/Texture/UI/InGameUI/ChampionCircle32/champion_yone_circle_32.png", "width": 32, "height": 32 },
    "champion.zed": { "path": "Resource/Texture/UI/InGameUI/ChampionCircle32/champion_zed_circle_32.png", "width": 32, "height": 32 }
  },
  "sprites": {
    "minimap.base": { "texture": "base", "x": 0, "y": 0, "w": 1254, "h": 1254 },
    "minimap.structure.tower.neutral": { "texture": "tower.neutral", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.structure.tower.blue": { "texture": "tower.blue", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.structure.tower.red": { "texture": "tower.red", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.structure.inhibitor.neutral": { "texture": "inhibitor.neutral", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.structure.inhibitor.blue": { "texture": "inhibitor.blue", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.structure.inhibitor.red": { "texture": "inhibitor.red", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.structure.nexus.neutral": { "texture": "nexus.neutral", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.structure.nexus.blue": { "texture": "nexus.blue", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.structure.nexus.red": { "texture": "nexus.red", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.jungle.camp": { "texture": "jungle.camp", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.jungle.baron": { "texture": "jungle.baron", "x": 0, "y": 0, "w": 64, "h": 64 },
    "minimap.jungle.dragon": { "texture": "jungle.dragon", "x": 0, "y": 0, "w": 64, "h": 64 },
    "minimap.jungle.blue": { "texture": "jungle.blue", "x": 0, "y": 0, "w": 64, "h": 64 },
    "minimap.jungle.red": { "texture": "jungle.red", "x": 0, "y": 0, "w": 64, "h": 64 },
    "minimap.champion.dead": { "texture": "champion.dead", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.champion.annie": { "texture": "champion.annie", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.champion.ashe": { "texture": "champion.ashe", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.champion.ezreal": { "texture": "champion.ezreal", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.champion.fiora": { "texture": "champion.fiora", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.champion.garen": { "texture": "champion.garen", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.champion.irelia": { "texture": "champion.irelia", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.champion.jax": { "texture": "champion.jax", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.champion.kalista": { "texture": "champion.kalista", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.champion.kindred": { "texture": "champion.kindred", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.champion.leesin": { "texture": "champion.leesin", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.champion.masteryi": { "texture": "champion.masteryi", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.champion.riven": { "texture": "champion.riven", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.champion.sylas": { "texture": "champion.sylas", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.champion.viego": { "texture": "champion.viego", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.champion.yasuo": { "texture": "champion.yasuo", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.champion.yone": { "texture": "champion.yone", "x": 0, "y": 0, "w": 32, "h": 32 },
    "minimap.champion.zed": { "texture": "champion.zed", "x": 0, "y": 0, "w": 32, "h": 32 }
  }
}
```

1-7. C:/Users/tnest/Desktop/Winters/Client/Bin/Resource/UI/minimap_layout.json

새 파일:

```json
{
  "show": true,
  "size": 236,
  "offsetX": 16,
  "offsetY": 12,
  "championSize": 22,
  "structureSize": 14,
  "jungleSize": 12,
  "minionSize": 4,
  "fogAlpha": 1.0
}
```

1-8. C:/Users/tnest/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h

기존 코드:

```cpp
#include "Manager/UI/UIAtlasManifest.h"
#include "Renderer/UIRenderer.h"
```

아래에 추가:

```cpp
#include "Manager/UI/MinimapHudPanel.h"
#include "Manager/UI/MinimapTypes.h"
```

기존 코드:

```cpp
void SetGameContextServerTimeMs(u64_t iServerTimeMs);
```

아래에 추가:

```cpp
void Set_MinimapFrameState(const MinimapFrameState& State);
```

기존 코드:

```cpp
void LoadInGameShopAssets();
void DrawInGameShop(ImDrawList* pDraw);
void DrawStatusPanel(ImDrawList* pDraw);
```

아래에 추가:

```cpp
void LoadMinimapAssets();
void ReleaseMinimapAssets();
```

기존 코드:

```cpp
std::vector<KillFeedPortraitCache> m_KillFeedPortraits;
GameContextHUDState m_GameContextHUD{};
```

아래로 교체:

```cpp
std::vector<KillFeedPortraitCache> m_KillFeedPortraits;
CUIAtlasManifest m_MinimapAtlasManifest;
CMinimapHudPanel m_MinimapHudPanel;
MinimapFrameState m_MinimapFrameState;
GameContextHUDState m_GameContextHUD{};
```

1-9. C:/Users/tnest/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

기존 코드:

```cpp
constexpr const wchar_t* kPathStatusPanel = L"Resource/Texture/UI/StatusPannel_final.png";
constexpr const wchar_t* kPathInGameShopManifest = L"Resource/UI/itemshop_atlas_manifest.json";
constexpr const wchar_t* kPathInGameShopManifestFallback = L"Client/Bin/Resource/UI/itemshop_atlas_manifest.json";
```

아래에 추가:

```cpp
constexpr const wchar_t* kPathMinimapManifest = L"Resource/UI/minimap_atlas_manifest.json";
constexpr const wchar_t* kPathMinimapManifestFallback = L"Client/Bin/Resource/UI/minimap_atlas_manifest.json";
constexpr const wchar_t* kPathMinimapLayout = L"Resource/UI/minimap_layout.json";
constexpr const wchar_t* kPathMinimapLayoutFallback = L"Client/Bin/Resource/UI/minimap_layout.json";
```

`Initialize()`의 기존 코드:

```cpp
LoadChampionHUDAssets();
LoadInGameShopAssets();
m_pLuaUIHost = std::make_unique<CLuaUIHost>();
```

아래로 교체:

```cpp
LoadMinimapAssets();
LoadChampionHUDAssets();
LoadInGameShopAssets();
m_pLuaUIHost = std::make_unique<CLuaUIHost>();
```

`Shutdown()`의 기존 코드:

```cpp
ReleaseChampionHUDAssets();
m_pRHIUIRenderer.reset();
```

아래로 교체:

```cpp
ReleaseMinimapAssets();
ReleaseChampionHUDAssets();
m_pRHIUIRenderer.reset();
```

`End_RawImagePass()` 아래에 추가:

```cpp
void CUI_Manager::LoadMinimapAssets()
{
    m_MinimapHudPanel.Initialize(&m_MinimapAtlasManifest);

    bool_t bManifestLoaded = m_MinimapAtlasManifest.LoadFromJson(kPathMinimapManifest);
    if (!bManifestLoaded)
        bManifestLoaded = m_MinimapAtlasManifest.LoadFromJson(kPathMinimapManifestFallback);

    if (bManifestLoaded)
    {
        m_MinimapAtlasManifest.ForEachTexture(
            [this](const std::string&, UIAtlasTextureDef& Texture)
            {
                if (Texture.strPath.empty())
                    return;

                void* pSRV = nullptr;
                if (SUCCEEDED(Load_TextureSRV(Texture.strPath.c_str(), &pSRV)))
                    Texture.pSRV = pSRV;
            });
    }
    else
    {
        OutputDebugStringA("[UI_Manager] minimap_atlas_manifest.json load failed - minimap sprites use fallback shapes\n");
    }

    if (!m_MinimapHudPanel.LoadLayout(kPathMinimapLayout) &&
        !m_MinimapHudPanel.LoadLayout(kPathMinimapLayoutFallback))
    {
        OutputDebugStringA("[UI_Manager] minimap_layout.json load failed - minimap default layout used\n");
    }
}

void CUI_Manager::ReleaseMinimapAssets()
{
    m_MinimapAtlasManifest.ForEachTexture(
        [](const std::string&, UIAtlasTextureDef& Texture)
        {
            ReleaseSRV(Texture.pSRV);
        });
    m_MinimapAtlasManifest.Clear();
    m_MinimapHudPanel.Shutdown();
    m_MinimapFrameState.Reset();
}

void CUI_Manager::Set_MinimapFrameState(const MinimapFrameState& State)
{
    m_MinimapFrameState = State;
}
```

`Render_Overlay()`의 기존 코드:

```cpp
{
    WINTERS_PROFILE_SCOPE("UI::StatusPanel");
    DrawStatusPanel(pFG);
}
```

아래로 교체:

```cpp
{
    WINTERS_PROFILE_SCOPE("UI::Minimap");
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    const f32_t screenW = (m_iWinSizeX > 0) ? static_cast<f32_t>(m_iWinSizeX) : displaySize.x;
    const f32_t screenH = (m_iWinSizeY > 0) ? static_cast<f32_t>(m_iWinSizeY) : displaySize.y;
    m_MinimapHudPanel.Draw(pFG, m_MinimapFrameState, screenW, screenH);
}
{
    WINTERS_PROFILE_SCOPE("UI::StatusPanel");
    DrawStatusPanel(pFG);
}
```

`OnImGui_Tuner()`의 기존 코드:

```cpp
ImGui::Text("Status Panel: %s", m_pSRV_StatusPanel ? "loaded" : "FALLBACK");
ImGui::SliderFloat("Status Panel Width", &m_fStatusPanelDrawWidth, 320.f, 1491.f, "%.0f");
ImGui::SliderFloat("Status Panel Height", &m_fStatusPanelDrawHeight, 220.f, 600.f, "%.0f");
ImGui::SliderFloat("Status Panel Y Offset", &m_fStatusPanelOffsetY, -240.f, 240.f, "%.0f");
```

아래에 추가:

```cpp
m_MinimapHudPanel.DrawTuner();
```

1-10. C:/Users/tnest/Desktop/Winters/Engine/Include/GameInstance.h

`NS_BEGIN(Engine)` 아래의 기존 코드:

```cpp
class CScene_Manager;
```

아래에 추가:

```cpp
struct MinimapFrameState;
```

기존 코드:

```cpp
void UI_SetGameContextServerTimeMs(u64_t iServerTimeMs);
```

아래에 추가:

```cpp
void UI_Set_MinimapFrameState(const MinimapFrameState& State);
```

1-11. C:/Users/tnest/Desktop/Winters/Engine/Private/GameInstance.cpp

기존 코드:

```cpp
void CGameInstance::UI_SetGameContextServerTimeMs(u64_t iServerTimeMs)
{
	if (m_pUI_Manager)
		m_pUI_Manager->SetGameContextServerTimeMs(iServerTimeMs);
}
```

아래에 추가:

```cpp
void CGameInstance::UI_Set_MinimapFrameState(const MinimapFrameState& State)
{
	if (m_pUI_Manager)
		m_pUI_Manager->Set_MinimapFrameState(State);
}
```

1-12. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/InGameRenderBridge.cpp

기존 코드:

```cpp
#include "UI/DebugDrawSystem.h"
```

아래에 추가:

```cpp
#include "UI/InGameMinimapBridge.h"
```

기존 코드:

```cpp
{
    WINTERS_PROFILE_SCOPE("UIOverlay::Render");
    CGameInstance::Get()->UI_Render_Overlay(vp);
}
```

아래로 교체:

```cpp
{
    WINTERS_PROFILE_SCOPE("UIOverlay::BuildMinimap");
    Engine::MinimapFrameState minimapState{};
    void* pMinimapFogSRV = nullptr;
    if (!bRevealAllForPlayback && scene.m_pFogOfWarRenderer)
        pMinimapFogSRV = scene.m_pFogOfWarRenderer->GetMinimapOverlaySRV();

    UI::BuildInGameMinimapFrame(
        scene.m_World,
        localTeam,
        bRevealAllForPlayback,
        pMinimapFogSRV,
        minimapState);
    CGameInstance::Get()->UI_Set_MinimapFrameState(minimapState);
}
{
    WINTERS_PROFILE_SCOPE("UIOverlay::Render");
    CGameInstance::Get()->UI_Render_Overlay(vp);
}
```

1-13. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/InGameDebugBridge.cpp

삭제할 코드:

```cpp
#include "UI/MinimapPanel.h"
```

삭제할 코드:

```cpp
UI::CMinimapPanel::Render(
    desc.pFogOfWarRenderer,
    desc.world,
    static_cast<u8_t>(desc.playerTeam));
```

1-14. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

기존 코드:

```cpp
#include "ECS/Components/TransformComponent.h"
```

아래에 추가:

```cpp
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/VisionComponents.h"
```

`MarkServerId()`의 기존 코드 아래에 추가:

```cpp
void EnsureSnapshotSpatialVisibilityTags(
    CWorld& world,
    EntityID entity,
    eSpatialKind kind,
    u8_t team,
    f32_t radius)
{
    if (entity == NULL_ENTITY)
        return;

    if (!world.HasComponent<VisibilityComponent>(entity))
        world.AddComponent<VisibilityComponent>(entity);

    SpatialAgentComponent spatial{};
    spatial.kind = kind;
    spatial.team = team;
    spatial.radius = radius;
    if (world.HasComponent<SpatialAgentComponent>(entity))
        world.GetComponent<SpatialAgentComponent>(entity) = spatial;
    else
        world.AddComponent<SpatialAgentComponent>(entity, spatial);
}
```

`EnsureSnapshotStructureRuntimeTags()`의 기존 코드:

```cpp
if (!world.HasComponent<TargetableTag>(entity))
    world.AddComponent<TargetableTag>(entity);
```

아래에 추가:

```cpp
eSpatialKind spatialKind = eSpatialKind::None;
if (kind == Shared::Schema::EntityKind::Turret)
    spatialKind = eSpatialKind::Turret;
else if (kind == Shared::Schema::EntityKind::Inhibitor)
    spatialKind = eSpatialKind::Inhibitor;
else if (kind == Shared::Schema::EntityKind::Nexus)
    spatialKind = eSpatialKind::Nexus;

if (spatialKind != eSpatialKind::None)
    EnsureSnapshotSpatialVisibilityTags(world, entity, spatialKind, team, 1.5f);
```

`EnsureSnapshotJungleRuntimeTags()`의 기존 코드:

```cpp
if (!world.HasComponent<TargetableTag>(entity))
    world.AddComponent<TargetableTag>(entity);
```

아래에 추가:

```cpp
EnsureSnapshotSpatialVisibilityTags(
    world,
    entity,
    eSpatialKind::JungleMob,
    static_cast<u8_t>(eTeam::Neutral),
    1.2f);
```

`EnsureEntity()`의 `kind == Shared::Schema::EntityKind::Minion` 처리 안에서 `MinionComponent` 보장 코드 아래에 추가:

기존 코드:

```cpp
        if (!world.HasComponent<MinionComponent>(e))
        {
            MinionComponent minion{};
            minion.team = static_cast<eTeam>(team);
            world.AddComponent<MinionComponent>(e, minion);
        }
```

아래에 추가:

```cpp
        EnsureSnapshotSpatialVisibilityTags(world, e, eSpatialKind::Minion, team, 0.5f);
```

snapshot per-frame `kind == Shared::Schema::EntityKind::Minion` 갱신 처리에서 `MinionComponent` 갱신 블록 아래에 추가:

기존 코드:

```cpp
            if (world.HasComponent<MinionComponent>(e))
            {
                auto& minion = world.GetComponent<MinionComponent>(e);
                minion.team = static_cast<eTeam>(es->team());
                minion.roleType = static_cast<u8_t>(es->subtype());
                minion.hp = es->hp();
                if (es->maxHp() > 0.f)
                    minion.maxHp = es->maxHp();
            }
```

아래에 추가:

```cpp
            EnsureSnapshotSpatialVisibilityTags(world, e, eSpatialKind::Minion, es->team(), 0.5f);
```

1-15. C:/Users/tnest/Desktop/Winters/Client/Private/Manager/Jungle_Manager.cpp

기존 코드:

```cpp
#include "ECS/Components/RenderComponent.h"
```

아래에 추가:

```cpp
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/VisionComponents.h"
```

기존 코드:

```cpp
m_pWorld->AddComponent<JungleMonsterTag>(id);
m_pWorld->AddComponent<TargetableTag>(id);
```

아래에 추가:

```cpp
VisibilityComponent visibility{};
m_pWorld->AddComponent<VisibilityComponent>(id, visibility);

SpatialAgentComponent spatial{};
spatial.kind = eSpatialKind::JungleMob;
spatial.team = static_cast<u8_t>(eTeam::Neutral);
spatial.radius = Resolve_ColliderRadius(static_cast<eJungleSub>(entry.subKind));
m_pWorld->AddComponent<SpatialAgentComponent>(id, spatial);
```

1-16. C:/Users/tnest/Desktop/Winters/Engine/Include/Engine.vcxproj

기존 코드:

```xml
    <ClCompile Include="..\Private\Manager\UI\LuaUIHost.cpp" />
    <ClCompile Include="..\Private\Manager\UI\UIAtlasManifest.cpp" />
    <ClCompile Include="..\Private\Manager\UI\UI_Manager.cpp" />
```

아래로 교체:

```xml
    <ClCompile Include="..\Private\Manager\UI\LuaUIHost.cpp" />
    <ClCompile Include="..\Private\Manager\UI\MinimapHudPanel.cpp" />
    <ClCompile Include="..\Private\Manager\UI\UIAtlasManifest.cpp" />
    <ClCompile Include="..\Private\Manager\UI\UI_Manager.cpp" />
```

기존 코드:

```xml
    <ClInclude Include="..\Public\Manager\UI\LuaUIHost.h" />
    <ClInclude Include="..\Public\Manager\UI\UIAtlasManifest.h" />
    <ClInclude Include="..\Public\Manager\UI\UI_Manager.h" />
```

아래로 교체:

```xml
    <ClInclude Include="..\Public\Manager\UI\LuaUIHost.h" />
    <ClInclude Include="..\Public\Manager\UI\MinimapHudPanel.h" />
    <ClInclude Include="..\Public\Manager\UI\MinimapTypes.h" />
    <ClInclude Include="..\Public\Manager\UI\UIAtlasManifest.h" />
    <ClInclude Include="..\Public\Manager\UI\UI_Manager.h" />
```

1-17. C:/Users/tnest/Desktop/Winters/Engine/Include/Engine.vcxproj.filters

기존 코드:

```xml
    <Filter Include="07. UI\03. Lua">
      <UniqueIdentifier>{cc00b8aa-7bce-46f9-87f1-59e1ea84d9f0}</UniqueIdentifier>
    </Filter>
```

아래에 추가:

```xml
    <Filter Include="07. UI\04. Minimap">
      <UniqueIdentifier>{13e015da-5607-4d01-8ca8-bd567f37a2d8}</UniqueIdentifier>
    </Filter>
```

기존 `LuaUIHost.h` filter 항목 아래에 추가:

```xml
    <ClInclude Include="..\Public\Manager\UI\MinimapHudPanel.h">
      <Filter>07. UI\04. Minimap</Filter>
    </ClInclude>
    <ClInclude Include="..\Public\Manager\UI\MinimapTypes.h">
      <Filter>07. UI\04. Minimap</Filter>
    </ClInclude>
```

기존 `LuaUIHost.cpp` filter 항목 아래에 추가:

```xml
    <ClCompile Include="..\Private\Manager\UI\MinimapHudPanel.cpp">
      <Filter>07. UI\04. Minimap</Filter>
    </ClCompile>
```

1-18. C:/Users/tnest/Desktop/Winters/Client/Include/Client.vcxproj

기존 코드:

```xml
    <ClCompile Include="..\Private\UI\MapTunerPanel.cpp" />
    <ClCompile Include="..\Private\UI\MinimapPanel.cpp" />
    <ClCompile Include="..\Private\UI\RenderDebug.cpp" />
```

아래로 교체:

```xml
    <ClCompile Include="..\Private\UI\InGameMinimapBridge.cpp" />
    <ClCompile Include="..\Private\UI\MapTunerPanel.cpp" />
    <ClCompile Include="..\Private\UI\MinimapPanel.cpp" />
    <ClCompile Include="..\Private\UI\RenderDebug.cpp" />
```

기존 코드:

```xml
    <ClInclude Include="..\Public\UI\ImageScenePresenter.h" />
    <ClInclude Include="..\Public\UI\MapTunerPanel.h" />
    <ClInclude Include="..\Public\UI\MinimapPanel.h" />
```

아래로 교체:

```xml
    <ClInclude Include="..\Public\UI\ImageScenePresenter.h" />
    <ClInclude Include="..\Public\UI\InGameMinimapBridge.h" />
    <ClInclude Include="..\Public\UI\MapTunerPanel.h" />
    <ClInclude Include="..\Public\UI\MinimapPanel.h" />
```

1-19. C:/Users/tnest/Desktop/Winters/Client/Include/Client.vcxproj.filters

기존 `MinimapPanel.h` filter 항목 아래에 추가:

```xml
    <ClInclude Include="..\Public\UI\InGameMinimapBridge.h">
      <Filter>05. UI\08. Minimap</Filter>
    </ClInclude>
```

기존 `MinimapPanel.cpp` filter 항목 아래에 추가:

```xml
    <ClCompile Include="..\Private\UI\InGameMinimapBridge.cpp">
      <Filter>05. UI\08. Minimap</Filter>
    </ClCompile>
```

1-20. C:/Users/tnest/Desktop/Winters/cmake/WintersEngine.cmake

기존 코드:

```cmake
WintersEngineSourceGroup("07. UI\\03. Lua"
    "/Engine/Private/Manager/UI/LuaUIHost\\.cpp$"
    "/Engine/Public/Manager/UI/LuaUIHost\\.h$"
)
```

아래에 추가:

```cmake
WintersEngineSourceGroup("07. UI\\04. Minimap"
    "/Engine/Private/Manager/UI/MinimapHudPanel\\.cpp$"
    "/Engine/Public/Manager/UI/MinimapHudPanel\\.h$"
    "/Engine/Public/Manager/UI/MinimapTypes\\.h$"
)
```

2. 검증

검증 명령:
- `git diff --check`
- `& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe' Winters.sln /m /p:Configuration=Debug /p:Platform=x64`

수동 확인:
- F10을 누르지 않은 기본 F5 실행에서 오른쪽 아래 미니맵이 항상 보이는지 확인.
- `Client/Private/Scene/InGameDebugBridge.cpp`의 legacy ImGui 미니맵이 제거되어 디버그 창 위치나 `m_bShowLegacyInGameDebug`에 의존하지 않는지 확인.
- `Resource/UI/minimap_atlas_manifest.json`에서 sprite path를 바꾸면 `CUI_Manager::Load_TextureSRV` 경로로 재로딩되는지 확인.
- `Resource/UI/minimap_layout.json` 기본값과 F8 UI Manager 튜너 값이 같은 계층에서 크기/오프셋/아이콘 크기를 조정하는지 확인.
- `minimap_base_clean.png`가 배경으로 깔리고 `CFogOfWarRenderer::GetMinimapOverlaySRV()`가 `MinimapFrameState.pFogOverlaySRV`를 통해서만 올라오는지 확인.
- `Engine/Private/Manager/UI/MinimapHudPanel.cpp`가 `Shared/GameSim` 또는 `CWorld`를 직접 include하지 않는지 확인.
- `Engine/Include/GameInstance.h`와 `Engine/Private/GameInstance.cpp`가 `CFogOfWarRenderer`를 알지 않고 `MinimapFrameState`만 받는지 확인.
- 챔피언 위치가 snapshot 이동과 동기화되고, 사망 시 회색 tint와 `minimap.champion.dead` overlay가 적용되는지 확인.
- 포탑/억제기/넥서스가 생존 중 표시되고, hp/dead flag 기준 파괴 시 숨겨지는지 확인.
- 정글몹은 배치 또는 snapshot 입력이 들어온 뒤 위치/생존/FOW 표시 조건을 확인.
- 아군 미니언은 작은 팀 컬러 점으로 항상 표시되고 snapshot 이동과 동기화되는지 확인.
- 적 미니언은 `VisibilityComponent.teamVisibilityMask` 기준으로 시야 안에서만 표시되고, `MinionStateComponent::Dead` 또는 hp 0 이하에서 즉시 사라지는지 확인.
- 적 챔피언/정글/구조물이 `VisibilityComponent.teamVisibilityMask` 기준으로 표시되는지 확인.

후속 동기화:
- Engine public header가 추가되므로 `UpdateLib.bat` 또는 기존 build prebuild로 `EngineSDK/inc` 동기화가 필요하다.
