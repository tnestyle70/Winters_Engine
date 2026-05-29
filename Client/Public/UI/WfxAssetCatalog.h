#pragma once

#include "Defines.h"
#include "FX/FxAsset.h"

#include <cstddef>
#include <string>
#include <vector>

namespace UI
{
	struct WfxAssetEntry final
	{
		wstring_t strPath{};
		std::string strPathText{};
		std::string strCueName{};
		std::string strChampion{};
		std::string strSkill{};
		std::string strRenderTypes{};
		std::string strLoadError{};
		std::vector<std::string> missingResources{};
		u32_t iEmitterCount = 0;
		bool_t bLoadSucceeded = false;
		bool_t bHasMissingResources = false;
	};

	struct WfxTextureEntry final
	{
		wstring_t strPath{};
		std::string strPathText{};
		std::string strChampion{};
		std::string strFolder{};
		std::string strName{};
		std::string strExtension{};
		bool_t bLikelyAtlasFrame = false;
	};

	class CWfxAssetCatalog final
	{
	public:
		void Clear();
		u32_t ScanDirectory(const wstring_t& strRootPath);
		u32_t ScanTextureDirectory(const wstring_t& strRootPath);

		const std::vector<WfxAssetEntry>& GetEntries() const { return m_Entries; }
		const std::vector<WfxTextureEntry>& GetTextureEntries() const { return m_TextureEntries; }
		const WfxAssetEntry* GetEntry(size_t iIndex) const;
		const WfxTextureEntry* GetTextureEntry(size_t iIndex) const;
		size_t GetEntryCount() const { return m_Entries.size(); }
		size_t GetTextureEntryCount() const { return m_TextureEntries.size(); }
		const std::string& GetLastError() const { return m_strLastError; }

	private:
		std::vector<WfxAssetEntry> m_Entries{};
		std::vector<WfxTextureEntry> m_TextureEntries{};
		std::string m_strLastError{};
	};
}
