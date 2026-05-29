#define WIN32_LEAN_AND_MEAN
#include "Manager/UI/Font_Manager.h"
#include "WintersPaths.h"

#include <Windows.h>

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

NS_BEGIN(Engine)

namespace
{
	bool ResolveFontPathUtf8(const std::wstring& strPath, std::string& outPath)
	{
		outPath.clear();
		if (strPath.empty())
			return false;

		wchar_t resolvedPath[MAX_PATH] = {};
		const wchar_t* pPath = strPath.c_str();
		if (WintersResolveContentPath(strPath.c_str(), resolvedPath, MAX_PATH))
			pPath = resolvedPath;

		const int iRequired = WideCharToMultiByte(
			CP_UTF8,
			0,
			pPath,
			-1,
			nullptr,
			0,
			nullptr,
			nullptr);
		if (iRequired <= 1)
			return false;

		outPath.resize(static_cast<std::size_t>(iRequired), '\0');
		WideCharToMultiByte(
			CP_UTF8,
			0,
			pPath,
			-1,
			outPath.data(),
			iRequired,
			nullptr,
			nullptr);
		if (!outPath.empty() && outPath.back() == '\0')
			outPath.pop_back();
		return !outPath.empty();
	}
}

bool_t CFont_Manager::AddFont(const std::string& strTag, const std::wstring& strPath, float fSize)
{
	if (strTag.empty() || fSize <= 0.f || !ImGui::GetCurrentContext())
		return false;

	std::string strUtf8Path;
	if (!ResolveFontPathUtf8(strPath, strUtf8Path))
		return false;

	ImGuiIO& io = ImGui::GetIO();
	ImFontConfig Config{};
	Config.Flags |= ImFontFlags_NoLoadError;
	Config.OversampleH = 2;
	Config.OversampleV = 1;

	ImFont* pFont = io.Fonts->AddFontFromFileTTF(
		strUtf8Path.c_str(),
		fSize,
		&Config,
		io.Fonts->GetGlyphRangesKorean());
	if (!pFont)
		return false;

	m_Fonts[strTag] = pFont;
	if (!m_pFallbackFont)
		m_pFallbackFont = pFont;
	return true;
}

ImFont* CFont_Manager::FindFont(const std::string& strTag) const
{
	const auto iter = m_Fonts.find(strTag);
	return iter == m_Fonts.end() ? nullptr : iter->second;
}

ImFont* CFont_Manager::GetFallbackFont() const
{
	return m_pFallbackFont;
}

void CFont_Manager::Clear()
{
	m_Fonts.clear();
	m_pFallbackFont = nullptr;
}

NS_END
