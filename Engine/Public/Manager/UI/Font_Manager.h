#pragma once

#include <DirectXMath.h>
#include <string>
#include <unordered_map>

#include "Engine_Macro.h"
#include "Engine_Typedef.h"

struct ImFont;

NS_BEGIN(Engine)

class CFont_Manager final
{
public:
	bool_t AddFont(const std::string& strTag, const std::wstring& strPath, float fSize);
	ImFont* FindFont(const std::string& strTag) const;
	ImFont* GetFallbackFont() const;
	void Clear();

private:
	std::unordered_map<std::string, ImFont*> m_Fonts{};
	ImFont* m_pFallbackFont = nullptr;
};

NS_END
