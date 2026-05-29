#include "Manager/UI/UIAtlasManifest.h"
#include "WintersPaths.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
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

        std::wstring Utf8ToWide(const std::string& value)
        {
            std::wstring out;
            out.reserve(value.size());
            for (char c : value)
                out.push_back(static_cast<unsigned char>(c));
            return out;
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

            std::size_t Position() const { return m_Pos; }
            void SetPosition(std::size_t pos) { m_Pos = pos; }

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

        bool ParseTextureObject(JsonCursor& json, UIAtlasTextureDef& out)
        {
            if (!json.Consume('{'))
                return false;

            while (!json.Peek('}'))
            {
                std::string key;
                if (!json.ParseString(key) || !json.Consume(':'))
                    return false;

                if (key == "path")
                {
                    std::string path;
                    if (!json.ParseString(path))
                        return false;
                    out.strPath = Utf8ToWide(path);
                }
                else if (key == "width")
                {
                    f32_t value = 0.f;
                    if (!json.ParseNumber(value))
                        return false;
                    out.iWidth = static_cast<u32_t>(value);
                }
                else if (key == "height")
                {
                    f32_t value = 0.f;
                    if (!json.ParseNumber(value))
                        return false;
                    out.iHeight = static_cast<u32_t>(value);
                }
                else
                {
                    if (!json.SkipValue())
                        return false;
                }

                if (json.Peek(','))
                    json.Consume(',');
            }

            return json.Consume('}');
        }

        bool ParseSpriteObject(JsonCursor& json, UISpriteDef& out)
        {
            if (!json.Consume('{'))
                return false;

            while (!json.Peek('}'))
            {
                std::string key;
                if (!json.ParseString(key) || !json.Consume(':'))
                    return false;

                if (key == "texture")
                {
                    if (!json.ParseString(out.strTextureID))
                        return false;
                }
                else if (key == "x")
                {
                    if (!json.ParseNumber(out.fX))
                        return false;
                }
                else if (key == "y")
                {
                    if (!json.ParseNumber(out.fY))
                        return false;
                }
                else if (key == "w")
                {
                    if (!json.ParseNumber(out.fW))
                        return false;
                }
                else if (key == "h")
                {
                    if (!json.ParseNumber(out.fH))
                        return false;
                }
                else
                {
                    if (!json.SkipValue())
                        return false;
                }

                if (json.Peek(','))
                    json.Consume(',');
            }

            return json.Consume('}');
        }
    }

    bool_t CUIAtlasManifest::LoadFromJson(const wchar_t* pPath)
    {
        Clear();

        std::string text;
        if (!ReadTextFileW(pPath, text))
            return false;

        JsonCursor textures(text);
        if (textures.FindKey("textures") && textures.Consume('{'))
        {
            while (!textures.Peek('}'))
            {
                std::string id;
                UIAtlasTextureDef texture{};
                if (!textures.ParseString(id) ||
                    !textures.Consume(':') ||
                    !ParseTextureObject(textures, texture))
                {
                    return false;
                }

                m_Textures[id] = std::move(texture);
                if (textures.Peek(','))
                    textures.Consume(',');
            }
            if (!textures.Consume('}'))
                return false;
        }

        JsonCursor sprites(text);
        if (sprites.FindKey("sprites") && sprites.Consume('{'))
        {
            while (!sprites.Peek('}'))
            {
                std::string id;
                UISpriteDef sprite{};
                if (!sprites.ParseString(id) ||
                    !sprites.Consume(':') ||
                    !ParseSpriteObject(sprites, sprite))
                {
                    return false;
                }

                m_Sprites[id] = std::move(sprite);
                if (sprites.Peek(','))
                    sprites.Consume(',');
            }
            if (!sprites.Consume('}'))
                return false;
        }

        return !m_Textures.empty() && !m_Sprites.empty();
    }

    void CUIAtlasManifest::Clear()
    {
        m_Textures.clear();
        m_Sprites.clear();
    }

    void CUIAtlasManifest::ForEachTexture(
        const std::function<void(const std::string&, UIAtlasTextureDef&)>& Fn)
    {
        for (auto& Pair : m_Textures)
            Fn(Pair.first, Pair.second);
    }

    bool_t CUIAtlasManifest::SetTextureSRV(const std::string& strID, void* pSRV)
    {
        auto it = m_Textures.find(strID);
        if (it == m_Textures.end())
            return false;

        it->second.pSRV = pSRV;
        return true;
    }

    const UIAtlasTextureDef* CUIAtlasManifest::FindTexture(const std::string& strID) const
    {
        const auto it = m_Textures.find(strID);
        return it == m_Textures.end() ? nullptr : &it->second;
    }

    const UISpriteDef* CUIAtlasManifest::FindSprite(const std::string& strID) const
    {
        const auto it = m_Sprites.find(strID);
        return it == m_Sprites.end() ? nullptr : &it->second;
    }

    Vec4 CUIAtlasManifest::ResolveUVRect(const UISpriteDef& Sprite) const
    {
        const UIAtlasTextureDef* pTexture = FindTexture(Sprite.strTextureID);
        if (!pTexture || pTexture->iWidth == 0 || pTexture->iHeight == 0)
            return Vec4(0.f, 0.f, 1.f, 1.f);

        const f32_t w = static_cast<f32_t>(pTexture->iWidth);
        const f32_t h = static_cast<f32_t>(pTexture->iHeight);
        return Vec4(
            Sprite.fX / w,
            Sprite.fY / h,
            (Sprite.fX + Sprite.fW) / w,
            (Sprite.fY + Sprite.fH) / h);
    }
}
