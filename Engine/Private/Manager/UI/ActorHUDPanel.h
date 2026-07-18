#pragma once

#include "Manager/UI/ActorHUDState.h"
#include "Manager/UI/UIAtlasManifest.h"
#include "Renderer/UIRenderer.h"

#include <array>
#include <string>
#include <vector>

struct ImFont;

namespace Engine
{
    class CActorHudPanel final
    {
    public:
        void Initialize(CUIRenderer* pRenderer, CUIAtlasManifest* pManifest);
        bool_t LoadLayout(const wchar_t* pPath);
        bool_t SaveLayout();
        void DrawRHI(const ActorHUDState& State, u32_t iScreenW, u32_t iScreenH);
        void DrawTextOverlay(const ActorHUDState& State);
        void DrawLayoutTunerImGui();

        void SetReferenceTexture(void* pSRV) { m_pReferenceSRV = pSRV; }
        void SetPortraitTexture(void* pSRV) { m_pPortraitSRV = pSRV; }
        void SetPassiveBarTexture(void* pSRV) { m_pPassiveBarSRV = pSRV; }
        void SetSkillIconTexture(u32_t iIndex, void* pSRV);
        void SetTextFont(ImFont* pFont) { m_pTextFont = pFont; }
        void SetReferenceAlpha(f32_t fAlpha) { m_fReferenceAlpha = fAlpha; }
        void SetShowReference(bool_t bShow) { m_bShowReference = bShow; }

    public:
        struct LayoutRect
        {
            f32_t fX = 0.f;
            f32_t fY = 0.f;
            f32_t fW = 0.f;
            f32_t fH = 0.f;
        };

        bool_t FindElementScreenRect(const char* pID,
            f32_t fScreenW, f32_t fScreenH, LayoutRect& OutRect) const;

        struct LayoutElement
        {
            std::string strID;
            std::string strSprite;
            std::string strImage;
            std::string strBind;
            std::string strClip;
            std::string strShape;
            std::string strVisibleBind;
            LayoutRect Rect{};
            Vec4 vUV = Vec4(0.f, 0.f, 1.f, 1.f);
            bool_t bHasUV = false;
        };

        struct LayoutText
        {
            std::string strID;
            std::string strBind;
            f32_t fCenterX = 0.f;
            f32_t fCenterY = 0.f;
            f32_t fFontScale = 1.f;
            std::string strAlign;
        };

    private:
        struct DrawRoot
        {
            f32_t fX = 0.f;
            f32_t fY = 0.f;
            f32_t fW = 0.f;
            f32_t fH = 0.f;
            f32_t fScaleX = 1.f;
            f32_t fScaleY = 1.f;
        };

        void UseDefaultLayout();
        bool_t ComputeRoot(f32_t fScreenW, f32_t fScreenH, DrawRoot& OutRoot) const;
        void DrawElementRHI(const ActorHUDState& State, const DrawRoot& Root,
            const LayoutElement& Element);
        std::string BuildLayoutJson() const;

        CUIRenderer* m_pRenderer = nullptr;
        CUIAtlasManifest* m_pManifest = nullptr;
        std::vector<LayoutElement> m_Elements;
        std::vector<LayoutText> m_Texts;
        i32_t m_iTunerSelectedElement = 0;
        i32_t m_iTunerSelectedText = 0;
        void* m_pReferenceSRV = nullptr;
        void* m_pPortraitSRV = nullptr;
        void* m_pPassiveBarSRV = nullptr;
        ImFont* m_pTextFont = nullptr;
        std::array<void*, 4> m_SkillIconSRVs{};
        std::wstring m_strLoadedLayoutPath;
        std::string m_strLastSaveMessage;
        bool_t m_bShowReference = true;
        bool_t m_bPreviewXpRatio = false;
        f32_t m_fReferenceAlpha = 0.10f;
        f32_t m_fPreviewXpRatio = 0.50f;
        f32_t m_fReferenceW = 861.f;
        f32_t m_fReferenceH = 167.f;
    };
}
