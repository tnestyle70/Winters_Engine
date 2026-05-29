#pragma once

#include "WintersTypes.h"
#include "WintersMath.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace Engine
{
	struct UIAtlasTextureDef
	{
		std::wstring strPath;
		u32_t iWidth = 1;
		u32_t iHeight = 1;
		void* pSRV = nullptr;
	};

	struct UISpriteDef
	{
		std::string strTextureID;
		f32_t fX = 0.f;
		f32_t fY = 0.f;
		f32_t fW = 0.f;
		f32_t fH = 0.f;
	};

	class CUIAtlasManifest final
	{
	public:
		bool_t LoadFromJson(const wchar_t* pPath);
		void Clear();
		void ForEachTexture(const std::function<void(const std::string&, UIAtlasTextureDef&)>& Fn);
		bool_t SetTextureSRV(const std::string& strID, void* pSRV);
		const UIAtlasTextureDef* FindTexture(const std::string& strID) const;
		const UISpriteDef* FindSprite(const std::string& strID) const;
		Vec4 ResolveUVRect(const UISpriteDef& Sprite) const;
	private:
		std::unordered_map<std::string, UIAtlasTextureDef> m_Textures;
		std::unordered_map<std::string, UISpriteDef> m_Sprites;
	};
}
