#define WIN32_LEAN_AND_MEAN
#include "ActorHUDPanel.h"
#include "WintersMath.h"
#include "WintersPaths.h"

#include <Windows.h>

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <cctype>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <utility>

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

            outText.resize(static_cast<std::size_t>(fileSize));
            if (fileSize > 0)
                fread(outText.data(), 1, static_cast<std::size_t>(fileSize), pFile);
            fclose(pFile);
            return true;
        }

        bool ResolveReadPathW(const wchar_t* pPath, std::wstring& outPath)
        {
            outPath.clear();
            if (!pPath)
                return false;

            wchar_t resolvedPath[MAX_PATH] = {};
            if (WintersResolveContentPath(pPath, resolvedPath, MAX_PATH))
            {
                outPath = resolvedPath;
                return true;
            }

            wchar_t fullPath[MAX_PATH] = {};
            const DWORD got = GetFullPathNameW(pPath, MAX_PATH, fullPath, nullptr);
            if (got > 0 && got < MAX_PATH)
            {
                outPath = fullPath;
                return true;
            }

            return false;
        }

        bool WriteTextFileW(const wchar_t* pPath, const std::string& text)
        {
            if (!pPath)
                return false;

            FILE* pFile = nullptr;
            if (_wfopen_s(&pFile, pPath, L"wb") != 0 || !pFile)
                return false;

            const std::size_t written = text.empty()
                ? 0u
                : fwrite(text.data(), 1, text.size(), pFile);
            fclose(pFile);
            return written == text.size();
        }

        bool ResolveSourceLayoutPath(std::wstring& outPath)
        {
            outPath.clear();

            wchar_t exePath[MAX_PATH] = {};
            const DWORD n = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            if (n == 0 || n >= MAX_PATH)
                return false;

            wchar_t* const pLastSlash = wcsrchr(exePath, L'\\');
            if (!pLastSlash)
                return false;
            *(pLastSlash + 1) = L'\0';

            std::wstring candidate = std::wstring(exePath) +
                L"..\\Resource\\UI\\hud_irelia_layout.json";
            wchar_t fullPath[MAX_PATH] = {};
            const DWORD got = GetFullPathNameW(candidate.c_str(), MAX_PATH, fullPath, nullptr);
            if (got == 0 || got >= MAX_PATH)
                return false;

            outPath = fullPath;
            return true;
        }

        bool SamePathInsensitive(const std::wstring& lhs, const std::wstring& rhs)
        {
            if (lhs.empty() || rhs.empty())
                return false;
            return _wcsicmp(lhs.c_str(), rhs.c_str()) == 0;
        }

        Vec4 WhiteVec(f32_t alpha = 1.f)
        {
            return Vec4(1.f, 1.f, 1.f, WintersMath::Clamp01(alpha));
        }

        class JsonCursor
        {
        public:
            explicit JsonCursor(const std::string& text)
                : m_Text(text)
            {
            }

            bool FindKey(const char* pKey)
            {
                if (!pKey)
                    return false;

                std::string needle = "\"";
                needle += pKey;
                needle += "\"";
                const std::size_t pos = m_Text.find(needle, m_Pos);
                if (pos == std::string::npos)
                    return false;

                m_Pos = pos + needle.size();
                SkipSpace();
                if (!Consume(':'))
                    return false;
                SkipSpace();
                return true;
            }

            bool Consume(char expected)
            {
                SkipSpace();
                if (m_Pos >= m_Text.size() || m_Text[m_Pos] != expected)
                    return false;
                ++m_Pos;
                return true;
            }

            bool Peek(char expected)
            {
                SkipSpace();
                return m_Pos < m_Text.size() && m_Text[m_Pos] == expected;
            }

            bool ParseString(std::string& out)
            {
                SkipSpace();
                if (m_Pos >= m_Text.size() || m_Text[m_Pos] != '"')
                    return false;

                ++m_Pos;
                out.clear();
                while (m_Pos < m_Text.size())
                {
                    const char c = m_Text[m_Pos++];
                    if (c == '"')
                        return true;
                    if (c == '\\' && m_Pos < m_Text.size())
                    {
                        out.push_back(m_Text[m_Pos++]);
                        continue;
                    }
                    out.push_back(c);
                }
                return false;
            }

            bool ParseNumber(f32_t& out)
            {
                SkipSpace();
                if (m_Pos >= m_Text.size())
                    return false;

                char* pEnd = nullptr;
                out = std::strtof(m_Text.c_str() + m_Pos, &pEnd);
                if (pEnd == m_Text.c_str() + m_Pos)
                    return false;

                m_Pos = static_cast<std::size_t>(pEnd - m_Text.c_str());
                return true;
            }

            bool SkipValue()
            {
                SkipSpace();
                if (m_Pos >= m_Text.size())
                    return false;

                if (m_Text[m_Pos] == '"')
                {
                    std::string ignored;
                    return ParseString(ignored);
                }

                if (m_Text[m_Pos] == '{')
                    return SkipCompound('{', '}');
                if (m_Text[m_Pos] == '[')
                    return SkipCompound('[', ']');

                while (m_Pos < m_Text.size() &&
                    m_Text[m_Pos] != ',' &&
                    m_Text[m_Pos] != '}' &&
                    m_Text[m_Pos] != ']')
                {
                    ++m_Pos;
                }
                return true;
            }

        private:
            void SkipSpace()
            {
                while (m_Pos < m_Text.size() &&
                    std::isspace(static_cast<unsigned char>(m_Text[m_Pos])))
                {
                    ++m_Pos;
                }
            }

            bool SkipCompound(char open, char close)
            {
                if (!Consume(open))
                    return false;

                i32_t depth = 1;
                bool inString = false;
                while (m_Pos < m_Text.size())
                {
                    const char c = m_Text[m_Pos++];
                    if (inString)
                    {
                        if (c == '\\' && m_Pos < m_Text.size())
                            ++m_Pos;
                        else if (c == '"')
                            inString = false;
                        continue;
                    }

                    if (c == '"')
                        inString = true;
                    else if (c == open)
                        ++depth;
                    else if (c == close && --depth == 0)
                        return true;
                }

                return false;
            }

            const std::string& m_Text;
            std::size_t m_Pos = 0;
        };

        bool ParseArray(JsonCursor& json, f32_t* pValues, u32_t iCount)
        {
            if (!pValues || iCount == 0 || !json.Consume('['))
                return false;

            for (u32_t i = 0; i < iCount; ++i)
            {
                if (!json.ParseNumber(pValues[i]))
                    return false;
                if (i + 1 < iCount && !json.Consume(','))
                    return false;
            }

            while (!json.Peek(']'))
            {
                if (!json.Consume(',') || !json.SkipValue())
                    return false;
            }

            return json.Consume(']');
        }

        bool ParseLayoutElement(JsonCursor& json, CActorHudPanel::LayoutElement& out)
        {
            if (!json.Consume('{'))
                return false;

            while (!json.Peek('}'))
            {
                std::string key;
                if (!json.ParseString(key) || !json.Consume(':'))
                    return false;

                if (key == "ID" || key == "id")
                {
                    if (!json.ParseString(out.strID))
                        return false;
                }
                else if (key == "sprite")
                {
                    if (!json.ParseString(out.strSprite))
                        return false;
                }
                else if (key == "image")
                {
                    if (!json.ParseString(out.strImage))
                        return false;
                }
                else if (key == "bind")
                {
                    if (!json.ParseString(out.strBind))
                        return false;
                }
                else if (key == "clip")
                {
                    if (!json.ParseString(out.strClip))
                        return false;
                }
                else if (key == "shape")
                {
                    if (!json.ParseString(out.strShape))
                        return false;
                }
                else if (key == "visibleBind")
                {
                    if (!json.ParseString(out.strVisibleBind))
                        return false;
                }
                else if (key == "rect")
                {
                    f32_t values[4] = {};
                    if (!ParseArray(json, values, 4))
                        return false;
                    out.Rect.fX = values[0];
                    out.Rect.fY = values[1];
                    out.Rect.fW = values[2];
                    out.Rect.fH = values[3];
                }
                else if (key == "uv")
                {
                    f32_t values[4] = {};
                    if (!ParseArray(json, values, 4))
                        return false;
                    out.vUV = Vec4(values[0], values[1], values[2], values[3]);
                    out.bHasUV = true;
                }
                else if (!json.SkipValue())
                {
                    return false;
                }

                if (json.Peek(','))
                    json.Consume(',');
            }

            return json.Consume('}');
        }

        bool ParseLayoutText(JsonCursor& json, CActorHudPanel::LayoutText& out)
        {
            if (!json.Consume('{'))
                return false;

            while (!json.Peek('}'))
            {
                std::string key;
                if (!json.ParseString(key) || !json.Consume(':'))
                    return false;

                if (key == "ID" || key == "id")
                {
                    if (!json.ParseString(out.strID))
                        return false;
                }
                else if (key == "bind")
                {
                    if (!json.ParseString(out.strBind))
                        return false;
                }
                else if (key == "center")
                {
                    f32_t values[2] = {};
                    if (!ParseArray(json, values, 2))
                        return false;
                    out.fCenterX = values[0];
                    out.fCenterY = values[1];
                }
                else if (key == "fontScale")
                {
                    if (!json.ParseNumber(out.fFontScale))
                        return false;
                }
                else if (key == "align")
                {
                    if (!json.ParseString(out.strAlign))
                        return false;
                }
                else if (!json.SkipValue())
                {
                    return false;
                }

                if (json.Peek(','))
                    json.Consume(',');
            }

            return json.Consume('}');
        }

        std::string ToRoundedString(f32_t value)
        {
            const i32_t rounded = static_cast<i32_t>(value + ((value >= 0.f) ? 0.5f : -0.5f));
            return std::to_string(rounded);
        }

        std::string ToFixed2String(f32_t value)
        {
            char text[32]{};
            std::snprintf(text, sizeof(text), "%.2f", value);
            return text;
        }

        void DrawOutlinedText(ImDrawList* pDraw, ImFont* pFont, f32_t fFontSize,
            const ImVec2& pos, ImU32 col, const char* pText)
        {
            if (!pDraw || !pText || !pFont)
                return;

            constexpr f32_t kOutline = 1.35f;
            const ImU32 outlineCol = IM_COL32(0, 0, 0, 220);
            pDraw->AddText(pFont, fFontSize, ImVec2(pos.x - kOutline, pos.y), outlineCol, pText);
            pDraw->AddText(pFont, fFontSize, ImVec2(pos.x + kOutline, pos.y), outlineCol, pText);
            pDraw->AddText(pFont, fFontSize, ImVec2(pos.x, pos.y - kOutline), outlineCol, pText);
            pDraw->AddText(pFont, fFontSize, ImVec2(pos.x, pos.y + kOutline), outlineCol, pText);
            pDraw->AddText(pFont, fFontSize, pos, col, pText);
        }

        bool GetRatioForBind(const ActorHUDState& State, const std::string& bind, f32_t& outRatio)
        {
            if (bind == "hpRatio")
            {
                outRatio = (State.MaxHp > 0.f) ? (State.Hp / State.MaxHp) : 0.f;
                return true;
            }
            if (bind == "mpRatio")
            {
                outRatio = (State.MaxMp > 0.f) ? (State.Mp / State.MaxMp) : 0.f;
                return true;
            }
            if (bind == "passiveRatio")
            {
                outRatio = (State.PassiveMax > 0.f) ? (State.PassiveValue / State.PassiveMax) : 0.f;
                return true;
            }
            if (bind == "passiveShieldRatio")
            {
                outRatio = (State.PassiveShieldMax > 0.f) ? (State.PassiveShield / State.PassiveShieldMax) : 0.f;
                return true;
            }
            if (bind == "xpRatio")
            {
                outRatio = State.XpRatio;
                return true;
            }

            return false;
        }

        bool IsVisibleForBind(const ActorHUDState& State, const std::string& bind)
        {
            if (bind.empty())
                return true;
            if (bind == "usesManaBar")
                return State.ResourceKind == eUIResourceKind::Mana;
            if (bind == "usesEnergyBar")
                return State.ResourceKind == eUIResourceKind::Energy;
            if (bind == "usesPassiveBar")
                return State.ResourceKind == eUIResourceKind::Flow;
            if (bind == "hasResourceBar")
                return State.ResourceKind != eUIResourceKind::None;
            if (bind == "passiveShieldVisible")
                return State.ResourceKind == eUIResourceKind::Flow &&
                    State.PassiveShield > 0.f;
            if (bind == "shopOpen")
                return State.bShopOpen;
            return true;
        }

        ImU32 TextColorForBind(
            const ActorHUDState& State,
            const std::string& bind)
        {
            if (bind == "mpText")
            {
                if (State.ResourceKind == eUIResourceKind::Energy)
                    return IM_COL32(255, 221, 72, 255);
                if (State.ResourceKind == eUIResourceKind::Flow)
                    return IM_COL32(242, 246, 255, 255);
                return IM_COL32(190, 238, 255, 255);
            }
            if (bind == "gold")
                return IM_COL32(255, 217, 91, 255);
            if (bind == "level")
                return IM_COL32(245, 231, 177, 255);
            if (bind == "ad" || bind == "ap" || bind == "armor" || bind == "mr")
                return IM_COL32(220, 226, 214, 255);
            return IM_COL32(240, 245, 225, 255);
        }

        std::string TextForBind(const ActorHUDState& State, const std::string& bind)
        {
            if (bind == "hpText")
                return ToRoundedString(State.Hp) + " / " + ToRoundedString(State.MaxHp);
            if (bind == "mpText")
            {
                if (State.ResourceKind == eUIResourceKind::None)
                    return {};
                if (State.ResourceKind == eUIResourceKind::Flow)
                    return ToRoundedString(State.PassiveValue) + " / " + ToRoundedString(State.PassiveMax);
                return ToRoundedString(State.Mp) + " / " + ToRoundedString(State.MaxMp);
            }
            if (bind == "level")
                return std::to_string(static_cast<u32_t>(State.Level));
            if (bind == "gold")
                return std::to_string(State.Gold);
            if (bind == "ad")
                return ToRoundedString(State.Ad);
            if (bind == "ap")
                return ToRoundedString(State.Ap);
            if (bind == "armor")
                return ToRoundedString(State.Armor);
            if (bind == "mr")
                return ToRoundedString(State.Mr);
            if (bind == "attackSpeed")
                return ToFixed2String(State.AttackSpeed);
            if (bind == "abilityHaste")
                return ToRoundedString(State.AbilityHaste);
            if (bind == "critChance")
                return ToRoundedString(State.CritChance * 100.f) + "%";
            if (bind == "moveSpeed")
                return ToRoundedString(State.MoveSpeed);
            return {};
        }

        const char* NonEmptyText(const std::string& text)
        {
            return text.empty() ? "(empty)" : text.c_str();
        }

        void AppendJsonEscaped(std::string& out, const std::string& text)
        {
            out.push_back('"');
            for (const char c : text)
            {
                if (c == '"' || c == '\\')
                    out.push_back('\\');
                out.push_back(c);
            }
            out.push_back('"');
        }
    }

    void CActorHudPanel::Initialize(CUIRenderer* pRenderer, CUIAtlasManifest* pManifest)
    {
        m_pRenderer = pRenderer;
        m_pManifest = pManifest;
        if (m_Elements.empty())
            UseDefaultLayout();
    }

    bool_t CActorHudPanel::LoadLayout(const wchar_t* pPath)
    {
        std::wstring resolvedPath;
        ResolveReadPathW(pPath, resolvedPath);

        std::string text;
        if (!ReadTextFileW(pPath, text))
        {
            UseDefaultLayout();
            return false;
        }

        std::vector<LayoutElement> elements;
        std::vector<LayoutText> texts;
        f32_t referenceW = m_fReferenceW;
        f32_t referenceH = m_fReferenceH;
        f32_t referenceAlpha = m_fReferenceAlpha;

        JsonCursor root(text);
        if (root.FindKey("root") && root.Consume('{'))
        {
            while (!root.Peek('}'))
            {
                std::string key;
                if (!root.ParseString(key) || !root.Consume(':'))
                {
                    UseDefaultLayout();
                    return false;
                }

                if (key == "width")
                    root.ParseNumber(referenceW);
                else if (key == "height")
                    root.ParseNumber(referenceH);
                else if (!root.SkipValue())
                {
                    UseDefaultLayout();
                    return false;
                }

                if (root.Peek(','))
                    root.Consume(',');
            }
            if (!root.Consume('}'))
            {
                UseDefaultLayout();
                return false;
            }
        }

        JsonCursor reference(text);
        if (reference.FindKey("reference") && reference.Consume('{'))
        {
            while (!reference.Peek('}'))
            {
                std::string key;
                if (!reference.ParseString(key) || !reference.Consume(':'))
                {
                    UseDefaultLayout();
                    return false;
                }

                if (key == "alpha")
                    reference.ParseNumber(referenceAlpha);
                else if (!reference.SkipValue())
                {
                    UseDefaultLayout();
                    return false;
                }

                if (reference.Peek(','))
                    reference.Consume(',');
            }
            if (!reference.Consume('}'))
            {
                UseDefaultLayout();
                return false;
            }
        }

        JsonCursor elementCursor(text);
        if (elementCursor.FindKey("elements") && elementCursor.Consume('['))
        {
            while (!elementCursor.Peek(']'))
            {
                LayoutElement element{};
                if (!ParseLayoutElement(elementCursor, element))
                {
                    UseDefaultLayout();
                    return false;
                }
                elements.push_back(std::move(element));
                if (elementCursor.Peek(','))
                    elementCursor.Consume(',');
            }
            if (!elementCursor.Consume(']'))
            {
                UseDefaultLayout();
                return false;
            }
        }

        JsonCursor textCursor(text);
        if (textCursor.FindKey("texts") && textCursor.Consume('['))
        {
            while (!textCursor.Peek(']'))
            {
                LayoutText layoutText{};
                if (!ParseLayoutText(textCursor, layoutText))
                {
                    UseDefaultLayout();
                    return false;
                }
                texts.push_back(std::move(layoutText));
                if (textCursor.Peek(','))
                    textCursor.Consume(',');
            }
            if (!textCursor.Consume(']'))
            {
                UseDefaultLayout();
                return false;
            }
        }

        if (elements.empty())
        {
            UseDefaultLayout();
            return false;
        }

        m_fReferenceW = referenceW;
        m_fReferenceH = referenceH;
        m_fReferenceAlpha = WintersMath::Clamp01(referenceAlpha);
        m_Elements = std::move(elements);
        m_Texts = std::move(texts);
        m_strLoadedLayoutPath = std::move(resolvedPath);
        return true;
    }

    bool_t CActorHudPanel::SaveLayout()
    {
        const std::string text = BuildLayoutJson();

        bool_t bSavedLoaded = false;
        if (!m_strLoadedLayoutPath.empty())
            bSavedLoaded = WriteTextFileW(m_strLoadedLayoutPath.c_str(), text);

        std::wstring sourcePath;
        bool_t bSavedSource = false;
        if (ResolveSourceLayoutPath(sourcePath))
        {
            if (SamePathInsensitive(m_strLoadedLayoutPath, sourcePath))
                bSavedSource = bSavedLoaded;
            else
                bSavedSource = WriteTextFileW(sourcePath.c_str(), text);
        }

        if (bSavedLoaded || bSavedSource)
        {
            m_strLastSaveMessage = "Saved hud_irelia_layout.json.";
            return true;
        }

        m_strLastSaveMessage = "Save failed: layout path was not writable.";
        return false;
    }

    void CActorHudPanel::SetSkillIconTexture(u32_t iIndex, void* pSRV)
    {
        if (iIndex >= m_SkillIconSRVs.size())
            return;

        m_SkillIconSRVs[iIndex] = pSRV;
    }

    bool_t CActorHudPanel::FindElementScreenRect(const char* pID, f32_t fScreenW, f32_t fScreenH, LayoutRect& OutRect) const
    {
        if (!pID)
            return false;

        DrawRoot root{};
        if (!ComputeRoot(fScreenW, fScreenH, root))
            return false;

        for (const LayoutElement& element : m_Elements)
        {
            if (element.strID == pID)
            {
                OutRect.fX = root.fX + element.Rect.fX * root.fScaleX;
                OutRect.fY = root.fY + element.Rect.fY * root.fScaleY;
                OutRect.fW = element.Rect.fW * root.fScaleX;
                OutRect.fH = element.Rect.fH * root.fScaleY;
                return true;
            }
        }

        return false;
    }

    void CActorHudPanel::DrawRHI(const ActorHUDState& State, u32_t iScreenW, u32_t iScreenH)
    {
        if (!m_pRenderer || iScreenW == 0 || iScreenH == 0)
            return;

        DrawRoot root{};
        if (!ComputeRoot(static_cast<f32_t>(iScreenW), static_cast<f32_t>(iScreenH), root))
            return;

        for (const LayoutElement& element : m_Elements)
            DrawElementRHI(State, root, element);
    }

    void CActorHudPanel::DrawTextOverlay(const ActorHUDState& State)
    {
        if (m_Texts.empty())
            return;

        const ImVec2 display = ImGui::GetIO().DisplaySize;
        DrawRoot root{};
        if (!ComputeRoot(display.x, display.y, root))
            return;

        ImDrawList* pDraw = ImGui::GetForegroundDrawList();
        ImFont* pFont = m_pTextFont ? m_pTextFont : ImGui::GetFont();
        if (!pDraw || !pFont)
            return;

        for (const LayoutText& layoutText : m_Texts)
        {
            const std::string text = TextForBind(State, layoutText.strBind);
            if (text.empty())
                continue;

            const f32_t fontSize = ImGui::GetFontSize() * root.fScaleY * layoutText.fFontScale;
            const ImVec2 textSize = pFont->CalcTextSizeA(fontSize, FLT_MAX, 0.f, text.c_str());
            ImVec2 pos(
                root.fX + layoutText.fCenterX * root.fScaleX,
                root.fY + layoutText.fCenterY * root.fScaleY - textSize.y * 0.5f);
            if (layoutText.strAlign == "right")
                pos.x -= textSize.x;
            else if (layoutText.strAlign != "left")
                pos.x -= textSize.x * 0.5f;
            DrawOutlinedText(
                pDraw,
                pFont,
                fontSize,
                pos,
                TextColorForBind(State, layoutText.strBind),
                text.c_str());
        }
    }

    void CActorHudPanel::DrawLayoutTunerImGui()
    {
        if (!ImGui::CollapsingHeader("Actor HUD Layout"))
            return;

        ImGui::Text("Root %.0f x %.0f", m_fReferenceW, m_fReferenceH);
        ImGui::TextDisabled("Tune values here, then save the layout JSON.");
        if (ImGui::Button("Save Layout"))
            SaveLayout();
        ImGui::SameLine();
        if (ImGui::Button("Copy Whole Layout JSON"))
        {
            const std::string layout = BuildLayoutJson();
            ImGui::SetClipboardText(layout.c_str());
        }
        if (!m_strLastSaveMessage.empty())
            ImGui::TextDisabled("%s", m_strLastSaveMessage.c_str());

        if (ImGui::TreeNode("Elements"))
        {
            LayoutElement* pXpArcTrack = nullptr;
            LayoutElement* pXpArcFill = nullptr;
            for (LayoutElement& candidate : m_Elements)
            {
                if (candidate.strID == "portrait.xp.arc.track")
                    pXpArcTrack = &candidate;
                else if (candidate.strID == "portrait.xp.arc.fill")
                    pXpArcFill = &candidate;
            }

            if (pXpArcTrack && pXpArcFill)
            {
                f32_t groupX = pXpArcFill->Rect.fX;
                if (ImGui::DragFloat("XP arc group X", &groupX, 0.25f, -512.f, 1024.f, "%.2f"))
                {
                    const f32_t deltaX = groupX - pXpArcFill->Rect.fX;
                    pXpArcTrack->Rect.fX += deltaX;
                    pXpArcFill->Rect.fX += deltaX;
                }
                ImGui::TextDisabled("Moves XP track + fill together. Save Layout writes the runtime JSON.");
                ImGui::Separator();
            }

            if (m_Elements.empty())
            {
                ImGui::TextDisabled("No layout elements loaded");
            }
            else
            {
                if (m_iTunerSelectedElement < 0 ||
                    m_iTunerSelectedElement >= static_cast<i32_t>(m_Elements.size()))
                {
                    m_iTunerSelectedElement = 0;
                }

                std::string preview = m_Elements[static_cast<std::size_t>(m_iTunerSelectedElement)].strID;
                if (preview.empty())
                    preview = "(unnamed)";

                if (ImGui::BeginCombo("Element", preview.c_str()))
                {
                    for (std::size_t i = 0; i < m_Elements.size(); ++i)
                    {
                        const LayoutElement& element = m_Elements[i];
                        std::string label = element.strID.empty() ? "(unnamed)" : element.strID;
                        label += "##element";
                        label += std::to_string(i);
                        const bool_t bSelected = (m_iTunerSelectedElement == static_cast<i32_t>(i));
                        if (ImGui::Selectable(label.c_str(), bSelected))
                            m_iTunerSelectedElement = static_cast<i32_t>(i);
                        if (bSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                LayoutElement& element = m_Elements[static_cast<std::size_t>(m_iTunerSelectedElement)];
                ImGui::Text("ID: %s", NonEmptyText(element.strID));
                ImGui::Text("sprite: %s", NonEmptyText(element.strSprite));
                ImGui::Text("image: %s", NonEmptyText(element.strImage));
                ImGui::Text("bind: %s", NonEmptyText(element.strBind));
                ImGui::Text("shape: %s", NonEmptyText(element.strShape));
                ImGui::Text("visibleBind: %s", NonEmptyText(element.strVisibleBind));

                f32_t rect[4] =
                {
                    element.Rect.fX,
                    element.Rect.fY,
                    element.Rect.fW,
                    element.Rect.fH,
                };
                if (ImGui::DragFloat4("rect", rect, 0.25f, -512.f, 1024.f, "%.2f"))
                {
                    element.Rect.fX = rect[0];
                    element.Rect.fY = rect[1];
                    element.Rect.fW = rect[2];
                    element.Rect.fH = rect[3];
                }

                if (element.strID == "portrait.xp.arc.fill")
                {
                    ImGui::Checkbox("preview XP ratio", &m_bPreviewXpRatio);
                    if (m_bPreviewXpRatio)
                        ImGui::SliderFloat("XP preview", &m_fPreviewXpRatio, 0.f, 1.f, "%.2f");
                }

                f32_t uv[4] = { element.vUV.x, element.vUV.y, element.vUV.z, element.vUV.w };
                if (ImGui::DragFloat4("uv", uv, 0.001f, 0.f, 1.f, "%.4f"))
                {
                    element.vUV = Vec4(uv[0], uv[1], uv[2], uv[3]);
                    element.bHasUV = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Clear UV"))
                {
                    element.vUV = Vec4(0.f, 0.f, 1.f, 1.f);
                    element.bHasUV = false;
                }

                if (ImGui::Button("Copy Element JSON"))
                {
                    const char* pKey = element.strSprite.empty() ? "image" : "sprite";
                    const char* pValue = element.strSprite.empty()
                        ? element.strImage.c_str()
                        : element.strSprite.c_str();
                    char rectSnippet[192]{};
                    std::snprintf(
                        rectSnippet,
                        sizeof(rectSnippet),
                        "{\"ID\":\"%s\",\"%s\":\"%s\",\"rect\":[%.2f,%.2f,%.2f,%.2f]",
                        element.strID.c_str(),
                        pKey,
                        pValue,
                        element.Rect.fX,
                        element.Rect.fY,
                        element.Rect.fW,
                        element.Rect.fH);

                    std::string snippet = rectSnippet;
                    if (!element.strShape.empty())
                    {
                        snippet += ",\"shape\":\"";
                        snippet += element.strShape;
                        snippet += "\"";
                    }
                    if (!element.strBind.empty())
                    {
                        snippet += ",\"bind\":\"";
                        snippet += element.strBind;
                        snippet += "\"";
                    }
                    if (!element.strVisibleBind.empty())
                    {
                        snippet += ",\"visibleBind\":\"";
                        snippet += element.strVisibleBind;
                        snippet += "\"";
                    }
                    if (element.bHasUV)
                    {
                        char uvSnippet[128]{};
                        std::snprintf(
                            uvSnippet,
                            sizeof(uvSnippet),
                            ",\"uv\":[%.4f,%.4f,%.4f,%.4f]",
                            element.vUV.x,
                            element.vUV.y,
                            element.vUV.z,
                            element.vUV.w);
                        snippet += uvSnippet;
                    }
                    snippet += "}";
                    ImGui::SetClipboardText(snippet.c_str());
                }
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Texts"))
        {
            if (m_Texts.empty())
            {
                ImGui::TextDisabled("No layout texts loaded");
            }
            else
            {
                if (m_iTunerSelectedText < 0 ||
                    m_iTunerSelectedText >= static_cast<i32_t>(m_Texts.size()))
                {
                    m_iTunerSelectedText = 0;
                }

                std::string preview = m_Texts[static_cast<std::size_t>(m_iTunerSelectedText)].strID;
                if (preview.empty())
                    preview = "(unnamed)";

                if (ImGui::BeginCombo("Text", preview.c_str()))
                {
                    for (std::size_t i = 0; i < m_Texts.size(); ++i)
                    {
                        const LayoutText& text = m_Texts[i];
                        std::string label = text.strID.empty() ? "(unnamed)" : text.strID;
                        label += "##text";
                        label += std::to_string(i);
                        const bool_t bSelected = (m_iTunerSelectedText == static_cast<i32_t>(i));
                        if (ImGui::Selectable(label.c_str(), bSelected))
                            m_iTunerSelectedText = static_cast<i32_t>(i);
                        if (bSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                LayoutText& text = m_Texts[static_cast<std::size_t>(m_iTunerSelectedText)];
                ImGui::Text("ID: %s", NonEmptyText(text.strID));
                ImGui::Text("bind: %s", NonEmptyText(text.strBind));
                f32_t center[2] = { text.fCenterX, text.fCenterY };
                if (ImGui::DragFloat2("center", center, 0.25f, -512.f, 1024.f, "%.2f"))
                {
                    text.fCenterX = center[0];
                    text.fCenterY = center[1];
                }
                ImGui::DragFloat("fontScale", &text.fFontScale, 0.01f, 0.1f, 3.f, "%.2f");

                if (ImGui::Button("Copy Text JSON"))
                {
                    char snippet[384]{};
                    std::snprintf(
                        snippet,
                        sizeof(snippet),
                        "{\"ID\":\"%s\",\"bind\":\"%s\",\"center\":[%.2f,%.2f],\"fontScale\":%.2f%s%s%s}",
                        text.strID.c_str(),
                        text.strBind.c_str(),
                        text.fCenterX,
                        text.fCenterY,
                        text.fFontScale,
                        text.strAlign.empty() ? "" : ",\"align\":\"",
                        text.strAlign.empty() ? "" : text.strAlign.c_str(),
                        text.strAlign.empty() ? "" : "\"");
                    ImGui::SetClipboardText(snippet);
                }
            }
            ImGui::TreePop();
        }
    }

    std::string CActorHudPanel::BuildLayoutJson() const
    {
        std::string out;
        out.reserve(8192);

        char line[256]{};
        out += "{\n";
        std::snprintf(
            line,
            sizeof(line),
            "    \"root\": {\n"
            "        \"width\": %.0f,\n"
            "        \"height\": %.0f,\n"
            "        \"anchor\": \"bottom-center\"\n"
            "    },\n",
            m_fReferenceW,
            m_fReferenceH);
        out += line;

        std::snprintf(
            line,
            sizeof(line),
            "    \"reference\": {\n"
            "        \"alpha\": %.2f\n"
            "    },\n",
            m_fReferenceAlpha);
        out += line;

        out += "    \"elements\": [\n";
        for (std::size_t i = 0; i < m_Elements.size(); ++i)
        {
            const LayoutElement& element = m_Elements[i];
            out += "        {\n";
            out += "            \"ID\": ";
            AppendJsonEscaped(out, element.strID);

            if (!element.strSprite.empty())
            {
                out += ",\n            \"sprite\": ";
                AppendJsonEscaped(out, element.strSprite);
            }
            if (!element.strImage.empty())
            {
                out += ",\n            \"image\": ";
                AppendJsonEscaped(out, element.strImage);
            }
            if (!element.strShape.empty())
            {
                out += ",\n            \"shape\": ";
                AppendJsonEscaped(out, element.strShape);
            }
            if (!element.strVisibleBind.empty())
            {
                out += ",\n            \"visibleBind\": ";
                AppendJsonEscaped(out, element.strVisibleBind);
            }
            if (!element.strBind.empty())
            {
                out += ",\n            \"bind\": ";
                AppendJsonEscaped(out, element.strBind);
            }
            if (!element.strClip.empty())
            {
                out += ",\n            \"clip\": ";
                AppendJsonEscaped(out, element.strClip);
            }
            std::snprintf(
                line,
                sizeof(line),
                ",\n            \"rect\": [%.2f, %.2f, %.2f, %.2f]",
                element.Rect.fX,
                element.Rect.fY,
                element.Rect.fW,
                element.Rect.fH);
            out += line;

            if (element.bHasUV)
            {
                std::snprintf(
                    line,
                    sizeof(line),
                    ",\n            \"uv\": [%.4f, %.4f, %.4f, %.4f]",
                    element.vUV.x,
                    element.vUV.y,
                    element.vUV.z,
                    element.vUV.w);
                out += line;
            }

            out += "\n        }";
            if (i + 1 < m_Elements.size())
                out += ",";
            out += "\n";
        }
        out += "    ],\n";

        out += "    \"texts\": [\n";
        for (std::size_t i = 0; i < m_Texts.size(); ++i)
        {
            const LayoutText& text = m_Texts[i];
            out += "        {\n";
            out += "            \"ID\": ";
            AppendJsonEscaped(out, text.strID);
            out += ",\n            \"bind\": ";
            AppendJsonEscaped(out, text.strBind);
            std::snprintf(
                line,
                sizeof(line),
                ",\n            \"center\": [%.2f, %.2f],\n"
                "            \"fontScale\": %.2f",
                text.fCenterX,
                text.fCenterY,
                text.fFontScale);
            out += line;
            if (!text.strAlign.empty())
            {
                out += ",\n            \"align\": ";
                AppendJsonEscaped(out, text.strAlign);
            }
            out += "\n        }";
            if (i + 1 < m_Texts.size())
                out += ",";
            out += "\n";
        }
        out += "    ]\n";
        out += "}\n";

        return out;
    }

    void CActorHudPanel::UseDefaultLayout()
    {
        m_fReferenceW = 861.f;
        m_fReferenceH = 167.f;

        m_Elements.clear();
        m_Texts.clear();

        auto addElement = [&](const char* pID, const char* pSprite, const char* pImage,
            f32_t x, f32_t y, f32_t w, f32_t h, const char* pBind = nullptr,
            const char* pVisibleBind = nullptr, const char* pClip = nullptr)
        {
            LayoutElement element{};
            if (pID)
                element.strID = pID;
            if (pSprite)
                element.strSprite = pSprite;
            if (pImage)
                element.strImage = pImage;
            if (pBind)
                element.strBind = pBind;
            if (pVisibleBind)
                element.strVisibleBind = pVisibleBind;
            if (pClip)
                element.strClip = pClip;
            element.Rect = { x, y, w, h };
            m_Elements.push_back(std::move(element));
        };

        addElement("reference", nullptr, "reference", 0.f, 0.f, 861.f, 167.f);
        addElement("portrait.face", nullptr, "portrait", 167.75f, 61.25f, 82.f, 82.f);
        m_Elements.back().strShape = "circle";
        addElement("portrait.xp.arc.track", "portrait.xp.arc.track", nullptr,
            244.25f, 48.5f, 30.42f, 91.43f);
        addElement("portrait.xp.arc.fill", "portrait.xp.arc.fill", nullptr,
            244.25f, 48.5f, 30.42f, 91.43f, "xpRatio", nullptr, "maskBottomToTop");
        addElement("portrait.frame", "portrait.frame", nullptr,
            159.75f, 48.5f, 128.f, 128.f);
        addElement("hp.bg", "bar.empty", nullptr, 287.f, 124.f, 319.f, 16.f);
        addElement("hp.fill", "bar.hp.fill", nullptr, 289.f, 127.f, 315.f, 10.f, "hpRatio");
        addElement("mp.bg", "bar.empty", nullptr, 287.f, 143.f, 319.f, 14.f,
            nullptr, "hasResourceBar");
        addElement("mp.fill", "bar.mp.fill", nullptr, 289.f, 146.f, 315.f, 9.f, "mpRatio", "usesManaBar");
        addElement("energy.fill", "bar.energy.fill", nullptr, 289.f, 146.f, 315.f, 9.f, "mpRatio", "usesEnergyBar");
        addElement("passive.fill", nullptr, "passive.bar", 289.f, 146.f, 315.f, 9.f, "passiveRatio", "usesPassiveBar");
        addElement("passive.shield", nullptr, "passive.bar", 289.f, 146.f, 315.f, 9.f, "passiveShieldRatio", "passiveShieldVisible");
        addElement("skill.q", nullptr, "skill.q", 291.f, 61.f, 43.f, 43.f);
        addElement("skill.w", nullptr, "skill.w", 344.f, 61.f, 43.f, 43.f);
        addElement("skill.e", nullptr, "skill.e", 398.f, 61.f, 43.f, 43.f);
        addElement("skill.r", nullptr, "skill.r", 452.f, 61.f, 43.f, 43.f);

        auto addText = [&](const char* pID, const char* pBind, f32_t x, f32_t y, f32_t fontScale)
        {
            LayoutText text{};
            text.strID = pID;
            text.strBind = pBind;
            text.fCenterX = x;
            text.fCenterY = y;
            text.fFontScale = fontScale;
            m_Texts.push_back(std::move(text));
        };

        addText("hp.text", "hpText", 447.f, 122.f, 0.92f);
        addText("mp.text", "mpText", 447.f, 143.f, 0.92f);
        addText("level.text", "level", 241.f, 139.f, 1.0f);
        addText("gold.text", "gold", 695.f, 137.f, 0.96f);
    }

    bool_t CActorHudPanel::ComputeRoot(f32_t fScreenW, f32_t fScreenH, DrawRoot& OutRoot) const
    {
        if (fScreenW <= 0.f || fScreenH <= 0.f || m_fReferenceW <= 0.f || m_fReferenceH <= 0.f)
            return false;

        f32_t hudW = m_fReferenceW;
        f32_t hudH = m_fReferenceH;
        if (hudW > fScreenW - 24.f)
        {
            const f32_t scale = (fScreenW - 24.f) / hudW;
            hudW *= scale;
            hudH *= scale;
        }

        OutRoot.fW = hudW;
        OutRoot.fH = hudH;
        OutRoot.fX = (fScreenW - hudW) * 0.5f;
        OutRoot.fY = fScreenH - hudH;
        OutRoot.fScaleX = hudW / m_fReferenceW;
        OutRoot.fScaleY = hudH / m_fReferenceH;
        return true;
    }

    void CActorHudPanel::DrawElementRHI(const ActorHUDState& State, const DrawRoot& Root,
        const LayoutElement& Element)
    {
        if (!m_pRenderer)
            return;
        if (!IsVisibleForBind(State, Element.strVisibleBind))
            return;

        void* pSRV = nullptr;
        Vec4 uv = Element.bHasUV ? Element.vUV : Vec4(0.f, 0.f, 1.f, 1.f);
        Vec4 color = WhiteVec();

        if (!Element.strImage.empty())
        {
            if (Element.strImage == "reference")
            {
                if (!m_bShowReference)
                    return;

                pSRV = m_pReferenceSRV;
                color = WhiteVec(m_fReferenceAlpha);
            }
            else if (Element.strImage == "portrait")
            {
                pSRV = m_pPortraitSRV;
            }
            else if (Element.strImage == "skill.q")
            {
                pSRV = m_SkillIconSRVs[0];
            }
            else if (Element.strImage == "skill.w")
            {
                pSRV = m_SkillIconSRVs[1];
            }
            else if (Element.strImage == "skill.e")
            {
                pSRV = m_SkillIconSRVs[2];
            }
            else if (Element.strImage == "skill.r")
            {
                pSRV = m_SkillIconSRVs[3];
            }
            else if (Element.strImage == "passive.bar")
            {
                pSRV = m_pPassiveBarSRV;
            }
        }
        else if (!Element.strSprite.empty() && m_pManifest)
        {
            const UISpriteDef* pSprite = m_pManifest->FindSprite(Element.strSprite);
            if (!pSprite)
                return;

            const UIAtlasTextureDef* pTexture = m_pManifest->FindTexture(pSprite->strTextureID);
            if (!pTexture)
                return;

            pSRV = pTexture->pSRV;
            uv = m_pManifest->ResolveUVRect(*pSprite);
        }

        if (!pSRV)
            return;

        f32_t drawXInLayout = Element.Rect.fX;
        f32_t drawYInLayout = Element.Rect.fY;
        f32_t drawW = Element.Rect.fW;
        f32_t drawH = Element.Rect.fH;
        f32_t ratio = 1.f;
        if (GetRatioForBind(State, Element.strBind, ratio))
        {
            ratio = WintersMath::Clamp01(ratio);
            if (Element.strID == "portrait.xp.arc.fill" && m_bPreviewXpRatio)
                ratio = WintersMath::Clamp01(m_fPreviewXpRatio);

            if (Element.strClip == "bottomToTop")
            {
                const f32_t sourceV0 = uv.y;
                const f32_t sourceV1 = uv.w;
                drawYInLayout += drawH * (1.f - ratio);
                drawH *= ratio;
                uv.y = sourceV1 - (sourceV1 - sourceV0) * ratio;
            }
            else if (Element.strClip != "maskBottomToTop")
            {
                drawW *= ratio;
                uv.z = uv.x + (uv.z - uv.x) * ratio;
            }
        }

        if (drawW <= 0.f || drawH <= 0.f)
            return;

        const f32_t drawX = Root.fX + drawXInLayout * Root.fScaleX;
        const f32_t drawY = Root.fY + drawYInLayout * Root.fScaleY;
        const f32_t drawScaledW = drawW * Root.fScaleX;
        const f32_t drawScaledH = drawH * Root.fScaleY;

        if (Element.strClip == "maskBottomToTop")
        {
            m_pRenderer->DrawImageVerticalReveal(
                pSRV,
                drawX,
                drawY,
                drawScaledW,
                drawScaledH,
                uv,
                color,
                ratio);
            return;
        }

        if (Element.strShape == "circle")
        {
            m_pRenderer->DrawImageCircle(
                pSRV,
                drawX,
                drawY,
                drawScaledW,
                drawScaledH,
                uv,
                color);
            return;
        }

        m_pRenderer->DrawImage(
            pSRV,
            drawX,
            drawY,
            drawScaledW,
            drawScaledH,
            uv,
            color);
    }
}
